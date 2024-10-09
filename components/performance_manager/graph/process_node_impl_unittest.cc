// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/graph/process_node_impl.h"

#include <optional>

#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/process/process.h"
#include "base/task/task_traits.h"
#include "base/test/bind.h"
#include "base/trace_event/named_trigger.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/public/render_process_host_id.h"
#include "components/performance_manager/public/render_process_host_proxy.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "content/public/browser/background_tracing_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace {

class ProcessNodeImplTest : public GraphTestHarness {};

}  // namespace

TEST_F(ProcessNodeImplTest, SafeDowncast) {
  auto process = CreateNode<ProcessNodeImpl>();
  ProcessNode* node = process.get();
  EXPECT_EQ(process.get(), ProcessNodeImpl::FromNode(node));
  NodeBase* base = process.get();
  EXPECT_EQ(base, NodeBase::FromNode(node));
  EXPECT_EQ(static_cast<Node*>(node), base->ToNode());
}

using ProcessNodeImplDeathTest = ProcessNodeImplTest;

TEST_F(ProcessNodeImplDeathTest, SafeDowncast) {
  auto process = CreateNode<ProcessNodeImpl>();
  ASSERT_DEATH_IF_SUPPORTED(FrameNodeImpl::FromNodeBase(process.get()), "");
}

TEST_F(ProcessNodeImplTest, ProcessLifeCycle) {
  auto process_node = CreateNode<ProcessNodeImpl>();

  // Test the potential lifecycles of a process node.
  // First go to exited without an intervening process attached, as would happen
  // in the case the process fails to start.
  EXPECT_FALSE(process_node->GetProcess().IsValid());
  EXPECT_FALSE(process_node->GetExitStatus());
  constexpr int32_t kExitStatus = 0xF00;
  process_node->SetProcessExitStatus(kExitStatus);
  EXPECT_TRUE(process_node->GetExitStatus());
  EXPECT_EQ(kExitStatus, process_node->GetExitStatus().value());

  // Next go through PID->exit status.
  const base::Process self = base::Process::Current();
  const base::TimeTicks launch_time = base::TimeTicks::Now();
  process_node->SetProcess(self.Duplicate(), launch_time);
  EXPECT_TRUE(process_node->GetProcess().IsValid());
  EXPECT_EQ(self.Pid(), process_node->GetProcessId());
  EXPECT_EQ(launch_time, process_node->GetLaunchTime());

  // Resurrection should clear the exit status.
  EXPECT_FALSE(process_node->GetExitStatus());

  EXPECT_EQ(0U, process_node->GetPrivateFootprintKb());
  EXPECT_EQ(0U, process_node->GetResidentSetKb());

  process_node->set_private_footprint_kb(10u);
  process_node->set_resident_set_kb(20u);

  // Kill it again.
  // Verify that the process is cleared, but the properties stick around.
  process_node->SetProcessExitStatus(kExitStatus);
  EXPECT_FALSE(process_node->GetProcess().IsValid());
  EXPECT_EQ(self.Pid(), process_node->GetProcessId());

  EXPECT_EQ(launch_time, process_node->GetLaunchTime());
  EXPECT_EQ(10u, process_node->GetPrivateFootprintKb());
  EXPECT_EQ(20u, process_node->GetResidentSetKb());

  // Resurrect again and verify the launch time and measurements
  // are cleared.
  const base::TimeTicks launch2_time = launch_time + base::Seconds(1);
  process_node->SetProcess(self.Duplicate(), launch2_time);

  EXPECT_EQ(launch2_time, process_node->GetLaunchTime());
  EXPECT_EQ(0U, process_node->GetPrivateFootprintKb());
  EXPECT_EQ(0U, process_node->GetResidentSetKb());
}

namespace {

class LenientMockObserver : public ProcessNodeImpl::Observer {
 public:
  LenientMockObserver() = default;
  ~LenientMockObserver() override = default;

  MOCK_METHOD(void, OnProcessNodeAdded, (const ProcessNode*), (override));
  MOCK_METHOD(void, OnProcessLifetimeChange, (const ProcessNode*), (override));
  MOCK_METHOD(void,
              OnBeforeProcessNodeRemoved,
              (const ProcessNode*),
              (override));
  MOCK_METHOD(void,
              OnMainThreadTaskLoadIsLow,
              (const ProcessNode*),
              (override));
  MOCK_METHOD(void,
              OnPriorityChanged,
              (const ProcessNode*, base::TaskPriority),
              (override));
  MOCK_METHOD(void,
              OnAllFramesInProcessFrozen,
              (const ProcessNode*),
              (override));

  void SetNotifiedProcessNode(const ProcessNode* process_node) {
    notified_process_node_ = process_node;
  }

  const ProcessNode* TakeNotifiedProcessNode() {
    const ProcessNode* node = notified_process_node_;
    notified_process_node_ = nullptr;
    return node;
  }

 private:
  raw_ptr<const ProcessNode, DanglingUntriaged> notified_process_node_ =
      nullptr;
};

using MockObserver = ::testing::StrictMock<LenientMockObserver>;

using testing::_;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::Return;

}  // namespace

TEST_F(ProcessNodeImplTest, ObserverWorks) {
  MockObserver head_obs;
  MockObserver obs;
  MockObserver tail_obs;
  graph()->AddProcessNodeObserver(&head_obs);
  graph()->AddProcessNodeObserver(&obs);
  graph()->AddProcessNodeObserver(&tail_obs);

  // Remove observers at the head and tail of the list inside a callback, and
  // expect that `obs` is still notified correctly.
  EXPECT_CALL(head_obs, OnProcessNodeAdded(_)).WillOnce(InvokeWithoutArgs([&] {
    graph()->RemoveProcessNodeObserver(&head_obs);
    graph()->RemoveProcessNodeObserver(&tail_obs);
  }));
  // `tail_obs` should not be notified as it was removed.
  EXPECT_CALL(tail_obs, OnProcessNodeAdded(_)).Times(0);

  // Create a page node and expect a matching call to "OnProcessNodeAdded".
  EXPECT_CALL(obs, OnProcessNodeAdded(_))
      .WillOnce(Invoke(&obs, &MockObserver::SetNotifiedProcessNode));
  auto process_node = CreateNode<ProcessNodeImpl>();
  const ProcessNode* raw_process_node = process_node.get();
  EXPECT_EQ(raw_process_node, obs.TakeNotifiedProcessNode());

  // Test process creation and exit events.
  EXPECT_CALL(obs, OnProcessLifetimeChange(_));
  process_node->SetProcess(base::Process::Current(),
                           /* launch_time=*/base::TimeTicks::Now());
  EXPECT_CALL(obs, OnProcessLifetimeChange(_));
  process_node->SetProcessExitStatus(10);

  EXPECT_CALL(obs, OnMainThreadTaskLoadIsLow(_))
      .WillOnce(Invoke(&obs, &MockObserver::SetNotifiedProcessNode));
  process_node->SetMainThreadTaskLoadIsLow(true);
  EXPECT_EQ(raw_process_node, obs.TakeNotifiedProcessNode());

  // This call does nothing as the priority is initialized at HIGHEST.
  EXPECT_EQ(base::TaskPriority::HIGHEST, process_node->GetPriority());
  process_node->set_priority(base::TaskPriority::HIGHEST);

  // This call should fire a notification.
  EXPECT_CALL(obs, OnPriorityChanged(_, base::TaskPriority::HIGHEST));
  process_node->set_priority(base::TaskPriority::LOWEST);

  EXPECT_CALL(obs, OnAllFramesInProcessFrozen(_))
      .WillOnce(Invoke(&obs, &MockObserver::SetNotifiedProcessNode));
  process_node->OnAllFramesInProcessFrozenForTesting();
  EXPECT_EQ(raw_process_node, obs.TakeNotifiedProcessNode());

  // Re-entrant iteration should work.
  EXPECT_CALL(obs, OnMainThreadTaskLoadIsLow(raw_process_node))
      .WillOnce(InvokeWithoutArgs([&] {
        process_node->set_priority(base::TaskPriority::USER_BLOCKING);
      }));
  EXPECT_CALL(obs, OnPriorityChanged(raw_process_node, _));
  process_node->SetMainThreadTaskLoadIsLow(false);

  // Release the page node and expect a call to "OnBeforeProcessNodeRemoved".
  EXPECT_CALL(obs, OnBeforeProcessNodeRemoved(_))
      .WillOnce(Invoke(&obs, &MockObserver::SetNotifiedProcessNode));
  process_node.reset();
  EXPECT_EQ(raw_process_node, obs.TakeNotifiedProcessNode());

  graph()->RemoveProcessNodeObserver(&obs);
}

TEST_F(ProcessNodeImplTest, ConstructionArguments_Browser) {
  auto process_node = CreateNode<ProcessNodeImpl>(BrowserProcessNodeTag{});

  EXPECT_EQ(content::PROCESS_TYPE_BROWSER, process_node->GetProcessType());
}

TEST_F(ProcessNodeImplTest, ConstructionArguments_Renderer) {
  constexpr RenderProcessHostId kRenderProcessHostId =
      RenderProcessHostId(0xF0B);
  auto process_node = CreateNode<ProcessNodeImpl>(
      RenderProcessHostProxy::CreateForTesting(kRenderProcessHostId));

  EXPECT_EQ(content::PROCESS_TYPE_RENDERER, process_node->GetProcessType());
  EXPECT_EQ(kRenderProcessHostId,
            process_node->GetRenderProcessHostProxy().render_process_host_id());
}

TEST_F(ProcessNodeImplTest, ConstructionArguments_NonRenderer) {
  constexpr BrowserChildProcessHostId kBrowserChildProcessHostId =
      BrowserChildProcessHostId(0xF0B);
  auto process_node = CreateNode<ProcessNodeImpl>(
      content::PROCESS_TYPE_GPU, BrowserChildProcessHostProxy::CreateForTesting(
                                     kBrowserChildProcessHostId));

  EXPECT_EQ(content::PROCESS_TYPE_GPU, process_node->GetProcessType());
  EXPECT_EQ(kBrowserChildProcessHostId,
            process_node->GetBrowserChildProcessHostProxy()
                .browser_child_process_host_id());
}

TEST_F(ProcessNodeImplTest, PublicInterface) {
  auto process_node = CreateNode<ProcessNodeImpl>();
  const ProcessNode* public_process_node = process_node.get();

  // Create a small frame-tree so that GetFrameNodes can be well tested.
  auto page_node = CreateNode<PageNodeImpl>();
  auto main_frame_node =
      CreateFrameNodeAutoId(process_node.get(), page_node.get());
  auto child_frame_node = CreateFrameNodeAutoId(
      process_node.get(), page_node.get(), main_frame_node.get());


  const std::string kMetricsName("TestUtilityProcess");
  process_node->SetProcessMetricsName(kMetricsName);
  EXPECT_EQ(process_node->GetMetricsName(), kMetricsName);

  process_node->SetMainThreadTaskLoadIsLow(true);
  EXPECT_TRUE(process_node->GetMainThreadTaskLoadIsLow());

  // For properties returning nodes, simply test that the public interface impls
  // yield the same result as their private counterpart.

  auto frame_nodes = process_node->frame_nodes();
  auto public_frame_nodes = public_process_node->GetFrameNodes();
  EXPECT_EQ(frame_nodes.size(), public_frame_nodes.size());
  for (const FrameNodeImpl* frame_node : frame_nodes) {
    const FrameNode* public_frame_node = frame_node;
    EXPECT_TRUE(base::Contains(public_frame_nodes, public_frame_node));
  }
}

}  // namespace performance_manager
