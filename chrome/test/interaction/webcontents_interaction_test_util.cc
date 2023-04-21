// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/interaction/webcontents_interaction_test_util.h"

#include <initializer_list>
#include <set>
#include <sstream>
#include <string>

#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_auto_reset.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/contents_web_view.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "chrome/test/interaction/tracked_element_webcontents.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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

content::WebContents* GetWebContents(Browser* browser,
                                     absl::optional<int> tab_index) {
  auto* const model = browser->tab_strip_model();
  return model->GetWebContentsAt(tab_index.value_or(model->active_index()));
}

// Provides a template function for "does this element exist" queries.
// Will return on_missing_selector if 'err?.selector' is valid.
// Will return on_found if el is valid.
std::string GetExistsQuery(const char* on_missing_selector,
                           const char* on_found) {
  return base::StringPrintf(R"((el, err) => {
        if (err?.selector) return %s;
        if (err) throw err;
        return %s;
      })",
                            on_missing_selector, on_found);
}

// Common execution code for `EvalJsLocal()` and `ExecuteJsLocal()`.
// Executes `script` on `host`.
void ExecuteScript(content::RenderFrameHost* host, const std::string& script) {
  const std::u16string script16 = base::UTF8ToUTF16(script);
  if (host->GetLifecycleState() !=
      content::RenderFrameHost::LifecycleState::kPrerendering) {
    host->ExecuteJavaScriptWithUserGestureForTests(
        script16, base::NullCallback());  // IN-TEST
  } else {
    host->ExecuteJavaScriptForTests(script16, base::NullCallback());  // IN-TEST
  }
}

// Our replacement for content::EvalJs() that uses the same underlying logic as
// ExecuteScriptAndExtract*(), because EvalJs() is not compatible with Content
// Security Policy of many internal pages we want to test :(
// TODO(dfried): migrate when this is not a problem.
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
      R"(Promise.resolve(%s)
         .then(func => [func()])
         .then((result) => Promise.all(result))
         .then((result) => [result[0], ''],
               (error) => [undefined,
                           error && error.stack ?
                               '\n' + error.stack :
                               'Error: "' + error + '"'])
         .then((reply) => window.domAutomationController.send(['%s', reply]));
      //# sourceURL=EvalJs-runner.js)",
      function.c_str(), token.c_str());

  if (!host->IsRenderFrameLive())
    return content::EvalJsResult(base::Value(), "Error: frame has crashed.");

  // This will queue up a message to be returned from the runner.
  ExecuteScript(host, runner_script);

  std::string json;
  if (!dom_message_queue.WaitForMessage(&json))
    return content::EvalJsResult(base::Value(),
                                 "Cannot communicate with DOMMessageQueue.");

  auto parsed_json = base::JSONReader::ReadAndReturnValueWithError(
      json, base::JSON_ALLOW_TRAILING_COMMAS);

  if (!parsed_json.has_value())
    return content::EvalJsResult(
        base::Value(), "JSON parse error: " + parsed_json.error().message);

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

std::string CreateDeepQuery(
    const WebContentsInteractionTestUtil::DeepQuery& where,
    const std::string& function) {
  DCHECK(!function.empty());

  // Safely convert the selector list in `where` to a JSON/JS list.
  base::Value::List selector_list;
  for (const auto& selector : where)
    selector_list.Append(selector);
  std::string selectors;
  CHECK(base::JSONWriter::Write(selector_list, &selectors));

  return base::StringPrintf(
      R"(function() {
         function deepQuery(selectors) {
           let cur = document;
           for (let selector of selectors) {
             if (cur.shadowRoot) {
               cur = cur.shadowRoot;
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
WebContentsInteractionTestUtil::DeepQuery::~DeepQuery() = default;

WebContentsInteractionTestUtil::StateChange::StateChange() = default;
WebContentsInteractionTestUtil::StateChange::StateChange(
    const WebContentsInteractionTestUtil::StateChange& other) = default;
WebContentsInteractionTestUtil::StateChange&
WebContentsInteractionTestUtil::StateChange::operator=(
    const WebContentsInteractionTestUtil::StateChange& other) = default;
WebContentsInteractionTestUtil::StateChange::~StateChange() = default;

class WebContentsInteractionTestUtil::NewTabWatcher
    : public TabStripModelObserver,
      public BrowserListObserver {
 public:
  NewTabWatcher(WebContentsInteractionTestUtil* owner, Browser* browser)
      : owner_(owner), browser_(browser) {
    if (browser_) {
      browser_->tab_strip_model()->AddObserver(this);
    } else {
      BrowserList::GetInstance()->AddObserver(this);
      for (Browser* const open_browser : *BrowserList::GetInstance())
        open_browser->tab_strip_model()->AddObserver(this);
    }
  }

  ~NewTabWatcher() override {
    BrowserList::GetInstance()->RemoveObserver(this);
  }

  Browser* browser() { return browser_; }

 private:
  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override {
    CHECK(!browser_);
    browser->tab_strip_model()->AddObserver(this);
  }

  void OnBrowserRemoved(Browser* browser) override { CHECK(!browser_); }

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    if (change.type() != TabStripModelChange::Type::kInserted)
      return;

    auto* const web_contents =
        change.GetInsert()->contents.front().contents.get();
    CHECK(!browser_ ||
          browser_ == chrome::FindBrowserWithWebContents(web_contents));
    owner_->StartWatchingWebContents(web_contents);
  }

  const base::raw_ptr<WebContentsInteractionTestUtil> owner_;
  const base::raw_ptr<Browser> browser_;
};

class WebContentsInteractionTestUtil::Poller {
 public:
  Poller(WebContentsInteractionTestUtil* const owner,
         const std::string& function,
         const DeepQuery& where,
         absl::optional<base::TimeDelta> timeout,
         base::TimeDelta interval)
      : function_(function),
        where_(where),
        interval_(interval),
        timeout_(timeout),
        owner_(owner) {}

  ~Poller() = default;

  void StartPolling() {
    CHECK(!timer_.IsRunning());
    timer_.Start(FROM_HERE, interval_,
                 base::BindRepeating(&Poller::Poll, base::Unretained(this)));
  }

 private:
  void Poll() {
    // Callback can get called again if Evaluate() below stalls. We don't want
    // to stack callbacks because of issues with message passing to/from web
    // contents.
    if (is_polling_)
      return;

    auto weak_ptr = weak_factory_.GetWeakPtr();
    base::WeakAutoReset is_polling_auto_reset(weak_ptr, &Poller::is_polling_,
                                              true);

    base::Value result;
    if (where_.empty()) {
      result = owner_->Evaluate(function_);
    } else if (function_.empty()) {
      result = base::Value(owner_->Exists(where_));
    } else {
      result = owner_->EvaluateAt(where_, function_);
    }

    // At this point, weak_ptr might be invalid since we could have been deleted
    // while we were waiting for Evaluate[At]() to complete.
    if (weak_ptr) {
      if (IsTruthy(result)) {
        owner_->OnPollEvent(this);
      } else if (timeout_.has_value() &&
                 elapsed_.Elapsed() > timeout_.value()) {
        owner_->OnPollTimeout(this);
      }
    }
  }

  const base::ElapsedTimer elapsed_;
  const std::string function_;
  const DeepQuery where_;
  const base::TimeDelta interval_;
  const absl::optional<base::TimeDelta> timeout_;
  const base::raw_ptr<WebContentsInteractionTestUtil> owner_;
  base::RepeatingTimer timer_;
  bool is_polling_ = false;
  base::WeakPtrFactory<Poller> weak_factory_{this};
};

struct WebContentsInteractionTestUtil::PollerData {
  std::unique_ptr<Poller> poller;
  ui::CustomElementEventType event;
  ui::CustomElementEventType timeout_event;
};

// Class that tracks a WebView and its WebContents in a secondary UI.
class WebContentsInteractionTestUtil::WebViewData : public views::ViewObserver {
 public:
  WebViewData(WebContentsInteractionTestUtil* owner, views::WebView* web_view)
      : owner_(owner), web_view_(web_view) {}
  ~WebViewData() override {
    EXPECT_FALSE(minimum_size_data_)
        << "Minimum size " << minimum_size_data_->webview_size.ToString()
        << " never reached; event never sent: "
        << minimum_size_data_->event_type;
  }

  // Separate init is required from construction so that the util object that
  // owns this object can store a pointer before any calls back to the util
  // object are performed.
  void Init() {
    scoped_observation_.Observe(web_view_);
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

  void SendEventOnMinimumSize(const gfx::Size& minimum_webview_size,
                              ui::CustomElementEventType event_type,
                              const DeepQuery& element_to_check,
                              const gfx::Size& minimum_element_size) {
    CHECK(!minimum_size_data_)
        << "Already have a pending minimum webview size with event "
        << minimum_size_data_->event_type;
    CHECK(!minimum_webview_size.IsEmpty());
    CHECK(element_to_check.empty() || !minimum_element_size.IsEmpty());

    minimum_size_data_ = std::make_unique<MinimumSizeData>();
    minimum_size_data_->webview_size = minimum_webview_size;
    minimum_size_data_->event_type = event_type;
    minimum_size_data_->element = element_to_check;
    minimum_size_data_->element_size = minimum_element_size;

    // If the WebView already meets the minimum size, queue the event now.
    if (Contains(minimum_webview_size, web_view_->size()))
      QueueMinimumSizeEvent();
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
    if (visible_)
      return;
    auto* el = element->AsA<views::TrackedElementViews>();
    if (!el || el->view() != web_view_)
      return;
    visible_ = true;
    owner_->Observe(web_view_->web_contents());
    owner_->MaybeCreateElement();
  }

  void OnElementHidden(ui::TrackedElement* element) {
    if (!visible_)
      return;
    auto* el = element->AsA<views::TrackedElementViews>();
    if (!el || el->view() != web_view_)
      return;
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

  void OnViewBoundsChanged(views::View* observed_view) override {
    if (!minimum_size_data_)
      return;
    if (Contains(minimum_size_data_->webview_size, observed_view->size()))
      QueueMinimumSizeEvent();
  }

  void QueueMinimumSizeEvent() {
    if (!owner_->current_element_)
      return;

    // This clears the current data, allowing us to queue another minimum size
    // event.
    std::unique_ptr<MinimumSizeData> data = std::move(minimum_size_data_);

    // The final step is to poke the WebView to determine when the target
    // element (or page, if one has not been specified) has actually been
    // rendered at a nonzero size.
    owner_->SendEventOnElementMinimumSize(data->event_type, data->element,
                                          data->element_size,
                                          /* must_already_exist =*/false);
  }

  static bool Contains(const gfx::Size& bounds, const gfx::Size& size) {
    return bounds.height() <= size.height() && bounds.width() <= size.width();
  }

  const raw_ptr<WebContentsInteractionTestUtil> owner_;
  base::raw_ptr<views::WebView> web_view_;
  bool visible_ = false;
  ui::ElementContext context_;
  ui::ElementTracker::Subscription shown_subscription_;
  ui::ElementTracker::Subscription hidden_subscription_;
  std::unique_ptr<MinimumSizeData> minimum_size_data_;
  base::ScopedObservation<views::View, views::ViewObserver> scoped_observation_{
      this};
  base::WeakPtrFactory<WebViewData> weak_factory_{this};
};

// static
constexpr base::TimeDelta
    WebContentsInteractionTestUtil::kDefaultPollingInterval;

WebContentsInteractionTestUtil::~WebContentsInteractionTestUtil() {
  // Stop observing before eliminating the element, as a callback could cascade
  // into additional events.
  new_tab_watcher_.reset();
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
    absl::optional<int> tab_index) {
  return ForExistingTabInBrowser(
      InteractionTestUtilBrowser::GetBrowserFromContext(context),
      page_identifier, tab_index);
}

// static
std::unique_ptr<WebContentsInteractionTestUtil>
WebContentsInteractionTestUtil::ForExistingTabInBrowser(
    Browser* browser,
    ui::ElementIdentifier page_identifier,
    absl::optional<int> tab_index) {
  return ForTabWebContents(GetWebContents(browser, tab_index), page_identifier);
}

// static
std::unique_ptr<WebContentsInteractionTestUtil>
WebContentsInteractionTestUtil::ForTabWebContents(
    content::WebContents* web_contents,
    ui::ElementIdentifier page_identifier) {
  return base::WrapUnique(new WebContentsInteractionTestUtil(
      web_contents, page_identifier, absl::nullopt, nullptr));
}

// static
std::unique_ptr<WebContentsInteractionTestUtil>
WebContentsInteractionTestUtil::ForNonTabWebView(
    views::WebView* web_view,
    ui::ElementIdentifier page_identifier) {
  return base::WrapUnique(new WebContentsInteractionTestUtil(
      web_view->GetWebContents(), page_identifier, absl::nullopt, web_view));
}

// static
std::unique_ptr<WebContentsInteractionTestUtil>
WebContentsInteractionTestUtil::ForNextTabInContext(
    ui::ElementContext context,
    ui::ElementIdentifier page_identifier) {
  Browser* const browser =
      InteractionTestUtilBrowser::GetBrowserFromContext(context);
  return ForNextTabInBrowser(browser, page_identifier);
}

// static
std::unique_ptr<WebContentsInteractionTestUtil>
WebContentsInteractionTestUtil::ForNextTabInBrowser(
    Browser* browser,
    ui::ElementIdentifier page_identifier) {
  CHECK(browser);
  return base::WrapUnique(new WebContentsInteractionTestUtil(
      nullptr, page_identifier, browser, nullptr));
}

// static
std::unique_ptr<WebContentsInteractionTestUtil>
WebContentsInteractionTestUtil::ForNextTabInAnyBrowser(
    ui::ElementIdentifier page_identifier) {
  return base::WrapUnique(new WebContentsInteractionTestUtil(
      nullptr, page_identifier, nullptr, nullptr));
}

views::WebView* WebContentsInteractionTestUtil::GetWebView() {
  if (web_view_data_)
    return web_view_data_->web_view();

  if (!current_element_)
    return nullptr;

  Browser* const browser = InteractionTestUtilBrowser::GetBrowserFromContext(
      current_element_->context());
  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser);
  CHECK(browser_view);
  if (web_contents() != browser_view->GetActiveWebContents())
    return nullptr;

  return browser_view->contents_web_view();
}

void WebContentsInteractionTestUtil::LoadPage(const GURL& url) {
  CHECK(web_contents());
  if (!web_contents()->GetURL().EqualsIgnoringRef(url)) {
    navigating_away_from_ = web_contents()->GetURL();
    DiscardCurrentElement();
  }
  if (url.SchemeIs("chrome") || web_view_data_) {
    // Secure pages and non-tab WebViews must be navigated via the controller.
    content::NavigationController::LoadURLParams params(url);
    CHECK(web_contents()->GetController().LoadURLWithParams(params));
  } else {
    // Regular web pages can be navigated directly.
    const bool result =
        content::BeginNavigateToURLFromRenderer(web_contents(), url);
    CHECK(result);
  }
}

void WebContentsInteractionTestUtil::LoadPageInNewTab(const GURL& url,
                                                      bool activate_tab) {
  // We use tertiary operator rather than value_or to avoid failing if we're in
  // a wait state.
  Browser* browser = new_tab_watcher_
                         ? new_tab_watcher_->browser()
                         : chrome::FindBrowserWithWebContents(web_contents());
  CHECK(browser);
  NavigateParams navigate_params(browser, url, ui::PAGE_TRANSITION_TYPED);
  navigate_params.disposition = activate_tab
                                    ? WindowOpenDisposition::NEW_FOREGROUND_TAB
                                    : WindowOpenDisposition::NEW_BACKGROUND_TAB;
  auto navigate_result = Navigate(&navigate_params);
  CHECK(navigate_result);
}

base::Value WebContentsInteractionTestUtil::Evaluate(
    const std::string& function) {
  CHECK(is_page_loaded());
  auto result = EvalJsLocal(web_contents(), function);
  CHECK(result.error.empty()) << result.error;

  // Despite the fact that EvalJsResult::value is const, base::Value in general
  // is moveable and nothing special is done on EvalJsResult destructor, which
  // means it's safe to const-cast and move the value out of the struct.
  auto& value = const_cast<base::Value&>(result.value);

  return std::move(value);
}

void WebContentsInteractionTestUtil::Execute(const std::string& function) {
  CHECK(is_page_loaded());
  ExecuteJsLocal(web_contents(), function);
}

void WebContentsInteractionTestUtil::SendEventOnElementMinimumSize(
    ui::CustomElementEventType event_type,
    const DeepQuery& where,
    const gfx::Size& minimum_size,
    bool must_already_exist) {
  DCHECK(!minimum_size.IsEmpty());
  StateChange change;
  change.event = event_type;
  change.type = must_already_exist ? StateChange::Type::kConditionTrue
                                   : StateChange::Type::kExistsAndConditionTrue;
  change.where = where;
  change.test_function =
      base::StringPrintf(R"(
        el => {
          const rect = el.getBoundingClientRect();
          return rect.width >= %i && rect.height >= %i;
        }
      )",
                         minimum_size.width(), minimum_size.height());
  SendEventOnStateChange(change);
}

void WebContentsInteractionTestUtil::SendEventOnStateChange(
    const StateChange& configuration) {
  CHECK(current_element_);
  CHECK(!configuration.where.empty() || !configuration.test_function.empty());
  CHECK(configuration.event);
  CHECK(configuration.timeout.has_value() || !configuration.timeout_event)
      << "Cannot specify timeout event without timeout.";

  // Determine the actual query we should use; for kConditionTrue we can use
  // configuration.test_function directly, but for the other options we need to
  // modify it.
  std::string actual_func;
  switch (configuration.type) {
    case StateChange::Type::kExists:
      DCHECK(configuration.test_function.empty());
      actual_func = GetExistsQuery("false", "true");
      break;
    case StateChange::Type::kConditionTrue:
      actual_func = configuration.test_function;
      break;
    case StateChange::Type::kExistsAndConditionTrue:
      const std::string on_found = "(" + configuration.test_function + ")(el)";
      actual_func = GetExistsQuery("false", on_found.c_str());
      break;
  }

  PollerData poller_data{
      std::make_unique<Poller>(this, actual_func, configuration.where,
                               configuration.timeout,
                               configuration.polling_interval),
      configuration.event, configuration.timeout_event};
  auto* const poller = poller_data.poller.get();
  pollers_.emplace(poller, std::move(poller_data));
  poller->StartPolling();
}

bool WebContentsInteractionTestUtil::Exists(const DeepQuery& query,
                                            std::string* not_found) {
  const std::string full_query =
      CreateDeepQuery(query, GetExistsQuery("err.selector", "''"));
  const std::string result = Evaluate(full_query).GetString();
  if (not_found)
    *not_found = result;
  return result.empty();
}

base::Value WebContentsInteractionTestUtil::EvaluateAt(
    const DeepQuery& where,
    const std::string& function) {
  const std::string full_query = CreateDeepQuery(where, function);
  return Evaluate(full_query);
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
    const DeepQuery& where) {
  if (!current_element_)
    return gfx::Rect();

  views::WebView* web_view = nullptr;
  if (web_view_data_) {
    DCHECK(web_view_data_->visible() && web_view_data_->web_view());
    web_view = web_view_data_->web_view();
  } else {
    Browser* const browser = chrome::FindBrowserWithWebContents(web_contents());
    if (!browser ||
        web_contents() != browser->tab_strip_model()->GetActiveWebContents()) {
      return gfx::Rect();
    }
    web_view =
        BrowserView::GetBrowserViewForBrowser(browser)->contents_web_view();
  }
  CHECK(web_view);

  // TODO(dfried): Screen bounds returned by GetBoundsInScreen() are in DIPs.
  // We are also assuming that Element.getBoundingClientRect() also returns a
  // value in DIPs (this seems to be borne out by anecdotal evidence in online
  // discussions). However, if that's not the case, either the offset or element
  // bounds will need to be adjusted by the current display's scale factor.
  const gfx::Point offset = web_view->GetBoundsInScreen().origin();

  const base::Value result = EvaluateAt(where,
                                        R"(el => {
      const rect = el.getBoundingClientRect();
      return {
        "x": rect.x,
        "y": rect.y,
        "w": rect.width,
        "h": rect.height
      };
    })");

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
    const std::string& where) {
  return GetElementBoundsInScreen(DeepQuery{where});
}

void WebContentsInteractionTestUtil::SendEventOnWebViewMinimumSize(
    const gfx::Size& minimum_webview_size,
    ui::CustomElementEventType event_type,
    const DeepQuery& element_to_check,
    const gfx::Size& minimum_element_size) {
  CHECK(web_view_data_)
      << "Only supported for util objects created with ForNonTabWebView()";
  web_view_data_->SendEventOnMinimumSize(
      minimum_webview_size, event_type, element_to_check, minimum_element_size);
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
  MaybeCreateElement(/*force =*/true);
}

void WebContentsInteractionTestUtil::PrimaryPageChanged(content::Page& page) {
  DiscardCurrentElement();
}

void WebContentsInteractionTestUtil::WebContentsDestroyed() {
  DiscardCurrentElement();
}

void WebContentsInteractionTestUtil::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  // Don't bother processing if we don't have a target WebContents.
  if (!web_contents())
    return;

  // Ensure that if a tab is moved to another browser, we track that move.
  if (change.type() == TabStripModelChange::Type::kRemoved) {
    for (auto& removed_tab : change.GetRemove()->contents) {
      if (removed_tab.contents != web_contents())
        continue;
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
      MaybeCreateElement(false);
    }
  }
}

WebContentsInteractionTestUtil::WebContentsInteractionTestUtil(
    content::WebContents* web_contents,
    ui::ElementIdentifier page_identifier,
    absl::optional<Browser*> browser,
    views::WebView* web_view)
    : WebContentsObserver(web_contents), page_identifier_(page_identifier) {
  CHECK(page_identifier);

  if (web_view) {
    // This is specifically for a web view that is not a tab.
    CHECK(web_contents);
    CHECK(!browser);
    CHECK(!chrome::FindBrowserWithWebContents(web_contents));
    web_view_data_ = std::make_unique<WebViewData>(this, web_view);
    web_view_data_->Init();
  } else if (browser.has_value()) {
    // Watching for a new tab.
    CHECK(!web_contents);
    new_tab_watcher_ = std::make_unique<NewTabWatcher>(this, browser.value());
  } else {
    // This has to be a tab, so use standard watching logic.
    StartWatchingWebContents(web_contents);
  }
}

void WebContentsInteractionTestUtil::MaybeCreateElement(bool force) {
  if (current_element_ || !web_contents())
    return;

  if (!force && !web_contents()->IsDocumentOnLoadCompletedInPrimaryMainFrame())
    return;

  ui::ElementContext context = ui::ElementContext();
  if (web_view_data_) {
    if (!web_view_data_->visible())
      return;
    context = web_view_data_->context();
  } else {
    Browser* const browser = chrome::FindBrowserWithWebContents(web_contents());
    if (!browser)
      return;
    context = browser->window()->GetElementContext();
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
}

void WebContentsInteractionTestUtil::DiscardCurrentElement() {
  current_element_.reset();
  CHECK(pollers_.empty())
      << "Unexpectedly left page while still waiting for event "
      << pollers_.begin()->second.event.GetName();
  pollers_.clear();
}

void WebContentsInteractionTestUtil::OnPollTimeout(Poller* poller) {
  CHECK(current_element_);
  auto it = pollers_.find(poller);
  CHECK(it != pollers_.end());
  auto event = it->second.timeout_event;
  pollers_.erase(it);
  CHECK(event) << "SendEventOnStateChange timed out, but no timeout event was "
                  "specified.";
  ui::ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(
      current_element_.get(), event);
}

void WebContentsInteractionTestUtil::OnPollEvent(Poller* poller) {
  CHECK(current_element_);
  auto it = pollers_.find(poller);
  CHECK(it != pollers_.end());
  auto event = it->second.event;
  pollers_.erase(it);
  ui::ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(
      current_element_.get(), event);
}

void WebContentsInteractionTestUtil::StartWatchingWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  Browser* const browser = chrome::FindBrowserWithWebContents(web_contents);
  CHECK(browser);
  browser->tab_strip_model()->AddObserver(this);
  if (new_tab_watcher_) {
    new_tab_watcher_.reset();
    Observe(web_contents);
  }
  MaybeCreateElement();
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
