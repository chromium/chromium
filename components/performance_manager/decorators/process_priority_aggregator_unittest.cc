// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/decorators/process_priority_aggregator.h"

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace {

using PriorityAndReason = execution_context_priority::PriorityAndReason;

static const char* kReason = FrameNodeImpl::kDefaultPriorityReason;

class ProcessPriorityAggregatorTest : public GraphTestHarness {
 public:
  using Super = GraphTestHarness;

  void SetUp() override {
    Super::SetUp();
    ppa_ = new ProcessPriorityAggregator();
    graph()->PassToGraph(base::WrapUnique(ppa_.get()));
  }

  void ExpectPriorityCounts(ProcessNodeImpl* process_node,
                            size_t user_visible_count,
                            size_t user_blocking_count) {
    auto& data = ProcessPriorityAggregator::Data::Get(process_node);
    EXPECT_EQ(user_visible_count, data.user_visible_count_for_testing());
    EXPECT_EQ(user_blocking_count, data.user_blocking_count_for_testing());
#if DCHECK_IS_ON()
    // In DCHECK mode the exact number of lowest priority contexts is tracked,
    // so IsEmpty only returns true when no ExecutionContexts exist. This is
    // only true during graph shutdown.
    EXPECT_FALSE(data.IsEmpty());
#else
    // In non-DCHECK builds only the user visible and user blocking contexts
    // are counted.
    EXPECT_EQ(user_visible_count == 0 && user_blocking_count == 0,
              data.IsEmpty());
#endif
  }

  raw_ptr<ProcessPriorityAggregator> ppa_ = nullptr;
};

}  // namespace

TEST_F(ProcessPriorityAggregatorTest, ProcessAggregation) {
  MockMultiplePagesAndWorkersWithMultipleProcessesGraph mock_graph(graph());

  auto& proc1 = mock_graph.process;
  auto& proc2 = mock_graph.other_process;
  auto& frame1_1 = mock_graph.frame;
  auto& frame1_2 = mock_graph.other_frame;
  auto& frame2_1 = mock_graph.child_frame;
  auto& worker1 = mock_graph.worker;
  auto& worker2 = mock_graph.other_worker;

  EXPECT_EQ(base::TaskPriority::LOWEST, proc1->GetPriority());
  EXPECT_EQ(base::TaskPriority::LOWEST, proc2->GetPriority());
  ExpectPriorityCounts(proc1.get(), 0, 0);
  ExpectPriorityCounts(proc2.get(), 0, 0);

  // Set the priority of a frame in process 1 to USER_VISIBLE.
  frame1_1->SetPriorityAndReason(
      PriorityAndReason(base::TaskPriority::USER_VISIBLE, kReason));
  EXPECT_EQ(base::TaskPriority::USER_VISIBLE, proc1->GetPriority());
  EXPECT_EQ(base::TaskPriority::LOWEST, proc2->GetPriority());
  ExpectPriorityCounts(proc1.get(), 1, 0);
  ExpectPriorityCounts(proc2.get(), 0, 0);

  // Set the priority of a frame in process 2 to USER_VISIBLE.
  frame2_1->SetPriorityAndReason(
      PriorityAndReason(base::TaskPriority::USER_VISIBLE, kReason));
  EXPECT_EQ(base::TaskPriority::USER_VISIBLE, proc1->GetPriority());
  EXPECT_EQ(base::TaskPriority::USER_VISIBLE, proc2->GetPriority());
  ExpectPriorityCounts(proc1.get(), 1, 0);
  ExpectPriorityCounts(proc2.get(), 1, 0);

  // Set the priority of another frame in process 1 to USER_BLOCKING. This
  // overwrites the vote from the first frame.
  frame1_2->SetPriorityAndReason(
      PriorityAndReason(base::TaskPriority::USER_BLOCKING, kReason));
  EXPECT_EQ(base::TaskPriority::USER_BLOCKING, proc1->GetPriority());
  EXPECT_EQ(base::TaskPriority::USER_VISIBLE, proc2->GetPriority());
  ExpectPriorityCounts(proc1.get(), 1, 1);
  ExpectPriorityCounts(proc2.get(), 1, 0);

  // Set the priority of a worker in process 2 to USER_BLOCKING. This overwrites
  // the vote from the sole frame in this process.
  worker2->SetPriorityAndReason(
      PriorityAndReason(base::TaskPriority::USER_BLOCKING, kReason));
  EXPECT_EQ(base::TaskPriority::USER_BLOCKING, proc1->GetPriority());
  EXPECT_EQ(base::TaskPriority::USER_BLOCKING, proc2->GetPriority());
  ExpectPriorityCounts(proc1.get(), 1, 1);
  ExpectPriorityCounts(proc2.get(), 1, 1);

  // Reduces the priority of the second frame in process 1 to USER_VISIBLE. Now
  // both frames in this process are at USER_VISIBLE.
  frame1_2->SetPriorityAndReason(
      PriorityAndReason(base::TaskPriority::USER_VISIBLE, kReason));
  EXPECT_EQ(base::TaskPriority::USER_VISIBLE, proc1->GetPriority());
  EXPECT_EQ(base::TaskPriority::USER_BLOCKING, proc2->GetPriority());
  ExpectPriorityCounts(proc1.get(), 2, 0);
  ExpectPriorityCounts(proc2.get(), 1, 1);

  // Reduces the priority of the worker in process 2 to LOWEST. The highest
  // execution context priority of that process is now due to the sole frame.
  worker2->SetPriorityAndReason(
      PriorityAndReason(base::TaskPriority::LOWEST, kReason));
  EXPECT_EQ(base::TaskPriority::USER_VISIBLE, proc1->GetPriority());
  EXPECT_EQ(base::TaskPriority::USER_VISIBLE, proc2->GetPriority());
  ExpectPriorityCounts(proc1.get(), 2, 0);
  ExpectPriorityCounts(proc2.get(), 1, 0);

  // Reduces the priority of the sole frame in process 2 to LOWEST. All
  // execution contexts in this process are now at LOWEST.
  frame2_1->SetPriorityAndReason(
      PriorityAndReason(base::TaskPriority::LOWEST, kReason));
  EXPECT_EQ(base::TaskPriority::USER_VISIBLE, proc1->GetPriority());
  EXPECT_EQ(base::TaskPriority::LOWEST, proc2->GetPriority());
  ExpectPriorityCounts(proc1.get(), 2, 0);
  ExpectPriorityCounts(proc2.get(), 0, 0);

  // Reduces the priority of the first frame in process 1 to LOWEST. The highest
  // execution priority of that process is now due to the second frame.
  frame1_1->SetPriorityAndReason(
      PriorityAndReason(base::TaskPriority::LOWEST, kReason));
  EXPECT_EQ(base::TaskPriority::USER_VISIBLE, proc1->GetPriority());
  EXPECT_EQ(base::TaskPriority::LOWEST, proc2->GetPriority());
  ExpectPriorityCounts(proc1.get(), 1, 0);
  ExpectPriorityCounts(proc2.get(), 0, 0);

  // Reduces the priority of the second frame in process 1 to LOWEST. All
  // execution contexts in this process are now at LOWEST.
  frame1_2->SetPriorityAndReason(
      PriorityAndReason(base::TaskPriority::LOWEST, kReason));
  EXPECT_EQ(base::TaskPriority::LOWEST, proc1->GetPriority());
  EXPECT_EQ(base::TaskPriority::LOWEST, proc2->GetPriority());
  ExpectPriorityCounts(proc1.get(), 0, 0);
  ExpectPriorityCounts(proc2.get(), 0, 0);

  // Set the priority of the worker in process 1. It is the execution context
  // with the highest priority and thus dictates the priority of this process.
  worker1->SetPriorityAndReason(
      PriorityAndReason(base::TaskPriority::USER_VISIBLE, kReason));
  EXPECT_EQ(base::TaskPriority::USER_VISIBLE, proc1->GetPriority());
  EXPECT_EQ(base::TaskPriority::LOWEST, proc2->GetPriority());
  ExpectPriorityCounts(proc1.get(), 1, 0);
  ExpectPriorityCounts(proc2.get(), 0, 0);

  // Reduces the priority of the worker in process 1 to LOWEST. All execution
  // contexts in this process are now at LOWEST.
  worker1->SetPriorityAndReason(
      PriorityAndReason(base::TaskPriority::LOWEST, kReason));
  EXPECT_EQ(base::TaskPriority::LOWEST, proc1->GetPriority());
  EXPECT_EQ(base::TaskPriority::LOWEST, proc2->GetPriority());
  ExpectPriorityCounts(proc1.get(), 0, 0);
  ExpectPriorityCounts(proc2.get(), 0, 0);
}

}  // namespace performance_manager
