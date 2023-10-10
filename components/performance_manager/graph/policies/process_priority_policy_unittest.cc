// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/graph/policies/process_priority_policy.h"

#include <memory>

#include "base/test/bind.h"
#include "components/performance_manager/graph/graph_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/render_process_user_data.h"
#include "components/performance_manager/test_support/performance_manager_test_harness.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/process_type.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {
namespace policies {

namespace {

// Returns a priority that will lead to an opposite process priority.
base::TaskPriority GetOppositePriority(base::TaskPriority priority) {
  switch (priority) {
    case base::TaskPriority::BEST_EFFORT:
      return base::TaskPriority::USER_BLOCKING;

    case base::TaskPriority::USER_VISIBLE:
    case base::TaskPriority::USER_BLOCKING:
      break;
  }

  return base::TaskPriority::BEST_EFFORT;
}

void PostToggleProcessNodePriority(content::RenderProcessHost* rph) {
  auto* rpud = RenderProcessUserData::GetForRenderProcessHost(rph);
  auto* process_node = rpud->process_node();

  PerformanceManager::CallOnGraph(
      FROM_HERE, base::BindLambdaForTesting([process_node]() {
        process_node->set_priority(
            GetOppositePriority(process_node->priority()));
      }));
}

class ProcessPriorityPolicyTest : public PerformanceManagerTestHarness {
 public:
  ProcessPriorityPolicyTest() {}
  ProcessPriorityPolicyTest(const ProcessPriorityPolicyTest&) = delete;
  ProcessPriorityPolicyTest(ProcessPriorityPolicyTest&&) = delete;
  ProcessPriorityPolicyTest& operator=(const ProcessPriorityPolicyTest&) =
      delete;
  ProcessPriorityPolicyTest& operator=(ProcessPriorityPolicyTest&&) = delete;
  ~ProcessPriorityPolicyTest() override {}

  void SetUp() override {
    PerformanceManagerTestHarness::SetUp();

    // It's safe to pass unretained as we clear the callback before being
    // torn down.
    ProcessPriorityPolicy::SetCallbackForTesting(
        base::BindRepeating(&ProcessPriorityPolicyTest::OnSetPriorityWrapper,
                            base::Unretained(this)));
  }

  void TearDown() override {
    ProcessPriorityPolicy::ClearCallbackForTesting();

    // Clean up the web contents, which should dispose of the page and frame
    // nodes involved.
    DeleteContents();

    PerformanceManagerTestHarness::TearDown();
  }

  void OnGraphCreated(GraphImpl* graph) override {
    graph->PassToGraph(std::make_unique<ProcessPriorityPolicy>());
  }

  void RunUntilOnSetPriority() {
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
    quit_closure_.Reset();
  }

  // This is eventually invoked by the testing callback when the policy sets a
  // process priority.
  MOCK_METHOD2(OnSetPriority, void(content::RenderProcessHost*, bool));

 private:
  void OnSetPriorityWrapper(RenderProcessHostProxy rph_proxy, bool foreground) {
    OnSetPriority(rph_proxy.Get(), foreground);
    quit_closure_.Run();
  }

  base::RepeatingClosure quit_closure_;
};

}  // namespace

TEST_F(ProcessPriorityPolicyTest, GraphReflectedToRenderProcessHost) {
  // Set the active contents in the RenderViewHostTestHarness.
  SetContents(CreateTestWebContents());
  auto* rvh = web_contents()->GetPrimaryMainFrame()->GetRenderViewHost();
  DCHECK(rvh);
  auto* rph = rvh->GetProcess();
  DCHECK(rph);

  // Simulate a navigation so that graph nodes spring into existence.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://www.foo.com/"));

  // Expect a background priority override to be set for process creation.
  // NOTE: This is going to change once we have provisional frames and the like,
  // and can calculate meaningful process startup priorities.
  EXPECT_CALL(*this, OnSetPriority(rph, false));
  RunUntilOnSetPriority();

  // Toggle the priority and expect it to change.
  PostToggleProcessNodePriority(rph);
  EXPECT_CALL(*this, OnSetPriority(rph, true));
  RunUntilOnSetPriority();

  testing::Mock::VerifyAndClearExpectations(this);
}

}  // namespace policies
}  // namespace performance_manager
