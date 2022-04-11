// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/interaction/interaction_sequence_browser_util.h"

#include <set>
#include <sstream>

#include "base/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/values.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/framework_specific_implementation.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"

namespace content {
class RenderFrameHost;
}

namespace {

content::WebContents* GetWebContents(Browser* browser,
                                     absl::optional<int> tab_index) {
  auto* const model = browser->tab_strip_model();
  return model->GetWebContentsAt(tab_index.value_or(model->active_index()));
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
  std::string token = "EvalJsLocal-" + base::GenerateGUID();
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

  const std::u16string script16 = base::UTF8ToUTF16(runner_script);
  if (host->GetLifecycleState() !=
      content::RenderFrameHost::LifecycleState::kPrerendering) {
    host->ExecuteJavaScriptWithUserGestureForTests(
        script16, base::NullCallback());  // IN-TEST
  } else {
    host->ExecuteJavaScriptForTests(script16, base::NullCallback());  // IN-TEST
  }

  std::string json;
  if (!dom_message_queue.WaitForMessage(&json))
    return content::EvalJsResult(base::Value(),
                                 "Cannot communicate with DOMMessageQueue.");

  base::JSONReader::ValueWithError parsed_json =
      base::JSONReader::ReadAndReturnValueWithError(
          json, base::JSON_ALLOW_TRAILING_COMMAS);

  if (!parsed_json.value.has_value())
    return content::EvalJsResult(
        base::Value(), "JSON parse error: " + parsed_json.error_message);

  if (!parsed_json.value->is_list() ||
      parsed_json.value->GetList().size() != 2U ||
      !parsed_json.value->GetList()[1].is_list() ||
      parsed_json.value->GetList()[1].GetList().size() != 2U ||
      !parsed_json.value->GetList()[1].GetList()[1].is_string() ||
      parsed_json.value->GetList()[0].GetString() != token) {
    std::ostringstream error_message;
    error_message << "Received unexpected result: "
                  << parsed_json.value.value();
    return content::EvalJsResult(base::Value(), error_message.str());
  }
  auto& result = parsed_json.value->GetList()[1].GetList();

  return content::EvalJsResult(std::move(result[0]), result[1].GetString());
}

std::string CreateDeepQuery(
    const InteractionSequenceBrowserUtil::DeepQuery& where,
    const std::string& function,
    bool is_exists) {
  const std::string not_found_action =
      is_exists ? "return selector"
                : "throw new Error('Selector not found: ' + selector)";
  const std::string deepquery_return_expression = is_exists ? "''" : "cur";
  const std::string final_return_expression =
      is_exists ? "el" : "(" + function + ")(el)";

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
               %s;
             }
           }
           return %s;
         }

         let el = deepQuery(%s);
         return %s;
       })",
      not_found_action.c_str(), deepquery_return_expression.c_str(),
      selectors.c_str(), final_return_expression.c_str());
}

}  // namespace

InteractionSequenceBrowserUtil::StateChange::StateChange() = default;
InteractionSequenceBrowserUtil::StateChange::~StateChange() = default;

class InteractionSequenceBrowserUtil::NewTabWatcher
    : public TabStripModelObserver,
      public BrowserListObserver {
 public:
  NewTabWatcher(InteractionSequenceBrowserUtil* owner, Browser* browser)
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

    auto* const web_contents = change.GetInsert()->contents.front().contents;
    CHECK(!browser_ ||
          browser_ == chrome::FindBrowserWithWebContents(web_contents));
    owner_->StartWatchingWebContents(web_contents);
  }

  const base::raw_ptr<InteractionSequenceBrowserUtil> owner_;
  const base::raw_ptr<Browser> browser_;
};

class InteractionSequenceBrowserUtil::Poller {
 public:
  Poller(InteractionSequenceBrowserUtil* const owner,
         const std::string function,
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
    base::Value result;
    if (where_.empty()) {
      result = owner_->Evaluate(function_);
    } else if (function_.empty()) {
      result = base::Value(owner_->Exists(where_));
    } else {
      result = owner_->EvaluateAt(where_, function_);
    }

    if (IsTruthy(result)) {
      owner_->OnPollEvent(this);
    } else if (timeout_.has_value() && elapsed_.Elapsed() > timeout_.value()) {
      owner_->OnPollTimeout(this);
    }
  }

  const base::ElapsedTimer elapsed_;
  const std::string function_;
  const DeepQuery where_;
  const base::TimeDelta interval_;
  const absl::optional<base::TimeDelta> timeout_;
  const base::raw_ptr<InteractionSequenceBrowserUtil> owner_;
  base::RepeatingTimer timer_;
};

struct InteractionSequenceBrowserUtil::PollerData {
  std::unique_ptr<Poller> poller;
  ui::CustomElementEventType event;
  ui::CustomElementEventType timeout_event;
};

// static
constexpr base::TimeDelta
    InteractionSequenceBrowserUtil::kDefaultPollingInterval;

InteractionSequenceBrowserUtil::~InteractionSequenceBrowserUtil() {
  // Stop observing before eliminating the element, as a callback could cascade
  // into additional events.
  new_tab_watcher_.reset();
  Observe(nullptr);
  pollers_.clear();
}

// static
bool InteractionSequenceBrowserUtil::IsTruthy(const base::Value& value) {
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
    case Type::DICTIONARY:
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
std::unique_ptr<InteractionSequenceBrowserUtil>
InteractionSequenceBrowserUtil::ForExistingTabInContext(
    ui::ElementContext context,
    ui::ElementIdentifier page_identifier,
    absl::optional<int> tab_index) {
  return ForExistingTabInBrowser(GetBrowserFromContext(context),
                                 page_identifier, tab_index);
}

// static
std::unique_ptr<InteractionSequenceBrowserUtil>
InteractionSequenceBrowserUtil::ForExistingTabInBrowser(
    Browser* browser,
    ui::ElementIdentifier page_identifier,
    absl::optional<int> tab_index) {
  return ForWebContents(GetWebContents(browser, tab_index), page_identifier,
                        nullptr);
}

// static
std::unique_ptr<InteractionSequenceBrowserUtil>
InteractionSequenceBrowserUtil::ForWebContents(
    content::WebContents* web_contents,
    ui::ElementIdentifier page_identifier,
    Browser* browser) {
  return base::WrapUnique(new InteractionSequenceBrowserUtil(
      web_contents, page_identifier,
      browser ? absl::make_optional(browser) : absl::nullopt));
}

// static
std::unique_ptr<InteractionSequenceBrowserUtil>
InteractionSequenceBrowserUtil::ForNextTabInContext(
    ui::ElementContext context,
    ui::ElementIdentifier page_identifier) {
  Browser* const browser = GetBrowserFromContext(context);
  return ForNextTabInBrowser(browser, page_identifier);
}

// static
std::unique_ptr<InteractionSequenceBrowserUtil>
InteractionSequenceBrowserUtil::ForNextTabInBrowser(
    Browser* browser,
    ui::ElementIdentifier page_identifier) {
  CHECK(browser);
  return base::WrapUnique(
      new InteractionSequenceBrowserUtil(nullptr, page_identifier, browser));
}

// static
std::unique_ptr<InteractionSequenceBrowserUtil>
InteractionSequenceBrowserUtil::ForNextTabInAnyBrowser(
    ui::ElementIdentifier page_identifier) {
  return base::WrapUnique(
      new InteractionSequenceBrowserUtil(nullptr, page_identifier, nullptr));
}

// static
Browser* InteractionSequenceBrowserUtil::GetBrowserFromContext(
    ui::ElementContext context) {
  BrowserList* const browsers = BrowserList::GetInstance();
  for (Browser* const browser : *browsers) {
    if (browser->window()->GetElementContext() == context)
      return browser;
  }
  return nullptr;
}

void InteractionSequenceBrowserUtil::LoadPage(const GURL& url) {
  CHECK(web_contents());
  if (!web_contents()->GetURL().EqualsIgnoringRef(url)) {
    navigating_away_from_ = web_contents()->GetURL();
    DiscardCurrentElement();
  }
  if (url.SchemeIs("chrome")) {
    Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
    CHECK(browser);
    NavigateParams navigate_params(browser, url, ui::PAGE_TRANSITION_TYPED);
    navigate_params.disposition = WindowOpenDisposition::CURRENT_TAB;
    auto navigate_result = Navigate(&navigate_params);
    CHECK(navigate_result);
  } else {
    const bool result =
        content::BeginNavigateToURLFromRenderer(web_contents(), url);
    CHECK(result);
  }
}

void InteractionSequenceBrowserUtil::LoadPageInNewTab(const GURL& url,
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

base::Value InteractionSequenceBrowserUtil::Evaluate(
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

void InteractionSequenceBrowserUtil::SendEventOnStateChange(
    const StateChange& configuration) {
  CHECK(current_element_);
  CHECK(!configuration.where.empty() || !configuration.test_function.empty());
  CHECK(configuration.event);
  CHECK(configuration.timeout.has_value() || !configuration.timeout_event)
      << "Cannot specify timeout event without timeout.";
  PollerData poller_data{
      std::make_unique<Poller>(this, configuration.test_function,
                               configuration.where,
                               configuration.timeout,
                               configuration.polling_interval),
      configuration.event, configuration.timeout_event};
  auto* const poller = poller_data.poller.get();
  pollers_.emplace(poller, std::move(poller_data));
  poller->StartPolling();
}

bool InteractionSequenceBrowserUtil::Exists(const DeepQuery& query,
                                            std::string* not_found) {
  const std::string full_query = CreateDeepQuery(query, "", true);
  const std::string result = Evaluate(full_query).GetString();
  if (not_found)
    *not_found = result;
  return result.empty();
}

base::Value InteractionSequenceBrowserUtil::EvaluateAt(
    const DeepQuery& where,
    const std::string& function) {
  const std::string full_query = CreateDeepQuery(where, function, false);
  return Evaluate(full_query);
}

bool InteractionSequenceBrowserUtil::Exists(const std::string& selector) {
  return Exists(DeepQuery{selector});
}

base::Value InteractionSequenceBrowserUtil::EvaluateAt(
    const std::string& selector,
    const std::string& function) {
  return EvaluateAt(DeepQuery{selector}, function);
}

void InteractionSequenceBrowserUtil::DidStopLoading() {
  // In some cases we will not have an "on load complete" event, so ensure that
  // we check for page fully loaded in other callbacks.
  MaybeCreateElement();
}

void InteractionSequenceBrowserUtil::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  // In some cases we will not have an "on load complete" event, so ensure that
  // we check for page fully loaded in other callbacks.
  MaybeCreateElement();
}

void InteractionSequenceBrowserUtil::
    DocumentOnLoadCompletedInPrimaryMainFrame() {
  // Even if the page is still "loading" it should be ready for interaction at
  // this point. Note that in some cases we won't receive this event, which is
  // why we also check at DidStopLoading() and DidFinishLoad().
  MaybeCreateElement(/*force =*/true);
}

void InteractionSequenceBrowserUtil::PrimaryPageChanged(content::Page& page) {
  DiscardCurrentElement();
}

void InteractionSequenceBrowserUtil::WebContentsDestroyed() {
  DiscardCurrentElement();
}

void InteractionSequenceBrowserUtil::OnTabStripModelChanged(
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
  }
}

InteractionSequenceBrowserUtil::InteractionSequenceBrowserUtil(
    content::WebContents* web_contents,
    ui::ElementIdentifier page_identifier,
    absl::optional<Browser*> browser)
    : WebContentsObserver(web_contents), page_identifier_(page_identifier) {
  CHECK(page_identifier);

  if (browser.has_value()) {
    // Are we watching for a new tab, or are we just specifying the browser?
    if (!web_contents) {
      // Watching for a new tab.
      new_tab_watcher_ = std::make_unique<NewTabWatcher>(this, browser.value());
    } else {
      // See if we redundantly specified a tab and its browser.
      Browser* const found = chrome::FindBrowserWithWebContents(web_contents);
      if (found == browser.value()) {
        // Yes, this is a tab. Watch it as normal.
        StartWatchingWebContents(web_contents);
      } else {
        // No this is not a tab. Remember the context so we can properly create
        // elements.
        force_context_ = browser.value()->window()->GetElementContext();
        MaybeCreateElement();
      }
    }
  } else {
    // This has to be a tab, so use standard watching logic.
    StartWatchingWebContents(web_contents);
  }
}

void InteractionSequenceBrowserUtil::MaybeCreateElement(bool force) {
  if (current_element_ || !web_contents())
    return;

  if (!force && !web_contents()->IsDocumentOnLoadCompletedInPrimaryMainFrame())
    return;

  ui::ElementContext context = force_context_;
  if (!context) {
    Browser* const browser = chrome::FindBrowserWithWebContents(web_contents());
    if (!browser)
      return;
    context = browser->window()->GetElementContext();
  }

  // Ignore events on a page we're navigating away from.
  if (navigating_away_from_.EqualsIgnoringRef(web_contents()->GetURL()))
    return;
  navigating_away_from_ = GURL();

  current_element_ =
      std::make_unique<TrackedElementWebPage>(page_identifier_, context, this);

  // Init (send shown event, etc.) after current_element_ is set in order to
  // ensure that is_page_loaded() is true during any callbacks.
  current_element_->Init();
}

void InteractionSequenceBrowserUtil::DiscardCurrentElement() {
  current_element_.reset();
  CHECK(pollers_.empty())
      << "Unexpectedly left page while still waiting for event "
      << pollers_.begin()->second.event.GetName();
  pollers_.clear();
}

void InteractionSequenceBrowserUtil::OnPollTimeout(Poller* poller) {
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

void InteractionSequenceBrowserUtil::OnPollEvent(Poller* poller) {
  CHECK(current_element_);
  auto it = pollers_.find(poller);
  CHECK(it != pollers_.end());
  auto event = it->second.event;
  pollers_.erase(it);
  ui::ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(
      current_element_.get(), event);
}

void InteractionSequenceBrowserUtil::StartWatchingWebContents(
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

TrackedElementWebPage::TrackedElementWebPage(
    ui::ElementIdentifier identifier,
    ui::ElementContext context,
    InteractionSequenceBrowserUtil* owner)
    : TrackedElement(identifier, context), owner_(owner) {}

TrackedElementWebPage::~TrackedElementWebPage() {
  ui::ElementTracker::GetFrameworkDelegate()->NotifyElementHidden(this);
}

void TrackedElementWebPage::Init() {
  ui::ElementTracker::GetFrameworkDelegate()->NotifyElementShown(this);
}

DEFINE_FRAMEWORK_SPECIFIC_METADATA(TrackedElementWebPage)
