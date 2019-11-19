// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/web/element_finder.h"

#include "components/autofill_assistant/browser/devtools/devtools_client.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/web/web_controller_util.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace autofill_assistant {

namespace {
// Javascript code to get document root element.
const char* const kGetDocumentElement =
    R"(
    (function() {
      return document.documentElement;
    }())
    )";

// Javascript code to query an elements for a selector, either the first
// (non-strict) or a single (strict) element.
//
// Returns undefined if no elements are found, TOO_MANY_ELEMENTS (18) if too
// many elements were found and strict mode is enabled.
const char* const kQuerySelector =
    R"(function (selector, strictMode) {
      var found = this.querySelectorAll(selector);
      if(found.length == 0) return undefined;
      if(found.length > 1 && strictMode) return 18;
      return found[0];
    })";

// Javascript code to query a visible elements for a selector, either the first
// (non-strict) or a single (strict) visible element.q
//
// Returns undefined if no elements are found, TOO_MANY_ELEMENTS (18) if too
// many elements were found and strict mode is enabled.
const char* const kQuerySelectorWithConditions =
    R"(function (selector, strict, visible, inner_text_str, value_str) {
        var found = this.querySelectorAll(selector);
        var found_index = -1;
        var inner_text_re = inner_text_str ? RegExp(inner_text_str) : undefined;
        var value_re = value_str ? RegExp(value_str) : undefined;
        var match = function(e) {
          if (visible && e.getClientRects().length == 0) return false;
          if (inner_text_re && !inner_text_re.test(e.innerText)) return false;
          if (value_re && !value_re.test(e.value)) return false;
          return true;
        };
        for (let i = 0; i < found.length; i++) {
          if (match(found[i])) {
            if (found_index != -1) return 18;
            found_index = i;
            if (!strict) break;
          }
        }
        return found_index == -1 ? undefined : found[found_index];
    })";

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

ElementFinder::ElementFinder(content::WebContents* web_contents,
                             DevtoolsClient* devtools_client,
                             const Selector& selector,
                             bool strict)
    : web_contents_(web_contents),
      devtools_client_(devtools_client),
      selector_(selector),
      strict_(strict),
      element_result_(std::make_unique<Result>()) {}

ElementFinder::~ElementFinder() = default;

void ElementFinder::Start(Callback callback) {
  callback_ = std::move(callback);

  if (selector_.empty()) {
    SendResult(ClientStatus(INVALID_SELECTOR));
    return;
  }

  devtools_client_->GetRuntime()->Evaluate(
      std::string(kGetDocumentElement), /* node_frame_id= */ std::string(),
      base::BindOnce(&ElementFinder::OnGetDocumentElement,
                     weak_ptr_factory_.GetWeakPtr(), 0));
}

void ElementFinder::SendResult(const ClientStatus& status) {
  DCHECK(callback_);
  DCHECK(element_result_);
  std::move(callback_).Run(status, std::move(element_result_));
}

void ElementFinder::OnGetDocumentElement(
    size_t index,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<runtime::EvaluateResult> result) {
  ClientStatus status =
      CheckJavaScriptResult(reply_status, result.get(), __FILE__, __LINE__);
  if (!status.ok()) {
    DVLOG(1) << __func__ << " Failed to get document root element.";
    SendResult(status);
    return;
  }
  std::string object_id;
  if (!SafeGetObjectId(result->GetResult(), &object_id)) {
    DVLOG(1) << __func__ << " Failed to get document root element.";
    SendResult(ClientStatus(ELEMENT_RESOLUTION_FAILED));
    return;
  }

  element_result_->container_frame_selector_index = index;
  if (element_result_->container_frame_host == nullptr) {
    // Don't overwrite results from previous OOPIF passes.
    element_result_->container_frame_host = web_contents_->GetMainFrame();
  }
  element_result_->object_id = std::string();
  RecursiveFindElement(object_id, index);
}

void ElementFinder::RecursiveFindElement(const std::string& object_id,
                                         size_t index) {
  std::vector<std::unique_ptr<runtime::CallArgument>> argument;
  argument.emplace_back(runtime::CallArgument::Builder()
                            .SetValue(base::Value::ToUniquePtrValue(
                                base::Value(selector_.selectors[index])))
                            .Build());
  // For finding intermediate elements, strict mode would be more appropriate,
  // as long as the logic does not support more than one intermediate match.
  //
  // TODO(b/129387787): first, add logging to figure out whether it matters and
  // decide between strict mode and full support for multiple matching
  // intermeditate elements.
  argument.emplace_back(
      runtime::CallArgument::Builder()
          .SetValue(base::Value::ToUniquePtrValue(base::Value(strict_)))
          .Build());
  std::string function;
  if (index == (selector_.selectors.size() - 1)) {
    if (selector_.must_be_visible || !selector_.inner_text_pattern.empty() ||
        !selector_.value_pattern.empty()) {
      function.assign(kQuerySelectorWithConditions);
      argument.emplace_back(runtime::CallArgument::Builder()
                                .SetValue(base::Value::ToUniquePtrValue(
                                    base::Value(selector_.must_be_visible)))
                                .Build());
      argument.emplace_back(runtime::CallArgument::Builder()
                                .SetValue(base::Value::ToUniquePtrValue(
                                    base::Value(selector_.inner_text_pattern)))
                                .Build());
      argument.emplace_back(runtime::CallArgument::Builder()
                                .SetValue(base::Value::ToUniquePtrValue(
                                    base::Value(selector_.value_pattern)))
                                .Build());
    }
  }
  if (function.empty()) {
    function.assign(kQuerySelector);
  }
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(object_id)
          .SetArguments(std::move(argument))
          .SetFunctionDeclaration(function)
          .Build(),
      element_result_->node_frame_id,
      base::BindOnce(&ElementFinder::OnQuerySelectorAll,
                     weak_ptr_factory_.GetWeakPtr(), index));
}

void ElementFinder::OnQuerySelectorAll(
    size_t index,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  if (!result) {
    // It is possible for a document element to already exist, but not be
    // available yet to query because the document hasn't been loaded. This
    // results in OnQuerySelectorAll getting a nullptr result. For this specific
    // call, it is expected.
    DVLOG(1) << __func__ << ": Context doesn't exist yet to query selector "
             << index << " of " << selector_;
    SendResult(ClientStatus(ELEMENT_RESOLUTION_FAILED));
    return;
  }
  ClientStatus status =
      CheckJavaScriptResult(reply_status, result.get(), __FILE__, __LINE__);
  if (!status.ok()) {
    DVLOG(1) << __func__ << ": Failed to query selector " << index << " of "
             << selector_;
    SendResult(status);
    return;
  }
  int int_result;
  if (SafeGetIntValue(result->GetResult(), &int_result)) {
    DCHECK(int_result == TOO_MANY_ELEMENTS);
    SendResult(ClientStatus(TOO_MANY_ELEMENTS));
    return;
  }
  std::string object_id;
  if (!SafeGetObjectId(result->GetResult(), &object_id)) {
    SendResult(ClientStatus(ELEMENT_RESOLUTION_FAILED));
    return;
  }

  if (selector_.selectors.size() == index + 1) {
    // The pseudo type is associated to the final element matched by
    // |selector_|, which means that we currently don't handle matching an
    // element inside a pseudo element.
    if (selector_.pseudo_type == PseudoType::UNDEFINED) {
      // Return object id of the element.
      element_result_->object_id = object_id;
      SendResult(OkClientStatus());
      return;
    }

    // We are looking for a pseudo element associated with this element.
    dom::PseudoType pseudo_type;
    if (!ConvertPseudoType(selector_.pseudo_type, &pseudo_type)) {
      // Return empty result.
      SendResult(ClientStatus(INVALID_ACTION));
      return;
    }

    devtools_client_->GetDOM()->DescribeNode(
        dom::DescribeNodeParams::Builder().SetObjectId(object_id).Build(),
        element_result_->node_frame_id,
        base::BindOnce(&ElementFinder::OnDescribeNodeForPseudoElement,
                       weak_ptr_factory_.GetWeakPtr(), pseudo_type));
    return;
  }

  devtools_client_->GetDOM()->DescribeNode(
      dom::DescribeNodeParams::Builder().SetObjectId(object_id).Build(),
      element_result_->node_frame_id,
      base::BindOnce(&ElementFinder::OnDescribeNode,
                     weak_ptr_factory_.GetWeakPtr(), object_id, index));
}

void ElementFinder::OnDescribeNodeForPseudoElement(
    dom::PseudoType pseudo_type,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<dom::DescribeNodeResult> result) {
  if (!result || !result->GetNode()) {
    DVLOG(1) << __func__ << " Failed to describe the node for pseudo element.";
    SendResult(UnexpectedDevtoolsErrorStatus(reply_status, __FILE__, __LINE__));
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
            element_result_->node_frame_id,
            base::BindOnce(&ElementFinder::OnResolveNodeForPseudoElement,
                           weak_ptr_factory_.GetWeakPtr()));
        return;
      }
    }
  }

  // Failed to find the pseudo element: run the callback with empty result.
  SendResult(ClientStatus(ELEMENT_RESOLUTION_FAILED));
}

void ElementFinder::OnResolveNodeForPseudoElement(
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<dom::ResolveNodeResult> result) {
  if (result && result->GetObject() && result->GetObject()->HasObjectId()) {
    element_result_->object_id = result->GetObject()->GetObjectId();
  }
  SendResult(OkClientStatus());
}

void ElementFinder::OnDescribeNode(
    const std::string& object_id,
    size_t index,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<dom::DescribeNodeResult> result) {
  if (!result || !result->GetNode()) {
    DVLOG(1) << __func__ << " Failed to describe the node.";
    SendResult(UnexpectedDevtoolsErrorStatus(reply_status, __FILE__, __LINE__));
    return;
  }

  auto* node = result->GetNode();
  std::vector<int> backend_ids;

  if (node->GetNodeName() == "IFRAME") {
    DCHECK(node->HasFrameId());  // Ensure all frames have an id.

    element_result_->container_frame_selector_index = index;
    element_result_->container_frame_host =
        FindCorrespondingRenderFrameHost(node->GetFrameId());

    if (!element_result_->container_frame_host) {
      DVLOG(1) << __func__ << " Failed to find corresponding owner frame.";
      SendResult(ClientStatus(FRAME_HOST_NOT_FOUND));
      return;
    }

    if (node->HasContentDocument()) {
      // If the frame has a ContentDocument it's considered a local frame. We
      // don't need to assign the frame id, since devtools can just send the
      // commands to the main session.

      backend_ids.emplace_back(node->GetContentDocument()->GetBackendNodeId());
    } else {
      // If the frame has no ContentDocument, it's considered an
      // OutOfProcessIFrame.
      // See https://www.chromium.org/developers/design-documents/oop-iframes
      // for full documentation.
      // With the iFrame running in a different process it is necessary to pass
      // the correct session id from devtools. We need to set the frame id,
      // such that devtools can resolve the corresponding session id.
      element_result_->node_frame_id = node->GetFrameId();

      // Kick off another find element chain to walk down the OOP iFrame.
      devtools_client_->GetRuntime()->Evaluate(
          std::string(kGetDocumentElement), element_result_->node_frame_id,
          base::BindOnce(&ElementFinder::OnGetDocumentElement,
                         weak_ptr_factory_.GetWeakPtr(), index + 1));
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
        element_result_->node_frame_id,
        base::BindOnce(&ElementFinder::OnResolveNode,
                       weak_ptr_factory_.GetWeakPtr(), index));
    return;
  }

  RecursiveFindElement(object_id, ++index);
}

void ElementFinder::OnResolveNode(
    size_t index,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<dom::ResolveNodeResult> result) {
  if (!result || !result->GetObject() || !result->GetObject()->HasObjectId()) {
    DVLOG(1) << __func__ << " Failed to resolve object id from backend id.";
    SendResult(UnexpectedDevtoolsErrorStatus(reply_status, __FILE__, __LINE__));
    return;
  }

  RecursiveFindElement(result->GetObject()->GetObjectId(), ++index);
}

content::RenderFrameHost* ElementFinder::FindCorrespondingRenderFrameHost(
    std::string frame_id) {
  for (auto* frame : web_contents_->GetAllFrames()) {
    if (frame->GetDevToolsFrameToken().ToString() == frame_id) {
      return frame;
    }
  }

  return nullptr;
}

}  // namespace autofill_assistant
