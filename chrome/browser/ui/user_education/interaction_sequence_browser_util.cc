// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/user_education/interaction_sequence_browser_util.h"
#include <set>

#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
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

namespace {

content::WebContents* GetWebContents(Browser* browser,
                                     absl::optional<int> tab_index) {
  auto* const model = browser->tab_strip_model();
  return model->GetWebContentsAt(tab_index.value_or(model->active_index()));
}

}  // namespace

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
         const std::string script,
         absl::optional<base::TimeDelta> timeout,
         base::TimeDelta interval)
      : script_(script),
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
    auto result = owner_->Evaluate(script_);
    if (IsTruthy(result)) {
      owner_->OnPollEvent(this);
    } else if (timeout_.has_value() && elapsed_.Elapsed() > timeout_.value()) {
      owner_->OnPollTimeout(this);
    }
  }

  const base::ElapsedTimer elapsed_;
  const std::string script_;
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

InteractionSequenceBrowserUtil::InteractionSequenceBrowserUtil(
    ui::ElementContext context,
    ui::ElementIdentifier page_identifier,
    absl::optional<int> tab_index)
    : InteractionSequenceBrowserUtil(GetBrowserFromContext(context),
                                     page_identifier,
                                     tab_index) {}

InteractionSequenceBrowserUtil::InteractionSequenceBrowserUtil(
    Browser* browser,
    ui::ElementIdentifier page_identifier,
    absl::optional<int> tab_index)
    : InteractionSequenceBrowserUtil(GetWebContents(browser, tab_index),
                                     page_identifier) {}

InteractionSequenceBrowserUtil::InteractionSequenceBrowserUtil(
    content::WebContents* web_contents,
    ui::ElementIdentifier page_identifier)
    : InteractionSequenceBrowserUtil(web_contents,
                                     page_identifier,
                                     absl::nullopt) {}

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
  const bool result =
      content::BeginNavigateToURLFromRenderer(web_contents(), url);
  CHECK(result);
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
    const std::string& script) {
  CHECK(is_page_loaded());
  auto result = content::EvalJs(web_contents(), script);
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
  CHECK(!configuration.test_script.empty());
  CHECK(configuration.event);
  CHECK(configuration.timeout.has_value() || !configuration.timeout_event)
      << "Cannot specify timeout event without timeout.";
  PollerData poller_data{
      std::make_unique<Poller>(this, configuration.test_script,
                               configuration.timeout,
                               configuration.polling_interval),
      configuration.event, configuration.timeout_event};
  auto* const poller = poller_data.poller.get();
  pollers_.emplace(poller, std::move(poller_data));
  poller->StartPolling();
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
    absl::optional<Browser*> wait_for_new_tab_target)
    : WebContentsObserver(web_contents), page_identifier_(page_identifier) {
  CHECK(page_identifier);

  if (wait_for_new_tab_target.has_value()) {
    DCHECK(!web_contents);
    new_tab_watcher_ =
        std::make_unique<NewTabWatcher>(this, wait_for_new_tab_target.value());
  } else {
    StartWatchingWebContents(web_contents);
  }
}

void InteractionSequenceBrowserUtil::MaybeCreateElement(bool force) {
  if (current_element_ || !web_contents())
    return;

  if (!force && !web_contents()->IsDocumentOnLoadCompletedInPrimaryMainFrame())
    return;

  Browser* const browser = chrome::FindBrowserWithWebContents(web_contents());
  if (!browser)
    return;

  const ui::ElementContext context = browser->window()->GetElementContext();
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
