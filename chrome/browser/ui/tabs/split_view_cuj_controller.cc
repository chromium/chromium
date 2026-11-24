// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/split_view_cuj_controller.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/supports_user_data.h"
#include "base/timer/timer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/tabs/public/split_tab_data.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/interaction/element_tracker.h"

namespace {

const void* const kProfileSplitViewCujStateKey = &kProfileSplitViewCujStateKey;

class SplitPaneNavObserver : public content::WebContentsObserver {
 public:
  SplitPaneNavObserver(content::WebContents* wc, base::RepeatingClosure cb)
      : WebContentsObserver(wc), cb_(std::move(cb)) {}
  void PrimaryPageChanged(content::Page&) override {
    base::RepeatingClosure(cb_).Run();
  }

 private:
  base::RepeatingClosure cb_;
};

bool IsUnpopulatedTab(tabs::TabInterface* tab, Profile* profile) {
  content::WebContents* const contents = tab->GetContents();
  const GURL& url = !contents->GetLastCommittedURL().is_empty()
                        ? contents->GetLastCommittedURL()
                        : contents->GetVisibleURL();
  return !url.is_valid() || search::IsSplitViewNewTabPage(url) ||
         search::IsWebUINewTabPageOrigin(url) ||
         search::IsNTPOrRelatedURL(url, profile);
}

bool IsPopulatedSplit(BrowserWindowInterface* bwi, split_tabs::SplitTabId id) {
  TabStripModel* const model = bwi->GetTabStripModel();
  if (!model->ContainsSplit(id)) {
    return false;
  }
  return std::ranges::none_of(model->GetSplitData(id)->ListTabs(),
                              [bwi](tabs::TabInterface* tab) {
                                return IsUnpopulatedTab(tab, bwi->GetProfile());
                              });
}

ui::CustomElementEventType GetEntryCustomEvent(
    std::optional<split_tabs::SplitTabCreatedSource> source) {
  using enum split_tabs::SplitTabCreatedSource;
  switch (source.value_or(kDuplicateSplit)) {
    case kTabContextMenu:
      return kSplitViewEnteredViaTabContextMenuCustomEventId;
    case kToolbarButton:
      return kSplitViewEnteredViaToolbarCustomEventId;
    case kDragAndDropLink:
      return kSplitViewEnteredViaDragLinkCustomEventId;
    case kLinkContextMenu:
      return kSplitViewEnteredViaLinkContextMenuCustomEventId;
    default:
      return kSplitViewEnteredViaOtherCustomEventId;
  }
}

struct ProfileSplitViewCujState : public base::SupportsUserData::Data {
  base::flat_map<split_tabs::SplitTabId, raw_ptr<BrowserWindowInterface>>
      active_splits;
  base::flat_set<split_tabs::SplitTabId> detached_split_ids;
  std::vector<std::unique_ptr<SplitPaneNavObserver>> pane_observers;
  base::CallbackListSubscription browser_view_shown_sub;
  base::CallbackListSubscription browser_view_hidden_sub;
  base::OneShotTimer sustained_timer;
  bool had_populated_split = false;
  bool outcome_emitted = false;

  void StopTracking() {
    sustained_timer.Stop();
    pane_observers.clear();
    browser_view_shown_sub = {};
    browser_view_hidden_sub = {};
  }

  void ResetSession() {
    StopTracking();
    had_populated_split = false;
    outcome_emitted = false;
  }

  bool HasPopulatedSplit() const {
    return std::ranges::any_of(active_splits, [](const auto& entry) {
      return IsPopulatedSplit(entry.second, entry.first);
    });
  }

  BrowserElements* FindVisiblePopulatedElements() const {
    for (const auto& [split_id, bwi] : active_splits) {
      if (IsPopulatedSplit(bwi, split_id)) {
        if (auto* const elements = BrowserElements::From(bwi);
            elements->GetElement(kBrowserViewElementId)) {
          return elements;
        }
      }
    }
    return nullptr;
  }

  void OnBrowserViewVisibilityChanged(ui::TrackedElement*) { Refresh(); }

  void OnTimerFired() {
    if (outcome_emitted) {
      return;
    }
    // Re-verify that a populated split still exists on a visible browser window
    // before emitting sustained completion; mark completion and tear down
    // tracking before `NotifyEvent()` so synchronous listeners cannot re-enter
    // or double-emit.
    if (BrowserElements* const elements = FindVisiblePopulatedElements()) {
      outcome_emitted = true;
      StopTracking();
      elements->NotifyEvent(kBrowserViewElementId,
                            kSplitViewSustainedComparisonCustomEventId);
      return;
    }
    Refresh();
  }

  void Refresh() {
    if (outcome_emitted) {
      return;
    }
    if (HasPopulatedSplit()) {
      had_populated_split = true;
    }
    // Start (without resetting, if already running) the 15s dwell timer as soon
    // as any visible window in the profile holds a split with two real pages,
    // and pause it whenever no visible split is populated.
    if (FindVisiblePopulatedElements()) {
      if (!sustained_timer.IsRunning()) {
        sustained_timer.Start(
            FROM_HERE, SplitViewCujController::kDefaultSustainedComparisonDelay,
            base::BindOnce(&ProfileSplitViewCujState::OnTimerFired,
                           base::Unretained(this)));
      }
    } else {
      sustained_timer.Stop();
    }

    if (active_splits.empty()) {
      pane_observers.clear();
      browser_view_shown_sub = {};
      browser_view_hidden_sub = {};
      return;
    }

    if (!browser_view_shown_sub) {
      auto* const tracker = ui::ElementTracker::GetElementTracker();
      auto visibility_cb = base::BindRepeating(
          &ProfileSplitViewCujState::OnBrowserViewVisibilityChanged,
          base::Unretained(this));
      browser_view_shown_sub = tracker->AddElementShownInAnyContextCallback(
          kBrowserViewElementId, visibility_cb);
      browser_view_hidden_sub = tracker->AddElementHiddenInAnyContextCallback(
          kBrowserViewElementId, std::move(visibility_cb));
    }

    // Watch all active split panes until `outcome_emitted` so navigating back
    // to an NTP or populating an NTP updates the dwell timer immediately.
    base::flat_set<content::WebContents*> split_contents;
    for (const auto& [split_id, bwi] : active_splits) {
      TabStripModel* const model = bwi->GetTabStripModel();
      if (model->ContainsSplit(split_id)) {
        for (tabs::TabInterface* tab :
             model->GetSplitData(split_id)->ListTabs()) {
          split_contents.insert(tab->GetContents());
        }
      }
    }
    std::erase_if(pane_observers, [&](const auto& obs) {
      return !split_contents.contains(obs->web_contents());
    });
    auto cb = base::BindRepeating(&ProfileSplitViewCujState::Refresh,
                                  base::Unretained(this));
    for (content::WebContents* wc : split_contents) {
      if (!std::ranges::any_of(pane_observers, [wc](const auto& obs) {
            return obs->web_contents() == wc;
          })) {
        pane_observers.push_back(
            std::make_unique<SplitPaneNavObserver>(wc, cb));
      }
    }
  }
};

ProfileSplitViewCujState* GetExistingProfileState(Profile* profile) {
  return static_cast<ProfileSplitViewCujState*>(
      profile->GetUserData(kProfileSplitViewCujStateKey));
}

ProfileSplitViewCujState& GetProfileState(Profile* profile) {
  if (auto* const state = GetExistingProfileState(profile)) {
    return *state;
  }
  auto state = std::make_unique<ProfileSplitViewCujState>();
  auto* const ptr = state.get();
  profile->SetUserData(kProfileSplitViewCujStateKey, std::move(state));
  return *ptr;
}

}  // namespace

SplitViewCujController::SplitViewCujController(
    BrowserWindowInterface* browser_window_interface)
    : browser_window_interface_(browser_window_interface) {
  CHECK(browser_window_interface_);
  browser_window_interface_->GetTabStripModel()->AddObserver(this);
}

SplitViewCujController::~SplitViewCujController() {
  auto* const state =
      GetExistingProfileState(browser_window_interface_->GetProfile());
  if (!state) {
    return;
  }
  const size_t erased =
      base::EraseIf(state->active_splits, [this](const auto& entry) {
        return entry.second == browser_window_interface_;
      });
  if (state->active_splits.empty() && state->detached_split_ids.empty()) {
    state->ResetSession();
  } else if (erased > 0) {
    state->Refresh();
  }
}

void SplitViewCujController::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (change.type() != TabStripModelChange::kReplaced) {
    return;
  }
  const std::optional<split_tabs::SplitTabId> split_id =
      change.GetReplace()->tab->GetSplit();
  if (!split_id) {
    return;
  }
  if (auto* const state =
          GetExistingProfileState(browser_window_interface_->GetProfile());
      state && state->active_splits.contains(*split_id)) {
    state->Refresh();
  }
}

void SplitViewCujController::OnSplitTabChanged(const SplitTabChange& change) {
  using AddReason = SplitTabChange::SplitTabAddReason;
  using RemoveReason = SplitTabChange::SplitTabRemoveReason;

  Profile* const profile = browser_window_interface_->GetProfile();
  ProfileSplitViewCujState& state = GetProfileState(profile);

  if (change.type == SplitTabChange::Type::kAdded) {
    const auto* const added = change.GetAddedChange();
    if (added->reason() == AddReason::kInsertedFromAnotherTabstrip &&
        state.detached_split_ids.erase(change.split_id) > 0) {
      // Re-associate a split moved from another window without starting a new
      // CUJ session or resetting the in-flight 15s timer.
      state.active_splits[change.split_id] = browser_window_interface_;
    } else if (added->reason() == AddReason::kNewSplitTabAdded) {
      auto* const elements = BrowserElements::From(browser_window_interface_);
      if (!elements->GetElement(kBrowserViewElementId)) {
        return;
      }
      // Only the first active split in the profile starts a new CUJ session;
      // concurrent splits share the ongoing session.
      if (state.active_splits.empty() && state.detached_split_ids.empty()) {
        state.ResetSession();
        elements->NotifyEvent(kBrowserViewElementId,
                              GetEntryCustomEvent(added->source()));
      }
      state.active_splits[change.split_id] = browser_window_interface_;
    } else if (added->reason() != AddReason::kSplitTabUpdated ||
               !state.active_splits.contains(change.split_id)) {
      return;
    }
    state.Refresh();
    return;
  }

  if (change.type != SplitTabChange::Type::kRemoved) {
    return;
  }

  const auto* const removed = change.GetRemovedChange();
  const RemoveReason reason = removed->reason();
  if (reason == RemoveReason::kSplitTabUpdated) {
    return;
  }
  if (reason == RemoveReason::kDetachedToAnotherTabstrip) {
    // Track splits mid-drag between windows so the session stays continuous.
    if (state.active_splits.erase(change.split_id) > 0) {
      state.detached_split_ids.insert(change.split_id);
    }
    return;
  }
  if (state.active_splits.erase(change.split_id) == 0) {
    return;
  }

  if (std::ranges::none_of(removed->tabs(), [profile](const auto& entry) {
        return IsUnpopulatedTab(entry.first, profile);
      })) {
    state.had_populated_split = true;
  }
  // Keep the session alive while other splits remain open or mid-drag in the
  // profile.
  if (!state.active_splits.empty() || !state.detached_split_ids.empty()) {
    state.Refresh();
    return;
  }

  const bool should_emit_exit = !state.outcome_emitted;
  const bool had_populated = state.had_populated_split;
  state.ResetSession();

  // Suppress early-exit events when Split View dissolves because the entire
  // browser window is closing.
  if (should_emit_exit && reason == RemoveReason::kSplitTabRemoved &&
      !change.model->closing_all() && !is_closing_all_tabs_) {
    BrowserElements::From(browser_window_interface_)
        ->NotifyEvent(kBrowserViewElementId,
                      had_populated ? kSplitViewExitedEarlyCustomEventId
                                    : kSplitViewExitedOnNtpCustomEventId);
  }
}

void SplitViewCujController::WillCloseAllTabs(TabStripModel* tab_strip_model) {
  is_closing_all_tabs_ = true;
}

void SplitViewCujController::CloseAllTabsStopped(TabStripModel* tab_strip_model,
                                                 CloseAllStoppedReason reason) {
  is_closing_all_tabs_ = false;
}
