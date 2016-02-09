/* This file is part of VoltDB.
 * Copyright (C) 2008-2015 VoltDB Inc.
 *
 * This file contains original code and/or modifications of original code.
 * Any modifications made by VoltDB Inc. are licensed under the following
 * terms and conditions:
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with VoltDB.  If not, see <http://www.gnu.org/licenses/>.
 */
/* Copyright (C) 2008 by H-Store Project
 * Brown University
 * Massachusetts Institute of Technology
 * Yale University
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */
#include "abstractjoinexecutor.h"
#include "executors/aggregateexecutor.h"
#include "execution/ProgressMonitorProxy.h"
#include "expressions/abstractexpression.h"
#include "plannodes/abstractjoinnode.h"
#include "storage/table.h"

using namespace std;
using namespace voltdb;

void AbstractJoinExecutor::CountingPostfilter::init(const AbstractExpression * postfilter, int limit, int offset) {
    m_postfilter = postfilter;
    m_limit = limit;
    m_offset = offset;
    m_tuple_skipped = 0;
    m_tuple_ctr = 0;
}

// Returns true if predicate evaluates to true and LIMIT/OFFSET conditions are satisfied.
bool AbstractJoinExecutor::CountingPostfilter::eval(const TableTuple& outer_tuple, const TableTuple& inner_tuple) {
    if (m_postfilter == NULL || m_postfilter->eval(&outer_tuple, &inner_tuple).isTrue()) {
        // Check if we have to skip this tuple because of offset
        if (m_tuple_skipped < m_offset) {
            m_tuple_skipped++;
            return false;
        }
        ++m_tuple_ctr;
        return true;
    }
    return false;
}

void AbstractJoinExecutor::outputTuple(TableTuple& join_tuple, ProgressMonitorProxy& pmp) {
    if (m_aggExec != NULL && m_aggExec->p_execute_tuple(join_tuple)) {
        m_postfilter.setAboveLimit();
    }
    m_tmpOutputTable->insertTempTuple(join_tuple);
    pmp.countdownProgress();
}

void AbstractJoinExecutor::p_init_null_tuples(Table* inner_table, Table* outer_table) {
    if (m_joinType != JOIN_TYPE_INNER) {
        assert(inner_table);
        m_null_inner_tuple.init(inner_table->schema());
        if (m_joinType == JOIN_TYPE_FULL) {
            assert(outer_table);
            m_null_outer_tuple.init(outer_table->schema());
        }
    }
}

bool AbstractJoinExecutor::p_init(AbstractPlanNode* abstract_node,
                              TempTableLimits* limits)
{
    VOLT_TRACE("init AbstractJoinExecutor Executor");

    AbstractJoinPlanNode* node = dynamic_cast<AbstractJoinPlanNode*>(abstract_node);
    assert(node);

    m_joinType = node->getJoinType();
    assert(m_joinType == JOIN_TYPE_INNER || m_joinType == JOIN_TYPE_LEFT || m_joinType == JOIN_TYPE_FULL);

    // Create output table based on output schema from the plan
    setTempOutputTable(limits);
    assert(m_tmpOutputTable);

    // Inline aggregation can be serial, partial or hash
    m_aggExec = voltdb::getInlineAggregateExecutor(m_abstractNode);

    return true;
}
