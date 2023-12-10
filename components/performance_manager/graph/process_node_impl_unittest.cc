// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/graph/process_node_impl.h"

#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/process/process.h"
#include "base/test/bind.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/public/render_process_host_id.h"
#include "components/performance_manager/public/render_process_host_proxy.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "content/public/browser/background_tracing_config.h"
#include "content/public/browser/background_tracing_manager.h"
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
  LenientMockObserver() {}
  ~LenientMockObserver() override {}

  MOCK_METHOD1(OnProcessNodeAdded, void(const ProcessNode*));
  MOCK_METHOD1(OnProcessLifetimeChange, void(const ProcessNode*));
  MOCK_METHOD1(OnBeforeProcessNodeRemoved, void(const ProcessNode*));
  MOCK_METHOD1(OnMainThreadTaskLoadIsLow, void(const ProcessNode*));
  MOCK_METHOD2(OnPriorityChanged, void(const ProcessNode*, base::TaskPriority));
  MOCK_METHOD1(OnAllFramesInProcessFrozen, void(const ProcessNode*));

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
using testing::Return;

}  // namespace

TEST_F(ProcessNodeImplTest, ObserverWorks) {
  MockObserver obs;
  graph()->AddProcessNodeObserver(&obs);

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

  // This call does nothing as the priority is always at LOWEST.
  EXPECT_EQ(base::TaskPriority::LOWEST, process_node->GetPriority());
  process_node->set_priority(base::TaskPriority::LOWEST);

  // This call should fire a notification.
  EXPECT_CALL(obs, OnPriorityChanged(_, base::TaskPriority::LOWEST));
  process_node->set_priority(base::TaskPriority::HIGHEST);

  EXPECT_CALL(obs, OnAllFramesInProcessFrozen(_))
      .WillOnce(Invoke(&obs, &MockObserver::SetNotifiedProcessNode));
  process_node->OnAllFramesInProcessFrozenForTesting();
  EXPECT_EQ(raw_process_node, obs.TakeNotifiedProcessNode());

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
  //  yield the same result as their private counterpart.

  const auto& frame_nodes = process_node->frame_nodes();
  auto public_frame_nodes = public_process_node->GetFrameNodes();
  EXPECT_EQ(frame_nodes.size(), public_frame_nodes.size());
  for (const auto* frame_node : frame_nodes) {
    const FrameNode* public_frame_node = frame_node;
    EXPECT_TRUE(base::Contains(public_frame_nodes, public_frame_node));
  }

  decltype(public_frame_nodes) visited_frame_nodes;
  public_process_node->VisitFrameNodes(
      [&visited_frame_nodes](const FrameNode* frame_node) -> bool {
        visited_frame_nodes.insert(frame_node);
        return true;
      });
  EXPECT_EQ(public_frame_nodes, visited_frame_nodes);
}

namespace {

class LenientFakeBackgroundTracingManager
    : public content::BackgroundTracingManager {
 public:
  LenientFakeBackgroundTracingManager() { SetInstance(this); }
  ~LenientFakeBackgroundTracingManager() override { SetInstance(nullptr); }

  // Functions we want to intercept.
  MOCK_METHOD(bool, HasActiveScenario, (), (override));
  MOCK_METHOD(bool,
              DoEmitNamedTrigger,
              (const std::string& trigger_name),
              (override));

  // Functions we don't care about.
  void SetReceiveCallback(ReceiveCallback receive_callback) override {}
  bool InitializeScenarios(
      const perfetto::protos::gen::ChromeFieldTracingConfig& config,
      DataFiltering data_filtering) override {
    return true;
  }

  bool SetActiveScenario(
      std::unique_ptr<content::BackgroundTracingConfig> config,
      DataFiltering data_filtering) override {
    return true;
  }

  bool HasTraceToUpload() override { return false; }
  void GetTraceToUpload(
      base::OnceCallback<void(absl::optional<std::string>,
                              absl::optional<std::string>)> callback) override {
  }
  std::unique_ptr<content::BackgroundTracingConfig> GetBackgroundTracingConfig(
      const std::string& trial_name) override {
    return nullptr;
  }
  void SetSystemProfileRecorder(
      base::RepeatingCallback<std::string()> recorder) override {}
  void AbortScenarioForTesting() override {}
  void SaveTraceForTesting(std::string&& trace_data,
                           const std::string& scenario_name,
                           const std::string& rule_name,
                           const base::Token& uuid) override {}

  void DeleteTracesInDateRange(base::Time start, base::Time end) override {}
};

using FakeBackgroundTracingManager =
    ::testing::StrictMock<LenientFakeBackgroundTracingManager>;

}  // namespace

TEST_F(ProcessNodeImplTest, FireBackgroundTracingTriggerOnUI) {
  const std::string kTrigger1("trigger1");

  FakeBackgroundTracingManager manager;

  // Expect a new trigger to be registered and triggered.
  EXPECT_CALL(manager, DoEmitNamedTrigger(_));
  ProcessNodeImpl::FireBackgroundTracingTriggerOnUIForTesting(kTrigger1);
  testing::Mock::VerifyAndClear(&manager);
}

}  // namespace performance_manager
