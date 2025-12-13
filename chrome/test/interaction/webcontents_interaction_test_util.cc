// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/interaction/webcontents_interaction_test_util.h"

#include <algorithm>
#include <initializer_list>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <string>

#include "base/callback_list.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_auto_reset.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/scoped_observation.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/to_string.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/uuid.h"
#include "base/values.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/contents_web_view.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "chrome/test/interaction/tracked_element_webcontents.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/common/url_utils.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/framework_specific_implementation.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_observer.h"

namespace content {
class RenderFrameHost;
}

namespace {

BrowserWindowInterface* GetBrowserWindowForWebContentsInTab(
    content::WebContents* contents) {
  auto* const tab = tabs::TabInterface::MaybeGetFromContents(contents);
  return tab ? tab->GetBrowserWindowInterface() : nullptr;
}

content::WebContents* GetWebContents(BrowserWindowInterface* browser,
                                     std::optional<int> tab_index) {
  auto* const model = browser->GetTabStripModel();
  return model->GetWebContentsAt(tab_index.value_or(model->active_index()));
}

// Provides a JavaScript skeleton for "does this element exist" queries.
//
// Will evaluate and return `on_not_found` if 'err?.selector' is valid.
// Will evaluate and return `on_found` if 'el' is valid.
std::string GetExistsQuery(const char* on_not_found, const char* on_found) {
  return base::StringPrintf(R"((el, err) => {
        if (err?.selector) return %s;
        if (err) throw err;
        return %s;
      })",
                            on_not_found, on_found);
}

// Does `StateChange` validation, including inferring the actual type for
// `Type::kAuto`, and returns the (potentially updated) StateChange.
WebContentsInteractionTestUtil::StateChange ValidateAndInferStateChange(
    const WebContentsInteractionTestUtil::StateChange& state_change) {
  WebContentsInteractionTestUtil::StateChange configuration = state_change;

  CHECK(configuration.event) << "StateChange missing event - " << configuration;
  CHECK(configuration.timeout.has_value() || !configuration.timeout_event)
      << "StateChange cannot specify timeout event without timeout - "
      << configuration;

  const bool has_function = !configuration.test_function.empty();
  const bool has_where = !configuration.where.empty();
  using Type = WebContentsInteractionTestUtil::StateChange::Type;
  switch (configuration.type) {
    case Type::kAuto:
      if (has_function) {
        configuration.type =
            has_where ? Type::kExistsAndConditionTrue : Type::kConditionTrue;
      } else if (has_where) {
        configuration.type = Type::kExists;
      } else {
        NOTREACHED() << "Unable to infer StateChange type - " << configuration;
      }
      break;
    case Type::kExists:
      CHECK(has_where) << "Expected where to be non-empty - " << configuration;
      CHECK(!has_function) << "Expected test function to be empty - "
                           << configuration;
      break;
    case Type::kDoesNotExist:
      CHECK(has_where) << "Expected where to be non-empty - " << configuration;
      CHECK(!has_function) << "Expected test function to be empty - "
                           << configuration;
      break;
    case Type::kConditionTrue:
      CHECK(!has_where) << "Expected where to be empty - " << configuration;
      CHECK(has_function) << "Expected test function to be non-empty - "
                          << configuration;
      break;
    case Type::kExistsAndConditionTrue:
      CHECK(has_where && has_function)
          << "Expected where and function to be non-empty - " << configuration;
  }
  if (!configuration.check_callback.is_null()) {
    CHECK(has_function)
        << "Cannot specify check callback without test function - "
        << configuration;
  }
  return configuration;
}

// Detects the presence of a javascript `function` that takes (el, err) as
// parameters, for backwards-compatibility with older tests that require this.
//
// Expectation is one of:
//  ... x, y ... => ...
//  ... x, y ... { ...
//
// Functions not in this format will not be recognized as taking an error param.
bool HasErrorParameter(const std::string& function) {
  size_t body1 = function.find("=>");
  size_t body2 = function.find('{');
  const size_t body =
      (body1 == std::string::npos)
          ? body2
          : (body2 == std::string::npos ? body1 : std::min(body1, body2));
  if (body == std::string::npos) {
    return false;
  }
  const size_t comma = function.find(',');
  return comma != std::string::npos && comma < body;
}

// Returns the JS query that must be sent to check a particular state change.
std::string GetStateChangeQuery(
    const WebContentsInteractionTestUtil::StateChange& configuration) {
  // For `kConditionTrue`, `configuration.test_function` can be used directly
  // directly, but for the other options it must be modified.
  using Type = WebContentsInteractionTestUtil::StateChange::Type;
  switch (configuration.type) {
    case Type::kAuto:
      NOTREACHED() << "Auto type should already have been inferred.";
    case Type::kExists:
      return GetExistsQuery(
          /* on_not_found = */ "false",
          /* on_found = */ "true");
    case Type::kDoesNotExist:
      return GetExistsQuery(
          /* on_not_found = */ "true",
          /* on_found = */ "false");
    case Type::kConditionTrue:
      return configuration.test_function;
    case Type::kExistsAndConditionTrue:
      if (HasErrorParameter(configuration.test_function)) {
        return configuration.test_function;
      }
      const std::string on_found = "(" + configuration.test_function + ")(el)";
      return GetExistsQuery(
          /* on_not_found = */ "null", on_found.c_str());
  }
}

// Common execution code for `EvalJsLocal()` and `ExecuteJsLocal()`.
// Executes `script` on `host`.
void ExecuteScript(content::RenderFrameHost* host, const std::string& script) {
  const std::u16string script16 = base::UTF8ToUTF16(script);
  if (host->GetLifecycleState() !=
      content::RenderFrameHost::LifecycleState::kPrerendering) {
    host->ExecuteJavaScriptWithUserGestureForTests(
        script16, base::NullCallback(),
        content::ISOLATED_WORLD_ID_GLOBAL);  // IN-TEST
  } else {
    host->ExecuteJavaScriptForTests(
        script16, base::NullCallback(),
        content::ISOLATED_WORLD_ID_GLOBAL);  // IN-TEST
  }
}

// TODO(dfried): migrate to EvalJs, now that it supports Content Security
// Policy.
content::EvalJsResult EvalJsLocal(
    const content::ToRenderFrameHost& execution_target,
    const std::string& function) {
  content::RenderFrameHost* const host = execution_target.render_frame_host();
  content::DOMMessageQueue dom_message_queue(host);

  // Theoretically, this script, when executed should produce an object with the
  // following value:
  //   [ <token>, [<result>, <error>] ]
  // The values <token> and <error> will be strings, while <result> can be any
  // type.
  std::string token =
      "EvalJsLocal-" + base::Uuid::GenerateRandomV4().AsLowercaseString();
  std::string runner_script = base::StringPrintf(
      R"(
        (() => {
          const replyFunc =
              (reply) => window.domAutomationController.send(['%s', reply]);
          const errorReply =
              (error) => [undefined,
                        error && error.stack ?
                            '\n' + error.stack :
                            'Error: "' + error + '"'];
          try {
            Promise.resolve((%s)())
              .then((result) => [result, ''],
                    (error) => errorReply(error))
              .then((result) => replyFunc(result));
          } catch (err) {
            replyFunc(errorReply(err));
          }
        })(); //# sourceURL=EvalJs-runner.js
      )",
      token.c_str(), function.c_str());

  if (!host->IsRenderFrameLive()) {
    return content::EvalJsResult(base::Value(), "Error: frame has crashed.");
  }

  // This will queue up a message to be returned from the runner.
  ExecuteScript(host, runner_script);

  std::string json;
  if (!dom_message_queue.WaitForMessage(&json)) {
    return content::EvalJsResult(base::Value(),
                                 "Cannot communicate with DOMMessageQueue.");
  }

  auto parsed_json = base::JSONReader::ReadAndReturnValueWithError(
      json, base::JSON_ALLOW_TRAILING_COMMAS);
  if (!parsed_json.has_value()) {
    return content::EvalJsResult(
        base::Value(), "JSON parse error: " + parsed_json.error().message);
  }

  if (!parsed_json->is_list() || parsed_json->GetList().size() != 2U ||
      !parsed_json->GetList()[1].is_list() ||
      parsed_json->GetList()[1].GetList().size() != 2U ||
      !parsed_json->GetList()[1].GetList()[1].is_string() ||
      parsed_json->GetList()[0].GetString() != token) {
    std::ostringstream error_message;
    error_message << "Received unexpected result: " << *parsed_json;
    return content::EvalJsResult(base::Value(), error_message.str());
  }
  auto& result = parsed_json->GetList()[1].GetList();

  return content::EvalJsResult(std::move(result[0]), result[1].GetString());
}

// As EvalJsLocal but does not wait for a response; errors will appear in the
// test log.
void ExecuteJsLocal(const content::ToRenderFrameHost& execution_target,
                    const std::string& function) {
  content::RenderFrameHost* const host = execution_target.render_frame_host();
  CHECK(host->IsRenderFrameLive());
  std::string runner_script = base::StringPrintf("(%s)();", function.c_str());
  ExecuteScript(host, runner_script);
}

std::string DeepQueryToJSON(
    const WebContentsInteractionTestUtil::DeepQuery& where) {
  // Safely convert the selector list in `where` to a JSON/JS list.
  base::Value::List selector_list;
  for (const auto& selector : where) {
    selector_list.Append(selector);
  }
  std::string selectors;
  CHECK(base::JSONWriter::Write(selector_list, &selectors));
  return selectors;
}

// Computes the bounds of the element at `where` relative to the top-level
// render window. This takes into account nested shadow DOMs and iframes.
// Result is a function that when executed returns a JSON object with
// {x, y, w, h}.
std::string GetElementBounds(
    const WebContentsInteractionTestUtil::DeepQuery& where) {
  const std::string selectors = DeepQueryToJSON(where);
  return base::StringPrintf(
      R"(function() {
         const selectors = (%s);
         let cur = document;
         let offsetX = 0;
         let offsetY = 0;
         for (let selector of selectors) {
           if (cur.shadowRoot) {
             // Handle shadow DOM case.
             cur = cur.shadowRoot;
           } else if (cur.contentDocument) {
             // Handle iframe case. Iframe bounds are not included in bounds
             // calculations for elements that reside inside of them, so these
             // need to be handled explicitly.

             // Grab the bounds - these will contain the border and padding.
             const bounds = cur.getBoundingClientRect();
             offsetX += bounds.x;
             offsetY += bounds.y;

             // Add the internal padding.
             const style = getComputedStyle(cur);
             offsetX += parseInt(style.borderLeftWidth) +
                        parseInt(style.paddingLeft);
             offsetY += parseInt(style.borderTopWidth) +
                        parseInt(style.paddingTop);

             // Move inside the iframe.
             cur = cur.contentDocument;
           }
           cur = cur.querySelector(selector);
           if (!cur) {
             const err = new Error('Selector not found: ' + selector);
             err.selector = selector;
             throw err;
           }
         }

         const rect = cur.getBoundingClientRect();
         return {
           "x": rect.x + offsetX,
           "y": rect.y + offsetY,
           "w": rect.width,
           "h": rect.height
         };
       })",
      selectors.c_str());
}

std::string CreateDeepQuery(
    const WebContentsInteractionTestUtil::DeepQuery& where,
    const std::string& function) {
  DCHECK(!function.empty());

  const std::string selectors = DeepQueryToJSON(where);

  return base::StringPrintf(
      R"(function() {
         function deepQuery(selectors) {
           let cur = document;
           for (let selector of selectors) {
             if (cur.shadowRoot) {
               // Handle shadow DOM case.
               cur = cur.shadowRoot;
             } else if (cur.contentDocument) {
               // Handle iframe case.
               cur = cur.contentDocument;
             }
             cur = cur.querySelector(selector);
             if (!cur) {
               const err = new Error('Selector not found: ' + selector);
               err.selector = selector;
               throw err;
             }
           }
           return cur;
         }

         let el, err;
         try {
           el = deepQuery(%s);
         } catch (error) {
           err = error;
         }

         const func = (%s);
         if (err && func.length <= 1) {
           throw err;
         }
         return func(el, err);
       })",
      selectors.c_str(), function.c_str());
}

}  // namespace

// Subclass used to handle WebContents in tabs.
class TabWebContentsInteractionTestUtil : public WebContentsInteractionTestUtil,
                                          public TabStripModelObserver {
 public:
  TabWebContentsInteractionTestUtil(content::WebContents* web_contents,
                                    ui::ElementIdentifier page_identifier);
  TabWebContentsInteractionTestUtil(BrowserWindowInterface* to_watch,
                                    ui::ElementIdentifier page_identifier);
  ~TabWebContentsInteractionTestUtil() override;

  // WebContentsInteractionTestUtil:
  void LoadPageInNewTab(const GURL& url, bool activate_tab) override;
  views::WebView* GetWebView() const override;

 protected:
  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
  ui::ElementContext GetElementContext() const override;

 private:
  void StartWatchingWebContents(content::WebContents* web_contents);

  // Optional object that watches for a new tab to be created, either in a
  // specific browser or in any browser.
  class NewTabWatcher;
  std::unique_ptr<NewTabWatcher> new_tab_watcher_;
};

// Subclass used to handle WebContents in a WebView.
class WebViewWebContentsInteractionTestUtil
    : public WebContentsInteractionTestUtil {
 public:
  WebViewWebContentsInteractionTestUtil(views::WebView* web_view,
                                        ui::ElementIdentifier page_identifier);
  ~WebViewWebContentsInteractionTestUtil() override;

  views::WebView* GetWebView() const override;

 protected:
  // TabStripModelObserver:
  bool ForceNavigateWithController() const override { return true; }
  ui::ElementContext GetElementContext() const override;

 private:
  class WebViewData;

  // Tracks the WebView that hosts a non-tab WebContents; null otherwise.
  std::unique_ptr<WebViewData> web_view_data_;
};

// Tracks an inner WebContents at a particular index in an outer instrumented
// WebContents. Valid only when the inner contents and outer contents are both
// valid and visible.
class InnerWebContentsInteractionTestUtil
    : public WebContentsInteractionTestUtil {
 public:
  InnerWebContentsInteractionTestUtil(
      ui::ElementIdentifier outer_webcontents_id,
      size_t inner_contents_index,
      ui::ElementIdentifier inner_page_identifier);
  ~InnerWebContentsInteractionTestUtil() override;

 public:
  views::WebView* GetWebView() const override;
  gfx::Rect GetElementBoundsInScreen(const DeepQuery& where) const override;
  ui::ElementContext GetElementContext() const override;

 private:
  class ParentWebContentsObserver;

  const WebContentsInteractionTestUtil* parent_util() const {
    return parent_element_ ? parent_element_->owner() : nullptr;
  }

  void MaybeObserveChild();

  void OnParentShown(ui::TrackedElement* parent);
  void OnParentHidden(ui::TrackedElement* parent);

  raw_ptr<const TrackedElementWebContents> parent_element_ = nullptr;
  const size_t inner_contents_index_;
  std::unique_ptr<ParentWebContentsObserver> parent_observer_;
  base::CallbackListSubscription parent_shown_subscription_;
  base::CallbackListSubscription parent_hidden_subscription_;
};

WebContentsInteractionTestUtil::DeepQuery::DeepQuery() = default;
WebContentsInteractionTestUtil::DeepQuery::DeepQuery(
    std::initializer_list<std::string> segments)
    : segments_(segments) {}
WebContentsInteractionTestUtil::DeepQuery::DeepQuery(
    const WebContentsInteractionTestUtil::DeepQuery& other) = default;
WebContentsInteractionTestUtil::DeepQuery&
WebContentsInteractionTestUtil::DeepQuery::operator=(
    const WebContentsInteractionTestUtil::DeepQuery& other) = default;
WebContentsInteractionTestUtil::DeepQuery&
WebContentsInteractionTestUtil::DeepQuery::operator=(
    std::initializer_list<std::string> segments) {
  segments_ = segments;
  return *this;
}
WebContentsInteractionTestUtil::DeepQuery
WebContentsInteractionTestUtil::DeepQuery::operator+(
    const std::string& segment) const {
  DeepQuery result(*this);
  result.segments_.emplace_back(segment);
  return result;
}
WebContentsInteractionTestUtil::DeepQuery::~DeepQuery() = default;

WebContentsInteractionTestUtil::StateChange::StateChange() = default;
WebContentsInteractionTestUtil::StateChange::StateChange(
    const WebContentsInteractionTestUtil::StateChange& other) = default;
WebContentsInteractionTestUtil::StateChange&
WebContentsInteractionTestUtil::StateChange::operator=(
    const WebContentsInteractionTestUtil::StateChange& other) = default;
WebContentsInteractionTestUtil::StateChange::~StateChange() = default;

class WebContentsInteractionTestUtil::Poller {
 public:
  Poller(WebContentsInteractionTestUtil* const owner, StateChange state_change)
      : state_change_(std::move(state_change)),
        js_query_(GetStateChangeQuery(state_change_)),
        owner_(owner) {}

  ~Poller() = default;

  void StartPolling() {
    CHECK(!timer_.IsRunning());
    timer_.Start(FROM_HERE, state_change_.polling_interval,
                 base::BindRepeating(&Poller::Poll, base::Unretained(this)));
  }

  const StateChange& state_change() const { return state_change_; }

 private:
  void Poll() {
    // Callback can get called again if Evaluate() below stalls. We don't want
    // to stack callbacks because of issues with message passing to/from web
    // contents.
    if (is_polling_) {
      return;
    }

    // If there is no page loaded, then there is nothing to poll.
    if (!owner_->is_page_loaded()) {
      CHECK(state_change_.continue_across_navigation)
          << "Page discarded waiting for StateChange event "
          << state_change_.event;
      return;
    }

    auto weak_ptr = weak_factory_.GetWeakPtr();
    base::WeakAutoReset is_polling_auto_reset(weak_ptr, &Poller::is_polling_,
                                              true);

    const base::Value result =
        state_change_.where.empty()
            ? owner_->Evaluate(js_query_)
            : owner_->EvaluateAt(state_change_.where, js_query_);

    // At this point, weak_ptr might be invalid since we could have been deleted
    // while we were waiting for Evaluate[At]() to complete.
    if (weak_ptr) {
      if (CheckResult(result)) {
        owner_->OnPollEvent(this, state_change_.event);
      } else if (state_change_.timeout.has_value() &&
                 elapsed_.Elapsed() > state_change_.timeout.value()) {
        owner_->OnPollEvent(this, state_change_.timeout_event);
      }
    }
  }

  // Determines if the result of calling the method passes the check.
  //
  // If no explicit check is specified, `value` will be evaluated for
  // truthiness.
  bool CheckResult(const base::Value& value) {
    if (state_change_.check_callback.is_null()) {
      return IsTruthy(value);
    }
    return state_change_.check_callback.Run(value);
  }

  const base::ElapsedTimer elapsed_;
  const StateChange state_change_;
  const std::string js_query_;
  const raw_ptr<WebContentsInteractionTestUtil> owner_;
  base::RepeatingTimer timer_;
  bool is_polling_ = false;
  base::WeakPtrFactory<Poller> weak_factory_{this};
};

// static
constexpr base::TimeDelta
    WebContentsInteractionTestUtil::kDefaultPollingInterval;

WebContentsInteractionTestUtil::~WebContentsInteractionTestUtil() {
  Observe(nullptr);
  pollers_.clear();
}

// static
bool WebContentsInteractionTestUtil::IsTruthy(const base::Value& value) {
  using Type = base::Value::Type;
  switch (value.type()) {
    case Type::BOOLEAN:
      return value.GetBool();
    case Type::INTEGER:
      return value.GetInt() != 0;
    case Type::DOUBLE:
      // Note: this should probably also include handling of NaN, but
      // base::Value itself cannot handle NaN values because JSON cannot.
      return value.GetDouble() != 0.0;
    case Type::BINARY:
      return true;
    case Type::DICT:
      return true;
    case Type::LIST:
      return true;
    case Type::STRING:
      return !value.GetString().empty();
    case Type::NONE:
      return false;
  }
}

// static
std::unique_ptr<WebContentsInteractionTestUtil>
WebContentsInteractionTestUtil::ForExistingTabInContext(
    ui::ElementContext context,
    ui::ElementIdentifier page_identifier,
    std::optional<int> tab_index) {
  return ForExistingTabInBrowser(
      InteractionTestUtilBrowser::GetBrowserFromContext(context),
      page_identifier, tab_index);
}

// static
std::unique_ptr<WebContentsInteractionTestUtil>
WebContentsInteractionTestUtil::ForExistingTabInBrowser(
    BrowserWindowInterface* browser,
    ui::ElementIdentifier page_identifier,
    std::optional<int> tab_index) {
  return ForTabWebContents(GetWebContents(browser, tab_index), page_identifier);
}

// static
std::unique_ptr<WebContentsInteractionTestUtil>
WebContentsInteractionTestUtil::ForTabWebContents(
    content::WebContents* web_contents,
    ui::ElementIdentifier page_identifier) {
  return std::make_unique<TabWebContentsInteractionTestUtil>(web_contents,
                                                             page_identifier);
}

// static
std::unique_ptr<WebContentsInteractionTestUtil>
WebContentsInteractionTestUtil::ForNonTabWebView(
    views::WebView* web_view,
    ui::ElementIdentifier page_identifier) {
  return std::make_unique<WebViewWebContentsInteractionTestUtil>(
      web_view, page_identifier);
}

// static
std::unique_ptr<WebContentsInteractionTestUtil>
WebContentsInteractionTestUtil::ForNextTabInContext(
    ui::ElementContext context,
    ui::ElementIdentifier page_identifier) {
  auto* const browser =
      InteractionTestUtilBrowser::GetBrowserFromContext(context);
  return ForNextTabInBrowser(browser, page_identifier);
}

// static
std::unique_ptr<WebContentsInteractionTestUtil>
WebContentsInteractionTestUtil::ForNextTabInBrowser(
    BrowserWindowInterface* browser,
    ui::ElementIdentifier page_identifier) {
  CHECK(browser);
  return std::make_unique<TabWebContentsInteractionTestUtil>(browser,
                                                             page_identifier);
}

// static
std::unique_ptr<WebContentsInteractionTestUtil>
WebContentsInteractionTestUtil::ForNextTabInAnyBrowser(
    ui::ElementIdentifier page_identifier) {
  return std::make_unique<TabWebContentsInteractionTestUtil>(
      static_cast<Browser*>(nullptr), page_identifier);
}

// static
std::unique_ptr<WebContentsInteractionTestUtil>
WebContentsInteractionTestUtil::ForInnerWebContents(
    ui::ElementIdentifier outer_page_identifier,
    size_t inner_contents_index,
    ui::ElementIdentifier inner_page_identifier) {
  return base::WrapUnique(new InnerWebContentsInteractionTestUtil(
      outer_page_identifier, inner_contents_index, inner_page_identifier));
}

bool WebContentsInteractionTestUtil::HasPageBeenPainted() const {
  return is_page_loaded() &&
         web_contents()->CompletedFirstVisuallyNonEmptyPaint();
}

void WebContentsInteractionTestUtil::LoadPage(const GURL& url) {
  CHECK(web_contents());
  if (!web_contents()->GetURL().EqualsIgnoringRef(url)) {
    navigating_away_from_ = web_contents()->GetURL();
    DiscardCurrentElement();
  }
  if (content::HasWebUIScheme(url) || ForceNavigateWithController()) {
    // Secure pages and non-tab WebViews must be navigated via the controller.
    content::NavigationController::LoadURLParams params(url);
    CHECK(web_contents()->GetController().LoadURLWithParams(params));
  } else {
    // Regular web pages can be navigated directly.
    //
    // In an ideal world, this should use `BeginNavigateToURLFromRenderer()`,
    // which verifies that the navigation successfully starts. However,
    // `BeginNavigateToURLFromRenderer()` itself uses a RunLoop to listen for
    // the navigation starting.
    //
    // For reasons that are not well understood, this is problematic when used
    // in conjunction with the interaction sequence test utils, which often
    // run the entire test inside a top-level RunLoop; the now nested RunLoop
    // inside `BeginNavigateToURLFromRenderer()` never receives the
    // `DidStartNavigation()` callback, and the test just ends up hanging.
    //
    // Use Execute() as a workaround this hang. Note that unlike the
    // similarly-named `content::ExecJs()`, this helper does not actually
    // validate or wait for the script to execute; hopefully, errors from
    // navigation failures will be obvious enough in subsequent steps.
    ExecuteJsLocal(web_contents(),
                   content::JsReplace("() => location = $1", url));
  }
}

void WebContentsInteractionTestUtil::LoadPageInNewTab(const GURL& url,
                                                      bool activate_tab) {
  NOTREACHED() << "Should only be called for tab WebContents.";
}

base::Value WebContentsInteractionTestUtil::Evaluate(
    const std::string& function,
    std::string* error_message) {
  CHECK(is_page_loaded());
  auto result = EvalJsLocal(web_contents(), function);
  if (!result.is_ok()) {
    if (error_message) {
      *error_message = result.ExtractError();
      return base::Value();
    } else {
      NOTREACHED() << "Uncaught JS exception: " << result;
    }
  }

  return std::move(result).TakeValue();
}

void WebContentsInteractionTestUtil::Execute(const std::string& function) {
  CHECK(is_page_loaded());
  ExecuteJsLocal(web_contents(), function);
}

void WebContentsInteractionTestUtil::SendEventOnStateChange(
    const StateChange& configuration) {
  CHECK(current_element_);

  auto actual_config = ValidateAndInferStateChange(configuration);
  const auto& poller = pollers_.emplace_back(
      std::make_unique<Poller>(this, std::move(actual_config)));
  poller->StartPolling();
}

bool WebContentsInteractionTestUtil::Exists(const DeepQuery& query,
                                            std::string* not_found) const {
  const std::string full_query =
      CreateDeepQuery(query, GetExistsQuery("err.selector", "''"));
  // Const cast is safe as the query cannot modify the WebContents.
  const std::string result = const_cast<WebContentsInteractionTestUtil*>(this)
                                 ->Evaluate(full_query)
                                 .GetString();
  if (not_found) {
    *not_found = result;
  }
  return result.empty();
}

base::Value WebContentsInteractionTestUtil::EvaluateAt(
    const DeepQuery& where,
    const std::string& function,
    std::string* error_message) {
  const std::string full_query = CreateDeepQuery(where, function);
  return Evaluate(full_query, error_message);
}

void WebContentsInteractionTestUtil::ExecuteAt(const DeepQuery& where,
                                               const std::string& function) {
  const std::string full_query = CreateDeepQuery(where, function);
  Execute(full_query);
}

bool WebContentsInteractionTestUtil::Exists(const std::string& selector) {
  return Exists(DeepQuery{selector});
}

base::Value WebContentsInteractionTestUtil::EvaluateAt(
    const std::string& selector,
    const std::string& function) {
  return EvaluateAt(DeepQuery{selector}, function);
}

void WebContentsInteractionTestUtil::ExecuteAt(const std::string& selector,
                                               const std::string& function) {
  ExecuteAt(DeepQuery{selector}, function);
}

gfx::Rect WebContentsInteractionTestUtil::GetElementBoundsInScreen(
    const DeepQuery& where) const {
  if (!current_element_) {
    return gfx::Rect();
  }

  views::WebView* const web_view = GetWebView();
  if (!web_view) {
    return gfx::Rect();
  }

  // TODO(dfried): Screen bounds returned by GetBoundsInScreen() are in DIPs.
  // We are also assuming that Element.getBoundingClientRect() also returns a
  // value in DIPs (this seems to be borne out by anecdotal evidence in online
  // discussions). However, if that's not the case, either the offset or element
  // bounds will need to be adjusted by the current display's scale factor.
  const gfx::Point offset = web_view->GetBoundsInScreen().origin();

  // Perform our custom bounds calculation, taking into account e.g. iframes.
  // Note that this does not modify the contents of the frame, so it's safe to
  // do this const cast.
  const base::Value result =
      const_cast<WebContentsInteractionTestUtil*>(this)->Evaluate(
          GetElementBounds(where));

  // This will crash if any of the values are not found, however, since this is
  // test code that's fine; it *should* crash the test.
  const auto& dict = result.GetDict();
  gfx::Rect element_bounds(
      dict.Find("x")->GetDouble(), dict.Find("y")->GetDouble(),
      dict.Find("w")->GetDouble(), dict.Find("h")->GetDouble());

  element_bounds.Offset(offset.x(), offset.y());
  return element_bounds;
}

gfx::Rect WebContentsInteractionTestUtil::GetElementBoundsInScreen(
    const std::string& where) const {
  return GetElementBoundsInScreen(DeepQuery{where});
}

void WebContentsInteractionTestUtil::DidStopLoading() {
  // In some cases we will not have an "on load complete" event, so ensure that
  // we check for page fully loaded in other callbacks.
  MaybeCreateElement();
}

void WebContentsInteractionTestUtil::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  // In some cases we will not have an "on load complete" event, so ensure that
  // we check for page fully loaded in other callbacks.
  MaybeCreateElement();
}

void WebContentsInteractionTestUtil::
    DocumentOnLoadCompletedInPrimaryMainFrame() {
  // Even if the page is still "loading" it should be ready for interaction at
  // this point. Note that in some cases we won't receive this event, which is
  // why we also check at DidStopLoading() and DidFinishLoad().
  MaybeCreateElement();
}

void WebContentsInteractionTestUtil::PrimaryPageChanged(content::Page& page) {
  DiscardCurrentElement();
}

void WebContentsInteractionTestUtil::WebContentsDestroyed() {
  DiscardCurrentElement();
}

void WebContentsInteractionTestUtil::DidFirstVisuallyNonEmptyPaint() {
  MaybeSendPaintEvent();
}

WebContentsInteractionTestUtil::WebContentsInteractionTestUtil(
    content::WebContents* web_contents,
    ui::ElementIdentifier page_identifier)
    : WebContentsObserver(web_contents), page_identifier_(page_identifier) {
  CHECK(page_identifier);
}

void WebContentsInteractionTestUtil::MaybeCreateElement() {
  if (current_element_ || !web_contents()) {
    return;
  }

  if (!web_contents()->IsDocumentOnLoadCompletedInPrimaryMainFrame() ||
      web_contents()->HasUncommittedNavigationInPrimaryMainFrame()) {
    return;
  }

  ui::ElementContext context = GetElementContext();
  if (!context) {
    return;
  }

  // Ignore events on a page we're navigating away from.
  if (navigating_away_from_ &&
      navigating_away_from_->EqualsIgnoringRef(web_contents()->GetURL())) {
    return;
  }
  navigating_away_from_.reset();

  current_element_ = std::make_unique<TrackedElementWebContents>(
      page_identifier_, context, this);

  // Init (send shown event, etc.) after current_element_ is set in order to
  // ensure that is_page_loaded() is true during any callbacks.
  current_element_->Init();

  // Because callbacks to the above method may result in the contents or current
  // element being destroyed, make sure to check before trying to access the
  // objects again.
  if (current_element_) {
    if (web_contents()->CompletedFirstVisuallyNonEmptyPaint()) {
      MaybeSendPaintEvent();
    }
  }
}

void WebContentsInteractionTestUtil::MaybeSendPaintEvent() {
  if (sent_paint_event_ || !current_element_) {
    return;
  }

  CHECK(web_contents());
  CHECK(web_contents()->CompletedFirstVisuallyNonEmptyPaint());

  sent_paint_event_ = true;
  ui::ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(
      current_element_.get(), TrackedElementWebContents::kFirstNonEmptyPaint);
}

void WebContentsInteractionTestUtil::DiscardCurrentElement() {
  sent_paint_event_ = false;
  current_element_.reset();
  for (const auto& poller : pollers_) {
    CHECK(poller->state_change().continue_across_navigation)
        << "Unexpectedly left page while still waiting for StateChange event "
        << poller->state_change().event;
  }
}

bool WebContentsInteractionTestUtil::ForceNavigateWithController() const {
  return false;
}

void WebContentsInteractionTestUtil::OnPollEvent(
    Poller* poller,
    ui::CustomElementEventType event) {
  CHECK(current_element_)
      << "StateChange succeeded (or failed) while no page was loaded; "
         "this is always an error even if continue_across_navigation is true.";
  const auto it =
      std::find_if(pollers_.begin(), pollers_.end(),
                   [poller](const auto& ptr) { return ptr.get() == poller; });
  CHECK(it != pollers_.end());
  pollers_.erase(it);
  if (event) {
    ui::ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(
        current_element_.get(), event);
  }
}

class TabWebContentsInteractionTestUtil::NewTabWatcher
    : public TabStripModelObserver,
      public BrowserListObserver {
 public:
  NewTabWatcher(TabWebContentsInteractionTestUtil* owner,
                BrowserWindowInterface* browser)
      : owner_(owner), browser_(browser) {
    if (browser_) {
      browser_->GetTabStripModel()->AddObserver(this);
    } else {
      BrowserList::GetInstance()->AddObserver(this);
      ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
          [this](BrowserWindowInterface* browser) {
            browser->GetTabStripModel()->AddObserver(this);
            return true;
          });
    }
  }

  ~NewTabWatcher() override {
    BrowserList::GetInstance()->RemoveObserver(this);
  }

  BrowserWindowInterface* browser() { return browser_; }

 private:
  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override {
    CHECK(!browser_);
    browser->GetTabStripModel()->AddObserver(this);
  }

  void OnBrowserRemoved(Browser* browser) override { CHECK(!browser_); }

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    if (change.type() != TabStripModelChange::Type::kInserted) {
      return;
    }

    auto* const web_contents =
        change.GetInsert()->contents.front().contents.get();
    CHECK(!browser_ ||
          browser_ == GetBrowserWindowForWebContentsInTab(web_contents));
    owner_->StartWatchingWebContents(web_contents);
  }

  const raw_ptr<TabWebContentsInteractionTestUtil> owner_;
  const raw_ptr<BrowserWindowInterface> browser_;
};

TabWebContentsInteractionTestUtil::TabWebContentsInteractionTestUtil(
    content::WebContents* web_contents,
    ui::ElementIdentifier page_identifier)
    : WebContentsInteractionTestUtil(web_contents, page_identifier) {
  StartWatchingWebContents(web_contents);
}

TabWebContentsInteractionTestUtil::TabWebContentsInteractionTestUtil(
    BrowserWindowInterface* to_watch,
    ui::ElementIdentifier page_identifier)
    : WebContentsInteractionTestUtil(nullptr, page_identifier),
      new_tab_watcher_(std::make_unique<NewTabWatcher>(this, to_watch)) {}

TabWebContentsInteractionTestUtil::~TabWebContentsInteractionTestUtil() =
    default;

void TabWebContentsInteractionTestUtil::LoadPageInNewTab(const GURL& url,
                                                         bool activate_tab) {
  // We use tertiary operator rather than value_or to avoid failing if we're in
  // a wait state.
  BrowserWindowInterface* browser =
      new_tab_watcher_ ? new_tab_watcher_->browser()
                       : GetBrowserWindowForWebContentsInTab(web_contents());
  NavigateParams navigate_params(browser, url, ui::PAGE_TRANSITION_TYPED);
  navigate_params.disposition = activate_tab
                                    ? WindowOpenDisposition::NEW_FOREGROUND_TAB
                                    : WindowOpenDisposition::NEW_BACKGROUND_TAB;
  auto navigate_result = Navigate(&navigate_params);
  CHECK(navigate_result);
}

views::WebView* TabWebContentsInteractionTestUtil::GetWebView() const {
  if (!current_element()) {
    return nullptr;
  }

  auto* const browser = InteractionTestUtilBrowser::GetBrowserFromContext(
      current_element()->context());
  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser);
  CHECK(browser_view);
  if (web_contents() != browser_view->GetActiveWebContents()) {
    return nullptr;
  }

  return browser_view->contents_web_view();
}

ui::ElementContext TabWebContentsInteractionTestUtil::GetElementContext()
    const {
  ui::ElementContext context;
  if (auto* const browser =
          GetBrowserWindowForWebContentsInTab(web_contents())) {
    context = BrowserElements::From(browser)->GetContext();
  }
  return context;
}

void TabWebContentsInteractionTestUtil::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  // Don't bother processing if we don't have a target WebContents.
  if (!web_contents()) {
    return;
  }

  // Ensure that if a tab is moved to another browser, we track that move.
  if (change.type() == TabStripModelChange::Type::kRemoved) {
    for (auto& removed_tab : change.GetRemove()->contents) {
      if (removed_tab.contents != web_contents()) {
        continue;
      }
      // We won't handle deleted reason here, since we already capture
      // WebContentsDestroyed().
      if (removed_tab.remove_reason ==
          TabStripModelChange::RemoveReason::kInsertedIntoOtherTabStrip) {
        DiscardCurrentElement();
        Observe(nullptr);
      }
    }
  } else if (change.type() == TabStripModelChange::Type::kReplaced) {
    auto* const replace = change.GetReplace();
    if (web_contents() == replace->old_contents) {
      DiscardCurrentElement();
      Observe(replace->new_contents);
      MaybeCreateElement();
    }
  }
}

void TabWebContentsInteractionTestUtil::StartWatchingWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  auto* const browser = GetBrowserWindowForWebContentsInTab(web_contents);
  CHECK(browser);
  browser->GetTabStripModel()->AddObserver(this);
  if (new_tab_watcher_) {
    new_tab_watcher_.reset();
    Observe(web_contents);
  }
  MaybeCreateElement();
}

// Class that tracks a WebView and its WebContents in a secondary UI.
class WebViewWebContentsInteractionTestUtil::WebViewData
    : public views::ViewObserver {
 public:
  WebViewData(WebViewWebContentsInteractionTestUtil* owner,
              views::WebView* web_view)
      : owner_(owner), web_view_(web_view) {}
  ~WebViewData() override = default;

  // Separate init is required from construction so that the util object that
  // owns this object can store a pointer before any calls back to the util
  // object are performed.
  void Init() {
    scoped_observation_.Observe(web_view_);
    web_contents_attached_subscription_ =
        web_view_->AddWebContentsAttachedCallback(base::BindRepeating(
            &WebViewData::OnWebContentsAttached, base::Unretained(this)));
    ui::ElementIdentifier id =
        web_view_->GetProperty(views::kElementIdentifierKey);
    if (!id) {
      id = ui::ElementTracker::kTemporaryIdentifier;
      web_view_->SetProperty(views::kElementIdentifierKey, id);
    }
    context_ = views::ElementTrackerViews::GetContextForView(web_view_);
    CHECK(context_);

    shown_subscription_ =
        ui::ElementTracker::GetElementTracker()->AddElementShownCallback(
            id, context_,
            base::BindRepeating(&WebViewData::OnElementShown,
                                base::Unretained(this)));
    hidden_subscription_ =
        ui::ElementTracker::GetElementTracker()->AddElementHiddenCallback(
            id, context_,
            base::BindRepeating(&WebViewData::OnElementHidden,
                                base::Unretained(this)));

    if (auto* const element =
            views::ElementTrackerViews::GetInstance()->GetElementForView(
                web_view_)) {
      OnElementShown(element);
    }
  }

  ui::ElementContext context() const { return context_; }

  bool visible() const { return visible_; }

  views::WebView* web_view() const { return web_view_; }

 private:
  struct MinimumSizeData {
    ui::CustomElementEventType event_type;
    gfx::Size webview_size;
    DeepQuery element;
    gfx::Size element_size;
  };

  void OnElementShown(ui::TrackedElement* element) {
    if (visible_) {
      return;
    }
    auto* el = element->AsA<views::TrackedElementViews>();
    if (!el || el->view() != web_view_) {
      return;
    }
    visible_ = true;
    owner_->Observe(web_view_->web_contents());
    owner_->MaybeCreateElement();
  }

  void OnElementHidden(ui::TrackedElement* element) {
    if (!visible_) {
      return;
    }
    auto* el = element->AsA<views::TrackedElementViews>();
    if (!el || el->view() != web_view_) {
      return;
    }
    visible_ = false;
    owner_->Observe(nullptr);
    owner_->DiscardCurrentElement();
  }

  // views::ViewObserver:
  void OnViewIsDeleting(views::View* view) override {
    visible_ = false;
    web_view_ = nullptr;
    shown_subscription_ = ui::ElementTracker::Subscription();
    hidden_subscription_ = ui::ElementTracker::Subscription();
    scoped_observation_.Reset();
    owner_->Observe(nullptr);
    owner_->DiscardCurrentElement();
  }

  void OnWebContentsAttached(views::WebView* observed_view) {
    CHECK_EQ(web_view_.get(), observed_view);
    content::WebContents* const to_observe =
        visible_ ? observed_view->web_contents() : nullptr;
    if (owner_->web_contents() == to_observe) {
      return;
    }
    owner_->Observe(to_observe);
    owner_->DiscardCurrentElement();
    owner_->MaybeCreateElement();
  }

  static bool Contains(const gfx::Size& bounds, const gfx::Size& size) {
    return bounds.height() <= size.height() && bounds.width() <= size.width();
  }

  const raw_ptr<WebViewWebContentsInteractionTestUtil> owner_;
  raw_ptr<views::WebView> web_view_;
  bool visible_ = false;
  ui::ElementContext context_;
  ui::ElementTracker::Subscription shown_subscription_;
  ui::ElementTracker::Subscription hidden_subscription_;
  base::ScopedObservation<views::View, views::ViewObserver> scoped_observation_{
      this};
  base::CallbackListSubscription web_contents_attached_subscription_;
  base::WeakPtrFactory<WebViewData> weak_factory_{this};
};

WebViewWebContentsInteractionTestUtil::WebViewWebContentsInteractionTestUtil(
    views::WebView* web_view,
    ui::ElementIdentifier page_identifier)
    : WebContentsInteractionTestUtil(web_view->GetWebContents(),
                                     page_identifier) {
  CHECK(web_contents());
  CHECK(!GetBrowserWindowForWebContentsInTab(web_contents()));
  web_view_data_ = std::make_unique<WebViewData>(this, web_view);
  web_view_data_->Init();
}

WebViewWebContentsInteractionTestUtil::
    ~WebViewWebContentsInteractionTestUtil() = default;

views::WebView* WebViewWebContentsInteractionTestUtil::GetWebView() const {
  return web_view_data_->visible() ? web_view_data_->web_view() : nullptr;
}

ui::ElementContext WebViewWebContentsInteractionTestUtil::GetElementContext()
    const {
  ui::ElementContext context;
  if (web_view_data_->visible()) {
    context = web_view_data_->context();
  }
  return context;
}

class InnerWebContentsInteractionTestUtil::ParentWebContentsObserver
    : public WebContentsObserver {
 public:
  explicit ParentWebContentsObserver(InnerWebContentsInteractionTestUtil& owner)
      : owner_(owner) {}
  ~ParentWebContentsObserver() override = default;

  void StartObserving(content::WebContents* parent_contents) {
    Observe(parent_contents);
  }

  void StopObserving() { Observe(nullptr); }

 private:
  // WebContentsObserver:
  void InnerWebContentsAttached(content::WebContents* inner_web_contents,
                                content::RenderFrameHost*) override {
    owner_->MaybeObserveChild();
  }

  const raw_ref<InnerWebContentsInteractionTestUtil> owner_;
};

InnerWebContentsInteractionTestUtil::InnerWebContentsInteractionTestUtil(
    ui::ElementIdentifier outer_webcontents_id,
    size_t inner_contents_index,
    ui::ElementIdentifier page_identifier)
    : WebContentsInteractionTestUtil(nullptr, page_identifier),
      inner_contents_index_(inner_contents_index),
      parent_observer_(std::make_unique<ParentWebContentsObserver>(*this)) {
  auto* const tracker = ui::ElementTracker::GetElementTracker();
  parent_shown_subscription_ = tracker->AddElementShownInAnyContextCallback(
      outer_webcontents_id,
      base::BindRepeating(&InnerWebContentsInteractionTestUtil::OnParentShown,
                          base::Unretained(this)));
  parent_hidden_subscription_ = tracker->AddElementHiddenInAnyContextCallback(
      outer_webcontents_id,
      base::BindRepeating(&InnerWebContentsInteractionTestUtil::OnParentHidden,
                          base::Unretained(this)));
  if (auto* const parent_el =
          tracker->GetElementInAnyContext(outer_webcontents_id)) {
    OnParentShown(parent_el);
  }
}

InnerWebContentsInteractionTestUtil::~InnerWebContentsInteractionTestUtil() =
    default;

views::WebView* InnerWebContentsInteractionTestUtil::GetWebView() const {
  if (auto* const parent = parent_util()) {
    return parent->GetWebView();
  }
  return nullptr;
}

gfx::Rect InnerWebContentsInteractionTestUtil::GetElementBoundsInScreen(
    const DeepQuery& where) const {
  // TODO(dfried): IMPLEMENT
  NOTREACHED();
}

ui::ElementContext InnerWebContentsInteractionTestUtil::GetElementContext()
    const {
  if (auto* const parent = parent_util()) {
    return parent->GetElementContext();
  }
  return ui::ElementContext();
}

void InnerWebContentsInteractionTestUtil::MaybeObserveChild() {
  if (auto* const parent = parent_observer_->web_contents()) {
    const auto inner_contents = parent->GetInnerWebContents();
    if (inner_contents_index_ < inner_contents.size()) {
      auto* inner = inner_contents[inner_contents_index_];
      if (web_contents() && web_contents() != inner) {
        DiscardCurrentElement();
      }
      Observe(inner_contents[inner_contents_index_]);
      MaybeCreateElement();
      return;
    }
  }
  DiscardCurrentElement();
}

void InnerWebContentsInteractionTestUtil::OnParentShown(
    ui::TrackedElement* parent_el) {
  CHECK(!parent_element_);
  parent_element_ = parent_el->AsA<TrackedElementWebContents>();
  CHECK(parent_element_);
  parent_observer_->StartObserving(parent_element_->owner()->web_contents());
  MaybeObserveChild();
}

void InnerWebContentsInteractionTestUtil::OnParentHidden(
    ui::TrackedElement* parent_el) {
  CHECK_EQ(parent_el, parent_element_.get());
  parent_element_ = nullptr;
  parent_observer_->StopObserving();
  DiscardCurrentElement();
}

void PrintTo(const WebContentsInteractionTestUtil::DeepQuery& deep_query,
             std::ostream* os) {
  *os << "{ \"" << base::JoinString(deep_query.segments_, "\", \"") << "\" }";
}

extern std::ostream& operator<<(
    std::ostream& os,
    const WebContentsInteractionTestUtil::DeepQuery& deep_query) {
  PrintTo(deep_query, &os);
  return os;
}

void PrintTo(const WebContentsInteractionTestUtil::StateChange& state_change,
             std::ostream* os) {
  using Type = WebContentsInteractionTestUtil::StateChange::Type;
  *os << "{ ";
  switch (state_change.type) {
    case Type::kAuto:
      *os << "kAuto";
      break;
    case Type::kExists:
      *os << "kExists";
      break;
    case Type::kExistsAndConditionTrue:
      *os << "kExistsAndConditionTrue";
      break;
    case Type::kConditionTrue:
      *os << "kConditionTrue";
      break;
    case Type::kDoesNotExist:
      *os << "kDoesNotExist";
      break;
  }

  *os << ", test_function: \"" << state_change.test_function << "\""
      << ", where: " << state_change.where << ", event: " << state_change.event
      << ", continue_across_navigation: "
      << base::ToString(state_change.continue_across_navigation)
      << ", timeout: " << state_change.timeout.value_or(base::TimeDelta())
      << ", timeout_event: " << state_change.timeout_event << " }";
}

extern std::ostream& operator<<(
    std::ostream& os,
    const WebContentsInteractionTestUtil::StateChange& state_change) {
  PrintTo(state_change, &os);
  return os;
}
