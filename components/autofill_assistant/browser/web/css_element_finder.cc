// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/web/css_element_finder.h"

#include <utility>

#include "components/autofill_assistant/browser/devtools/devtools_client.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/user_data.h"
#include "components/autofill_assistant/browser/user_data_util.h"
#include "components/autofill_assistant/browser/web/element_finder_result.h"
#include "components/autofill_assistant/browser/web/js_filter_builder.h"
#include "components/autofill_assistant/browser/web/web_controller_util.h"
#include "components/autofill_assistant/content/browser/content_autofill_assistant_driver.h"
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

}  // namespace

CssElementFinder::CssElementFinder(content::WebContents* web_contents,
                                   DevtoolsClient* devtools_client,
                                   const UserData* user_data,
                                   const ElementFinderResultType result_type,
                                   const Selector& selector)
    : web_contents_(web_contents),
      devtools_client_(devtools_client),
      user_data_(user_data),
      result_type_(result_type),
      selector_(selector) {}

CssElementFinder::~CssElementFinder() = default;

void CssElementFinder::Start(const ElementFinderResult& start_element,
                             Callback callback) {
  callback_ = std::move(callback);

  selector_proto_ = selector_.proto;
  ClientStatus resolve_status =
      user_data::ResolveSelectorUserData(&selector_proto_, user_data_);
  if (!resolve_status.ok()) {
    SendResult(resolve_status, ElementFinderResult::EmptyResult());
    return;
  }

  auto* frame = start_element.render_frame_host();
  if (frame == nullptr) {
    frame = web_contents_->GetPrimaryMainFrame();
  }
  current_frame_global_id_ = frame->GetGlobalId();
  current_frame_devtools_id_ = start_element.node_frame_id();
  frame_stack_ = start_element.frame_stack();

  if (start_element.object_id().empty()) {
    GetDocumentElement();
  } else {
    current_matches_.emplace_back(start_element.object_id());
    ExecuteNextTask();
  }
}

ElementFinderInfoProto CssElementFinder::GetLogInfo() const {
  DCHECK(!callback_);  // Run after finish.

  ElementFinderInfoProto info;
  if (!client_status_.ok()) {
    info.set_failed_filter_index_range_start(current_filter_index_range_start_);
    info.set_failed_filter_index_range_end(next_filter_index_);
    info.set_get_document_failed(get_document_failed_);
  }

  return info;
}

void CssElementFinder::GiveUpWithError(const ClientStatus& status) {
  DCHECK(!status.ok());
  if (!callback_) {
    return;
  }

  SendResult(status, ElementFinderResult::EmptyResult());
}

void CssElementFinder::ResultFound(const std::string& object_id) {
  if (!callback_) {
    return;
  }

  devtools_client_->GetDOM()->DescribeNode(
      dom::DescribeNodeParams::Builder().SetObjectId(object_id).Build(),
      current_frame_devtools_id_,
      base::BindOnce(&CssElementFinder::OnDescribeNodeForId,
                     weak_ptr_factory_.GetWeakPtr(), object_id));
}

void CssElementFinder::OnDescribeNodeForId(
    const std::string& object_id,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<dom::DescribeNodeResult> node_result) {
  if (node_result && node_result->GetNode()) {
    backend_node_id_ = node_result->GetNode()->GetBackendNodeId();
  }
  BuildAndSendResult(object_id);
}

void CssElementFinder::BuildAndSendResult(const std::string& object_id) {
  ElementFinderResult result;
  result.SetRenderFrameHostGlobalId(current_frame_global_id_);
  result.SetObjectId(object_id);
  result.SetBackendNodeId(backend_node_id_);
  result.SetNodeFrameId(current_frame_devtools_id_);
  result.SetFrameStack(frame_stack_);

  SendResult(OkClientStatus(), result);
}

void CssElementFinder::SendResult(const ClientStatus& status,
                                  const ElementFinderResult& result) {
  client_status_ = status;
  DCHECK(callback_);
  std::move(callback_).Run(status,
                           std::make_unique<ElementFinderResult>(result));
}

void CssElementFinder::ExecuteNextTask() {
  const auto& filters = selector_proto_.filters();

  if (next_filter_index_ >= filters.size()) {
    std::string object_id;
    switch (result_type_) {
      case ElementFinderResultType::kExactlyOneMatch:
        if (!ConsumeOneMatchOrFail(object_id)) {
          return;
        }
        break;

      case ElementFinderResultType::kAnyMatch:
        if (!ConsumeMatchAtOrFail(0, object_id)) {
          return;
        }
        break;

      case ElementFinderResultType::kMatchArray:
        if (!ConsumeMatchArrayOrFail(object_id)) {
          return;
        }
        break;
    }
    ResultFound(object_id);
    return;
  }

  current_filter_index_range_start_ = next_filter_index_;
  if (filters.Get(next_filter_index_).filter_case() ==
      SelectorProto::Filter::kSemantic) {
    // TODO(b/233340267): By convention the semantic filter must be the first.
    // Skip it.
    DCHECK_EQ(next_filter_index_, 0);
    ++next_filter_index_;
  }
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
    case SelectorProto::Filter::kOnTop:
    case SelectorProto::Filter::kParent: {
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

    case SelectorProto::Filter::kSemantic:
    case SelectorProto::Filter::FILTER_NOT_SET:
      VLOG(1) << __func__ << " Unexpected filter in " << filter << " in "
              << selector_;
      GiveUpWithError(ClientStatus(INVALID_SELECTOR));
      return;
  }
}

bool CssElementFinder::ConsumeOneMatchOrFail(std::string& object_id_out) {
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

bool CssElementFinder::ConsumeMatchAtOrFail(size_t index,
                                            std::string& object_id_out) {
  if (index < current_matches_.size()) {
    object_id_out = current_matches_[index];
    current_matches_.clear();
    return true;
  }

  GiveUpWithError(ClientStatus(ELEMENT_RESOLUTION_FAILED));
  return false;
}

bool CssElementFinder::ConsumeAllMatchesOrFail(
    std::vector<std::string>& matches_out) {
  if (!current_matches_.empty()) {
    matches_out = std::move(current_matches_);
    current_matches_.clear();
    return true;
  }
  GiveUpWithError(ClientStatus(ELEMENT_RESOLUTION_FAILED));
  return false;
}

bool CssElementFinder::ConsumeMatchArrayOrFail(std::string& array_object_id) {
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

void CssElementFinder::MoveMatchesToJSArrayRecursive(size_t index) {
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
      current_frame_devtools_id_,
      base::BindOnce(&CssElementFinder::OnMoveMatchesToJSArrayRecursive,
                     weak_ptr_factory_.GetWeakPtr(), index));
}

void CssElementFinder::OnMoveMatchesToJSArrayRecursive(
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

void CssElementFinder::GetDocumentElement() {
  devtools_client_->GetRuntime()->Evaluate(
      std::string(kGetDocumentElement), current_frame_devtools_id_,
      base::BindOnce(&CssElementFinder::OnGetDocumentElement,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CssElementFinder::OnGetDocumentElement(
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

void CssElementFinder::ApplyJsFilters(
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
        current_frame_devtools_id_,
        base::BindOnce(&CssElementFinder::OnApplyJsFilters,
                       weak_ptr_factory_.GetWeakPtr(), task_id));
  }
}

void CssElementFinder::OnApplyJsFilters(
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

void CssElementFinder::ResolvePseudoElement(
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
        current_frame_devtools_id_,
        base::BindOnce(&CssElementFinder::OnDescribeNodeForPseudoElement,
                       weak_ptr_factory_.GetWeakPtr(), pseudo_type, task_id));
  }
}

void CssElementFinder::OnDescribeNodeForPseudoElement(
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
            current_frame_devtools_id_,
            base::BindOnce(&CssElementFinder::OnResolveNodeForPseudoElement,
                           weak_ptr_factory_.GetWeakPtr(), task_id));
        return;
      }
    }
  }

  ReportNoMatchingElement(task_id);
}

void CssElementFinder::OnResolveNodeForPseudoElement(
    size_t task_id,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<dom::ResolveNodeResult> result) {
  if (result && result->GetObject() && result->GetObject()->HasObjectId()) {
    ReportMatchingElement(task_id, result->GetObject()->GetObjectId());
    return;
  }

  ReportNoMatchingElement(task_id);
}

void CssElementFinder::EnterFrame(const std::string& object_id) {
  devtools_client_->GetDOM()->DescribeNode(
      dom::DescribeNodeParams::Builder().SetObjectId(object_id).Build(),
      current_frame_devtools_id_,
      base::BindOnce(&CssElementFinder::OnDescribeNodeForFrame,
                     weak_ptr_factory_.GetWeakPtr(), object_id));
}

void CssElementFinder::OnDescribeNodeForFrame(
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

    frame_stack_.push_back({object_id, current_frame_devtools_id_});

    auto* frame =
        FindCorrespondingRenderFrameHost(node->GetFrameId(), web_contents_);
    if (!frame) {
      VLOG(1) << __func__ << " Failed to find corresponding owner frame.";
      GiveUpWithError(ClientStatus(FRAME_HOST_NOT_FOUND));
      return;
    }
    current_frame_global_id_ = frame->GetGlobalId();

    if (node->HasContentDocument()) {
      // If the frame has a ContentDocument it's considered a local frame. In
      // this case, current frame doesn't change and can directly use the
      // content document as root for the evaluation.
      backend_ids.emplace_back(node->GetContentDocument()->GetBackendNodeId());
    } else {
      current_frame_devtools_id_ = node->GetFrameId();
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
        current_frame_devtools_id_,
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

void CssElementFinder::OnResolveNode(
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

void CssElementFinder::PrepareBatchTasks(int n) {
  tasks_results_.clear();
  tasks_results_.resize(n);
}

void CssElementFinder::ReportMatchingElement(size_t task_id,
                                             const std::string& object_id) {
  tasks_results_[task_id] =
      std::make_unique<std::vector<std::string>>(1, object_id);
  MaybeFinalizeBatchTasks();
}

void CssElementFinder::ReportNoMatchingElement(size_t task_id) {
  tasks_results_[task_id] = std::make_unique<std::vector<std::string>>();
  MaybeFinalizeBatchTasks();
}

void CssElementFinder::ReportMatchingElementsArray(
    size_t task_id,
    const std::string& array_object_id) {
  // Recursively add each element ID to a vector then report it as this task
  // result.
  ReportMatchingElementsArrayRecursive(
      task_id, array_object_id, std::make_unique<std::vector<std::string>>(),
      /* index= */ 0);
}

void CssElementFinder::ReportMatchingElementsArrayRecursive(
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
      current_frame_devtools_id_,
      base::BindOnce(&CssElementFinder::OnReportMatchingElementsArrayRecursive,
                     weak_ptr_factory_.GetWeakPtr(), task_id, array_object_id,
                     std::move(acc), index));
}

void CssElementFinder::OnReportMatchingElementsArrayRecursive(
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

void CssElementFinder::MaybeFinalizeBatchTasks() {
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
