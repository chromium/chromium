// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/web/web_controller.h"

#include <math.h>
#include <algorithm>
#include <ctime>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/containers/cxx20_erase.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/data_model/autofill_data_model.h"
#include "components/autofill/core/browser/data_model/autofillable_data.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/public/rectf.h"
#include "components/autofill_assistant/browser/string_conversions_util.h"
#include "components/autofill_assistant/browser/user_data_util.h"
#include "components/autofill_assistant/browser/web/element.h"
#include "components/autofill_assistant/browser/web/element_finder_result.h"
#include "components/autofill_assistant/browser/web/element_finder_result_type.h"
#include "components/autofill_assistant/browser/web/selector_observer.h"
#include "components/autofill_assistant/browser/web/web_controller_util.h"
#include "components/autofill_assistant/content/browser/content_autofill_assistant_driver.h"
#include "components/autofill_assistant/content/common/autofill_assistant_agent.mojom.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

namespace autofill_assistant {
using autofill::ContentAutofillDriver;

namespace {

// Get the visual viewport as a list of values to fill into RectF, that is:
// left, top, right, bottom.
const char* const kGetVisualViewport =
    R"({ const v = window.visualViewport;
         [v.pageLeft,
          v.pageTop,
          v.pageLeft + v.width,
          v.pageTop + v.height] })";

// Scrolls to the specified node with top padding. The top padding can
// be specified through pixels or ratio. Pixels take precedence.
const char* const kScrollIntoViewWithPaddingScript =
    R"(function(node, topPaddingPixels, topPaddingRatio, container = null) {
    node.scrollIntoViewIfNeeded();

    let scrollable = window;
    let containerTop = 0;
    if (container){
      scrollable = container;
      containerTop = container.getBoundingClientRect().top;
    }

    const rect = node.getBoundingClientRect();
    let topPadding = topPaddingPixels;
    if (!topPadding){
      topPadding = window.innerHeight * topPaddingRatio;
    }

    scrollable.scrollBy({top: rect.top - topPadding - containerTop});
  })";

// Scroll the window or any scrollable container as needed for the element to
// appear. Behave as specified according to |animation|, |verticalAlignment|
// and |horizontalAlignment|.
const char* const kScrollIntoViewScript =
    R"(function(animation, verticalAlignment, horizontalAlignment) {
      const options = {};
      if (animation !== '') {
        options.behavior = animation;
      }
      if (verticalAlignment !== '') {
        options.block = verticalAlignment;
      }
      if (horizontalAlignment !== '') {
        options.inline = horizontalAlignment;
      }
      this.scrollIntoView(options);
  })";

// Scroll the window or any scrollable container as needed for the element to
// appear. Center the element if specified.
const char* const kScrollIntoViewIfNeededScript =
    R"(function(center) {
      this.scrollIntoViewIfNeeded(center);
    })";

// Scroll the current window by a given amount of |pixels|. If |pixels| is 0,
// take a ratio of the window's height instead.
const char* const kScrollWindowScript =
    R"(function(pixels, windowRatio, animation) {
      let scrollDistance = pixels;
      if (scrollDistance === 0) {
        scrollDistance = window.innerHeight * windowRatio;
      }
      const options = {};
      options.top = scrollDistance;
      if (animation !== '') {
        options.behavior = animation;
      }
      window.scrollBy(options);
    })";

// Scroll the container by a given amount of |pixels|. If |pixels| is 0,
// take a ratio of the window's height instead.
const char* const kScrollContainerScript =
    R"(function(pixels, windowRatio, animation) {
      let scrollDistance = pixels;
      if (scrollDistance === 0) {
        scrollDistance = window.innerHeight * windowRatio;
      }
      const options = {};
      options.top = scrollDistance;
      if (animation !== '') {
        options.behavior = animation;
      }
      this.scrollBy(options);
    })";

// Javascript to select a value from a select box. Also fires a "change" event
// to trigger any listeners. Changing the index directly does not trigger this.
// See |WebController::OnSelectOptionJavascriptResult| for result handling.
const char* const kSelectOptionScript =
    R"(function(re2, valueSourceAttribute, caseSensitive, strict) {
      if (this.options == null) return 31; // INVALID_TARGET
      let regexp;
      try {
        regexp = RegExp(re2, caseSensitive ? '' : 'i');
      } catch (e) {
        return 11; // INVALID_ACTION
      }
      let numResults = 0;
      let newIndex = -1;
      for (let i = 0; i < this.options.length; ++i) {
        if (!this.options[i].disabled &&
            regexp.test(this.options[i][valueSourceAttribute])) {
          ++numResults;
          if (newIndex === -1) {
            newIndex = i;
          }
        }
      }
      if (numResults === 0) {
        return 16; // OPTION_VALUE_NOT_FOUND
      }
      if (numResults == 1 || !strict) {
        this.options.selectedIndex = newIndex;
        const e = document.createEvent('HTMLEvents');
        e.initEvent('change', true, true);
        this.dispatchEvent(e);
        return 2; // ACTION_APPLIED
      }
      return 30; // TOO_MANY_OPTION_VALUES_FOUND
    })";

// Javascript to select the option element in a select element. This does *not*
// fire a "change" event.
// See |WebController::OnSelectOptionJavascriptResult| for result handling.
const char* const kSelectOptionElementScript =
    R"(function(option) {
      if (this.options == null) return 31; // INVALID_TARGET
      for (let i = 0; i < this.options.length; ++i) {
        if (this.options[i] === option) {
          this.options.selectedIndex = i;
          return 2; // ACTION_APPLIED
        }
      }
      return 16; // OPTION_VALUE_NOT_FOUND
    })";

// Javascript to check the option element in a select element against an
// expected match.
// See |WebController::OnSelectOptionJavascriptResult| for result handling.
const char* const kCheckOptionElementScript =
    R"(function(option) {
      if (this.options == null) return 31; // INVALID_TARGET
      if (this.options[this.options.selectedIndex] === option) {
        return 2; // ACTION_APPLIED
      }
      return 26; // ELEMENT_MISMATCH
    })";

// Javascript code to retrieve the 'value' attribute of a node.
const char* const kGetValueAttributeScript =
    "function () { return this.value; }";

// Javascript code to retrieve the nested |attribute| of a node.
// The function intentionally has no "has value" check, such that a bad access
// will return an error.
const char* const kGetElementAttributeScript =
    R"(function (attributes) {
        let it = this;
        for (let i = 0; i < attributes.length; ++i) {
          it = it[attributes[i]];
        }
        return it;
      })";

// Javascript code to select the current value.
const char* const kSelectFieldValueScript = "function() { this.select(); }";

// Javascript code to focus a field.
const char* const kFocusFieldScript = "function() { this.focus(); }";

// Javascript code to blur a field.
const char* const kBlurFieldScript = "function() { this.blur(); }";

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
// The function intentionally has no "has value" check, such that a bad access
// will return an error.
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

// Javascript code to get the outerHTML of each node in a list.
const char* const kGetOuterHtmlsScript =
    "function () { return this.map((e) => e.outerHTML); }";

// Javascript code to get the outerHTML with redacted text.
const char* const kGetOuterHtmlRedactTextScript =
    R"(
      function () {
        function traverse(element, visit_fn) {
          visit_fn(element);
          for (const child of element.childNodes)
            traverse(child, visit);
        }
        function visit(node) {
          if (node.nodeType === Node.TEXT_NODE) {
            // New lines from the inner text are preserved.
            node.textContent = node.textContent.replaceAll(/./g, '');
          }
        }

        if (Array.isArray(this)) {
          return this.map((e) => {
            let clone = e.cloneNode(true);
            traverse(clone, visit);
            return clone.outerHTML;
          });
        } else {
          let clone = this.cloneNode(true);
          traverse(clone, visit);
          return clone.outerHTML;
        }
      }
   )";
const char* const kGetElementTagScript = "function () { return this.tagName; }";

// Javascript code to click on an element.
const char* const kClickElementScript =
    R"(function () {
      this.click();
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

const char* const kSendChangeEventScript =
    R"(function () {
         const e = document.createEvent('HTMLEvents');
         e.initEvent('change', true, true);
         this.dispatchEvent(e);
       })";

const char* const kDispatchEventToDocumentScript =
    R"(const event = new Event('duplexweb');
       document.dispatchEvent(event);)";

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
void AppendWaitForDocumentReadyStateFunction(DocumentReadyState min_ready_state,
                                             std::string* out) {
  // quoted_names covers all possible DocumentReadyState values.
  std::vector<std::string> quoted_names(DOCUMENT_MAX_READY_STATE + 1);
  for (int i = 0; i <= DOCUMENT_MAX_READY_STATE; i++) {
    quoted_names[i] = DocumentReadyStateToQuotedJsString(i);
  }
  base::StrAppend(
      out, {R"((function (minReadyStateNum) {
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
}))",
            base::StringPrintf("(%d)", static_cast<int>(min_ready_state))});
}

void WrapCallbackNoWait(
    base::OnceCallback<void(const ClientStatus&, DocumentReadyState)> callback,
    const ClientStatus& status,
    DocumentReadyState state,
    base::TimeDelta ignored_time) {
  std::move(callback).Run(status, state);
}

void DecorateWebControllerStatus(
    WebControllerErrorInfoProto::WebAction web_action,
    base::OnceCallback<void(const ClientStatus&)> callback,
    const ClientStatus& status) {
  ClientStatus copy = status;
  if (!status.ok()) {
    VLOG(1) << web_action << " failed with status: " << status;
    FillWebControllerErrorInfo(web_action, &copy);
  }
  std::move(callback).Run(copy);
}

template <typename T>
void DecorateControllerStatusWithValue(
    WebControllerErrorInfoProto::WebAction web_action,
    base::OnceCallback<void(const ClientStatus&, const T&)> callback,
    const ClientStatus& status,
    const T& result) {
  ClientStatus copy = status;
  if (!status.ok()) {
    VLOG(1) << web_action << " failed with status: " << status;
    FillWebControllerErrorInfo(web_action, &copy);
  }
  std::move(callback).Run(copy, result);
}

}  // namespace

// static
std::unique_ptr<WebController> WebController::CreateForWebContents(
    content::WebContents* web_contents,
    const UserData* user_data,
    ProcessedActionStatusDetailsProto* log_info,
    AnnotateDomModelService* annotate_dom_model_service,
    bool enable_full_stack_traces) {
  return std::make_unique<WebController>(
      web_contents,
      std::make_unique<DevtoolsClient>(
          content::DevToolsAgentHost::GetOrCreateFor(web_contents),
          enable_full_stack_traces),
      user_data, log_info, annotate_dom_model_service);
}

WebController::WebController(
    content::WebContents* web_contents,
    std::unique_ptr<DevtoolsClient> devtools_client,
    const UserData* user_data,
    ProcessedActionStatusDetailsProto* log_info,
    AnnotateDomModelService* annotate_dom_model_service)
    : web_contents_(web_contents),
      devtools_client_(std::move(devtools_client)),
      user_data_(user_data),
      log_info_(log_info),
      annotate_dom_model_service_(annotate_dom_model_service) {}

WebController::~WebController() {}

void WebController::LoadURL(const GURL& url) {
#ifdef NDEBUG
  VLOG(3) << __func__ << " <redacted>";
#else
  DVLOG(3) << __func__ << " " << url;
#endif
  web_contents_->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(url));
}

void WebController::OnJavaScriptResult(
    base::OnceCallback<void(const ClientStatus&)> callback,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  ClientStatus status =
      CheckJavaScriptResult(reply_status, result.get(), __FILE__, __LINE__);
  if (!status.ok()) {
    DVLOG(1) << __func__ << " Failed JavaScript with status: " << status;
  }
  std::move(callback).Run(status);
}

void WebController::OnJavaScriptResultForInt(
    base::OnceCallback<void(const ClientStatus&, int)> callback,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  int value;
  ClientStatus status =
      CheckJavaScriptResult(reply_status, result.get(), __FILE__, __LINE__);
  if (!status.ok()) {
    DVLOG(1) << __func__ << "Failed JavaScript with status: " << status;
    std::move(callback).Run(status, 0);
    return;
  }
  if (!SafeGetIntValue(result->GetResult(), &value)) {
    std::move(callback).Run(
        UnexpectedDevtoolsErrorStatus(reply_status, __FILE__, __LINE__), 0);
    return;
  }
  std::move(callback).Run(status, value);
}

void WebController::OnJavaScriptResultForString(
    base::OnceCallback<void(const ClientStatus&, const std::string&)> callback,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  std::string value;
  ClientStatus status =
      CheckJavaScriptResult(reply_status, result.get(), __FILE__, __LINE__);
  if (!status.ok()) {
    DVLOG(1) << __func__ << "Failed JavaScript with status: " << status;
    std::move(callback).Run(status, std::string());
    return;
  }
  SafeGetStringValue(result->GetResult(), &value);
  std::move(callback).Run(status, value);
}

void WebController::OnJavaScriptResultForStringArray(
    base::OnceCallback<void(const ClientStatus&,
                            const std::vector<std::string>&)> callback,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  ClientStatus status =
      CheckJavaScriptResult(reply_status, result.get(), __FILE__, __LINE__);
  if (!status.ok()) {
    DVLOG(1) << __func__ << "Failed JavaScript with status: " << status;
    std::move(callback).Run(status, {});
    return;
  }

  auto* remote_object = result->GetResult();
  if (!remote_object || !remote_object->HasValue() ||
      !remote_object->GetValue()->is_list()) {
    DVLOG(1) << __func__ << "JavaScript result is not an array.";
    std::move(callback).Run(
        JavaScriptErrorStatus(reply_status, __FILE__, __LINE__,
                              /* exception= */ nullptr),
        {});
    return;
  }

  const base::Value::List& values = remote_object->GetValue()->GetList();
  std::vector<std::string> v;
  for (const base::Value& value : values) {
    if (!value.is_string()) {
      DVLOG(1) << __func__
               << "JavaScript array content is not a string: " << value.type();
      std::move(callback).Run(
          JavaScriptErrorStatus(reply_status, __FILE__, __LINE__,
                                /* exception= */ nullptr),
          {});
      return;
    }

    v.push_back(value.GetString());
  }

  std::move(callback).Run(status, v);
}

void WebController::ExecuteJsWithoutArguments(
    const ElementFinderResult& element,
    const std::string& js_snippet,
    WebControllerErrorInfoProto::WebAction web_action,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(element.object_id())
          .SetFunctionDeclaration(js_snippet)
          .SetReturnByValue(true)
          .SetAwaitPromise(true)
          .Build(),
      element.node_frame_id(),
      base::BindOnce(&WebController::OnExecuteJsWithoutArguments,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::BindOnce(&DecorateWebControllerStatus, web_action,
                                    std::move(callback))));
}

void WebController::OnExecuteJsWithoutArguments(
    base::OnceCallback<void(const ClientStatus&)> callback,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  ClientStatus status =
      CheckJavaScriptResult(reply_status, result.get(), __FILE__, __LINE__);
  if (!status.ok()) {
    DVLOG(1) << __func__ << " Failed JavaScript with status: " << status;
    std::move(callback).Run(status);
    return;
  }

  if (!result->GetResult() || !result->GetResult()->HasValue()) {
    // No result means everything went as expected.
    std::move(callback).Run(OkClientStatus());
    return;
  }
  int value;
  if (!SafeGetIntValue(result->GetResult(), &value) ||
      !ProcessedActionStatusProto_IsValid(value)) {
    // If a result is present, we expect it to be an integer.
    std::move(callback).Run(ClientStatus(INVALID_ACTION));
    return;
  }

  std::move(callback).Run(
      ClientStatus(static_cast<ProcessedActionStatusProto>(value)));
}

void WebController::ScrollIntoView(
    const std::string& animation,
    const std::string& vertical_alignment,
    const std::string& horizontal_alignment,
    const ElementFinderResult& element,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  std::vector<std::unique_ptr<runtime::CallArgument>> arguments;
  AddRuntimeCallArgument(animation, &arguments);
  AddRuntimeCallArgument(vertical_alignment, &arguments);
  AddRuntimeCallArgument(horizontal_alignment, &arguments);
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(element.object_id())
          .SetArguments(std::move(arguments))
          .SetFunctionDeclaration(std::string(kScrollIntoViewScript))
          .SetReturnByValue(true)
          .Build(),
      element.node_frame_id(),
      base::BindOnce(
          &WebController::OnJavaScriptResult, weak_ptr_factory_.GetWeakPtr(),
          base::BindOnce(&DecorateWebControllerStatus,
                         WebControllerErrorInfoProto::SCROLL_INTO_VIEW,
                         std::move(callback))));
}

void WebController::ScrollIntoViewIfNeeded(
    bool center,
    const ElementFinderResult& element,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  std::vector<std::unique_ptr<runtime::CallArgument>> argument;
  AddRuntimeCallArgument(center, &argument);
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(element.object_id())
          .SetArguments(std::move(argument))
          .SetFunctionDeclaration(std::string(kScrollIntoViewIfNeededScript))
          .SetReturnByValue(true)
          .Build(),
      element.node_frame_id(),
      base::BindOnce(
          &WebController::OnJavaScriptResult, weak_ptr_factory_.GetWeakPtr(),
          base::BindOnce(
              &DecorateWebControllerStatus,
              WebControllerErrorInfoProto::SCROLL_INTO_VIEW_IF_NEEDED,
              std::move(callback))));
}

void WebController::ScrollWindow(
    const ScrollDistance& scroll_distance,
    const std::string& animation,
    const ElementFinderResult& optional_frame,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  std::string scroll_script = base::StrCat(
      {"(", kScrollWindowScript, ")",
       base::StringPrintf("(%d, %f, %s)", scroll_distance.pixels(),
                          scroll_distance.window_ratio(), animation.c_str())});
  // Note: An optional frame element will have an empty node_frame_id which
  // will be considered as operating in the main frame.
  devtools_client_->GetRuntime()->Evaluate(
      runtime::EvaluateParams::Builder()
          .SetExpression(scroll_script)
          .SetReturnByValue(true)
          .Build(),
      optional_frame.node_frame_id(),
      base::BindOnce(&WebController::OnScrollWindow,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::BindOnce(&DecorateWebControllerStatus,
                                    WebControllerErrorInfoProto::SCROLL_WINDOW,
                                    std::move(callback))));
}

void WebController::OnScrollWindow(
    base::OnceCallback<void(const ClientStatus&)> callback,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<runtime::EvaluateResult> result) {
  std::move(callback).Run(
      CheckJavaScriptResult(reply_status, result.get(), __FILE__, __LINE__));
}

void WebController::ScrollContainer(
    const ScrollDistance& scroll_distance,
    const std::string& animation,
    const ElementFinderResult& element,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  std::vector<std::unique_ptr<runtime::CallArgument>> arguments;
  AddRuntimeCallArgument(scroll_distance.pixels(), &arguments);
  AddRuntimeCallArgument(scroll_distance.window_ratio(), &arguments);
  AddRuntimeCallArgument(animation, &arguments);
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(element.object_id())
          .SetArguments(std::move(arguments))
          .SetFunctionDeclaration(std::string(kScrollContainerScript))
          .SetReturnByValue(true)
          .Build(),
      element.node_frame_id(),
      base::BindOnce(
          &WebController::OnJavaScriptResult, weak_ptr_factory_.GetWeakPtr(),
          base::BindOnce(&DecorateWebControllerStatus,
                         WebControllerErrorInfoProto::SCROLL_CONTAINER,
                         std::move(callback))));
}

void WebController::CheckOnTop(
    const ElementFinderResult& element,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  auto worker = std::make_unique<CheckOnTopWorker>(devtools_client_.get());
  auto* ptr = worker.get();
  pending_workers_.emplace_back(std::move(worker));
  ptr->Start(element,
             base::BindOnce(&WebController::OnCheckOnTop,
                            weak_ptr_factory_.GetWeakPtr(), ptr,
                            base::BindOnce(&DecorateWebControllerStatus,
                                           WebControllerErrorInfoProto::ON_TOP,
                                           std::move(callback))));
}

void WebController::OnCheckOnTop(
    CheckOnTopWorker* worker_to_release,
    base::OnceCallback<void(const ClientStatus&)> callback,
    const ClientStatus& status) {
  base::EraseIf(pending_workers_, [worker_to_release](const auto& worker) {
    return worker.get() == worker_to_release;
  });
  if (!status.ok()) {
    VLOG(1) << __func__ << " Element is not on top: " << status;
  }
  std::move(callback).Run(status);
}

void WebController::WaitUntilElementIsStable(
    int max_rounds,
    base::TimeDelta check_interval,
    const ElementFinderResult& element,
    base::OnceCallback<void(const ClientStatus&, base::TimeDelta)> callback) {
  std::unique_ptr<ElementPositionGetter> getter =
      std::make_unique<ElementPositionGetter>(devtools_client_.get(),
                                              max_rounds, check_interval,
                                              element.node_frame_id());
  auto* ptr = getter.get();
  pending_workers_.emplace_back(std::move(getter));
  ptr->Start(element.render_frame_host(), element.object_id(),
             base::BindOnce(&WebController::OnWaitUntilElementIsStable,
                            weak_ptr_factory_.GetWeakPtr(), ptr,
                            base::TimeTicks::Now(), std::move(callback)));
}

void WebController::OnWaitUntilElementIsStable(
    ElementPositionGetter* getter_to_release,
    base::TimeTicks wait_start_time,
    base::OnceCallback<void(const ClientStatus&, base::TimeDelta)> callback,
    const ClientStatus& status) {
  base::EraseIf(pending_workers_, [getter_to_release](const auto& worker) {
    return worker.get() == getter_to_release;
  });
  ClientStatus end_status = status;
  if (!status.ok()) {
    VLOG(1) << __func__ << " Element unstable.";
    FillWebControllerErrorInfo(
        WebControllerErrorInfoProto::WAIT_UNTIL_ELEMENT_IS_STABLE, &end_status);
  }
  std::move(callback).Run(end_status, base::TimeTicks::Now() - wait_start_time);
}

void WebController::JsClickElement(
    const ElementFinderResult& element,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  ExecuteJsWithoutArguments(element, std::string(kClickElementScript),
                            WebControllerErrorInfoProto::JS_CLICK_ELEMENT,
                            std::move(callback));
}

void WebController::ClickOrTapElement(
    ClickType click_type,
    const ElementFinderResult& element,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  if (click_type != ClickType::TAP && click_type != ClickType::CLICK) {
    ClientStatus status(INVALID_ACTION);
    FillWebControllerErrorInfo(
        WebControllerErrorInfoProto::CLICK_OR_TAP_ELEMENT, &status);
    std::move(callback).Run(status);
    return;
  }
  std::unique_ptr<ClickOrTapWorker> worker =
      std::make_unique<ClickOrTapWorker>(devtools_client_.get());
  auto* ptr = worker.get();
  pending_workers_.emplace_back(std::move(worker));
  ptr->Start(
      element, click_type,
      base::BindOnce(
          &WebController::OnClickOrTapElement, weak_ptr_factory_.GetWeakPtr(),
          ptr,
          base::BindOnce(&DecorateWebControllerStatus,
                         WebControllerErrorInfoProto::CLICK_OR_TAP_ELEMENT,
                         std::move(callback))));
}

void WebController::OnClickOrTapElement(
    ClickOrTapWorker* getter_to_release,
    base::OnceCallback<void(const ClientStatus&)> callback,
    const ClientStatus& status) {
  base::EraseIf(pending_workers_, [getter_to_release](const auto& worker) {
    return worker.get() == getter_to_release;
  });
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
    const ElementFinderResult& optional_frame_element,
    base::OnceCallback<void(const ClientStatus&, DocumentReadyState)>
        callback) {
  WaitForDocumentReadyState(
      optional_frame_element, DOCUMENT_UNKNOWN_READY_STATE,
      base::BindOnce(&WrapCallbackNoWait, std::move(callback)));
}

void WebController::WaitForDocumentReadyState(
    const ElementFinderResult& optional_frame_element,
    DocumentReadyState min_ready_state,
    base::OnceCallback<void(const ClientStatus&,
                            DocumentReadyState,
                            base::TimeDelta)> callback) {
  // Note: An optional frame element will have an empty node_frame_id which
  // will be considered as operating in the main frame.
  std::string expression;
  AppendWaitForDocumentReadyStateFunction(min_ready_state, &expression);
  devtools_client_->GetRuntime()->Evaluate(
      runtime::EvaluateParams::Builder()
          .SetExpression(expression)
          .SetReturnByValue(true)
          .SetAwaitPromise(true)
          .Build(),
      optional_frame_element.node_frame_id(),
      base::BindOnce(&WebController::OnWaitForDocumentReadyState,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     base::TimeTicks::Now()));
}

void WebController::OnWaitForDocumentReadyState(
    base::OnceCallback<void(const ClientStatus&,
                            DocumentReadyState,
                            base::TimeDelta)> callback,
    base::TimeTicks wait_start_time,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<runtime::EvaluateResult> result) {
  ClientStatus status =
      CheckJavaScriptResult(reply_status, result.get(), __FILE__, __LINE__);
  if (!status.ok()) {
    VLOG(1) << __func__ << " Failed to get document ready state.";
    FillWebControllerErrorInfo(
        WebControllerErrorInfoProto::WAIT_FOR_DOCUMENT_READY_STATE, &status);
  }

  int ready_state = 0;
  SafeGetIntValue(result ? result->GetResult() : nullptr, &ready_state);
  std::move(callback).Run(status, static_cast<DocumentReadyState>(ready_state),
                          base::TimeTicks::Now() - wait_start_time);
}

void WebController::FindElement(const Selector& selector,
                                bool strict_mode,
                                ElementFinder::Callback callback) {
  RunElementFinder(/* start_element= */ ElementFinderResult::EmptyResult(),
                   selector,
                   strict_mode ? ElementFinderResultType::kExactlyOneMatch
                               : ElementFinderResultType::kAnyMatch,
                   std::move(callback));
}

void WebController::FindAllElements(const Selector& selector,
                                    ElementFinder::Callback callback) {
  RunElementFinder(/* start_element= */ ElementFinderResult::EmptyResult(),
                   selector, ElementFinderResultType::kMatchArray,
                   std::move(callback));
}

void WebController::RunElementFinder(const ElementFinderResult& start_element,
                                     const Selector& selector,
                                     ElementFinderResultType result_type,
                                     ElementFinder::Callback callback) {
  auto finder = std::make_unique<ElementFinder>(
      web_contents_, devtools_client_.get(), user_data_, log_info_,
      annotate_dom_model_service_, selector, result_type);

  auto* ptr = finder.get();
  pending_workers_.emplace_back(std::move(finder));
  ptr->Start(start_element, base::BindOnce(&WebController::OnFindElementResult,
                                           weak_ptr_factory_.GetWeakPtr(), ptr,
                                           std::move(callback)));
}

void WebController::OnFindElementResult(
    ElementFinder* finder_to_release,
    ElementFinder::Callback callback,
    const ClientStatus& status,
    std::unique_ptr<ElementFinderResult> result) {
  base::EraseIf(pending_workers_, [finder_to_release](const auto& worker) {
    return worker.get() == finder_to_release;
  });
  std::move(callback).Run(status, std::move(result));
}

ClientStatus WebController::ObserveSelectors(
    const std::vector<SelectorObserver::ObservableSelector>& selectors,
    const SelectorObserver::Settings& settings,
    SelectorObserver::Callback callback) {
  auto observer = std::make_unique<SelectorObserver>(
      selectors, settings, web_contents_, devtools_client_.get(), user_data_,
      std::move(callback));
  auto* ptr = observer.get();
  pending_workers_.emplace_back(std::move(observer));
  return ptr->Start(base::BindOnce(&WebController::OnSelectorObserverFinished,
                                   weak_ptr_factory_.GetWeakPtr(), ptr));
}

void WebController::OnSelectorObserverFinished(SelectorObserver* observer) {
  base::EraseIf(pending_workers_, [observer](const auto& worker) {
    return worker.get() == observer;
  });
}

void WebController::FillAddressForm(
    std::unique_ptr<autofill::AutofillProfile> profile,
    const AutofillAssistantIntent intent,
    const ElementFinderResult& element,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  autofill::AutofillableData data_to_autofill(profile.get());
  GetElementFormAndFieldData(
      element, base::BindOnce(&WebController::OnGetFormAndFieldDataForFilling,
                              weak_ptr_factory_.GetWeakPtr(), data_to_autofill,
                              std::move(profile), intent, std::move(callback)));
}

void WebController::FillCardForm(
    std::unique_ptr<autofill::CreditCard> card,
    const AutofillAssistantIntent intent,
    const std::u16string& cvc,
    const ElementFinderResult& element,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  autofill::AutofillableData data_to_autofill(card.get(), cvc);
  GetElementFormAndFieldData(
      element, base::BindOnce(&WebController::OnGetFormAndFieldDataForFilling,
                              weak_ptr_factory_.GetWeakPtr(), data_to_autofill,
                              std::move(card), intent, std::move(callback)));
}

void WebController::RetrieveElementFormAndFieldData(
    const Selector& selector,
    base::OnceCallback<void(const ClientStatus&,
                            content::RenderFrameHost* rfh,
                            const autofill::FormData& form_data,
                            const autofill::FormFieldData& field_data)>
        callback) {
  DVLOG(3) << __func__ << " " << selector;
  FindElement(
      selector, /* strict_mode= */ true,
      base::BindOnce(
          &WebController::OnFindElementForRetrieveElementFormAndFieldData,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnFindElementForRetrieveElementFormAndFieldData(
    base::OnceCallback<void(const ClientStatus&,
                            content::RenderFrameHost* rfh,
                            const autofill::FormData& form_data,
                            const autofill::FormFieldData& field_data)>
        callback,
    const ClientStatus& element_status,
    std::unique_ptr<ElementFinderResult> element_result) {
  if (!element_status.ok()) {
    DVLOG(1) << __func__
             << " Failed to find the element for getting Autofill data.";
    std::move(callback).Run(element_status, element_result->render_frame_host(),
                            autofill::FormData(), autofill::FormFieldData());
    return;
  }

  const ElementFinderResult* element_result_ptr = element_result.get();
  GetElementFormAndFieldData(
      *element_result_ptr,
      base::BindOnce(&WebController::OnGetFormAndFieldDataForRetrieving,
                     weak_ptr_factory_.GetWeakPtr(), std::move(element_result),
                     std::move(callback)));
}

void WebController::GetElementFormAndFieldData(
    const ElementFinderResult& element,
    base::OnceCallback<void(const ClientStatus&,
                            ContentAutofillDriver* driver,
                            const autofill::FormData&,
                            const autofill::FormFieldData&)> callback) {
  if (!element.backend_node_id()) {
    DVLOG(1) << __func__
             << "No backend node id on element intended for native execution.";
    std::move(callback).Run(UnexpectedErrorStatus(__FILE__, __LINE__), nullptr,
                            autofill::FormData(), autofill::FormFieldData());
    return;
  }

  ContentAutofillDriver* driver =
      ContentAutofillDriver::GetForRenderFrameHost(element.render_frame_host());
  if (driver == nullptr) {
    DVLOG(1) << __func__ << " Failed to get the autofill driver.";
    std::move(callback).Run(UnexpectedErrorStatus(__FILE__, __LINE__), nullptr,
                            autofill::FormData(), autofill::FormFieldData());
    return;
  }

  driver->GetAutofillAgent()->GetElementFormAndFieldDataForDevToolsNodeId(
      *element.backend_node_id(),
      base::BindOnce(&WebController::OnGetFormAndFieldData,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     driver));
}

void WebController::GetBackendNodeId(
    const ElementFinderResult& element,
    base::OnceCallback<void(const ClientStatus&, int)> callback) {
  devtools_client_->GetDOM()->DescribeNode(
      dom::DescribeNodeParams::Builder()
          .SetObjectId(element.object_id())
          .Build(),
      element.node_frame_id(),
      base::BindOnce(&WebController::OnGetBackendNodeId,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnGetBackendNodeId(
    base::OnceCallback<void(const ClientStatus&, int)> callback,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<dom::DescribeNodeResult> result) {
  if (!result || !result->GetNode()) {
    VLOG(1) << __func__ << " Failed to describe the node";
    std::move(callback).Run(
        UnexpectedDevtoolsErrorStatus(reply_status, __FILE__, __LINE__), 0);
    return;
  }

  std::move(callback).Run(OkClientStatus(),
                          result->GetNode()->GetBackendNodeId());
}

void WebController::OnGetFormAndFieldData(
    base::OnceCallback<void(const ClientStatus&,
                            ContentAutofillDriver* driver,
                            const autofill::FormData&,
                            const autofill::FormFieldData&)> callback,
    ContentAutofillDriver* driver,
    const autofill::FormData& form_data,
    const autofill::FormFieldData& form_field_data) {
  if (form_data.fields.empty()) {
    DVLOG(1) << __func__ << " Failed to get form data.";
    std::move(callback).Run(UnexpectedErrorStatus(__FILE__, __LINE__), driver,
                            autofill::FormData(), autofill::FormFieldData());
    return;
  }
  std::move(callback).Run(OkClientStatus(), driver, form_data, form_field_data);
}

void WebController::OnGetFormAndFieldDataForFilling(
    const autofill::AutofillableData& data_to_autofill,
    std::unique_ptr<autofill::AutofillDataModel> retain_data,
    const AutofillAssistantIntent intent,
    base::OnceCallback<void(const ClientStatus&)> callback,
    const ClientStatus& form_status,
    ContentAutofillDriver* driver,
    const autofill::FormData& form_data,
    const autofill::FormFieldData& form_field) {
  if (!form_status.ok()) {
    std::move(callback).Run(form_status);
    return;
  }
  driver->FillFormForAssistant(data_to_autofill, form_data, form_field, intent);
  std::move(callback).Run(OkClientStatus());
}

void WebController::OnGetFormAndFieldDataForRetrieving(
    std::unique_ptr<ElementFinderResult> element,
    base::OnceCallback<void(const ClientStatus&,
                            content::RenderFrameHost* rfh,
                            const autofill::FormData& form_data,
                            const autofill::FormFieldData& field_data)>
        callback,
    const ClientStatus& form_status,
    ContentAutofillDriver* driver,
    const autofill::FormData& form_data,
    const autofill::FormFieldData& form_field) {
  std::move(callback).Run(form_status, element->render_frame_host(), form_data,
                          form_field);
}

void WebController::SelectOption(
    const std::string& re2,
    bool case_sensitive,
    SelectOptionProto::OptionComparisonAttribute option_comparison_attribute,
    bool strict,
    const ElementFinderResult& element,
    base::OnceCallback<void(const ClientStatus&)> callback) {
#ifdef NDEBUG
  VLOG(3) << __func__ << " re2=(redacted)"
          << ", case_sensitive=" << case_sensitive
          << ", option_comparison_attribute=" << option_comparison_attribute;
#else
  DVLOG(3) << __func__ << " re2=" << re2
           << ", case_sensitive=" << case_sensitive
           << ", option_comparison_attribute=" << option_comparison_attribute;
#endif

  std::vector<std::unique_ptr<runtime::CallArgument>> arguments;
  AddRuntimeCallArgument(re2, &arguments);
  switch (option_comparison_attribute) {
    case SelectOptionProto::VALUE:
      AddRuntimeCallArgument("value", &arguments);
      break;
    case SelectOptionProto::LABEL:
      AddRuntimeCallArgument("label", &arguments);
      break;
    case SelectOptionProto::NOT_SET:
      ClientStatus error(INVALID_ACTION);
      FillWebControllerErrorInfo(WebControllerErrorInfoProto::SELECT_OPTION,
                                 &error);
      std::move(callback).Run(error);
      return;
  }
  AddRuntimeCallArgument(case_sensitive, &arguments);
  AddRuntimeCallArgument(strict, &arguments);

  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(element.object_id())
          .SetArguments(std::move(arguments))
          .SetFunctionDeclaration(std::string(kSelectOptionScript))
          .SetReturnByValue(true)
          .Build(),
      element.node_frame_id(),
      base::BindOnce(&WebController::OnSelectOptionJavascriptResult,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::BindOnce(&DecorateWebControllerStatus,
                                    WebControllerErrorInfoProto::SELECT_OPTION,
                                    std::move(callback))));
}

void WebController::SelectOptionElement(
    const ElementFinderResult& option,
    const ElementFinderResult& element,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  std::vector<std::unique_ptr<runtime::CallArgument>> argument;
  AddRuntimeCallArgumentObjectId(option.object_id(), &argument);
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(element.object_id())
          .SetArguments(std::move(argument))
          .SetFunctionDeclaration(std::string(kSelectOptionElementScript))
          .SetReturnByValue(true)
          .Build(),
      element.node_frame_id(),
      base::BindOnce(
          &WebController::OnSelectOptionJavascriptResult,
          weak_ptr_factory_.GetWeakPtr(),
          base::BindOnce(&DecorateWebControllerStatus,
                         WebControllerErrorInfoProto::SELECT_OPTION_ELEMENT,
                         std::move(callback))));
}

void WebController::CheckSelectedOptionElement(
    const ElementFinderResult& option,
    const ElementFinderResult& element,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  std::vector<std::unique_ptr<runtime::CallArgument>> argument;
  AddRuntimeCallArgumentObjectId(option.object_id(), &argument);
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(element.object_id())
          .SetArguments(std::move(argument))
          .SetFunctionDeclaration(std::string(kCheckOptionElementScript))
          .SetReturnByValue(true)
          .Build(),
      element.node_frame_id(),
      base::BindOnce(
          &WebController::OnSelectOptionJavascriptResult,
          weak_ptr_factory_.GetWeakPtr(),
          base::BindOnce(&DecorateWebControllerStatus,
                         WebControllerErrorInfoProto::CHECK_OPTION_ELEMENT,
                         std::move(callback))));
}

void WebController::OnSelectOptionJavascriptResult(
    base::OnceCallback<void(const ClientStatus&)> callback,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  ClientStatus status =
      CheckJavaScriptResult(reply_status, result.get(), __FILE__, __LINE__);
  if (!status.ok()) {
    std::move(callback).Run(status);
    return;
  }
  int status_result;
  if (!SafeGetIntValue(result->GetResult(), &status_result)) {
    std::move(callback).Run(
        UnexpectedDevtoolsErrorStatus(reply_status, __FILE__, __LINE__));
    return;
  }
  std::move(callback).Run(
      ClientStatus(static_cast<ProcessedActionStatusProto>(status_result)));
}

void WebController::ScrollToElementPosition(
    std::unique_ptr<ElementFinderResult> container,
    const TopPadding& top_padding,
    const ElementFinderResult& element,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  std::vector<std::unique_ptr<runtime::CallArgument>> arguments;
  AddRuntimeCallArgumentObjectId(element.object_id(), &arguments);
  AddRuntimeCallArgument(top_padding.pixels(), &arguments);
  AddRuntimeCallArgument(top_padding.ratio(), &arguments);
  if (container) {
    AddRuntimeCallArgumentObjectId(container->object_id(), &arguments);
  }

  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(element.object_id())
          .SetArguments(std::move(arguments))
          .SetFunctionDeclaration(std::string(kScrollIntoViewWithPaddingScript))
          .SetReturnByValue(true)
          .Build(),
      element.node_frame_id(),
      base::BindOnce(
          &WebController::OnJavaScriptResult, weak_ptr_factory_.GetWeakPtr(),
          base::BindOnce(
              &DecorateWebControllerStatus,
              WebControllerErrorInfoProto::SCROLL_INTO_VIEW_WITH_PADDING,
              std::move(callback))));
}

void WebController::GetFieldValue(
    const ElementFinderResult& element,
    base::OnceCallback<void(const ClientStatus&, const std::string&)>
        callback) {
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(element.object_id())
          .SetFunctionDeclaration(std::string(kGetValueAttributeScript))
          .SetReturnByValue(true)
          .Build(),
      element.node_frame_id(),
      base::BindOnce(
          &WebController::OnJavaScriptResultForString,
          weak_ptr_factory_.GetWeakPtr(),
          base::BindOnce(&DecorateControllerStatusWithValue<std::string>,
                         WebControllerErrorInfoProto::GET_FIELD_VALUE,
                         std::move(callback))));
}

void WebController::GetStringAttribute(
    const std::vector<std::string>& attributes,
    const ElementFinderResult& element,
    base::OnceCallback<void(const ClientStatus&, const std::string&)>
        callback) {
  VLOG(3) << __func__ << " attributes=[" << base::JoinString(attributes, ",")
          << "]";

  if (attributes.empty()) {
    ClientStatus error_status = UnexpectedErrorStatus(__FILE__, __LINE__);
    FillWebControllerErrorInfo(
        WebControllerErrorInfoProto::GET_STRING_ATTRIBUTE, &error_status);
    std::move(callback).Run(error_status, "");
    return;
  }
  base::Value::List attribute_values;
  for (const std::string& attribute : attributes) {
    attribute_values.Append(attribute);
  }

  std::vector<std::unique_ptr<runtime::CallArgument>> arguments;
  AddRuntimeCallArgument(std::move(attribute_values), &arguments);
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(element.object_id())
          .SetArguments(std::move(arguments))
          .SetFunctionDeclaration(std::string(kGetElementAttributeScript))
          .SetReturnByValue(true)
          .Build(),
      element.node_frame_id(),
      base::BindOnce(
          &WebController::OnJavaScriptResultForString,
          weak_ptr_factory_.GetWeakPtr(),
          base::BindOnce(&DecorateControllerStatusWithValue<std::string>,
                         WebControllerErrorInfoProto::GET_STRING_ATTRIBUTE,
                         std::move(callback))));
}

void WebController::SelectFieldValue(
    const ElementFinderResult& element,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  ExecuteJsWithoutArguments(element, std::string(kSelectFieldValueScript),
                            WebControllerErrorInfoProto::SELECT_FIELD_VALUE,
                            std::move(callback));
}

void WebController::SetValueAttribute(
    const std::string& value,
    const ElementFinderResult& element,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  std::vector<std::unique_ptr<runtime::CallArgument>> argument;
  AddRuntimeCallArgument(value, &argument);
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(element.object_id())
          .SetArguments(std::move(argument))
          .SetFunctionDeclaration(std::string(kSetValueAttributeScript))
          .Build(),
      element.node_frame_id(),
      base::BindOnce(
          &WebController::OnJavaScriptResult, weak_ptr_factory_.GetWeakPtr(),
          base::BindOnce(&DecorateWebControllerStatus,
                         WebControllerErrorInfoProto::SET_VALUE_ATTRIBUTE,
                         std::move(callback))));
}

void WebController::SetAttribute(
    const std::vector<std::string>& attributes,
    const std::string& value,
    const ElementFinderResult& element,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  DVLOG(3) << __func__ << " attributes=[" << base::JoinString(attributes, ",")
           << "], value=" << value;

  if (attributes.empty()) {
    ClientStatus error_status = UnexpectedErrorStatus(__FILE__, __LINE__);
    FillWebControllerErrorInfo(WebControllerErrorInfoProto::SET_ATTRIBUTE,
                               &error_status);
    std::move(callback).Run(error_status);
    return;
  }
  base::Value::List attribute_values;
  for (const std::string& attribute : attributes) {
    attribute_values.Append(attribute);
  }

  std::vector<std::unique_ptr<runtime::CallArgument>> arguments;
  AddRuntimeCallArgument(std::move(attribute_values), &arguments);
  AddRuntimeCallArgument(value, &arguments);
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(element.object_id())
          .SetArguments(std::move(arguments))
          .SetFunctionDeclaration(std::string(kSetAttributeScript))
          .Build(),
      element.node_frame_id(),
      base::BindOnce(&WebController::OnJavaScriptResult,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::BindOnce(&DecorateWebControllerStatus,
                                    WebControllerErrorInfoProto::SET_ATTRIBUTE,
                                    std::move(callback))));
}

void WebController::SendTextInput(
    int key_press_delay_in_millisecond,
    const std::string& value,
    const ElementFinderResult& element,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  SendKeyboardInput(UTF8ToUnicode(value), key_press_delay_in_millisecond,
                    element, std::move(callback));
}

void WebController::SendKeyboardInput(
    const std::vector<UChar32>& codepoints,
    const int key_press_delay_in_millisecond,
    const ElementFinderResult& element,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  std::vector<KeyEvent> key_events;
  for (UChar32 codepoint : codepoints) {
    key_events.emplace_back(
        SendKeyboardInputWorker::KeyEventFromCodepoint(codepoint));
  }
  SendKeyEvents(WebControllerErrorInfoProto::SEND_KEYBOARD_INPUT, key_events,
                key_press_delay_in_millisecond, element, std::move(callback));
}

void WebController::SendKeyEvent(
    const KeyEvent& key_event,
    const ElementFinderResult& element,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  SendKeyEvents(WebControllerErrorInfoProto::SEND_KEY_EVENT, {key_event}, 0,
                element, std::move(callback));
}

void WebController::SendKeyEvents(
    WebControllerErrorInfoProto::WebAction web_action,
    const std::vector<KeyEvent>& key_events,
    int key_press_delay,
    const ElementFinderResult& element,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  auto worker =
      std::make_unique<SendKeyboardInputWorker>(devtools_client_.get());
  auto* ptr = worker.get();
  pending_workers_.emplace_back(std::move(worker));
  ptr->Start(
      element.node_frame_id(), key_events, key_press_delay,
      base::BindOnce(&DecorateWebControllerStatus, web_action,
                     base::BindOnce(&WebController::OnSendKeyboardInputDone,
                                    weak_ptr_factory_.GetWeakPtr(), ptr,
                                    std::move(callback))));
}

void WebController::OnSendKeyboardInputDone(
    SendKeyboardInputWorker* worker_to_release,
    base::OnceCallback<void(const ClientStatus&)> callback,
    const ClientStatus& status) {
  base::EraseIf(pending_workers_, [worker_to_release](const auto& worker) {
    return worker.get() == worker_to_release;
  });
  std::move(callback).Run(status);
}

void WebController::FocusField(
    const ElementFinderResult& element,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  ExecuteJsWithoutArguments(element, std::string(kFocusFieldScript),
                            WebControllerErrorInfoProto::FOCUS_FIELD,
                            std::move(callback));
}

void WebController::BlurField(
    const ElementFinderResult& element,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  ExecuteJsWithoutArguments(element, std::string(kBlurFieldScript),
                            WebControllerErrorInfoProto::BLUR_FIELD,
                            std::move(callback));
}

void WebController::GetVisualViewport(
    base::OnceCallback<void(const ClientStatus&, const RectF&)> callback) {
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
    base::OnceCallback<void(const ClientStatus&, const RectF&)> callback,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<runtime::EvaluateResult> result) {
  ClientStatus status =
      CheckJavaScriptResult(reply_status, result.get(), __FILE__, __LINE__);
  if (!status.ok() || !result->GetResult()->HasValue() ||
      !result->GetResult()->GetValue()->is_list() ||
      result->GetResult()->GetValue()->GetList().size() != 4u) {
    VLOG(1) << __func__ << " Failed to get visual viewport: " << status;
    std::move(callback).Run(
        JavaScriptErrorStatus(reply_status, __FILE__, __LINE__, nullptr),
        RectF());
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

  std::move(callback).Run(OkClientStatus(), rect);
}

void WebController::GetElementRect(
    const ElementFinderResult& element,
    ElementRectGetter::ElementRectCallback callback) {
  std::unique_ptr<ElementRectGetter> getter =
      std::make_unique<ElementRectGetter>(devtools_client_.get());
  auto* ptr = getter.get();
  pending_workers_.emplace_back(std::move(getter));
  ptr->Start(
      // TODO(b/172041811): Ownership of element.
      std::make_unique<ElementFinderResult>(element),
      base::BindOnce(&WebController::OnGetElementRect,
                     weak_ptr_factory_.GetWeakPtr(), ptr, std::move(callback)));
}

void WebController::OnGetElementRect(
    ElementRectGetter* getter_to_release,
    ElementRectGetter::ElementRectCallback callback,
    const ClientStatus& rect_status,
    const RectF& element_rect) {
  base::EraseIf(pending_workers_, [getter_to_release](const auto& worker) {
    return worker.get() == getter_to_release;
  });
  std::move(callback).Run(rect_status, element_rect);
}

void WebController::GetOuterHtml(
    bool include_all_inner_text,
    const ElementFinderResult& element,
    base::OnceCallback<void(const ClientStatus&, const std::string&)>
        callback) {
  std::string script = include_all_inner_text ? kGetOuterHtmlScript
                                              : kGetOuterHtmlRedactTextScript;

  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(element.object_id())
          .SetFunctionDeclaration(script)
          .SetReturnByValue(true)
          .Build(),
      element.node_frame_id(),
      base::BindOnce(
          &WebController::OnJavaScriptResultForString,
          weak_ptr_factory_.GetWeakPtr(),
          base::BindOnce(&DecorateControllerStatusWithValue<std::string>,
                         WebControllerErrorInfoProto::GET_OUTER_HTML,
                         std::move(callback))));
}

void WebController::GetOuterHtmls(
    bool include_all_inner_text,
    const ElementFinderResult& elements,
    base::OnceCallback<void(const ClientStatus&,
                            const std::vector<std::string>&)> callback) {
  std::string script = include_all_inner_text ? kGetOuterHtmlsScript
                                              : kGetOuterHtmlRedactTextScript;

  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(elements.object_id())
          .SetFunctionDeclaration(script)
          .SetReturnByValue(true)
          .Build(),
      elements.node_frame_id(),
      base::BindOnce(
          &WebController::OnJavaScriptResultForStringArray,
          weak_ptr_factory_.GetWeakPtr(),
          base::BindOnce(
              &DecorateControllerStatusWithValue<std::vector<std::string>>,
              WebControllerErrorInfoProto::GET_OUTER_HTML,
              std::move(callback))));
}

void WebController::GetElementTag(
    const ElementFinderResult& element,
    base::OnceCallback<void(const ClientStatus&, const std::string&)>
        callback) {
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(element.object_id())
          .SetFunctionDeclaration(std::string(kGetElementTagScript))
          .SetReturnByValue(true)
          .Build(),
      element.node_frame_id(),
      base::BindOnce(
          &WebController::OnJavaScriptResultForString,
          weak_ptr_factory_.GetWeakPtr(),
          base::BindOnce(&DecorateControllerStatusWithValue<std::string>,
                         WebControllerErrorInfoProto::GET_ELEMENT_TAG,
                         std::move(callback))));
}

void WebController::SendChangeEvent(
    const ElementFinderResult& element,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  ExecuteJsWithoutArguments(element, std::string(kSendChangeEventScript),
                            WebControllerErrorInfoProto::SEND_CHANGE_EVENT,
                            std::move(callback));
}

void WebController::DispatchJsEvent(
    base::OnceCallback<void(const ClientStatus&)> callback) {
  devtools_client_->GetRuntime()->Evaluate(
      runtime::EvaluateParams::Builder()
          .SetExpression(kDispatchEventToDocumentScript)
          .SetReturnByValue(true)
          .Build(),
      ElementFinderResult().node_frame_id(),
      base::BindOnce(
          &WebController::OnDispatchJsEvent, weak_ptr_factory_.GetWeakPtr(),
          base::BindOnce(
              &DecorateWebControllerStatus,
              WebControllerErrorInfoProto::DISPATCH_EVENT_ON_DOCUMENT,
              std::move(callback))));
}

void WebController::OnDispatchJsEvent(
    base::OnceCallback<void(const ClientStatus&)> callback,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<runtime::EvaluateResult> result) const {
  ClientStatus status =
      CheckJavaScriptResult(reply_status, result.get(), __FILE__, __LINE__);
  if (!status.ok()) {
    VLOG(1) << __func__
            << "Failed dispatching JavaScript event with status: " << status;
  }
  std::move(callback).Run(status);
}

void WebController::ExecuteJS(
    const std::string& js_snippet,
    const ElementFinderResult& element,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  // We do not add a leading newline to have consistent line numbers from
  // errors. We cannot omit the trailing newline, in case the snippet ends
  // with a comment.
  ExecuteJsWithoutArguments(
      element, base::StrCat({"function() { ", js_snippet, "\n}"}),
      WebControllerErrorInfoProto::EXECUTE_JS, std::move(callback));
}

ContentAutofillAssistantDriver* WebController::GetDriverForElement(
    const ElementFinderResult& element) const {
  if (!element.backend_node_id()) {
    DVLOG(1) << __func__
             << "No backend node id on element intended for native execution.";
    return nullptr;
  }

  auto* render_frame_host = element.render_frame_host();
  DCHECK(render_frame_host);

  return ContentAutofillAssistantDriver::GetOrCreateForRenderFrameHost(
      render_frame_host, annotate_dom_model_service_);
}

void WebController::SetNativeValue(
    const std::string& value,
    const ElementFinderResult& element,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  ContentAutofillAssistantDriver* driver = GetDriverForElement(element);
  if (!driver) {
    std::move(callback).Run(UnexpectedErrorStatus(__FILE__, __LINE__));
    return;
  }
  driver->GetAutofillAssistantAgent()->SetElementValue(
      *element.backend_node_id(), base::UTF8ToUTF16(value),
      /* send_events= */ true,
      base::BindOnce(&WebController::OnSetNativeExecution,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::SetNativeChecked(
    bool checked,
    const ElementFinderResult& element,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  ContentAutofillAssistantDriver* driver = GetDriverForElement(element);
  if (!driver) {
    std::move(callback).Run(UnexpectedErrorStatus(__FILE__, __LINE__));
    return;
  }
  driver->GetAutofillAssistantAgent()->SetElementChecked(
      *element.backend_node_id(), checked,
      /* send_events= */ true,
      base::BindOnce(&WebController::OnSetNativeExecution,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnSetNativeExecution(
    base::OnceCallback<void(const ClientStatus&)> callback,
    bool success) const {
  std::move(callback).Run(success ? OkClientStatus()
                                  : UnexpectedErrorStatus(__FILE__, __LINE__));
}

base::WeakPtr<WebController> WebController::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace autofill_assistant
