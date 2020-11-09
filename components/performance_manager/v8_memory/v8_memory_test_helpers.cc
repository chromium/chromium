// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/v8_memory/v8_memory_test_helpers.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/performance_manager/public/performance_manager.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "url/gurl.h"

namespace performance_manager {

namespace v8_memory {

using ::testing::_;

////////////////////////////////////////////////////////////////////////////////
// LenientMockV8DetailedMemoryReporter

LenientMockV8DetailedMemoryReporter::LenientMockV8DetailedMemoryReporter() =
    default;

LenientMockV8DetailedMemoryReporter::~LenientMockV8DetailedMemoryReporter() =
    default;

void LenientMockV8DetailedMemoryReporter::Bind(
    mojo::PendingReceiver<blink::mojom::V8DetailedMemoryReporter>
        pending_receiver) {
  return receiver_.Bind(std::move(pending_receiver));
}

////////////////////////////////////////////////////////////////////////////////
// V8MemoryTestBase

V8MemoryTestBase::V8MemoryTestBase()
    : bind_callback_(
          base::BindRepeating(&V8MemoryTestBase::BindReceiverOnMainSequence,
                              base::Unretained(this))) {
  internal::SetBindV8DetailedMemoryReporterCallbackForTesting(&bind_callback_);
}

V8MemoryTestBase::~V8MemoryTestBase() {
  internal::SetBindV8DetailedMemoryReporterCallbackForTesting(nullptr);
}

void V8MemoryTestBase::ReplyWithData(
    blink::mojom::PerProcessV8MemoryUsagePtr data,
    MockV8DetailedMemoryReporter::GetV8MemoryUsageCallback callback) {
  std::move(callback).Run(std::move(data));
}

void V8MemoryTestBase::DelayedReplyWithData(
    const base::TimeDelta& delay,
    blink::mojom::PerProcessV8MemoryUsagePtr data,
    MockV8DetailedMemoryReporter::GetV8MemoryUsageCallback callback) {
  GetMainThreadTaskRunner()->PostDelayedTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(data)), delay);
}

void V8MemoryTestBase::ExpectQuery(
    MockV8DetailedMemoryReporter* mock_reporter,
    base::RepeatingCallback<
        void(MockV8DetailedMemoryReporter::GetV8MemoryUsageCallback callback)>
        responder,
    ExpectedMode expected_mode) {
  EXPECT_CALL(*mock_reporter, GetV8MemoryUsage(expected_mode, _))
      .WillOnce(
          [this, responder](
              ExpectedMode mode,
              MockV8DetailedMemoryReporter::GetV8MemoryUsageCallback callback) {
            this->last_query_time_ = base::TimeTicks::Now();
            responder.Run(std::move(callback));
          });
}

void V8MemoryTestBase::ExpectQueryAndReply(
    MockV8DetailedMemoryReporter* mock_reporter,
    blink::mojom::PerProcessV8MemoryUsagePtr data,
    ExpectedMode expected_mode) {
  ExpectQuery(mock_reporter,
              base::BindRepeating(&V8MemoryTestBase::ReplyWithData,
                                  base::Unretained(this), base::Passed(&data)),
              expected_mode);
}

void V8MemoryTestBase::ExpectQueryAndDelayReply(
    MockV8DetailedMemoryReporter* mock_reporter,
    const base::TimeDelta& delay,
    blink::mojom::PerProcessV8MemoryUsagePtr data,
    ExpectedMode expected_mode) {
  ExpectQuery(
      mock_reporter,
      base::BindRepeating(&V8MemoryTestBase::DelayedReplyWithData,
                          base::Unretained(this), delay, base::Passed(&data)),
      expected_mode);
}

void V8MemoryTestBase::ExpectBindReceiver(
    MockV8DetailedMemoryReporter* mock_reporter,
    RenderProcessHostId expected_process_id) {
  using ::testing::Eq;
  using ::testing::Invoke;
  using ::testing::Property;
  using ::testing::WithArg;

  // Arg 0 is a
  // mojo::PendingReceiver<blink::mojom::V8DetailedMemoryReporter>. Pass it
  // to mock_reporter->Bind().
  //
  // Arg 1 is a RenderProcessHostProxy. Expect it to have the expected
  // process ID.
  EXPECT_CALL(*this,
              BindReceiverWithProxyHost(
                  _, Property(&RenderProcessHostProxy::render_process_host_id,
                              Eq(expected_process_id))))
      .WillOnce(WithArg<0>(
          Invoke(mock_reporter, &MockV8DetailedMemoryReporter::Bind)));
}

void V8MemoryTestBase::ExpectBindAndRespondToQuery(
    MockV8DetailedMemoryReporter* mock_reporter,
    blink::mojom::PerProcessV8MemoryUsagePtr data,
    RenderProcessHostId expected_process_id,
    ExpectedMode expected_mode) {
  ::testing::InSequence seq;
  ExpectBindReceiver(mock_reporter, expected_process_id);
  ExpectQueryAndReply(mock_reporter, std::move(data), expected_mode);
}

void V8MemoryTestBase::BindReceiverOnMainSequence(
    mojo::PendingReceiver<blink::mojom::V8DetailedMemoryReporter>
        pending_receiver,
    RenderProcessHostProxy proxy) {
  GetMainThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&V8MemoryTestBase::BindReceiverWithProxyHost,
                                base::Unretained(this),
                                std::move(pending_receiver), proxy));
}

// Storage for static members.
constexpr RenderProcessHostId V8MemoryTestBase::kTestProcessID;

////////////////////////////////////////////////////////////////////////////////
// V8MemoryPerformanceManagerTestHarness

V8MemoryPerformanceManagerTestHarness::V8MemoryPerformanceManagerTestHarness()
    : PerformanceManagerTestHarness(
          // Use MOCK_TIME so that ExpectQueryAndDelayReply can be used.
          base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

V8MemoryPerformanceManagerTestHarness::
    ~V8MemoryPerformanceManagerTestHarness() = default;

void V8MemoryPerformanceManagerTestHarness::SetUp() {
  PerformanceManagerTestHarness::SetUp();

  // Precondition: CallOnGraph must run on a different sequence. Note that
  // all tasks passed to CallOnGraph will only run when run_loop.Run() is
  // called.
  ASSERT_TRUE(GetMainThreadTaskRunner()->RunsTasksInCurrentSequence());
  base::RunLoop run_loop;
  PerformanceManager::CallOnGraph(
      FROM_HERE, base::BindLambdaForTesting([&] {
        EXPECT_FALSE(
            this->GetMainThreadTaskRunner()->RunsTasksInCurrentSequence());
        run_loop.Quit();
      }));
  run_loop.Run();

  // Set the active contents and simulate a navigation, which adds nodes to
  // the graph.
  content::IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());
  SetContents(CreateTestWebContents());
  main_frame_ = content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL(kMainFrameUrl));
  main_process_id_ = RenderProcessHostId(main_frame_->GetProcess()->GetID());
}

void V8MemoryPerformanceManagerTestHarness::CreateCrossProcessChildFrame() {
  // Since kMainFrameUrl has a different domain than kChildFrameUrl, the main
  // and child frames should end up in different processes.
  child_frame_ =
      content::RenderFrameHostTester::For(main_frame_)->AppendChild("frame1");
  child_frame_ = content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL(kChildFrameUrl), child_frame_);
  child_process_id_ = RenderProcessHostId(child_frame_->GetProcess()->GetID());
  ASSERT_NE(main_process_id_, child_process_id_);
}

scoped_refptr<base::SingleThreadTaskRunner>
V8MemoryPerformanceManagerTestHarness::GetMainThreadTaskRunner() {
  return task_environment()->GetMainThreadTaskRunner();
}

// Storage for static members.
constexpr char V8MemoryPerformanceManagerTestHarness::kMainFrameUrl[];
constexpr char V8MemoryPerformanceManagerTestHarness::kChildFrameUrl[];

////////////////////////////////////////////////////////////////////////////////
// Free functions

blink::mojom::PerProcessV8MemoryUsagePtr NewPerProcessV8MemoryUsage(
    size_t number_of_isolates) {
  auto data = blink::mojom::PerProcessV8MemoryUsage::New();
  for (size_t i = 0; i < number_of_isolates; ++i) {
    data->isolates.push_back(blink::mojom::PerIsolateV8MemoryUsage::New());
  }
  return data;
}

void AddIsolateMemoryUsage(const blink::LocalFrameToken& frame_token,
                           uint64_t bytes_used,
                           blink::mojom::PerIsolateV8MemoryUsage* isolate) {
  for (auto& entry : isolate->contexts) {
    if (entry->token == blink::ExecutionContextToken(frame_token)) {
      entry->bytes_used = bytes_used;
      return;
    }
  }

  auto context = blink::mojom::PerContextV8MemoryUsage::New();
  context->token = blink::ExecutionContextToken(frame_token);
  context->bytes_used = bytes_used;
  isolate->contexts.push_back(std::move(context));
}

}  // namespace v8_memory

}  // namespace performance_manager
