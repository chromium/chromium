// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/graph/policies/process_priority_policy.h"

#include <memory>

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "components/performance_manager/graph/graph_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/public/features.h"
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

base::TaskPriority ToTaskPriority(base::Process::Priority priority) {
  switch (priority) {
    case base::Process::Priority::kBestEffort:
      return base::TaskPriority::BEST_EFFORT;
    case base::Process::Priority::kUserVisible:
      return base::TaskPriority::USER_VISIBLE;
    case base::Process::Priority::kUserBlocking:
      return base::TaskPriority::USER_BLOCKING;
  }
}

void PostProcessNodePriority(content::RenderProcessHost* rph,
                             base::Process::Priority priority) {
  auto* rpud = RenderProcessUserData::GetForRenderProcessHost(rph);
  auto* process_node = rpud->process_node();

  PerformanceManager::CallOnGraph(
      FROM_HERE, base::BindLambdaForTesting([process_node, priority]() {
        process_node->set_priority(ToTaskPriority(priority));
      }));
}

// Tests ProcessPriorityPolicy in different threading configurations.
class ProcessPriorityPolicyTest : public PerformanceManagerTestHarness,
                                  public ::testing::WithParamInterface<bool> {
 public:
  ProcessPriorityPolicyTest() {
    scoped_feature_list_.InitWithFeatureState(features::kRunOnMainThreadSync,
                                              GetParam());
  }

  ProcessPriorityPolicyTest(const ProcessPriorityPolicyTest&) = delete;
  ProcessPriorityPolicyTest(ProcessPriorityPolicyTest&&) = delete;
  ProcessPriorityPolicyTest& operator=(const ProcessPriorityPolicyTest&) =
      delete;
  ProcessPriorityPolicyTest& operator=(ProcessPriorityPolicyTest&&) = delete;
  ~ProcessPriorityPolicyTest() override = default;

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
    task_environment()->RunUntilQuit();
    // RunUntilQuit() invalidated the old closure.
    quit_closure_ = task_environment()->QuitClosure();
  }

  // This is eventually invoked by the testing callback when the policy sets a
  // process priority.
  MOCK_METHOD(void,
              OnSetPriority,
              (content::RenderProcessHost*, base::Process::Priority));

 private:
  void OnSetPriorityWrapper(RenderProcessHostProxy rph_proxy,
                            base::Process::Priority priority) {
    OnSetPriority(rph_proxy.Get(), priority);
    quit_closure_.Run();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::RepeatingClosure quit_closure_ = task_environment()->QuitClosure();
};

INSTANTIATE_TEST_SUITE_P(, ProcessPriorityPolicyTest, ::testing::Bool());

}  // namespace

TEST_P(ProcessPriorityPolicyTest, GraphReflectedToRenderProcessHost) {
  // Set the active contents in the RenderViewHostTestHarness.
  SetContents(CreateTestWebContents());
  auto* rvh = web_contents()->GetPrimaryMainFrame()->GetRenderViewHost();
  DCHECK(rvh);
  auto* rph = rvh->GetProcess();
  DCHECK(rph);

  // Simulate a navigation so that graph nodes spring into existence.
  // Expect a foreground priority override to be set for process creation.
  // NOTE: This is going to change once we have provisional frames and the like,
  // and can calculate meaningful process startup priorities.
  EXPECT_CALL(*this,
              OnSetPriority(rph, base::Process::Priority::kUserBlocking));
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://www.foo.com/"));
  RunUntilOnSetPriority();

  // Toggle the priority and expect it to change.
  EXPECT_CALL(*this, OnSetPriority(rph, base::Process::Priority::kBestEffort));
  PostProcessNodePriority(rph, base::Process::Priority::kBestEffort);
  RunUntilOnSetPriority();

  testing::Mock::VerifyAndClearExpectations(this);
}

}  // namespace policies
}  // namespace performance_manager
