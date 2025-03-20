// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/v8_memory/v8_context_tracker.h"

#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/values.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/public/execution_context/execution_context_registry.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/node_data_describer_registry.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/public/render_process_host_id.h"
#include "components/performance_manager/v8_memory/v8_context_tracker_helpers.h"
#include "components/performance_manager/v8_memory/v8_context_tracker_internal.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/message.h"

namespace performance_manager {
namespace v8_memory {

namespace {

using ExecutionContextData = internal::ExecutionContextData;
using ProcessData = internal::ProcessData;
using RemoteFrameData = internal::RemoteFrameData;
using V8ContextData = internal::V8ContextData;

// A function that can be bound to as a mojo::ReportBadMessage
// callback. Only used in testing.
void FakeReportBadMessageForTesting(std::string_view error) {
  // This is used in DCHECK death tests, so must use a DCHECK.
  DCHECK(false) << "Bad mojo message: " << error;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// V8ContextTracker::ExecutionContextState implementation:

V8ContextTracker::ExecutionContextState::ExecutionContextState(
    const blink::ExecutionContextToken& token,
    mojom::IframeAttributionDataPtr iframe_attribution_data)
    : token(token),
      iframe_attribution_data(std::move(iframe_attribution_data)) {}

V8ContextTracker::ExecutionContextState::~ExecutionContextState() = default;

////////////////////////////////////////////////////////////////////////////////
// V8ContextTracker::V8ContextState implementation:

V8ContextTracker::V8ContextState::V8ContextState(
    const mojom::V8ContextDescription& description,
    ExecutionContextState* execution_context_state)
    : description(description),
      execution_context_state(execution_context_state) {}

V8ContextTracker::V8ContextState::~V8ContextState() = default;

////////////////////////////////////////////////////////////////////////////////
// V8ContextTracker implementation:

V8ContextTracker::V8ContextTracker()
    : data_store_(std::make_unique<DataStore>()) {}

V8ContextTracker::~V8ContextTracker() = default;

const V8ContextTracker::ExecutionContextState*
V8ContextTracker::GetExecutionContextState(
    const blink::ExecutionContextToken& token) const {
  return data_store_->Get(token);
}

const V8ContextTracker::V8ContextState* V8ContextTracker::GetV8ContextState(
    const blink::V8ContextToken& token) const {
  return data_store_->Get(token);
}

void V8ContextTracker::OnV8ContextCreated(
    base::PassKey<ProcessNodeImpl> key,
    ProcessNodeImpl* process_node,
    const mojom::V8ContextDescription& description,
    mojom::IframeAttributionDataPtr iframe_attribution_data) {
  DCHECK_ON_GRAPH_SEQUENCE(process_node->graph());

  // Validate the |description|.
  {
    auto result = ValidateV8ContextDescription(description);
    if (result != V8ContextDescriptionStatus::kValid) {
      LOG(ERROR) << "V8ContextDescriptionStatus = " << static_cast<int>(result);
      mojo::ReportBadMessage("invalid V8ContextDescription");
      return;
    }
  }

  // Validate the |iframe_attribution_data|.
  {
    std::optional<bool> result =
        ExpectIframeAttributionDataForV8ContextDescription(
            description, process_node->graph());
    if (result) {
      bool expected = *result;
      bool received = static_cast<bool>(iframe_attribution_data);
      if (expected != received) {
        LOG(ERROR) << "IframeAttributionData: expected = " << expected
                   << ", received = " << received;
        mojo::ReportBadMessage("invalid IframeAttributionData");
        return;
      }
    }
  }

  // Ensure that the V8Context creation notification isn't repeated.
  if (data_store_->Get(description.token)) {
    mojo::ReportBadMessage("repeated OnV8ContextCreated notification");
    return;
  }

  auto* process_data = ProcessData::GetOrCreate(process_node);

  // Get or create an ExecutionContextData if necessary. If it doesn't get
  // committed below it will safely tear itself down.
  std::unique_ptr<ExecutionContextData> ec_data;
  ExecutionContextData* raw_ec_data = nullptr;
  if (description.execution_context_token) {
    raw_ec_data = data_store_->Get(*description.execution_context_token);
    if (!raw_ec_data) {
      ec_data = std::make_unique<ExecutionContextData>(
          process_data, *description.execution_context_token,
          std::move(iframe_attribution_data));
      raw_ec_data = ec_data.get();
    }
  }

  if (raw_ec_data && raw_ec_data->process_data() != process_data) {
    mojo::ReportBadMessage(
        "OnV8ContextCreated refers to an out-of-process ExecutionContext");
    return;
  }

  // Create the V8ContextData.
  std::unique_ptr<V8ContextData> v8_data =
      std::make_unique<V8ContextData>(process_data, description, raw_ec_data);

  // Try to commit the objects.
  if (!data_store_->Pass(std::move(v8_data))) {
    mojo::ReportBadMessage("Multiple main worlds seen for an ExecutionContext");
    return;
  }
  if (ec_data)
    data_store_->Pass(std::move(ec_data));
}

void V8ContextTracker::OnV8ContextDetached(
    base::PassKey<ProcessNodeImpl> key,
    ProcessNodeImpl* process_node,
    const blink::V8ContextToken& v8_context_token) {
  DCHECK_ON_GRAPH_SEQUENCE(process_node->graph());

  auto* process_data = ProcessData::Get(process_node);
  auto* v8_data = data_store_->Get(v8_context_token);
  if (!process_data || !v8_data) {
    mojo::ReportBadMessage("unexpected OnV8ContextDetached");
    return;
  }

  if (!data_store_->MarkDetached(v8_data)) {
    mojo::ReportBadMessage("repeated OnV8ContextDetached");
    return;
  }
}

void V8ContextTracker::OnV8ContextDestroyed(
    base::PassKey<ProcessNodeImpl> key,
    ProcessNodeImpl* process_node,
    const blink::V8ContextToken& v8_context_token) {
  DCHECK_ON_GRAPH_SEQUENCE(process_node->graph());

  auto* process_data = ProcessData::Get(process_node);
  auto* v8_data = data_store_->Get(v8_context_token);
  if (!process_data || !v8_data) {
    mojo::ReportBadMessage("unexpected OnV8ContextDestroyed");
    return;
  }
  data_store_->Destroy(v8_context_token);
}

void V8ContextTracker::OnRemoteIframeAttached(
    base::PassKey<ProcessNodeImpl> key,
    FrameNodeImpl* parent_frame_node,
    const blink::RemoteFrameToken& remote_frame_token,
    mojom::IframeAttributionDataPtr iframe_attribution_data) {
  DCHECK(parent_frame_node);
  DCHECK_ON_GRAPH_SEQUENCE(parent_frame_node->graph());

  // RemoteFrameTokens are issued by the browser to a renderer, so if we receive
  // an IPC from a renderer using that token, then the corresponding
  // RenderFrameProxyHost and FrameNode should exist. If not, either a renderer
  // is sending bad data, or the frame has subsequently been torn down (the IPC
  // races with frame death). Since the two cases can't be distinguished, DON'T
  // report a bad message if the token can't be resolved. DO report a bad
  // message on other errors, such as when the token is for a frame with a
  // different parent.

  auto rph_id = parent_frame_node->process_node()->GetRenderProcessHostId();
  auto* rfh = content::RenderFrameHost::FromPlaceholderToken(
      rph_id.value(), remote_frame_token);
  if (!rfh) {
    return;
  }

  base::WeakPtr<const FrameNode> weak_frame_node =
      PerformanceManager::GetFrameNodeForRenderFrameHost(rfh);
  if (!weak_frame_node) {
    return;
  }
  FrameNodeImpl* frame_node = FrameNodeImpl::FromNode(weak_frame_node.get());

  OnRemoteIframeAttachedImpl(mojo::GetBadMessageCallback(), frame_node,
                             parent_frame_node, remote_frame_token,
                             std::move(iframe_attribution_data));
}

void V8ContextTracker::OnRemoteIframeDetached(
    base::PassKey<ProcessNodeImpl> key,
    FrameNodeImpl* parent_frame_node,
    const blink::RemoteFrameToken& remote_frame_token) {
  DCHECK_ON_GRAPH_SEQUENCE(parent_frame_node->graph());
  OnRemoteIframeDetachedImpl(parent_frame_node, remote_frame_token);
}

void V8ContextTracker::OnRemoteIframeAttachedForTesting(
    FrameNodeImpl* frame_node,
    FrameNodeImpl* parent_frame_node,
    const blink::RemoteFrameToken& remote_frame_token,
    mojom::IframeAttributionDataPtr iframe_attribution_data) {
  OnRemoteIframeAttachedImpl(base::BindOnce(&FakeReportBadMessageForTesting),
                             frame_node, parent_frame_node, remote_frame_token,
                             std::move(iframe_attribution_data));
}

void V8ContextTracker::OnRemoteIframeDetachedForTesting(
    FrameNodeImpl* parent_frame_node,
    const blink::RemoteFrameToken& remote_frame_token) {
  OnRemoteIframeDetachedImpl(parent_frame_node, remote_frame_token);
}

size_t V8ContextTracker::GetExecutionContextCountForTesting() const {
  return data_store_->GetExecutionContextDataCount();
}

size_t V8ContextTracker::GetV8ContextCountForTesting() const {
  return data_store_->GetV8ContextDataCount();
}

size_t V8ContextTracker::GetDestroyedExecutionContextCountForTesting() const {
  return data_store_->GetDestroyedExecutionContextDataCount();
}

size_t V8ContextTracker::GetDetachedV8ContextCountForTesting() const {
  return data_store_->GetDetachedV8ContextDataCount();
}

void V8ContextTracker::OnBeforeExecutionContextRemoved(
    const execution_context::ExecutionContext* ec) {
  DCHECK_ON_GRAPH_SEQUENCE(ec->GetGraph());
  if (auto* ec_data = data_store_->Get(ec->GetToken()))
    data_store_->MarkDestroyed(ec_data);
}

void V8ContextTracker::OnPassedToGraph(Graph* graph) {
  DCHECK_ON_GRAPH_SEQUENCE(graph);

  graph->AddProcessNodeObserver(this);
  graph->GetNodeDataDescriberRegistry()->RegisterDescriber(this,
                                                           "V8ContextTracker");
  auto* registry =
      execution_context::ExecutionContextRegistry::GetFromGraph(graph);
  // We expect the registry to exist before we are passed to the graph.
  CHECK(registry);
  registry->AddObserver(this);
}

void V8ContextTracker::OnTakenFromGraph(Graph* graph) {
  DCHECK_ON_GRAPH_SEQUENCE(graph);

  auto* registry =
      execution_context::ExecutionContextRegistry::GetFromGraph(graph);
  CHECK(registry);
  registry->RemoveObserver(this);

  graph->GetNodeDataDescriberRegistry()->UnregisterDescriber(this);
  graph->RemoveProcessNodeObserver(this);
}

base::Value::Dict V8ContextTracker::DescribeFrameNodeData(
    const FrameNode* node) const {
  DCHECK_ON_GRAPH_SEQUENCE(node->GetGraph());

  size_t v8_context_count = 0;
  const auto* ec_data =
      data_store_->Get(blink::ExecutionContextToken(node->GetFrameToken()));
  if (ec_data)
    v8_context_count = ec_data->v8_context_count();

  base::Value::Dict dict;
  dict.Set("v8_context_count", static_cast<int>(v8_context_count));
  return dict;
}

base::Value::Dict V8ContextTracker::DescribeProcessNodeData(
    const ProcessNode* node) const {
  DCHECK_ON_GRAPH_SEQUENCE(node->GetGraph());

  size_t v8_context_count = 0;
  size_t detached_v8_context_count = 0;
  size_t execution_context_count = 0;
  size_t destroyed_execution_context_count = 0;
  const auto* process_data = ProcessData::Get(ProcessNodeImpl::FromNode(node));
  if (process_data) {
    v8_context_count = process_data->GetV8ContextDataCount();
    detached_v8_context_count = process_data->GetDetachedV8ContextDataCount();
    execution_context_count = process_data->GetExecutionContextDataCount();
    destroyed_execution_context_count =
        process_data->GetDestroyedExecutionContextDataCount();
  }

  base::Value::Dict dict;
  dict.Set("v8_context_count", static_cast<int>(v8_context_count));
  dict.Set("detached_v8_context_count",
           static_cast<int>(detached_v8_context_count));
  dict.Set("execution_context_count",
           static_cast<int>(execution_context_count));
  dict.Set("destroyed_execution_context_count",
           static_cast<int>(destroyed_execution_context_count));
  return dict;
}

base::Value::Dict V8ContextTracker::DescribeWorkerNodeData(
    const WorkerNode* node) const {
  DCHECK_ON_GRAPH_SEQUENCE(node->GetGraph());
  size_t v8_context_count = 0;
  const auto* ec_data =
      data_store_->Get(blink::ExecutionContextToken(node->GetWorkerToken()));
  if (ec_data)
    v8_context_count = ec_data->v8_context_count();

  base::Value::Dict dict;
  dict.Set("v8_context_count", static_cast<int>(v8_context_count));
  return dict;
}

void V8ContextTracker::OnBeforeProcessNodeRemoved(const ProcessNode* node) {
  DCHECK_ON_GRAPH_SEQUENCE(node->GetGraph());
  auto* process_node = ProcessNodeImpl::FromNode(node);
  auto* process_data = ProcessData::Get(process_node);
  if (process_data)
    process_data->TearDown();
}

void V8ContextTracker::OnRemoteIframeAttachedImpl(
    mojo::ReportBadMessageCallback bad_message_callback,
    FrameNodeImpl* frame_node,
    FrameNodeImpl* parent_frame_node,
    const blink::RemoteFrameToken& remote_frame_token,
    mojom::IframeAttributionDataPtr iframe_attribution_data) {
  DCHECK(bad_message_callback);
  DCHECK_ON_GRAPH_SEQUENCE(frame_node->graph());

  if (!frame_node->parent_frame_node()) {
    // This may happen for custom HTML elements. Ignore such calls.
    return;
  }

  if (frame_node->parent_frame_node() != parent_frame_node) {
    std::move(bad_message_callback)
        .Run("OnRemoteIframeAttached has wrong parent frame");
    return;
  }

  if (data_store_->Get(remote_frame_token)) {
    std::move(bad_message_callback).Run("repeated OnRemoteIframeAttached");
    return;
  }

  // Get or create an ExecutionContextData if necessary. If it doesn't get
  // committed below it will safely tear itself down.
  auto* process_data = ProcessData::GetOrCreate(frame_node->process_node());
  std::unique_ptr<ExecutionContextData> ec_data;
  blink::ExecutionContextToken ec_token(frame_node->GetFrameToken());
  auto* raw_ec_data = data_store_->Get(ec_token);
  if (!raw_ec_data) {
    ec_data =
        std::make_unique<ExecutionContextData>(process_data, ec_token, nullptr);
    raw_ec_data = ec_data.get();
  }

  if (raw_ec_data->remote_frame_data()) {
    std::move(bad_message_callback).Run("unexpected OnRemoteIframeAttached");
    return;
  }

  // This used to assert that `raw_ec_data` had no `iframe_attribution_data`
  // already attached. In general, the renderer should not send multiple updates
  // for a given RenderFrameHost parent <-> RenderFrameProxyHost child pairing.
  // However, when //content needs to undo a `CommitNavigation()` sent to a
  // speculative RenderFrameHost, the renderer ends up swapping in a
  // RenderFrameProxy with the same RemoteFrameToken back in. Allow it as an
  // unfortunate exception--but ignore the update to retain the previous
  // behavior. See https://crbug.com/1221955 for more background.
  if (!raw_ec_data->iframe_attribution_data) {
    // Attach the iframe data to the ExecutionContextData.
    // If there was already iframe data, keep the original data, to be
    // consistent with the behaviour of all other paths that ignore changes to
    // the `src` and `id` attributes.
    raw_ec_data->iframe_attribution_data = std::move(iframe_attribution_data);
  }

  // Create the RemoteFrameData reference to this context.
  auto* parent_process_data =
      ProcessData::GetOrCreate(frame_node->parent_frame_node()->process_node());
  std::unique_ptr<RemoteFrameData> rf_data = std::make_unique<RemoteFrameData>(
      parent_process_data, remote_frame_token, raw_ec_data);

  // Commit the objects.
  data_store_->Pass(std::move(rf_data));
  if (ec_data)
    data_store_->Pass(std::move(ec_data));
}

void V8ContextTracker::OnRemoteIframeDetachedImpl(
    FrameNodeImpl* parent_frame_node,
    const blink::RemoteFrameToken& remote_frame_token) {
  DCHECK_ON_GRAPH_SEQUENCE(parent_frame_node->graph());

  // TODO(https://crbug.com/40132061): This should call ReportBadMessage if the
  // remote frame still exists but `parent_frame_node` doesn't match it's
  // parent.

  // Look up the RemoteFrameData. This can fail because the notification to
  // clean up RemoteFrameData can race with process death, so ignore the message
  // if the data has already been cleaned up.
  auto* rf_data = data_store_->Get(remote_frame_token);
  if (!rf_data)
    return;

  data_store_->Destroy(remote_frame_token);
}

}  // namespace v8_memory
}  // namespace performance_manager
