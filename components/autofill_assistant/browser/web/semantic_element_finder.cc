// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/web/semantic_element_finder.h"

#include <utility>

#include "base/barrier_callback.h"
#include "base/time/time.h"
#include "components/autofill_assistant/browser/devtools/devtools_client.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/user_data.h"
#include "components/autofill_assistant/browser/user_data_util.h"
#include "components/autofill_assistant/browser/web/element.h"
#include "components/autofill_assistant/browser/web/element_finder_result.h"
#include "components/autofill_assistant/browser/web/js_filter_builder.h"
#include "components/autofill_assistant/browser/web/web_controller_util.h"
#include "components/autofill_assistant/content/browser/content_autofill_assistant_driver.h"
#include "components/autofill_assistant/content/common/autofill_assistant_agent.mojom.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace autofill_assistant {

namespace {

ElementFinderInfoProto::SemanticInferenceStatus
NodeDataStatusToSemanticInferenceStatus(
    mojom::NodeDataStatus node_data_status) {
  switch (node_data_status) {
    case mojom::NodeDataStatus::kSuccess:
      return ElementFinderInfoProto::SUCCESS;
    case mojom::NodeDataStatus::kUnexpectedError:
      return ElementFinderInfoProto::UNEXPECTED_ERROR;
    case mojom::NodeDataStatus::kInitializationError:
      return ElementFinderInfoProto::INITIALIZATION_ERROR;
    case mojom::NodeDataStatus::kModelLoadError:
      return ElementFinderInfoProto::MODEL_LOAD_ERROR;
    case mojom::NodeDataStatus::kModelLoadTimeout:
      return ElementFinderInfoProto::MODEL_LOAD_TIMEOUT;
  }
}

}  // namespace

SemanticElementFinder::SemanticElementFinder(
    content::WebContents* web_contents,
    DevtoolsClient* devtools_client,
    AnnotateDomModelService* annotate_dom_model_service,
    const Selector& selector)
    : WebContentsObserver(web_contents),
      web_contents_(web_contents),
      devtools_client_(devtools_client),
      annotate_dom_model_service_(annotate_dom_model_service),
      selector_(selector) {
  DCHECK(annotate_dom_model_service_);

  DCHECK_GT(selector_.proto.filters_size(), 0);
  DCHECK(selector_.proto.filters(0).filter_case() ==
         SelectorProto::Filter::kSemantic);
  filter_ = selector_.proto.filters(0).semantic();
}

SemanticElementFinder::~SemanticElementFinder() = default;

void SemanticElementFinder::GiveUpWithError(const ClientStatus& status) {
  DCHECK(!status.ok());
  if (!callback_) {
    return;
  }

  SendResult(status, ElementFinderResult::EmptyResult());
}

void SemanticElementFinder::ResultFound(const GlobalBackendNodeId& node_id,
                                        const std::string& object_id,
                                        const std::string& devtools_frame_id) {
  if (!callback_) {
    return;
  }

  ElementFinderResult result;
  result.SetRenderFrameHostGlobalId(node_id.host_id());
  result.SetObjectId(object_id);
  result.SetNodeFrameId(devtools_frame_id);
  result.SetBackendNodeId(node_id.backend_node_id());

  SendResult(OkClientStatus(), result);
}

void SemanticElementFinder::SendResult(const ClientStatus& status,
                                       const ElementFinderResult& result) {
  DCHECK(callback_);
  std::move(callback_).Run(status,
                           std::make_unique<ElementFinderResult>(result));
}

void SemanticElementFinder::Start(const ElementFinderResult& start_element,
                                  BaseElementFinder::Callback callback) {
  callback_ = std::move(callback);

  auto* start_frame = start_element.render_frame_host();
  if (!start_frame) {
    start_frame = web_contents_->GetPrimaryMainFrame();
  }
  RunAnnotateDomModel(start_frame);
}

ElementFinderInfoProto SemanticElementFinder::GetLogInfo() const {
  DCHECK(!callback_);  // Run after finish.

  ElementFinderInfoProto info;
  for (auto node_data_status : node_data_frame_status_) {
    info.mutable_semantic_inference_result()->add_status_per_frame(
        NodeDataStatusToSemanticInferenceStatus(node_data_status));
  }
  for (const auto& semantic_node_result : semantic_node_results_) {
    auto* predicted_element =
        info.mutable_semantic_inference_result()->add_predicted_elements();
    predicted_element->set_backend_node_id(
        semantic_node_result.backend_node_id());
    *predicted_element->mutable_semantic_filter() = filter_;
    // TODO(b/217160707): For the ignore_objective case this is not correct
    // and the inferred objective should be returned from the Agent and used
    // here.
  }

  return info;
}

void SemanticElementFinder::RunAnnotateDomModel(
    content::RenderFrameHost* start_frame) {
  DCHECK(expected_frame_ids_.empty());

  start_frame->ForEachRenderFrameHost([this](content::RenderFrameHost* host) {
    if (host->IsRenderFrameLive()) {
      expected_frame_ids_.insert(host->GetGlobalId());
    }
  });

  if (expected_frame_ids_.empty()) {
    GiveUpWithError(ClientStatus(ELEMENT_RESOLUTION_FAILED));
    return;
  }

  timer_ = std::make_unique<base::OneShotTimer>();
  timer_->Start(FROM_HERE, base::Milliseconds(filter_.model_timeout_ms()),
                base::BindOnce(&SemanticElementFinder::OnTimeout,
                               weak_ptr_factory_.GetWeakPtr()));

  for (const auto& host_id : expected_frame_ids_) {
    RunAnnotateDomModelOnFrame(host_id);
  }
}

void SemanticElementFinder::OnTimeout() {
  VLOG(1) << "AnnotateDomModel timeout.";
  Finalize();
}

void SemanticElementFinder::RunAnnotateDomModelOnFrame(
    const content::GlobalRenderFrameHostId& host_id) {
  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(host_id);
  if (!render_frame_host) {
    GiveUpWithError(ClientStatus(ELEMENT_RESOLUTION_FAILED));
    return;
  }

  auto* driver = ContentAutofillAssistantDriver::GetOrCreateForRenderFrameHost(
      render_frame_host, annotate_dom_model_service_);
  if (!driver) {
    GiveUpWithError(ClientStatus(ELEMENT_RESOLUTION_FAILED));
    return;
  }

  driver->GetAutofillAssistantAgent()->GetSemanticNodes(
      filter_.role(), filter_.objective(), filter_.ignore_objective(),
      base::Milliseconds(filter_.model_timeout_ms()),
      base::BindOnce(&SemanticElementFinder::OnRunAnnotateDomModelOnFrame,
                     weak_ptr_factory_.GetWeakPtr(), host_id));
}

void SemanticElementFinder::OnRunAnnotateDomModelOnFrame(
    const content::GlobalRenderFrameHostId& host_id,
    mojom::NodeDataStatus status,
    const std::vector<NodeData>& node_data) {
  if (!IsRenderFrameExpected(host_id)) {
    // This can occur if the callback is called after the timeout.
    return;
  }

  node_data_frame_status_.emplace_back(status);

  std::vector<GlobalBackendNodeId> node_ids;
  for (const auto& node : node_data) {
    node_ids.emplace_back(GlobalBackendNodeId(host_id, node.backend_node_id));
  }

  received_results_.emplace(host_id, std::move(node_ids));

  MarkRenderFrameProcessed(host_id);
}

void SemanticElementFinder::OnRunAnnotateDomModel() {
  for (const auto& [backend_id, node_ids] : received_results_) {
    semantic_node_results_.insert(semantic_node_results_.end(),
                                  node_ids.begin(), node_ids.end());
  }

  // For now we only support finding a single element.
  // TODO(b/224746702): Emit multiple ResolveNode calls for the case where the
  // result type is not ResultType::kExactlyOneMatch.
  if (semantic_node_results_.size() > 1) {
    VLOG(1) << __func__ << " Got " << semantic_node_results_.size()
            << " matches for " << selector_ << ", when only 1 was expected.";
    expected_frame_ids_.clear();
    GiveUpWithError(ClientStatus(TOO_MANY_ELEMENTS));
    return;
  }
  if (semantic_node_results_.empty()) {
    if (expected_frame_ids_.empty()) {
      GiveUpWithError(ClientStatus(ELEMENT_RESOLUTION_FAILED));
    } else {
      expected_frame_ids_.clear();
      GiveUpWithError(ClientStatus(TIMED_OUT));
    }
    return;
  }
  const auto& semantic_node_result = semantic_node_results_[0];

  // A non-exitent frame should never happen at this point, better to be safe.
  // E.g. crbug/1335205.
  // Only assign a devtools frame id if the owning frame is in a different
  // process than the main frame (in process frames are not tracked and do
  // not have a session id in our |DevtoolsClient|).
  std::string devtools_frame_id;
  auto* frame =
      content::RenderFrameHost::FromID(semantic_node_result.host_id());
  if (frame != nullptr && frame->IsRenderFrameLive() &&
      web_contents_->GetPrimaryMainFrame()->GetProcess() !=
          frame->GetProcess()) {
    devtools_frame_id = frame->GetDevToolsFrameToken().ToString();
  }

  expected_frame_ids_.clear();

  devtools_client_->GetDOM()->ResolveNode(
      dom::ResolveNodeParams::Builder()
          .SetBackendNodeId(semantic_node_result.backend_node_id())
          .Build(),
      devtools_frame_id,
      base::BindOnce(&SemanticElementFinder::OnResolveNodeForAnnotateDom,
                     weak_ptr_factory_.GetWeakPtr(), semantic_node_result,
                     devtools_frame_id));
}

void SemanticElementFinder::OnResolveNodeForAnnotateDom(
    const GlobalBackendNodeId& node,
    const std::string& devtools_frame_id,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<dom::ResolveNodeResult> result) {
  if (result && result->GetObject() && result->GetObject()->HasObjectId()) {
    ResultFound(node, result->GetObject()->GetObjectId(), devtools_frame_id);
    return;
  }
  SendResult(ClientStatus(ELEMENT_RESOLUTION_FAILED),
             ElementFinderResult::EmptyResult());
}

void SemanticElementFinder::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  const content::GlobalRenderFrameHostId host_id =
      render_frame_host->GetGlobalId();
  MarkRenderFrameProcessed(host_id);
}

void SemanticElementFinder::MarkRenderFrameProcessed(
    content::GlobalRenderFrameHostId host_id) {
  auto it = expected_frame_ids_.find(host_id);

  if (it != expected_frame_ids_.end()) {
    expected_frame_ids_.erase(it);
    if (expected_frame_ids_.empty()) {
      Finalize();
    }
  }
}

void SemanticElementFinder::Finalize() {
  if (!timer_) {
    // Do nothing if annotation has not been started.
    NOTREACHED();
    return;
  }
  timer_->Stop();

  OnRunAnnotateDomModel();
}

bool SemanticElementFinder::IsRenderFrameExpected(
    content::GlobalRenderFrameHostId host_id) {
  auto it = expected_frame_ids_.find(host_id);
  return it != expected_frame_ids_.end();
}

}  // namespace autofill_assistant
