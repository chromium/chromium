// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/web/element_finder.h"

#include <utility>

#include "base/barrier_callback.h"
#include "base/time/time.h"
#include "components/autofill_assistant/browser/devtools/devtools_client.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/user_data.h"
#include "components/autofill_assistant/browser/user_data_util.h"
#include "components/autofill_assistant/browser/web/element.h"
#include "components/autofill_assistant/browser/web/js_filter_builder.h"
#include "components/autofill_assistant/browser/web/web_controller_util.h"
#include "components/autofill_assistant/content/browser/content_autofill_assistant_driver.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace autofill_assistant {

namespace {
// Javascript code to get document root element.
const char kGetDocumentElement[] = "document.documentElement;";

const char kGetArrayElement[] = "function(index) { return this[index]; }";

bool ConvertPseudoType(const PseudoType pseudo_type,
                       dom::PseudoType* pseudo_type_output) {
  switch (pseudo_type) {
    case PseudoType::UNDEFINED:
      break;
    case PseudoType::FIRST_LINE:
      *pseudo_type_output = dom::PseudoType::FIRST_LINE;
      return true;
    case PseudoType::FIRST_LETTER:
      *pseudo_type_output = dom::PseudoType::FIRST_LETTER;
      return true;
    case PseudoType::BEFORE:
      *pseudo_type_output = dom::PseudoType::BEFORE;
      return true;
    case PseudoType::AFTER:
      *pseudo_type_output = dom::PseudoType::AFTER;
      return true;
    case PseudoType::BACKDROP:
      *pseudo_type_output = dom::PseudoType::BACKDROP;
      return true;
    case PseudoType::SELECTION:
      *pseudo_type_output = dom::PseudoType::SELECTION;
      return true;
    case PseudoType::FIRST_LINE_INHERITED:
      *pseudo_type_output = dom::PseudoType::FIRST_LINE_INHERITED;
      return true;
    case PseudoType::SCROLLBAR:
      *pseudo_type_output = dom::PseudoType::SCROLLBAR;
      return true;
    case PseudoType::SCROLLBAR_THUMB:
      *pseudo_type_output = dom::PseudoType::SCROLLBAR_THUMB;
      return true;
    case PseudoType::SCROLLBAR_BUTTON:
      *pseudo_type_output = dom::PseudoType::SCROLLBAR_BUTTON;
      return true;
    case PseudoType::SCROLLBAR_TRACK:
      *pseudo_type_output = dom::PseudoType::SCROLLBAR_TRACK;
      return true;
    case PseudoType::SCROLLBAR_TRACK_PIECE:
      *pseudo_type_output = dom::PseudoType::SCROLLBAR_TRACK_PIECE;
      return true;
    case PseudoType::SCROLLBAR_CORNER:
      *pseudo_type_output = dom::PseudoType::SCROLLBAR_CORNER;
      return true;
    case PseudoType::RESIZER:
      *pseudo_type_output = dom::PseudoType::RESIZER;
      return true;
    case PseudoType::INPUT_LIST_BUTTON:
      *pseudo_type_output = dom::PseudoType::INPUT_LIST_BUTTON;
      return true;
  }
  return false;
}

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

ElementFinderResult::ElementFinderResult() = default;

ElementFinderResult::~ElementFinderResult() = default;

ElementFinderResult::ElementFinderResult(const ElementFinderResult&) = default;

ElementFinderResult ElementFinderResult::EmptyResult() {
  return ElementFinderResult();
}

ElementFinder::ElementFinder(
    content::WebContents* web_contents,
    DevtoolsClient* devtools_client,
    const UserData* user_data,
    ProcessedActionStatusDetailsProto* log_info,
    AnnotateDomModelService* annotate_dom_model_service,
    const Selector& selector,
    ResultType result_type)
    : web_contents_(web_contents),
      devtools_client_(devtools_client),
      user_data_(user_data),
      log_info_(log_info),
      annotate_dom_model_service_(annotate_dom_model_service),
      selector_(selector),
      result_type_(result_type) {}

ElementFinder::~ElementFinder() = default;

void ElementFinder::Start(const ElementFinderResult& start_element,
                          Callback callback) {
  callback_ = std::move(callback);

  if (selector_.empty()) {
    SendResult(ClientStatus(INVALID_SELECTOR),
               std::make_unique<ElementFinderResult>(
                   ElementFinderResult::EmptyResult()));
    return;
  }

  // TODO(b/224747076): Coordinate the dom_model_service experiment in the
  // backend. So that we don't get semantic selectors if the client doesn't
  // support the model.
  if (selector_.proto.has_semantic_information()) {
    if (!annotate_dom_model_service_) {
      SendResult(ClientStatus(PRECONDITION_FAILED),
                 std::make_unique<ElementFinderResult>(
                     ElementFinderResult::EmptyResult()));
      return;
    }

    if (selector_.proto.semantic_information().check_matches_css_element()) {
      // This will return the element being used.
      AddAndStartRunner(start_element,
                        std::make_unique<CssElementFinder>(
                            web_contents_, devtools_client_, user_data_,
                            result_type_, selector_));
    }

    AddAndStartRunner(start_element,
                      std::make_unique<SemanticElementFinder>(
                          web_contents_, devtools_client_,
                          annotate_dom_model_service_, selector_));
    return;
  }

  AddAndStartRunner(start_element, std::make_unique<CssElementFinder>(
                                       web_contents_, devtools_client_,
                                       user_data_, result_type_, selector_));
}

void ElementFinder::AddAndStartRunner(
    const ElementFinderResult& start_element,
    std::unique_ptr<ElementFinderBase> runner) {
  auto* runner_ptr = runner.get();
  runners_.emplace_back(std::move(runner));
  results_.resize(runners_.size());
  runner_ptr->Start(
      start_element,
      base::BindOnce(&ElementFinder::OnResult, weak_ptr_factory_.GetWeakPtr(),
                     /* index= */ runners_.size() - 1));
}

void ElementFinder::UpdateLogInfo(const ClientStatus& status) {
  if (log_info_ == nullptr) {
    return;
  }

  auto* info = log_info_->add_element_finder_info();
  for (const auto& runner : runners_) {
    info->MergeFrom(runner->GetLogInfo());
  }

  info->set_status(status.proto_status());
  if (selector_.proto.has_tracking_id()) {
    info->set_tracking_id(selector_.proto.tracking_id());
  }

  if (runners_.size() > 1u) {
    // By convention the 0th result is used as the result being returned for
    // usage. If there's more than one runner, use it to compare it to the
    // semantic results.
    int css_backend_node_id = runners_[0]->GetBackendNodeId();
    for (auto& predicted_element : *info->mutable_semantic_inference_result()
                                        ->mutable_predicted_elements()) {
      predicted_element.set_matches_css_element(
          predicted_element.backend_node_id() == css_backend_node_id);
    }
  }
}

void ElementFinder::SendResult(const ClientStatus& status,
                               std::unique_ptr<ElementFinderResult> result) {
  UpdateLogInfo(status);
  DCHECK(callback_);
  std::move(callback_).Run(
      ClientStatus(status.proto_status(), status.details()), std::move(result));
}

void ElementFinder::OnResult(size_t index,
                             const ClientStatus& status,
                             std::unique_ptr<ElementFinderResult> result) {
  results_[index] = std::make_pair(status, std::move(result));
  ++num_results_;

  if (num_results_ < results_.size()) {
    return;
  }

  DCHECK(!results_.empty());
  DCHECK(!runners_.empty());
  SendResult(results_[0].first, std::move(results_[0].second));
}

ElementFinder::ElementFinderBase::~ElementFinderBase() = default;

ElementFinder::SemanticElementFinder::SemanticElementFinder(
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
ElementFinder::SemanticElementFinder::~SemanticElementFinder() = default;

void ElementFinder::SemanticElementFinder::GiveUpWithError(
    const ClientStatus& status) {
  DCHECK(!status.ok());
  if (!callback_) {
    return;
  }

  SendResult(status, ElementFinderResult::EmptyResult());
}

void ElementFinder::SemanticElementFinder::ResultFound(
    content::RenderFrameHost* render_frame_host,
    const std::string& object_id) {
  if (!callback_) {
    return;
  }

  ElementFinderResult result;
  result.SetRenderFrameHost(render_frame_host);
  result.SetObjectId(object_id);

  SendResult(OkClientStatus(), result);
}

void ElementFinder::SemanticElementFinder::SendResult(
    const ClientStatus& status,
    const ElementFinderResult& result) {
  DCHECK(callback_);
  std::move(callback_).Run(status,
                           std::make_unique<ElementFinderResult>(result));
}

void ElementFinder::SemanticElementFinder::Start(
    const ElementFinderResult& start_element,
    Callback callback) {
  callback_ = std::move(callback);

  auto* start_frame = start_element.render_frame_host();
  if (!start_frame) {
    start_frame = web_contents_->GetMainFrame();
  }
  RunAnnotateDomModel(start_frame);
}

ElementFinderInfoProto ElementFinder::SemanticElementFinder::GetLogInfo()
    const {
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

int ElementFinder::SemanticElementFinder::GetBackendNodeId() const {
  if (semantic_node_results_.empty()) {
    return 0;
  }
  return semantic_node_results_[0].backend_node_id();
}

void ElementFinder::SemanticElementFinder::RunAnnotateDomModel(
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

void ElementFinder::SemanticElementFinder::RunAnnotateDomModelOnFrame(
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

void ElementFinder::SemanticElementFinder::OnRunAnnotateDomModelOnFrame(
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

void ElementFinder::SemanticElementFinder::OnRunAnnotateDomModel(
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
                     semantic_node_results_[0].host_id()));
}

void ElementFinder::SemanticElementFinder::OnResolveNodeForAnnotateDom(
    content::GlobalRenderFrameHostId host_id,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<dom::ResolveNodeResult> result) {
  if (result && result->GetObject() && result->GetObject()->HasObjectId()) {
    ResultFound(content::RenderFrameHost::FromID(host_id),
                result->GetObject()->GetObjectId());
    return;
  }
  SendResult(ClientStatus(ELEMENT_RESOLUTION_FAILED),
             ElementFinderResult::EmptyResult());
}

ElementFinder::CssElementFinder::CssElementFinder(
    content::WebContents* web_contents,
    DevtoolsClient* devtools_client,
    const UserData* user_data,
    const ResultType result_type,
    const Selector& selector)
    : web_contents_(web_contents),
      devtools_client_(devtools_client),
      user_data_(user_data),
      result_type_(result_type),
      selector_(selector) {}
ElementFinder::CssElementFinder::~CssElementFinder() = default;

void ElementFinder::CssElementFinder::Start(
    const ElementFinderResult& start_element,
    Callback callback) {
  callback_ = std::move(callback);

  selector_proto_ = selector_.proto;
  ClientStatus resolve_status =
      user_data::ResolveSelectorUserData(&selector_proto_, user_data_);
  if (!resolve_status.ok()) {
    SendResult(resolve_status, ElementFinderResult::EmptyResult());
    return;
  }

  current_frame_ = start_element.render_frame_host();
  if (current_frame_ == nullptr) {
    current_frame_ = web_contents_->GetMainFrame();
  }
  current_frame_id_ = start_element.node_frame_id();
  frame_stack_ = start_element.frame_stack();

  if (start_element.object_id().empty()) {
    GetDocumentElement();
  } else {
    current_matches_.emplace_back(start_element.object_id());
    ExecuteNextTask();
  }
}

ElementFinderInfoProto ElementFinder::CssElementFinder::GetLogInfo() const {
  DCHECK(!callback_);  // Run after finish.

  ElementFinderInfoProto info;
  if (!client_status_.ok()) {
    info.set_failed_filter_index_range_start(current_filter_index_range_start_);
    info.set_failed_filter_index_range_end(next_filter_index_);
    info.set_get_document_failed(get_document_failed_);
  }

  return info;
}

int ElementFinder::CssElementFinder::GetBackendNodeId() const {
  return backend_node_id_.value_or(0);
}

void ElementFinder::CssElementFinder::GiveUpWithError(
    const ClientStatus& status) {
  DCHECK(!status.ok());
  if (!callback_) {
    return;
  }

  SendResult(status, ElementFinderResult::EmptyResult());
}

void ElementFinder::CssElementFinder::ResultFound(
    const std::string& object_id) {
  if (!callback_) {
    return;
  }

  if (selector_.proto.has_semantic_information()) {
    devtools_client_->GetDOM()->DescribeNode(
        dom::DescribeNodeParams::Builder().SetObjectId(object_id).Build(),
        current_frame_id_,
        base::BindOnce(&CssElementFinder::OnDescribeNodeForId,
                       weak_ptr_factory_.GetWeakPtr(), object_id));
    return;
  }

  BuildAndSendResult(object_id);
}

void ElementFinder::CssElementFinder::OnDescribeNodeForId(
    const std::string& object_id,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<dom::DescribeNodeResult> node_result) {
  if (node_result && node_result->GetNode()) {
    backend_node_id_ = node_result->GetNode()->GetBackendNodeId();
  }
  BuildAndSendResult(object_id);
}

void ElementFinder::CssElementFinder::BuildAndSendResult(
    const std::string& object_id) {
  ElementFinderResult result;
  result.SetRenderFrameHost(current_frame_);
  result.SetObjectId(object_id);
  result.SetNodeFrameId(current_frame_id_);
  result.SetFrameStack(frame_stack_);

  SendResult(OkClientStatus(), result);
}

void ElementFinder::CssElementFinder::SendResult(
    const ClientStatus& status,
    const ElementFinderResult& result) {
  client_status_ = status;
  DCHECK(callback_);
  std::move(callback_).Run(status,
                           std::make_unique<ElementFinderResult>(result));
}

void ElementFinder::CssElementFinder::ExecuteNextTask() {
  const auto& filters = selector_proto_.filters();

  if (next_filter_index_ >= filters.size()) {
    std::string object_id;
    switch (result_type_) {
      case ResultType::kExactlyOneMatch:
        if (!ConsumeOneMatchOrFail(object_id)) {
          return;
        }
        break;

      case ResultType::kAnyMatch:
        if (!ConsumeMatchAtOrFail(0, object_id)) {
          return;
        }
        break;

      case ResultType::kMatchArray:
        if (!ConsumeMatchArrayOrFail(object_id)) {
          return;
        }
        break;
    }
    ResultFound(object_id);
    return;
  }

  current_filter_index_range_start_ = next_filter_index_;
  const auto& filter = filters.Get(next_filter_index_);
  switch (filter.filter_case()) {
    case SelectorProto::Filter::kEnterFrame: {
      std::string object_id;
      if (!ConsumeOneMatchOrFail(object_id))
        return;

      // The above fails if there is more than one frame. To preserve
      // backward-compatibility with the previous, lax behavior, callers must
      // add pick_one before enter_frame. TODO(b/155264465): allow searching in
      // more than one frame.
      next_filter_index_++;
      EnterFrame(object_id);
      return;
    }

    case SelectorProto::Filter::kPseudoType: {
      std::vector<std::string> matches;
      if (!ConsumeAllMatchesOrFail(matches))
        return;

      next_filter_index_++;
      matching_pseudo_elements_ = true;
      ResolvePseudoElement(filter.pseudo_type(), matches);
      return;
    }

    case SelectorProto::Filter::kNthMatch: {
      // TODO(b/205676462): This could be done with javascript like in
      // |SelectorObserver|.
      std::string object_id;
      if (!ConsumeMatchAtOrFail(filter.nth_match().index(), object_id))
        return;

      next_filter_index_++;
      current_matches_ = {object_id};
      ExecuteNextTask();
      return;
    }

    case SelectorProto::Filter::kCssSelector:
    case SelectorProto::Filter::kInnerText:
    case SelectorProto::Filter::kValue:
    case SelectorProto::Filter::kProperty:
    case SelectorProto::Filter::kBoundingBox:
    case SelectorProto::Filter::kPseudoElementContent:
    case SelectorProto::Filter::kMatchCssSelector:
    case SelectorProto::Filter::kCssStyle:
    case SelectorProto::Filter::kLabelled:
    case SelectorProto::Filter::kOnTop: {
      std::vector<std::string> matches;
      if (!ConsumeAllMatchesOrFail(matches))
        return;

      JsFilterBuilder js_filter;
      for (int i = next_filter_index_; i < filters.size(); i++) {
        if (!js_filter.AddFilter(filters.Get(i))) {
          break;
        }
        next_filter_index_++;
      }
      ApplyJsFilters(js_filter, matches);
      return;
    }

    case SelectorProto::Filter::FILTER_NOT_SET:
      VLOG(1) << __func__ << " Unset or unknown filter in " << filter << " in "
              << selector_;
      GiveUpWithError(ClientStatus(INVALID_SELECTOR));
      return;
  }
}

bool ElementFinder::CssElementFinder::ConsumeOneMatchOrFail(
    std::string& object_id_out) {
  if (current_matches_.size() > 1) {
    VLOG(1) << __func__ << " Got " << current_matches_.size() << " matches for "
            << selector_ << ", when only 1 was expected.";
    GiveUpWithError(ClientStatus(TOO_MANY_ELEMENTS));
    return false;
  }
  if (current_matches_.empty()) {
    GiveUpWithError(ClientStatus(ELEMENT_RESOLUTION_FAILED));
    return false;
  }

  object_id_out = current_matches_[0];
  current_matches_.clear();
  return true;
}

bool ElementFinder::CssElementFinder::ConsumeMatchAtOrFail(
    size_t index,
    std::string& object_id_out) {
  if (index < current_matches_.size()) {
    object_id_out = current_matches_[index];
    current_matches_.clear();
    return true;
  }

  GiveUpWithError(ClientStatus(ELEMENT_RESOLUTION_FAILED));
  return false;
}

bool ElementFinder::CssElementFinder::ConsumeAllMatchesOrFail(
    std::vector<std::string>& matches_out) {
  if (!current_matches_.empty()) {
    matches_out = std::move(current_matches_);
    current_matches_.clear();
    return true;
  }
  GiveUpWithError(ClientStatus(ELEMENT_RESOLUTION_FAILED));
  return false;
}

bool ElementFinder::CssElementFinder::ConsumeMatchArrayOrFail(
    std::string& array_object_id) {
  if (!current_matches_js_array_.empty()) {
    array_object_id = current_matches_js_array_;
    current_matches_js_array_.clear();
    return true;
  }

  if (current_matches_.empty()) {
    GiveUpWithError(ClientStatus(ELEMENT_RESOLUTION_FAILED));
    return false;
  }

  MoveMatchesToJSArrayRecursive(/* index= */ 0);
  return false;
}

void ElementFinder::CssElementFinder::MoveMatchesToJSArrayRecursive(
    size_t index) {
  if (index >= current_matches_.size()) {
    current_matches_.clear();
    ExecuteNextTask();
    return;
  }

  // Push the value at |current_matches_[index]| to |current_matches_js_array_|.
  std::string function;
  std::vector<std::unique_ptr<runtime::CallArgument>> arguments;
  if (index == 0) {
    // Create an array containing a single element.
    function = "function() { return [this]; }";
  } else {
    // Add an element to an existing array.
    function = "function(dest) { dest.push(this); }";
    AddRuntimeCallArgumentObjectId(current_matches_js_array_, &arguments);
  }

  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(current_matches_[index])
          .SetArguments(std::move(arguments))
          .SetFunctionDeclaration(function)
          .Build(),
      current_frame_id_,
      base::BindOnce(&CssElementFinder::OnMoveMatchesToJSArrayRecursive,
                     weak_ptr_factory_.GetWeakPtr(), index));
}

void ElementFinder::CssElementFinder::OnMoveMatchesToJSArrayRecursive(
    size_t index,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  ClientStatus status =
      CheckJavaScriptResult(reply_status, result.get(), __FILE__, __LINE__);
  if (!status.ok()) {
    VLOG(1) << __func__ << ": Failed to push value to JS array.";
    GiveUpWithError(status);
    return;
  }

  // We just created an array which contains the first element. We store its ID
  // in |current_matches_js_array_|.
  if (index == 0 &&
      !SafeGetObjectId(result->GetResult(), &current_matches_js_array_)) {
    VLOG(1) << __func__ << " Failed to get array ID.";
    GiveUpWithError(ClientStatus(ELEMENT_RESOLUTION_FAILED));
    return;
  }

  // Continue the recursion to push the other values into the array.
  MoveMatchesToJSArrayRecursive(index + 1);
}

void ElementFinder::CssElementFinder::GetDocumentElement() {
  devtools_client_->GetRuntime()->Evaluate(
      std::string(kGetDocumentElement), current_frame_id_,
      base::BindOnce(&CssElementFinder::OnGetDocumentElement,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ElementFinder::CssElementFinder::OnGetDocumentElement(
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<runtime::EvaluateResult> result) {
  ClientStatus status =
      CheckJavaScriptResult(reply_status, result.get(), __FILE__, __LINE__);
  if (!status.ok()) {
    VLOG(1) << __func__ << " Failed to get document root element.";
    get_document_failed_ = true;
    GiveUpWithError(status);
    return;
  }
  std::string object_id;
  if (!SafeGetObjectId(result->GetResult(), &object_id)) {
    VLOG(1) << __func__ << " Failed to get document root element.";
    get_document_failed_ = true;
    GiveUpWithError(ClientStatus(ELEMENT_RESOLUTION_FAILED));
    return;
  }

  // Use the node as root for the rest of the evaluation.
  current_matches_.emplace_back(object_id);

  ExecuteNextTask();
}

void ElementFinder::CssElementFinder::ApplyJsFilters(
    const JsFilterBuilder& builder,
    const std::vector<std::string>& object_ids) {
  DCHECK(!object_ids.empty());  // Guaranteed by ExecuteNextTask()
  PrepareBatchTasks(object_ids.size());
  std::string function = builder.BuildFunction();
  for (size_t task_id = 0; task_id < object_ids.size(); task_id++) {
    devtools_client_->GetRuntime()->CallFunctionOn(
        runtime::CallFunctionOnParams::Builder()
            .SetObjectId(object_ids[task_id])
            .SetArguments(builder.BuildArgumentList())
            .SetFunctionDeclaration(function)
            .Build(),
        current_frame_id_,
        base::BindOnce(&CssElementFinder::OnApplyJsFilters,
                       weak_ptr_factory_.GetWeakPtr(), task_id));
  }
}

void ElementFinder::CssElementFinder::OnApplyJsFilters(
    size_t task_id,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  if (!result) {
    // It is possible for a document element to already exist, but not be
    // available yet to query because the document hasn't been loaded. This
    // results in OnQuerySelectorAll getting a nullptr result. For this specific
    // call, it is expected.
    VLOG(1) << __func__ << ": Context doesn't exist yet to query frame "
            << frame_stack_.size() << " of " << selector_;
    GiveUpWithError(ClientStatus(ELEMENT_RESOLUTION_FAILED));
    return;
  }
  ClientStatus status =
      CheckJavaScriptResult(reply_status, result.get(), __FILE__, __LINE__);
  if (!status.ok()) {
    VLOG(1) << __func__ << ": Failed to query selector for frame "
            << frame_stack_.size() << " of " << selector_ << ": " << status;
    GiveUpWithError(status);
    return;
  }

  // The result can be empty (nothing found), an array (multiple matches
  // found) or a single node.
  std::string object_id;
  if (!SafeGetObjectId(result->GetResult(), &object_id)) {
    ReportNoMatchingElement(task_id);
    return;
  }

  if (result->GetResult()->HasSubtype() &&
      result->GetResult()->GetSubtype() ==
          runtime::RemoteObjectSubtype::ARRAY) {
    ReportMatchingElementsArray(task_id, object_id);
    return;
  }

  ReportMatchingElement(task_id, object_id);
}

void ElementFinder::CssElementFinder::ResolvePseudoElement(
    PseudoType proto_pseudo_type,
    const std::vector<std::string>& object_ids) {
  dom::PseudoType pseudo_type;
  if (!ConvertPseudoType(proto_pseudo_type, &pseudo_type)) {
    VLOG(1) << __func__ << ": Unsupported pseudo-type "
            << PseudoTypeName(proto_pseudo_type);
    GiveUpWithError(ClientStatus(INVALID_ACTION));
    return;
  }

  DCHECK(!object_ids.empty());  // Guaranteed by ExecuteNextTask()
  PrepareBatchTasks(object_ids.size());
  for (size_t task_id = 0; task_id < object_ids.size(); task_id++) {
    devtools_client_->GetDOM()->DescribeNode(
        dom::DescribeNodeParams::Builder()
            .SetObjectId(object_ids[task_id])
            .Build(),
        current_frame_id_,
        base::BindOnce(&CssElementFinder::OnDescribeNodeForPseudoElement,
                       weak_ptr_factory_.GetWeakPtr(), pseudo_type, task_id));
  }
}

void ElementFinder::CssElementFinder::OnDescribeNodeForPseudoElement(
    dom::PseudoType pseudo_type,
    size_t task_id,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<dom::DescribeNodeResult> result) {
  if (!result || !result->GetNode()) {
    VLOG(1) << __func__ << " Failed to describe the node for pseudo element.";
    GiveUpWithError(
        UnexpectedDevtoolsErrorStatus(reply_status, __FILE__, __LINE__));
    return;
  }

  auto* node = result->GetNode();
  if (node->HasPseudoElements()) {
    for (const auto& pseudo_element : *(node->GetPseudoElements())) {
      if (pseudo_element->HasPseudoType() &&
          pseudo_element->GetPseudoType() == pseudo_type) {
        devtools_client_->GetDOM()->ResolveNode(
            dom::ResolveNodeParams::Builder()
                .SetBackendNodeId(pseudo_element->GetBackendNodeId())
                .Build(),
            current_frame_id_,
            base::BindOnce(&CssElementFinder::OnResolveNodeForPseudoElement,
                           weak_ptr_factory_.GetWeakPtr(), task_id));
        return;
      }
    }
  }

  ReportNoMatchingElement(task_id);
}

void ElementFinder::CssElementFinder::OnResolveNodeForPseudoElement(
    size_t task_id,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<dom::ResolveNodeResult> result) {
  if (result && result->GetObject() && result->GetObject()->HasObjectId()) {
    ReportMatchingElement(task_id, result->GetObject()->GetObjectId());
    return;
  }

  ReportNoMatchingElement(task_id);
}

void ElementFinder::CssElementFinder::EnterFrame(const std::string& object_id) {
  devtools_client_->GetDOM()->DescribeNode(
      dom::DescribeNodeParams::Builder().SetObjectId(object_id).Build(),
      current_frame_id_,
      base::BindOnce(&CssElementFinder::OnDescribeNodeForFrame,
                     weak_ptr_factory_.GetWeakPtr(), object_id));
}

void ElementFinder::CssElementFinder::OnDescribeNodeForFrame(
    const std::string& object_id,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<dom::DescribeNodeResult> result) {
  if (!result || !result->GetNode()) {
    VLOG(1) << __func__ << " Failed to describe the node.";
    GiveUpWithError(
        UnexpectedDevtoolsErrorStatus(reply_status, __FILE__, __LINE__));
    return;
  }

  auto* node = result->GetNode();
  std::vector<int> backend_ids;

  if (node->GetNodeName() == "IFRAME") {
    // See: b/206647825
    if (!node->HasFrameId()) {
      NOTREACHED() << "Frame without ID";  // Ensure all frames have an id.
      GiveUpWithError(ClientStatus(FRAME_HOST_NOT_FOUND));
      return;
    }

    frame_stack_.push_back({object_id, current_frame_id_});

    auto* frame =
        FindCorrespondingRenderFrameHost(node->GetFrameId(), web_contents_);
    if (!frame) {
      VLOG(1) << __func__ << " Failed to find corresponding owner frame.";
      GiveUpWithError(ClientStatus(FRAME_HOST_NOT_FOUND));
      return;
    }
    current_frame_ = frame;

    if (node->HasContentDocument()) {
      // If the frame has a ContentDocument it's considered a local frame. In
      // this case, current_frame_ doesn't change and can directly use the
      // content document as root for the evaluation.
      backend_ids.emplace_back(node->GetContentDocument()->GetBackendNodeId());
    } else {
      current_frame_id_ = node->GetFrameId();
      // Kick off another find element chain to walk down the OOP iFrame.
      GetDocumentElement();
      return;
    }
  }

  if (node->HasShadowRoots()) {
    // TODO(crbug.com/806868): Support multiple shadow roots.
    backend_ids.emplace_back(
        node->GetShadowRoots()->front()->GetBackendNodeId());
  }

  if (!backend_ids.empty()) {
    devtools_client_->GetDOM()->ResolveNode(
        dom::ResolveNodeParams::Builder()
            .SetBackendNodeId(backend_ids[0])
            .Build(),
        current_frame_id_,
        base::BindOnce(&CssElementFinder::OnResolveNode,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  // Element was not a frame and didn't have shadow dom. This is unexpected, but
  // to remain backward compatible, don't complain and just continue filtering
  // with the current element as root.
  current_matches_.emplace_back(object_id);
  ExecuteNextTask();
}

void ElementFinder::CssElementFinder::OnResolveNode(
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<dom::ResolveNodeResult> result) {
  if (!result || !result->GetObject() || !result->GetObject()->HasObjectId()) {
    VLOG(1) << __func__ << " Failed to resolve object id from backend id.";
    GiveUpWithError(
        UnexpectedDevtoolsErrorStatus(reply_status, __FILE__, __LINE__));
    return;
  }

  // Use the node as root for the rest of the evaluation.
  current_matches_.emplace_back(result->GetObject()->GetObjectId());
  ExecuteNextTask();
}

void ElementFinder::CssElementFinder::PrepareBatchTasks(int n) {
  tasks_results_.clear();
  tasks_results_.resize(n);
}

void ElementFinder::CssElementFinder::ReportMatchingElement(
    size_t task_id,
    const std::string& object_id) {
  tasks_results_[task_id] =
      std::make_unique<std::vector<std::string>>(1, object_id);
  MaybeFinalizeBatchTasks();
}

void ElementFinder::CssElementFinder::ReportNoMatchingElement(size_t task_id) {
  tasks_results_[task_id] = std::make_unique<std::vector<std::string>>();
  MaybeFinalizeBatchTasks();
}

void ElementFinder::CssElementFinder::ReportMatchingElementsArray(
    size_t task_id,
    const std::string& array_object_id) {
  // Recursively add each element ID to a vector then report it as this task
  // result.
  ReportMatchingElementsArrayRecursive(
      task_id, array_object_id, std::make_unique<std::vector<std::string>>(),
      /* index= */ 0);
}

void ElementFinder::CssElementFinder::ReportMatchingElementsArrayRecursive(
    size_t task_id,
    const std::string& array_object_id,
    std::unique_ptr<std::vector<std::string>> acc,
    int index) {
  std::vector<std::unique_ptr<runtime::CallArgument>> arguments;
  AddRuntimeCallArgument(index, &arguments);
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(array_object_id)
          .SetArguments(std::move(arguments))
          .SetFunctionDeclaration(std::string(kGetArrayElement))
          .Build(),
      current_frame_id_,
      base::BindOnce(&CssElementFinder::OnReportMatchingElementsArrayRecursive,
                     weak_ptr_factory_.GetWeakPtr(), task_id, array_object_id,
                     std::move(acc), index));
}

void ElementFinder::CssElementFinder::OnReportMatchingElementsArrayRecursive(
    size_t task_id,
    const std::string& array_object_id,
    std::unique_ptr<std::vector<std::string>> acc,
    int index,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  ClientStatus status =
      CheckJavaScriptResult(reply_status, result.get(), __FILE__, __LINE__);
  if (!status.ok()) {
    VLOG(1) << __func__ << ": Failed to get element from array for "
            << selector_;
    GiveUpWithError(status);
    return;
  }

  std::string object_id;
  if (!SafeGetObjectId(result->GetResult(), &object_id)) {
    // We've reached the end of the array.
    tasks_results_[task_id] = std::move(acc);
    MaybeFinalizeBatchTasks();
    return;
  }

  acc->emplace_back(object_id);

  // Fetch the next element.
  ReportMatchingElementsArrayRecursive(task_id, array_object_id, std::move(acc),
                                       index + 1);
}

void ElementFinder::CssElementFinder::MaybeFinalizeBatchTasks() {
  // Return early if one of the tasks is still pending.
  for (const auto& result : tasks_results_) {
    if (!result) {
      return;
    }
  }

  // Add all matching elements to current_matches_.
  for (const auto& result : tasks_results_) {
    current_matches_.insert(current_matches_.end(), result->begin(),
                            result->end());
  }
  tasks_results_.clear();

  ExecuteNextTask();
}

}  // namespace autofill_assistant
