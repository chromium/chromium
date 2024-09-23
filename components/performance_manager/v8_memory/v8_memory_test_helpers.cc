// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/v8_memory/v8_memory_test_helpers.h"

#include <utility>

#include "base/check.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/performance_manager/embedder/graph_features.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/mojom/v8_contexts.mojom.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/v8_memory/v8_context_tracker.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "url/gurl.h"
#include "url/origin.h"

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
    base::OnceCallback<
        void(MockV8DetailedMemoryReporter::GetV8MemoryUsageCallback callback)>
        responder,
    ExpectedMode expected_mode) {
  EXPECT_CALL(*mock_reporter, GetV8MemoryUsage(expected_mode, _))
      .WillOnce([this, responder = std::move(responder)](
                    ExpectedMode mode,
                    MockV8DetailedMemoryReporter::GetV8MemoryUsageCallback
                        callback) mutable {
        this->last_query_time_ = base::TimeTicks::Now();
        std::move(responder).Run(std::move(callback));
      });
}

void V8MemoryTestBase::ExpectQueryAndReply(
    MockV8DetailedMemoryReporter* mock_reporter,
    blink::mojom::PerProcessV8MemoryUsagePtr data,
    ExpectedMode expected_mode) {
  ExpectQuery(mock_reporter,
              base::BindOnce(&V8MemoryTestBase::ReplyWithData,
                             base::Unretained(this), std::move(data)),
              expected_mode);
}

void V8MemoryTestBase::ExpectQueryAndDelayReply(
    MockV8DetailedMemoryReporter* mock_reporter,
    const base::TimeDelta& delay,
    blink::mojom::PerProcessV8MemoryUsagePtr data,
    ExpectedMode expected_mode) {
  ExpectQuery(mock_reporter,
              base::BindOnce(&V8MemoryTestBase::DelayedReplyWithData,
                             base::Unretained(this), delay, std::move(data)),
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
          base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
  GetGraphFeatures().EnableV8ContextTracker();
}

V8MemoryPerformanceManagerTestHarness::
    ~V8MemoryPerformanceManagerTestHarness() = default;

void V8MemoryPerformanceManagerTestHarness::SetUp() {
  PerformanceManagerTestHarness::SetUp();

  if (!base::FeatureList::IsEnabled(features::kRunOnMainThreadSync)) {
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
  }

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
// WebMemoryTestHarness

WebMemoryTestHarness::WebMemoryTestHarness() = default;

WebMemoryTestHarness::~WebMemoryTestHarness() = default;

void WebMemoryTestHarness::SetUp() {
  GetGraphFeatures().EnableV8ContextTracker();
  Super::SetUp();
  process_ = CreateNode<ProcessNodeImpl>();
  other_process_ = CreateNode<ProcessNodeImpl>();
  pages_.push_back(CreateNode<PageNodeImpl>());
}

int WebMemoryTestHarness::GetNextUniqueId() {
  return next_unique_id_++;
}

FrameNodeImpl* WebMemoryTestHarness::AddFrameNodeImpl(
    std::optional<std::string> url,
    int browsing_instance_id,
    Bytes memory_usage,
    FrameNodeImpl* parent,
    FrameNodeImpl* opener,
    ProcessNodeImpl* process,
    std::optional<std::string> id_attribute,
    std::optional<std::string> src_attribute,
    Bytes canvas_memory_usage) {
  // If there's an opener, the new frame is also a new page.
  auto* page = pages_.front().get();
  if (opener) {
    pages_.push_back(CreateNode<PageNodeImpl>());
    page = pages_.back().get();
    page->SetOpenerFrameNode(opener);
  }

  int frame_routing_id = GetNextUniqueId();
  auto frame_token = blink::LocalFrameToken();
  auto frame = CreateNode<FrameNodeImpl>(
      process, page, parent, /*outer_document_for_fenced_frame=*/nullptr,
      frame_routing_id, frame_token,
      content::BrowsingInstanceId(browsing_instance_id));
  if (url) {
    // "about:blank" uses the parent's origin. url::Origin::Resolve() does the
    // right thing. Fall back to url::Origin::Create() if there's no parent
    // origin to resolve against.
    const GURL gurl(*url);
    const auto origin =
        parent && parent->GetOrigin().has_value()
            ? url::Origin::Resolve(gurl, parent->GetOrigin().value())
            : url::Origin::Create(gurl);
    frame->OnNavigationCommitted(gurl, origin,
                                 /*same_document=*/false,
                                 /*is_served_from_back_forward_cache=*/false);
  }
  if (memory_usage || canvas_memory_usage) {
    auto* data =
        V8DetailedMemoryExecutionContextData::CreateForTesting(frame.get());
    if (memory_usage)
      data->set_v8_bytes_used(memory_usage.value());
    if (canvas_memory_usage)
      data->set_canvas_bytes_used(canvas_memory_usage.value());
  }
  frames_.push_back(std::move(frame));
  FrameNodeImpl* frame_impl = frames_.back().get();

  // Create a V8ContextDescription with attribution data for this frame. (In
  // production this is done by PerformanceManager monitoring frame lifetime
  // events.)
  auto description = mojom::V8ContextDescription::New();
  description->token = blink::V8ContextToken();
  description->world_type = mojom::V8ContextWorldType::kMain;
  description->execution_context_token = frame_token;

  mojom::IframeAttributionDataPtr attribution;
  if (parent) {
    // Frame attribution attributes come from the frame's parent node, so
    // V8ContextTracker expects an IframeAttributionData. The attribute values
    // may be empty.
    attribution = mojom::IframeAttributionData::New();
    attribution->id = id_attribute;
    attribution->src = src_attribute;
  } else {
    // V8ContextTracker expects no IframeAttributionData.
    DCHECK(!id_attribute);
    DCHECK(!src_attribute);
  }

  // If the frame is in the same process as its parent include the attribution
  // in OnV8ContextCreated, otherwise it must be attached separately with
  // OnRemoteIframeAttached.
  DCHECK(frame_impl->process_node());
  if (parent && parent->process_node() != frame_impl->process_node()) {
    frame_impl->process_node()->OnV8ContextCreated(
        std::move(description), mojom::IframeAttributionDataPtr());
    V8ContextTracker::GetFromGraph(graph())->OnRemoteIframeAttachedForTesting(
        frame_impl, parent, blink::RemoteFrameToken(), std::move(attribution));
  } else {
    frame_impl->process_node()->OnV8ContextCreated(std::move(description),
                                                   std::move(attribution));
  }

  return frame_impl;
}

WorkerNodeImpl* WebMemoryTestHarness::AddWorkerNode(
    WorkerNode::WorkerType worker_type,
    std::string script_url,
    Bytes bytes,
    FrameNodeImpl* parent) {
  auto* worker_node = AddWorkerNodeImpl(
      worker_type, parent->GetOrigin().value_or(url::Origin()), script_url,
      bytes);
  worker_node->AddClientFrame(parent);
  return worker_node;
}

WorkerNodeImpl* WebMemoryTestHarness::AddWorkerNodeWithoutData(
    WorkerNode::WorkerType worker_type,
    FrameNodeImpl* parent) {
  auto* worker_node = AddWorkerNodeImpl(
      worker_type, parent->GetOrigin().value_or(url::Origin()));
  worker_node->AddClientFrame(parent);
  return worker_node;
}

WorkerNodeImpl* WebMemoryTestHarness::AddWorkerNode(
    WorkerNode::WorkerType worker_type,
    std::string script_url,
    Bytes bytes,
    WorkerNodeImpl* parent) {
  auto* worker_node =
      AddWorkerNodeImpl(worker_type, parent->GetOrigin(), script_url, bytes);
  worker_node->AddClientWorker(parent);
  return worker_node;
}

WorkerNodeImpl* WebMemoryTestHarness::AddWorkerNodeImpl(
    WorkerNode::WorkerType worker_type,
    const url::Origin& origin,
    std::string script_url,
    Bytes bytes) {
  auto worker_token = [worker_type]() -> blink::WorkerToken {
    switch (worker_type) {
      case WorkerNode::WorkerType::kDedicated:
        return blink::DedicatedWorkerToken();
      case WorkerNode::WorkerType::kShared:
        return blink::SharedWorkerToken();
      case WorkerNode::WorkerType::kService:
        return blink::ServiceWorkerToken();
    }
    NOTREACHED();
  }();
  auto worker_node = CreateNode<WorkerNodeImpl>(
      worker_type, process_.get(), /*browser_context_id=*/std::string(),
      worker_token, origin);
  if (!script_url.empty()) {
    worker_node->OnFinalResponseURLDetermined(GURL(script_url));
  }
  if (bytes) {
    V8DetailedMemoryExecutionContextData::CreateForTesting(worker_node.get())
        ->set_v8_bytes_used(*bytes);
  }
  workers_.push_back(std::move(worker_node));
  return workers_.back().get();
}

void WebMemoryTestHarness::SetBlinkMemory(Bytes bytes) {
  V8DetailedMemoryProcessData::GetOrCreateForTesting(process_node())
      ->set_blink_bytes_used(*bytes);
}

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

void AddIsolateMemoryUsage(blink::ExecutionContextToken token,
                           uint64_t bytes_used,
                           blink::mojom::PerIsolateV8MemoryUsage* isolate) {
  for (auto& entry : isolate->contexts) {
    if (entry->token == token) {
      entry->bytes_used = bytes_used;
      return;
    }
  }

  auto context = blink::mojom::PerContextV8MemoryUsage::New();
  context->token = token;
  context->bytes_used = bytes_used;
  isolate->contexts.push_back(std::move(context));
}

void AddIsolateCanvasMemoryUsage(
    blink::ExecutionContextToken token,
    uint64_t bytes_used,
    blink::mojom::PerIsolateV8MemoryUsage* isolate) {
  for (auto& entry : isolate->canvas_contexts) {
    if (entry->token == token) {
      entry->bytes_used = bytes_used;
      return;
    }
  }

  auto context = blink::mojom::PerContextCanvasMemoryUsage::New();
  context->token = token;
  context->bytes_used = bytes_used;
  isolate->canvas_contexts.push_back(std::move(context));
}

}  // namespace v8_memory

}  // namespace performance_manager
