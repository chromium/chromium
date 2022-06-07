// Copyright 2022 The Chromium Authors. All rights reserved.
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

void AddHostToList(std::vector<content::GlobalRenderFrameHostId>& host_ids,
                   content::RenderFrameHost* host) {
  host_ids.push_back(host->GetGlobalId());
}

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
    : web_contents_(web_contents),
      devtools_client_(devtools_client),
      annotate_dom_model_service_(annotate_dom_model_service),
      selector_(selector) {
  DCHECK(annotate_dom_model_service_);
}

SemanticElementFinder::~SemanticElementFinder() = default;

void SemanticElementFinder::GiveUpWithError(const ClientStatus& status) {
  DCHECK(!status.ok());
  if (!callback_) {
    return;
  }

  SendResult(status, ElementFinderResult::EmptyResult());
}

void SemanticElementFinder::ResultFound(
    content::RenderFrameHost* render_frame_host,
    const std::string& object_id,
    int backend_node_id) {
  if (!callback_) {
    return;
  }

  ElementFinderResult result;
  result.SetRenderFrameHost(render_frame_host);
  result.SetObjectId(object_id);
  result.SetBackendNodeId(backend_node_id);

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
  DCHECK(selector_.proto.has_semantic_information());
  for (auto node_data_status : node_data_frame_status_) {
    info.mutable_semantic_inference_result()->add_status_per_frame(
        NodeDataStatusToSemanticInferenceStatus(node_data_status));
  }
  for (const auto& semantic_node_result : semantic_node_results_) {
    auto* predicted_element =
        info.mutable_semantic_inference_result()->add_predicted_elements();
    predicted_element->set_backend_node_id(
        semantic_node_result.backend_node_id());
    *predicted_element->mutable_semantic_information() =
        selector_.proto.semantic_information();
    // TODO(b/217160707): For the ignore_objective case this is not correct
    // and the inferred objective should be returned from the Agent and used
    // here.
  }

  return info;
}

int SemanticElementFinder::GetBackendNodeId() const {
  if (semantic_node_results_.empty()) {
    return 0;
  }
  return semantic_node_results_[0].backend_node_id();
}

void SemanticElementFinder::RunAnnotateDomModel(
    content::RenderFrameHost* start_frame) {
  std::vector<content::GlobalRenderFrameHostId> host_ids;
  start_frame->ForEachRenderFrameHost(
      base::BindRepeating(&AddHostToList, std::ref(host_ids)));
  const auto run_on_frame =
      base::BarrierCallback<std::vector<GlobalBackendNodeId>>(
          host_ids.size(),
          base::BindOnce(&SemanticElementFinder::OnRunAnnotateDomModel,
                         weak_ptr_factory_.GetWeakPtr()));
  for (const auto& host_id : host_ids) {
    RunAnnotateDomModelOnFrame(host_id, run_on_frame);
  }
}

void SemanticElementFinder::RunAnnotateDomModelOnFrame(
    const content::GlobalRenderFrameHostId& host_id,
    base::OnceCallback<void(std::vector<GlobalBackendNodeId>)> callback) {
  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(host_id);
  if (!render_frame_host) {
    std::move(callback).Run(std::vector<GlobalBackendNodeId>());
    return;
  }

  auto* driver = ContentAutofillAssistantDriver::GetOrCreateForRenderFrameHost(
      render_frame_host, annotate_dom_model_service_);
  if (!driver) {
    NOTREACHED();
    std::move(callback).Run(std::vector<GlobalBackendNodeId>());
    return;
  }

  driver->GetAutofillAssistantAgent()->GetSemanticNodes(
      selector_.proto.semantic_information().semantic_role(),
      selector_.proto.semantic_information().objective(),
      selector_.proto.semantic_information().ignore_objective(),
      base::Milliseconds(
          selector_.proto.semantic_information().model_timeout_ms()),
      base::BindOnce(&SemanticElementFinder::OnRunAnnotateDomModelOnFrame,
                     weak_ptr_factory_.GetWeakPtr(), host_id,
                     std::move(callback)));
}

void SemanticElementFinder::OnRunAnnotateDomModelOnFrame(
    const content::GlobalRenderFrameHostId& host_id,
    base::OnceCallback<void(std::vector<GlobalBackendNodeId>)> callback,
    mojom::NodeDataStatus status,
    const std::vector<NodeData>& node_data) {
  node_data_frame_status_.emplace_back(status);

  std::vector<GlobalBackendNodeId> node_ids;
  for (const auto& node : node_data) {
    node_ids.emplace_back(GlobalBackendNodeId(host_id, node.backend_node_id));
  }
  std::move(callback).Run(node_ids);
}

void SemanticElementFinder::OnRunAnnotateDomModel(
    const std::vector<std::vector<GlobalBackendNodeId>>& all_nodes) {
  for (const auto& node_ids : all_nodes) {
    semantic_node_results_.insert(semantic_node_results_.end(),
                                  node_ids.begin(), node_ids.end());
  }

  // For now we only support finding a single element.
  // TODO(b/224746702): Emit multiple ResolveNode calls for the case where the
  // result type is not ResultType::kExactlyOneMatch.
  if (semantic_node_results_.size() > 1) {
    VLOG(1) << __func__ << " Got " << semantic_node_results_.size()
            << " matches for " << selector_ << ", when only 1 was expected.";
    GiveUpWithError(ClientStatus(TOO_MANY_ELEMENTS));
    return;
  }
  if (semantic_node_results_.empty()) {
    GiveUpWithError(ClientStatus(ELEMENT_RESOLUTION_FAILED));
    return;
  }

  // We need to set the empty string for the frame id. The expectation is that
  // backend node ids are global and devtools is able to resolve the node
  // without an explicit frame id.
  devtools_client_->GetDOM()->ResolveNode(
      dom::ResolveNodeParams::Builder()
          .SetBackendNodeId(semantic_node_results_[0].backend_node_id())
          .Build(),
      /* current_frame_id= */ std::string(),
      base::BindOnce(&SemanticElementFinder::OnResolveNodeForAnnotateDom,
                     weak_ptr_factory_.GetWeakPtr(),
                     semantic_node_results_[0]));
}

void SemanticElementFinder::OnResolveNodeForAnnotateDom(
    GlobalBackendNodeId node,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<dom::ResolveNodeResult> result) {
  if (result && result->GetObject() && result->GetObject()->HasObjectId()) {
    ResultFound(content::RenderFrameHost::FromID(node.host_id()),
                result->GetObject()->GetObjectId(), node.backend_node_id());
    return;
  }
  SendResult(ClientStatus(ELEMENT_RESOLUTION_FAILED),
             ElementFinderResult::EmptyResult());
}

}  // namespace autofill_assistant
