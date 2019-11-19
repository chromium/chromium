// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/web/web_controller.h"

#include <math.h>
#include <algorithm>
#include <ctime>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill_assistant/browser/client_settings.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/rectf.h"
#include "components/autofill_assistant/browser/string_conversions_util.h"
#include "components/autofill_assistant/browser/web/web_controller_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

namespace autofill_assistant {
using autofill::ContentAutofillDriver;

namespace {
const char* const kGetBoundingClientRectAsList =
    R"(function(node) {
      const r = node.getBoundingClientRect();
      return [window.scrollX + r.left,
              window.scrollY + r.top,
              window.scrollX + r.right,
              window.scrollY + r.bottom];
    })";

const char* const kGetVisualViewport =
    R"({ const v = window.visualViewport;
         [v.pageLeft,
          v.pageTop,
          v.width,
          v.height] })";

// Scrolls to the specified node with top padding. The top padding can
// be specified through pixels or ratio. Pixels take precedence.
const char* const kScrollIntoViewWithPaddingScript =
    R"(function(node, topPaddingPixels, topPaddingRatio) {
    node.scrollIntoViewIfNeeded();
    const rect = node.getBoundingClientRect();
    let topPadding = topPaddingPixels;
    if (!topPadding){
      topPadding = window.innerHeight * topPaddingRatio;
    }
    window.scrollBy({top: rect.top - topPadding});
  })";

// Scroll the window or any scrollable container as needed for the element to
// appear centered. This is in preparation of a click, to improve the chances
// for the element to click to be visible.
const char* const kScrollIntoViewCenterScript =
    R"(function(node) {
    node.scrollIntoView({block: "center", inline: "center"});
  })";

// Javascript to select a value from a select box. Also fires a "change" event
// to trigger any listeners. Changing the index directly does not trigger this.
const char* const kSelectOptionScript =
    R"(function(value) {
      const uppercaseValue = value.toUpperCase();
      var found = false;
      for (var i = 0; i < this.options.length; ++i) {
        const label = this.options[i].label.toUpperCase();
        if (label.length > 0 && label.startsWith(uppercaseValue)) {
          this.options.selectedIndex = i;
          found = true;
          break;
        }
      }
      if (!found) {
        return false;
      }
      const e = document.createEvent('HTMLEvents');
      e.initEvent('change', true, true);
      this.dispatchEvent(e);
      return true;
    })";

// Javascript to highlight an element.
const char* const kHighlightElementScript =
    R"(function() {
      this.style.boxShadow = '0px 0px 0px 3px white, ' +
          '0px 0px 0px 6px rgb(66, 133, 244)';
      return true;
    })";

// Javascript code to retrieve the 'value' attribute of a node.
const char* const kGetValueAttributeScript =
    "function () { return this.value; }";

// Javascript code to set the 'value' attribute of a node and then fire a
// "change" event to trigger any listeners.
const char* const kSetValueAttributeScript =
    R"(function (value) {
         this.value = value;
         const e = document.createEvent('HTMLEvents');
         e.initEvent('change', true, true);
         this.dispatchEvent(e);
       })";

// Javascript code to set an attribute of a node to a given value.
const char* const kSetAttributeScript =
    R"(function (attribute, value) {
         let receiver = this;
         for (let i = 0; i < attribute.length - 1; i++) {
           receiver = receiver[attribute[i]];
         }
         receiver[attribute[attribute.length - 1]] = value;
       })";

// Javascript code to get the outerHTML of a node.
// TODO(crbug.com/806868): Investigate if using DOM.GetOuterHtml would be a
// better solution than injecting Javascript code.
const char* const kGetOuterHtmlScript =
    "function () { return this.outerHTML; }";

// Javascript code to query whether the document is ready for interact.
const char* const kIsDocumentReadyForInteract =
    R"(function () {
      return document.readyState == 'interactive'
          || document.readyState == 'complete';
    })";

// Javascript code to click on an element.
const char* const kClickElement =
    R"(function (selector) {
      selector.click();
    })";

// Javascript code that returns a promise that will succeed once the main
// document window has changed height.
//
// This ignores width changes, to filter out resizes caused by changes to the
// screen orientation.
const char* const kWaitForWindowHeightChange = R"(
new Promise((fulfill, reject) => {
  var lastWidth = window.innerWidth;
  var handler = function(event) {
    if (window.innerWidth != lastWidth) {
      lastWidth = window.innerWidth;
      return
    }
    window.removeEventListener('resize', handler)
    fulfill(true)
  }
  window.addEventListener('resize', handler)
})
)";

// Converts a int that correspond to the DocumentReadyState enum into an
// equivalent quoted Javascript string.
std::string DocumentReadyStateToQuotedJsString(int state) {
  switch (static_cast<DocumentReadyState>(state)) {
    case DOCUMENT_UNKNOWN_READY_STATE:
      return "''";
    case DOCUMENT_UNINITIALIZED:
      return "'uninitialized'";
    case DOCUMENT_LOADING:
      return "'loading'";
    case DOCUMENT_LOADED:
      return "'loaded'";
    case DOCUMENT_INTERACTIVE:
      return "'interactive'";
    case DOCUMENT_COMPLETE:
      return "'complete'";

      // No default, to get a compilation error if a new enum value is left
      // unsupported.
  }

  // If the enum values aren't sequential, just add empty strings to fill in the
  // blanks.
  return "''";
}

// Appends to |out| the definition of a function that'll wait for a
// ready state, expressed as a DocumentReadyState enum value.
void AppendWaitForDocumentReadyStateFunction(std::string* out) {
  // quoted_names covers all possible DocumentReadyState values.
  std::vector<std::string> quoted_names(DOCUMENT_MAX_READY_STATE + 1);
  for (int i = 0; i <= DOCUMENT_MAX_READY_STATE; i++) {
    quoted_names[i] = DocumentReadyStateToQuotedJsString(i);
  }
  base::StrAppend(out, {R"(function (minReadyStateNum) {
  return new Promise((fulfill, reject) => {
    let handler = function(event) {
      let readyState = document.readyState;
      let readyStates = [)",
                        base::JoinString(quoted_names, ", "), R"(];
      let readyStateNum = readyStates.indexOf(readyState);
      if (readyStateNum == -1) readyStateNum = 0;
      if (readyStateNum >= minReadyStateNum) {
        document.removeEventListener('readystatechange', handler);
        fulfill(readyStateNum);
      }
    }
    document.addEventListener('readystatechange', handler)
    handler();
  })
})"});
}

// Forward the result of WaitForDocumentReadyState to the callback. The same
// code work on both EvaluateResult and CallFunctionOnResult.
template <typename T>
void OnWaitForDocumentReadyState(
    base::OnceCallback<void(const ClientStatus&, DocumentReadyState)> callback,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<T> result) {
  ClientStatus status =
      CheckJavaScriptResult(reply_status, result.get(), __FILE__, __LINE__);
  DVLOG_IF(1, !status.ok())
      << __func__ << " Failed to get document ready state.";
  int ready_state;
  SafeGetIntValue(result->GetResult(), &ready_state);
  std::move(callback).Run(status, static_cast<DocumentReadyState>(ready_state));
}
}  // namespace

// static
std::unique_ptr<WebController> WebController::CreateForWebContents(
    content::WebContents* web_contents,
    const ClientSettings* settings) {
  return std::make_unique<WebController>(
      web_contents,
      std::make_unique<DevtoolsClient>(
          content::DevToolsAgentHost::GetOrCreateFor(web_contents)),
      settings);
}

WebController::WebController(content::WebContents* web_contents,
                             std::unique_ptr<DevtoolsClient> devtools_client,
                             const ClientSettings* settings)
    : web_contents_(web_contents),
      devtools_client_(std::move(devtools_client)),
      settings_(settings) {}

WebController::~WebController() {}

WebController::FillFormInputData::FillFormInputData() {}

WebController::FillFormInputData::~FillFormInputData() {}

void WebController::LoadURL(const GURL& url) {
  DVLOG(3) << __func__ << " " << url;
  web_contents_->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(url));
}

void WebController::ClickOrTapElement(
    const Selector& selector,
    ClickAction::ClickType click_type,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  DVLOG(3) << __func__ << " " << selector;
  DCHECK(!selector.empty());
  FindElement(selector,
              /* strict_mode= */ true,
              base::BindOnce(&WebController::OnFindElementForClickOrTap,
                             weak_ptr_factory_.GetWeakPtr(),
                             std::move(callback), click_type));
}

void WebController::OnFindElementForClickOrTap(
    base::OnceCallback<void(const ClientStatus&)> callback,
    ClickAction::ClickType click_type,
    const ClientStatus& status,
    std::unique_ptr<ElementFinder::Result> result) {
  // Found element must belong to a frame.
  if (!status.ok()) {
    DVLOG(1) << __func__ << " Failed to find the element to click or tap.";
    std::move(callback).Run(status);
    return;
  }

  std::string element_object_id = result->object_id;
  WaitForDocumentToBecomeInteractive(
      settings_->document_ready_check_count, element_object_id,
      result->node_frame_id,
      base::BindOnce(
          &WebController::OnWaitDocumentToBecomeInteractiveForClickOrTap,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback), click_type,
          std::move(result)));
}

void WebController::OnWaitDocumentToBecomeInteractiveForClickOrTap(
    base::OnceCallback<void(const ClientStatus&)> callback,
    ClickAction::ClickType click_type,
    std::unique_ptr<ElementFinder::Result> target_element,
    bool result) {
  if (!result) {
    std::move(callback).Run(ClientStatus(TIMED_OUT));
    return;
  }

  ClickOrTapElement(std::move(target_element), click_type, std::move(callback));
}

void WebController::ClickOrTapElement(
    std::unique_ptr<ElementFinder::Result> target_element,
    ClickAction::ClickType click_type,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  std::string element_object_id = target_element->object_id;
  std::vector<std::unique_ptr<runtime::CallArgument>> argument;
  argument.emplace_back(
      runtime::CallArgument::Builder().SetObjectId(element_object_id).Build());
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(element_object_id)
          .SetArguments(std::move(argument))
          .SetFunctionDeclaration(std::string(kScrollIntoViewCenterScript))
          .SetReturnByValue(true)
          .Build(),
      target_element->node_frame_id,
      base::BindOnce(&WebController::OnScrollIntoView,
                     weak_ptr_factory_.GetWeakPtr(), std::move(target_element),
                     std::move(callback), click_type));
}

void WebController::OnScrollIntoView(
    std::unique_ptr<ElementFinder::Result> target_element,
    base::OnceCallback<void(const ClientStatus&)> callback,
    ClickAction::ClickType click_type,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  ClientStatus status =
      CheckJavaScriptResult(reply_status, result.get(), __FILE__, __LINE__);
  if (!status.ok()) {
    DVLOG(1) << __func__ << " Failed to scroll the element.";
    std::move(callback).Run(status);
    return;
  }

  if (click_type == ClickAction::JAVASCRIPT) {
    std::string element_object_id = target_element->object_id;
    std::vector<std::unique_ptr<runtime::CallArgument>> argument;
    argument.emplace_back(runtime::CallArgument::Builder()
                              .SetObjectId(element_object_id)
                              .Build());
    devtools_client_->GetRuntime()->CallFunctionOn(
        runtime::CallFunctionOnParams::Builder()
            .SetObjectId(element_object_id)
            .SetArguments(std::move(argument))
            .SetFunctionDeclaration(kClickElement)
            .Build(),
        target_element->node_frame_id,
        base::BindOnce(&WebController::OnClickJS,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }

  std::unique_ptr<ElementPositionGetter> getter =
      std::make_unique<ElementPositionGetter>(
          devtools_client_.get(), *settings_, target_element->node_frame_id);
  auto* ptr = getter.get();
  pending_workers_.emplace_back(std::move(getter));
  ptr->Start(
      target_element->container_frame_host, target_element->object_id,
      base::BindOnce(&WebController::TapOrClickOnCoordinates,
                     weak_ptr_factory_.GetWeakPtr(), ptr, std::move(callback),
                     target_element->node_frame_id, click_type));
}

void WebController::OnClickJS(
    base::OnceCallback<void(const ClientStatus&)> callback,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  ClientStatus status =
      CheckJavaScriptResult(reply_status, result.get(), __FILE__, __LINE__);
  if (!status.ok()) {
    DVLOG(1) << __func__ << " Failed to click (javascript) the element.";
  }
  std::move(callback).Run(status);
}

void WebController::TapOrClickOnCoordinates(
    ElementPositionGetter* getter_to_release,
    base::OnceCallback<void(const ClientStatus&)> callback,
    const std::string& node_frame_id,
    ClickAction::ClickType click_type,
    bool has_coordinates,
    int x,
    int y) {
  base::EraseIf(pending_workers_, [getter_to_release](const auto& worker) {
    return worker.get() == getter_to_release;
  });

  if (!has_coordinates) {
    DVLOG(1) << __func__ << " Failed to get element position.";
    std::move(callback).Run(ClientStatus(ELEMENT_UNSTABLE));
    return;
  }

  DCHECK(click_type == ClickAction::TAP || click_type == ClickAction::CLICK);
  if (click_type == ClickAction::CLICK) {
    devtools_client_->GetInput()->DispatchMouseEvent(
        input::DispatchMouseEventParams::Builder()
            .SetX(x)
            .SetY(y)
            .SetClickCount(1)
            .SetButton(input::DispatchMouseEventButton::LEFT)
            .SetType(input::DispatchMouseEventType::MOUSE_PRESSED)
            .Build(),
        node_frame_id,
        base::BindOnce(&WebController::OnDispatchPressMouseEvent,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       node_frame_id, x, y));
    return;
  }

  std::vector<std::unique_ptr<::autofill_assistant::input::TouchPoint>>
      touch_points;
  touch_points.emplace_back(
      input::TouchPoint::Builder().SetX(x).SetY(y).Build());
  devtools_client_->GetInput()->DispatchTouchEvent(
      input::DispatchTouchEventParams::Builder()
          .SetType(input::DispatchTouchEventType::TOUCH_START)
          .SetTouchPoints(std::move(touch_points))
          .Build(),
      node_frame_id,
      base::BindOnce(&WebController::OnDispatchTouchEventStart,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     node_frame_id));
}

void WebController::OnDispatchPressMouseEvent(
    base::OnceCallback<void(const ClientStatus&)> callback,
    const std::string& node_frame_id,
    int x,
    int y,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<input::DispatchMouseEventResult> result) {
  if (!result) {
    DVLOG(1) << __func__
             << " Failed to dispatch mouse left button pressed event.";
    std::move(callback).Run(
        UnexpectedDevtoolsErrorStatus(reply_status, __FILE__, __LINE__));
    return;
  }

  devtools_client_->GetInput()->DispatchMouseEvent(
      input::DispatchMouseEventParams::Builder()
          .SetX(x)
          .SetY(y)
          .SetClickCount(1)
          .SetButton(input::DispatchMouseEventButton::LEFT)
          .SetType(input::DispatchMouseEventType::MOUSE_RELEASED)
          .Build(),
      node_frame_id,
      base::BindOnce(&WebController::OnDispatchReleaseMouseEvent,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnDispatchReleaseMouseEvent(
    base::OnceCallback<void(const ClientStatus&)> callback,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<input::DispatchMouseEventResult> result) {
  if (!result) {
    DVLOG(1) << __func__ << " Failed to dispatch release mouse event.";
    std::move(callback).Run(
        UnexpectedDevtoolsErrorStatus(reply_status, __FILE__, __LINE__));
    return;
  }
  std::move(callback).Run(OkClientStatus());
}

void WebController::OnDispatchTouchEventStart(
    base::OnceCallback<void(const ClientStatus&)> callback,
    const std::string& node_frame_id,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<input::DispatchTouchEventResult> result) {
  if (!result) {
    DVLOG(1) << __func__ << " Failed to dispatch touch start event.";
    std::move(callback).Run(
        UnexpectedDevtoolsErrorStatus(reply_status, __FILE__, __LINE__));
    return;
  }

  std::vector<std::unique_ptr<::autofill_assistant::input::TouchPoint>>
      touch_points;
  devtools_client_->GetInput()->DispatchTouchEvent(
      input::DispatchTouchEventParams::Builder()
          .SetType(input::DispatchTouchEventType::TOUCH_END)
          .SetTouchPoints(std::move(touch_points))
          .Build(),
      node_frame_id,
      base::BindOnce(&WebController::OnDispatchTouchEventEnd,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnDispatchTouchEventEnd(
    base::OnceCallback<void(const ClientStatus&)> callback,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<input::DispatchTouchEventResult> result) {
  if (!result) {
    DVLOG(1) << __func__ << " Failed to dispatch touch end event.";
    std::move(callback).Run(
        UnexpectedDevtoolsErrorStatus(reply_status, __FILE__, __LINE__));
    return;
  }
  std::move(callback).Run(OkClientStatus());
}

void WebController::ElementCheck(
    const Selector& selector,
    bool strict,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  DCHECK(!selector.empty());
  FindElement(
      selector, strict,
      base::BindOnce(&WebController::OnFindElementForCheck,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnFindElementForCheck(
    base::OnceCallback<void(const ClientStatus&)> callback,
    const ClientStatus& status,
    std::unique_ptr<ElementFinder::Result> result) {
  DVLOG_IF(1,
           !status.ok() && status.proto_status() != ELEMENT_RESOLUTION_FAILED)
      << __func__ << ": " << status;
  std::move(callback).Run(status);
}

void WebController::WaitForWindowHeightChange(
    base::OnceCallback<void(const ClientStatus&)> callback) {
  devtools_client_->GetRuntime()->Evaluate(
      runtime::EvaluateParams::Builder()
          .SetExpression(kWaitForWindowHeightChange)
          .SetAwaitPromise(true)
          .Build(),
      /* node_frame_id= */ std::string(),
      base::BindOnce(&WebController::OnWaitForWindowHeightChange,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnWaitForWindowHeightChange(
    base::OnceCallback<void(const ClientStatus&)> callback,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<runtime::EvaluateResult> result) {
  std::move(callback).Run(
      CheckJavaScriptResult(reply_status, result.get(), __FILE__, __LINE__));
}

void WebController::GetDocumentReadyState(
    const Selector& optional_frame,
    base::OnceCallback<void(const ClientStatus&, DocumentReadyState)>
        callback) {
  WaitForDocumentReadyState(optional_frame, DOCUMENT_UNKNOWN_READY_STATE,
                            std::move(callback));
}

void WebController::WaitForDocumentReadyState(
    const Selector& optional_frame,
    DocumentReadyState min_ready_state,
    base::OnceCallback<void(const ClientStatus&, DocumentReadyState)>
        callback) {
  if (optional_frame.empty()) {
    std::string expression;
    expression.append("(");
    AppendWaitForDocumentReadyStateFunction(&expression);
    base::StringAppendF(&expression, ")(%d)",
                        static_cast<int>(min_ready_state));
    devtools_client_->GetRuntime()->Evaluate(
        runtime::EvaluateParams::Builder()
            .SetExpression(expression)
            .SetReturnByValue(true)
            .SetAwaitPromise(true)
            .Build(),
        /* node_frame_id= */ std::string(),
        base::BindOnce(&OnWaitForDocumentReadyState<runtime::EvaluateResult>,
                       std::move(callback)));
    return;
  }
  FindElement(
      optional_frame, /* strict= */ false,
      base::BindOnce(&WebController::OnFindElementForWaitForDocumentReadyState,
                     weak_ptr_factory_.GetWeakPtr(), min_ready_state,
                     std::move(callback)));
}

void WebController::OnFindElementForWaitForDocumentReadyState(
    DocumentReadyState min_ready_state,
    base::OnceCallback<void(const ClientStatus&, DocumentReadyState)> callback,
    const ClientStatus& status,
    std::unique_ptr<ElementFinder::Result> element) {
  if (!status.ok()) {
    std::move(callback).Run(status, DOCUMENT_UNKNOWN_READY_STATE);
    return;
  }

  std::string function_declaration;
  AppendWaitForDocumentReadyStateFunction(&function_declaration);

  std::vector<std::unique_ptr<runtime::CallArgument>> arguments;
  arguments.emplace_back(
      runtime::CallArgument::Builder()
          .SetValue(base::Value::ToUniquePtrValue(
              base::Value(static_cast<int>(min_ready_state))))
          .Build());
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(element ? element->object_id : "")
          .SetFunctionDeclaration(function_declaration)
          .SetArguments(std::move(arguments))
          .SetReturnByValue(true)
          .SetAwaitPromise(true)
          .Build(),
      element->node_frame_id,
      base::BindOnce(
          &OnWaitForDocumentReadyState<runtime::CallFunctionOnResult>,
          std::move(callback)));
}

void WebController::FindElement(const Selector& selector,
                                bool strict_mode,
                                ElementFinder::Callback callback) {
  auto finder = std::make_unique<ElementFinder>(
      web_contents_, devtools_client_.get(), selector, strict_mode);
  auto* ptr = finder.get();
  pending_workers_.emplace_back(std::move(finder));
  ptr->Start(base::BindOnce(&WebController::OnFindElementResult,
                            base::Unretained(this), ptr, std::move(callback)));
}

void WebController::OnFindElementResult(
    ElementFinder* finder_to_release,
    ElementFinder::Callback callback,
    const ClientStatus& status,
    std::unique_ptr<ElementFinder::Result> result) {
  base::EraseIf(pending_workers_, [finder_to_release](const auto& worker) {
    return worker.get() == finder_to_release;
  });
  std::move(callback).Run(status, std::move(result));
}

void WebController::OnFindElementForFocusElement(
    const TopPadding& top_padding,
    base::OnceCallback<void(const ClientStatus&)> callback,
    const ClientStatus& status,
    std::unique_ptr<ElementFinder::Result> element_result) {
  if (!status.ok()) {
    DVLOG(1) << __func__ << " Failed to find the element to focus on.";
    std::move(callback).Run(status);
    return;
  }

  std::string element_object_id = element_result->object_id;
  WaitForDocumentToBecomeInteractive(
      settings_->document_ready_check_count, element_object_id,
      element_result->node_frame_id,
      base::BindOnce(
          &WebController::OnWaitDocumentToBecomeInteractiveForFocusElement,
          weak_ptr_factory_.GetWeakPtr(), top_padding, std::move(callback),
          std::move(element_result)));
}

void WebController::OnWaitDocumentToBecomeInteractiveForFocusElement(
    const TopPadding& top_padding,
    base::OnceCallback<void(const ClientStatus&)> callback,
    std::unique_ptr<ElementFinder::Result> target_element,
    bool result) {
  if (!result) {
    std::move(callback).Run(ClientStatus(ELEMENT_UNSTABLE));
    return;
  }

  std::vector<std::unique_ptr<runtime::CallArgument>> arguments;
  arguments.emplace_back(runtime::CallArgument::Builder()
                             .SetObjectId(target_element->object_id)
                             .Build());
  arguments.emplace_back(runtime::CallArgument::Builder()
                             .SetValue(base::Value::ToUniquePtrValue(
                                 base::Value(top_padding.pixels())))
                             .Build());
  arguments.emplace_back(runtime::CallArgument::Builder()
                             .SetValue(base::Value::ToUniquePtrValue(
                                 base::Value(top_padding.ratio())))
                             .Build());
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(target_element->object_id)
          .SetArguments(std::move(arguments))
          .SetFunctionDeclaration(std::string(kScrollIntoViewWithPaddingScript))
          .SetReturnByValue(true)
          .Build(),
      target_element->node_frame_id,
      base::BindOnce(&WebController::OnFocusElement,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnFocusElement(
    base::OnceCallback<void(const ClientStatus&)> callback,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  ClientStatus status =
      CheckJavaScriptResult(reply_status, result.get(), __FILE__, __LINE__);
  DVLOG_IF(1, !status.ok()) << __func__ << " Failed to focus on element.";
  std::move(callback).Run(status);
}

void WebController::FillAddressForm(
    const autofill::AutofillProfile* profile,
    const Selector& selector,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  DVLOG(3) << __func__ << selector;
  auto data_to_autofill = std::make_unique<FillFormInputData>();
  data_to_autofill->profile =
      std::make_unique<autofill::AutofillProfile>(*profile);
  FindElement(selector,
              /* strict_mode= */ true,
              base::BindOnce(&WebController::OnFindElementForFillingForm,
                             weak_ptr_factory_.GetWeakPtr(),
                             std::move(data_to_autofill), selector,
                             std::move(callback)));
}

void WebController::FillCardForm(
    std::unique_ptr<autofill::CreditCard> card,
    const base::string16& cvc,
    const Selector& selector,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  DVLOG(3) << __func__ << " " << selector;
  auto data_to_autofill = std::make_unique<FillFormInputData>();
  data_to_autofill->card = std::move(card);
  data_to_autofill->cvc = cvc;
  FindElement(selector,
              /* strict_mode= */ true,
              base::BindOnce(&WebController::OnFindElementForFillingForm,
                             weak_ptr_factory_.GetWeakPtr(),
                             std::move(data_to_autofill), selector,
                             std::move(callback)));
}

void WebController::OnFindElementForFillingForm(
    std::unique_ptr<FillFormInputData> data_to_autofill,
    const Selector& selector,
    base::OnceCallback<void(const ClientStatus&)> callback,
    const ClientStatus& status,
    std::unique_ptr<ElementFinder::Result> element_result) {
  if (!status.ok()) {
    DVLOG(1) << __func__ << " Failed to find the element for filling the form.";
    std::move(callback).Run(FillAutofillErrorStatus(status));
    return;
  }

  ContentAutofillDriver* driver = ContentAutofillDriver::GetForRenderFrameHost(
      element_result->container_frame_host);
  if (driver == nullptr) {
    DVLOG(1) << __func__ << " Failed to get the autofill driver.";
    std::move(callback).Run(
        FillAutofillErrorStatus(UnexpectedErrorStatus(__FILE__, __LINE__)));
    return;
  }
  DCHECK(!selector.empty());
  // TODO(crbug.com/806868): Figure out whether there are cases where we need
  // more than one selector, and come up with a solution that can figure out the
  // right number of selectors to include.
  driver->GetAutofillAgent()->GetElementFormAndFieldData(
      std::vector<std::string>(1, selector.selectors.back()),
      base::BindOnce(&WebController::OnGetFormAndFieldDataForFillingForm,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(data_to_autofill), std::move(callback),
                     element_result->container_frame_host));
}

void WebController::OnGetFormAndFieldDataForFillingForm(
    std::unique_ptr<FillFormInputData> data_to_autofill,
    base::OnceCallback<void(const ClientStatus&)> callback,
    content::RenderFrameHost* container_frame_host,
    const autofill::FormData& form_data,
    const autofill::FormFieldData& form_field) {
  if (form_data.fields.empty()) {
    DVLOG(1) << __func__ << " Failed to get form data to fill form.";
    std::move(callback).Run(
        FillAutofillErrorStatus(UnexpectedErrorStatus(__FILE__, __LINE__)));
    return;
  }

  ContentAutofillDriver* driver =
      ContentAutofillDriver::GetForRenderFrameHost(container_frame_host);
  if (driver == nullptr) {
    DVLOG(1) << __func__ << " Failed to get the autofill driver.";
    std::move(callback).Run(
        FillAutofillErrorStatus(UnexpectedErrorStatus(__FILE__, __LINE__)));
    return;
  }

  if (data_to_autofill->card) {
    driver->autofill_manager()->FillCreditCardForm(
        autofill::kNoQueryId, form_data, form_field, *data_to_autofill->card,
        data_to_autofill->cvc);
  } else {
    driver->autofill_manager()->FillProfileForm(*data_to_autofill->profile,
                                                form_data, form_field);
  }

  std::move(callback).Run(OkClientStatus());
}

void WebController::SelectOption(
    const Selector& selector,
    const std::string& selected_option,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  DVLOG(3) << __func__ << " " << selector << ", option=" << selected_option;
  FindElement(selector,
              /* strict_mode= */ true,
              base::BindOnce(&WebController::OnFindElementForSelectOption,
                             weak_ptr_factory_.GetWeakPtr(), selected_option,
                             std::move(callback)));
}

void WebController::OnFindElementForSelectOption(
    const std::string& selected_option,
    base::OnceCallback<void(const ClientStatus&)> callback,
    const ClientStatus& status,
    std::unique_ptr<ElementFinder::Result> element_result) {
  if (!status.ok()) {
    DVLOG(1) << __func__ << " Failed to find the element to select an option.";
    std::move(callback).Run(status);
    return;
  }

  std::vector<std::unique_ptr<runtime::CallArgument>> argument;
  argument.emplace_back(
      runtime::CallArgument::Builder()
          .SetValue(base::Value::ToUniquePtrValue(base::Value(selected_option)))
          .Build());
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(element_result->object_id)
          .SetArguments(std::move(argument))
          .SetFunctionDeclaration(std::string(kSelectOptionScript))
          .SetReturnByValue(true)
          .Build(),
      element_result->node_frame_id,
      base::BindOnce(&WebController::OnSelectOption,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnSelectOption(
    base::OnceCallback<void(const ClientStatus&)> callback,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  ClientStatus status =
      CheckJavaScriptResult(reply_status, result.get(), __FILE__, __LINE__);
  if (!status.ok()) {
    DVLOG(1) << __func__ << " Failed to select option.";
    std::move(callback).Run(status);
    return;
  }
  bool found;
  if (!SafeGetBool(result->GetResult(), &found)) {
    std::move(callback).Run(
        UnexpectedDevtoolsErrorStatus(reply_status, __FILE__, __LINE__));
    return;
  }
  if (!found) {
    DVLOG(1) << __func__ << " Failed to find option.";
    std::move(callback).Run(ClientStatus(OPTION_VALUE_NOT_FOUND));
    return;
  }
  std::move(callback).Run(OkClientStatus());
}

void WebController::HighlightElement(
    const Selector& selector,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  DVLOG(3) << __func__ << " " << selector;
  FindElement(
      selector,
      /* strict_mode= */ true,
      base::BindOnce(&WebController::OnFindElementForHighlightElement,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnFindElementForHighlightElement(
    base::OnceCallback<void(const ClientStatus&)> callback,
    const ClientStatus& status,
    std::unique_ptr<ElementFinder::Result> element_result) {
  if (!status.ok()) {
    DVLOG(1) << __func__ << " Failed to find the element to highlight.";
    std::move(callback).Run(status);
    return;
  }

  const std::string& object_id = element_result->object_id;
  std::vector<std::unique_ptr<runtime::CallArgument>> argument;
  argument.emplace_back(
      runtime::CallArgument::Builder().SetObjectId(object_id).Build());
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(object_id)
          .SetArguments(std::move(argument))
          .SetFunctionDeclaration(std::string(kHighlightElementScript))
          .SetReturnByValue(true)
          .Build(),
      element_result->node_frame_id,
      base::BindOnce(&WebController::OnHighlightElement,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnHighlightElement(
    base::OnceCallback<void(const ClientStatus&)> callback,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  ClientStatus status =
      CheckJavaScriptResult(reply_status, result.get(), __FILE__, __LINE__);
  DVLOG_IF(1, !status.ok()) << __func__ << " Failed to highlight element.";
  std::move(callback).Run(status);
}

void WebController::FocusElement(
    const Selector& selector,
    const TopPadding& top_padding,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  DVLOG(3) << __func__ << " " << selector;
  DCHECK(!selector.empty());
  FindElement(selector,
              /* strict_mode= */ false,
              base::BindOnce(&WebController::OnFindElementForFocusElement,
                             weak_ptr_factory_.GetWeakPtr(), top_padding,
                             std::move(callback)));
}

void WebController::GetFieldValue(
    const Selector& selector,
    base::OnceCallback<void(const ClientStatus&, const std::string&)>
        callback) {
  FindElement(
      selector,
      /* strict_mode= */ true,
      base::BindOnce(&WebController::OnFindElementForGetFieldValue,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnFindElementForGetFieldValue(
    base::OnceCallback<void(const ClientStatus&, const std::string&)> callback,
    const ClientStatus& status,
    std::unique_ptr<ElementFinder::Result> element_result) {
  const std::string object_id = element_result->object_id;
  if (!status.ok()) {
    std::move(callback).Run(status, "");
    return;
  }

  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(object_id)
          .SetFunctionDeclaration(std::string(kGetValueAttributeScript))
          .SetReturnByValue(true)
          .Build(),
      element_result->node_frame_id,
      base::BindOnce(&WebController::OnGetValueAttribute,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnGetValueAttribute(
    base::OnceCallback<void(const ClientStatus& element_status,
                            const std::string&)> callback,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  std::string value;
  ClientStatus status =
      CheckJavaScriptResult(reply_status, result.get(), __FILE__, __LINE__);
  // Read the result returned from Javascript code.
  DVLOG_IF(1, !status.ok())
      << __func__ << "Failed to get attribute value: " << status;
  SafeGetStringValue(result->GetResult(), &value);
  std::move(callback).Run(status, value);
}

void WebController::SetFieldValue(
    const Selector& selector,
    const std::string& value,
    bool simulate_key_presses,
    int key_press_delay_in_millisecond,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  DVLOG(3) << __func__ << " " << selector << ", value=" << value
           << ", simulate_key_presses=" << simulate_key_presses;
  if (simulate_key_presses) {
    // We first clear the field value, and then simulate the key presses.
    // TODO(crbug.com/806868): Disable keyboard during this action and then
    // reset to previous state.
    InternalSetFieldValue(
        selector, "",
        base::BindOnce(&WebController::OnClearFieldForSendKeyboardInput,
                       weak_ptr_factory_.GetWeakPtr(), selector,
                       UTF8ToUnicode(value), key_press_delay_in_millisecond,
                       std::move(callback)));
    return;
  }
  InternalSetFieldValue(selector, value, std::move(callback));
}

void WebController::InternalSetFieldValue(
    const Selector& selector,
    const std::string& value,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  FindElement(selector,
              /* strict_mode= */ true,
              base::BindOnce(&WebController::OnFindElementForSetFieldValue,
                             weak_ptr_factory_.GetWeakPtr(), value,
                             std::move(callback)));
}

void WebController::OnClearFieldForSendKeyboardInput(
    const Selector& selector,
    const std::vector<UChar32>& codepoints,
    int key_press_delay_in_millisecond,
    base::OnceCallback<void(const ClientStatus&)> callback,
    const ClientStatus& clear_status) {
  if (!clear_status.ok()) {
    std::move(callback).Run(clear_status);
    return;
  }
  SendKeyboardInput(selector, codepoints, key_press_delay_in_millisecond,
                    std::move(callback));
}

void WebController::OnClickElementForSendKeyboardInput(
    const std::string& node_frame_id,
    const std::vector<UChar32>& codepoints,
    int delay_in_millisecond,
    base::OnceCallback<void(const ClientStatus&)> callback,
    const ClientStatus& click_status) {
  if (!click_status.ok()) {
    std::move(callback).Run(click_status);
    return;
  }
  DispatchKeyboardTextDownEvent(node_frame_id, codepoints, 0,
                                /* delay= */ false, delay_in_millisecond,
                                std::move(callback));
}

void WebController::DispatchKeyboardTextDownEvent(
    const std::string& node_frame_id,
    const std::vector<UChar32>& codepoints,
    size_t index,
    bool delay,
    int delay_in_millisecond,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  if (index >= codepoints.size()) {
    std::move(callback).Run(OkClientStatus());
    return;
  }

  if (delay && delay_in_millisecond > 0) {
    base::PostDelayedTask(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(
            &WebController::DispatchKeyboardTextDownEvent,
            weak_ptr_factory_.GetWeakPtr(), node_frame_id, codepoints, index,
            /* delay= */ false, delay_in_millisecond, std::move(callback)),
        base::TimeDelta::FromMilliseconds(delay_in_millisecond));
    return;
  }

  devtools_client_->GetInput()->DispatchKeyEvent(
      CreateKeyEventParamsForCharacter(
          autofill_assistant::input::DispatchKeyEventType::KEY_DOWN,
          codepoints[index]),
      node_frame_id,
      base::BindOnce(&WebController::DispatchKeyboardTextUpEvent,
                     weak_ptr_factory_.GetWeakPtr(), node_frame_id, codepoints,
                     index, delay_in_millisecond, std::move(callback)));
}

void WebController::DispatchKeyboardTextUpEvent(
    const std::string& node_frame_id,
    const std::vector<UChar32>& codepoints,
    size_t index,
    int delay_in_millisecond,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  DCHECK_LT(index, codepoints.size());
  devtools_client_->GetInput()->DispatchKeyEvent(
      CreateKeyEventParamsForCharacter(
          autofill_assistant::input::DispatchKeyEventType::KEY_UP,
          codepoints[index]),
      node_frame_id,
      base::BindOnce(
          &WebController::DispatchKeyboardTextDownEvent,
          weak_ptr_factory_.GetWeakPtr(), node_frame_id, codepoints, index + 1,
          /* delay= */ true, delay_in_millisecond, std::move(callback)));
}

auto WebController::CreateKeyEventParamsForCharacter(
    autofill_assistant::input::DispatchKeyEventType type,
    UChar32 codepoint) -> DispatchKeyEventParamsPtr {
  auto params = input::DispatchKeyEventParams::Builder().SetType(type).Build();

  std::string text;
  if (AppendUnicodeToUTF8(codepoint, &text)) {
    params->SetText(text);
  } else {
    DVLOG(1) << __func__
             << ": Failed to convert codepoint to UTF-8: " << codepoint;
  }

  auto dom_key = ui::DomKey::FromCharacter(codepoint);
  if (dom_key.IsValid()) {
    params->SetKey(ui::KeycodeConverter::DomKeyToKeyString(dom_key));
  } else {
    DVLOG(1) << __func__
             << ": Failed to set DomKey for codepoint: " << codepoint;
  }

  return params;
}

void WebController::OnFindElementForSetFieldValue(
    const std::string& value,
    base::OnceCallback<void(const ClientStatus&)> callback,
    const ClientStatus& status,
    std::unique_ptr<ElementFinder::Result> element_result) {
  if (!status.ok()) {
    std::move(callback).Run(status);
    return;
  }

  std::vector<std::unique_ptr<runtime::CallArgument>> argument;
  argument.emplace_back(
      runtime::CallArgument::Builder()
          .SetValue(base::Value::ToUniquePtrValue(base::Value(value)))
          .Build());
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(element_result->object_id)
          .SetArguments(std::move(argument))
          .SetFunctionDeclaration(std::string(kSetValueAttributeScript))
          .Build(),
      element_result->node_frame_id,
      base::BindOnce(&WebController::OnSetValueAttribute,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnSetValueAttribute(
    base::OnceCallback<void(const ClientStatus&)> callback,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  std::move(callback).Run(
      CheckJavaScriptResult(reply_status, result.get(), __FILE__, __LINE__));
}

void WebController::SetAttribute(
    const Selector& selector,
    const std::vector<std::string>& attribute,
    const std::string& value,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  DVLOG(3) << __func__ << " " << selector << ", attribute=["
           << base::JoinString(attribute, ",") << "], value=" << value;

  DCHECK(!selector.empty());
  DCHECK_GT(attribute.size(), 0u);
  FindElement(selector,
              /* strict_mode= */ true,
              base::BindOnce(&WebController::OnFindElementForSetAttribute,
                             weak_ptr_factory_.GetWeakPtr(), attribute, value,
                             std::move(callback)));
}

void WebController::OnFindElementForSetAttribute(
    const std::vector<std::string>& attribute,
    const std::string& value,
    base::OnceCallback<void(const ClientStatus&)> callback,
    const ClientStatus& status,
    std::unique_ptr<ElementFinder::Result> element_result) {
  if (!status.ok()) {
    std::move(callback).Run(status);
    return;
  }

  base::Value::ListStorage attribute_values;
  for (const std::string& string : attribute) {
    attribute_values.emplace_back(base::Value(string));
  }

  std::vector<std::unique_ptr<runtime::CallArgument>> arguments;
  arguments.emplace_back(runtime::CallArgument::Builder()
                             .SetValue(base::Value::ToUniquePtrValue(
                                 base::Value(attribute_values)))
                             .Build());
  arguments.emplace_back(
      runtime::CallArgument::Builder()
          .SetValue(base::Value::ToUniquePtrValue(base::Value(value)))
          .Build());
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(element_result->object_id)
          .SetArguments(std::move(arguments))
          .SetFunctionDeclaration(std::string(kSetAttributeScript))
          .Build(),
      element_result->node_frame_id,
      base::BindOnce(&WebController::OnSetAttribute,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnSetAttribute(
    base::OnceCallback<void(const ClientStatus&)> callback,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  std::move(callback).Run(
      CheckJavaScriptResult(reply_status, result.get(), __FILE__, __LINE__));
}

void WebController::SendKeyboardInput(
    const Selector& selector,
    const std::vector<UChar32>& codepoints,
    const int delay_in_millisecond,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  if (VLOG_IS_ON(3)) {
    std::string input_str;
    if (!UnicodeToUTF8(codepoints, &input_str)) {
      input_str.assign("<invalid input>");
    }
    DVLOG(3) << __func__ << " " << selector << ", input=" << input_str;
  }

  DCHECK(!selector.empty());
  FindElement(
      selector, /* strict_mode= */ true,
      base::BindOnce(&WebController::OnFindElementForSendKeyboardInput,
                     weak_ptr_factory_.GetWeakPtr(), selector, codepoints,
                     delay_in_millisecond, std::move(callback)));
}

void WebController::OnFindElementForSendKeyboardInput(
    const Selector& selector,
    const std::vector<UChar32>& codepoints,
    const int delay_in_millisecond,
    base::OnceCallback<void(const ClientStatus&)> callback,
    const ClientStatus& status,
    std::unique_ptr<ElementFinder::Result> element_result) {
  if (!status.ok()) {
    std::move(callback).Run(status);
    return;
  }
  ClickOrTapElement(
      selector, ClickAction::CLICK,
      base::BindOnce(&WebController::OnClickElementForSendKeyboardInput,
                     weak_ptr_factory_.GetWeakPtr(),
                     element_result->node_frame_id, codepoints,
                     delay_in_millisecond, std::move(callback)));
}

void WebController::GetOuterHtml(
    const Selector& selector,
    base::OnceCallback<void(const ClientStatus&, const std::string&)>
        callback) {
  DVLOG(3) << __func__ << " " << selector;
  FindElement(
      selector,
      /* strict_mode= */ true,
      base::BindOnce(&WebController::OnFindElementForGetOuterHtml,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::GetVisualViewport(
    base::OnceCallback<void(bool, const RectF&)> callback) {
  devtools_client_->GetRuntime()->Evaluate(
      runtime::EvaluateParams::Builder()
          .SetExpression(std::string(kGetVisualViewport))
          .SetReturnByValue(true)
          .Build(),
      /* node_frame_id= */ std::string(),
      base::BindOnce(&WebController::OnGetVisualViewport,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnGetVisualViewport(
    base::OnceCallback<void(bool, const RectF&)> callback,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<runtime::EvaluateResult> result) {
  ClientStatus status =
      CheckJavaScriptResult(reply_status, result.get(), __FILE__, __LINE__);
  if (!status.ok() || !result->GetResult()->HasValue() ||
      !result->GetResult()->GetValue()->is_list() ||
      result->GetResult()->GetValue()->GetList().size() != 4u) {
    DVLOG(1) << __func__ << " Failed to get visual viewport: " << status;
    RectF empty;
    std::move(callback).Run(false, empty);
    return;
  }
  const auto& list = result->GetResult()->GetValue()->GetList();
  // Value::GetDouble() is safe to call without checking the value type; it'll
  // return 0.0 if the value has the wrong type.

  float left = static_cast<float>(list[0].GetDouble());
  float top = static_cast<float>(list[1].GetDouble());
  float width = static_cast<float>(list[2].GetDouble());
  float height = static_cast<float>(list[3].GetDouble());

  RectF rect;
  rect.left = left;
  rect.top = top;
  rect.right = left + width;
  rect.bottom = top + height;

  std::move(callback).Run(true, rect);
}

void WebController::GetElementPosition(
    const Selector& selector,
    base::OnceCallback<void(bool, const RectF&)> callback) {
  FindElement(
      selector, /* strict_mode= */ true,
      base::BindOnce(&WebController::OnFindElementForPosition,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnFindElementForPosition(
    base::OnceCallback<void(bool, const RectF&)> callback,
    const ClientStatus& status,
    std::unique_ptr<ElementFinder::Result> result) {
  if (!status.ok()) {
    RectF empty;
    std::move(callback).Run(false, empty);
    return;
  }

  std::vector<std::unique_ptr<runtime::CallArgument>> argument;
  argument.emplace_back(
      runtime::CallArgument::Builder().SetObjectId(result->object_id).Build());
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(result->object_id)
          .SetArguments(std::move(argument))
          .SetFunctionDeclaration(std::string(kGetBoundingClientRectAsList))
          .SetReturnByValue(true)
          .Build(),
      result->node_frame_id,
      base::BindOnce(&WebController::OnGetElementPositionResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnGetElementPositionResult(
    base::OnceCallback<void(bool, const RectF&)> callback,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  ClientStatus status =
      CheckJavaScriptResult(reply_status, result.get(), __FILE__, __LINE__);
  if (!status.ok() || !result->GetResult()->HasValue() ||
      !result->GetResult()->GetValue()->is_list() ||
      result->GetResult()->GetValue()->GetList().size() != 4u) {
    DVLOG(2) << __func__ << " Failed to get element position: " << status;
    RectF empty;
    std::move(callback).Run(false, empty);
    return;
  }
  const auto& list = result->GetResult()->GetValue()->GetList();
  // Value::GetDouble() is safe to call without checking the value type; it'll
  // return 0.0 if the value has the wrong type.

  RectF rect;
  rect.left = static_cast<float>(list[0].GetDouble());
  rect.top = static_cast<float>(list[1].GetDouble());
  rect.right = static_cast<float>(list[2].GetDouble());
  rect.bottom = static_cast<float>(list[3].GetDouble());

  std::move(callback).Run(true, rect);
}

void WebController::OnFindElementForGetOuterHtml(
    base::OnceCallback<void(const ClientStatus&, const std::string&)> callback,
    const ClientStatus& status,
    std::unique_ptr<ElementFinder::Result> element_result) {
  if (!status.ok()) {
    DVLOG(2) << __func__ << " Failed to find element for GetOuterHtml";
    std::move(callback).Run(status, "");
    return;
  }

  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(element_result->object_id)
          .SetFunctionDeclaration(std::string(kGetOuterHtmlScript))
          .SetReturnByValue(true)
          .Build(),
      element_result->node_frame_id,
      base::BindOnce(&WebController::OnGetOuterHtml,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnGetOuterHtml(
    base::OnceCallback<void(const ClientStatus&, const std::string&)> callback,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  ClientStatus status =
      CheckJavaScriptResult(reply_status, result.get(), __FILE__, __LINE__);
  if (!status.ok()) {
    DVLOG(2) << __func__ << " Failed to get HTML content for GetOuterHtml";
    std::move(callback).Run(status, "");
    return;
  }
  std::string value;
  SafeGetStringValue(result->GetResult(), &value);
  std::move(callback).Run(OkClientStatus(), value);
}

void WebController::WaitForDocumentToBecomeInteractive(
    int remaining_rounds,
    const std::string& object_id,
    const std::string& node_frame_id,
    base::OnceCallback<void(bool)> callback) {
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(object_id)
          .SetFunctionDeclaration(std::string(kIsDocumentReadyForInteract))
          .SetReturnByValue(true)
          .Build(),
      node_frame_id,
      base::BindOnce(&WebController::OnWaitForDocumentToBecomeInteractive,
                     weak_ptr_factory_.GetWeakPtr(), remaining_rounds,
                     object_id, node_frame_id, std::move(callback)));
}

void WebController::OnWaitForDocumentToBecomeInteractive(
    int remaining_rounds,
    const std::string& object_id,
    const std::string& node_frame_id,
    base::OnceCallback<void(bool)> callback,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  ClientStatus status =
      CheckJavaScriptResult(reply_status, result.get(), __FILE__, __LINE__);
  if (!status.ok() || remaining_rounds <= 0) {
    DVLOG(1) << __func__
             << " Failed to wait for the document to become interactive with "
                "remaining_rounds: "
             << remaining_rounds;
    std::move(callback).Run(false);
    return;
  }

  bool ready;
  if (SafeGetBool(result->GetResult(), &ready) && ready) {
    std::move(callback).Run(true);
    return;
  }

  base::PostDelayedTask(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&WebController::WaitForDocumentToBecomeInteractive,
                     weak_ptr_factory_.GetWeakPtr(), --remaining_rounds,
                     object_id, node_frame_id, std::move(callback)),
      settings_->document_ready_check_interval);
}

}  // namespace autofill_assistant
