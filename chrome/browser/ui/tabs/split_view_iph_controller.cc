// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/split_view_iph_controller.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/pref_names.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/prefs/pref_service.h"
#include "ui/views/interaction/element_tracker_views.h"

DEFINE_USER_DATA(SplitViewIphController);
SplitViewIphController::SplitViewIphController(
    BrowserWindowInterface* interface)
    : browser_window_interface_(interface),
      recent_tabs_(kNumTabsTracked),
      scoped_data_(interface->GetUnownedUserDataHost(), *this) {
  browser_window_interface_->GetTabStripModel()->AddObserver(this);
}

SplitViewIphController::~SplitViewIphController() = default;

SplitViewIphController* SplitViewIphController::From(
    BrowserWindowInterface* interface) {
  return ui::ScopedUnownedUserData<SplitViewIphController>::Get(
      interface->GetUnownedUserDataHost());
}

void SplitViewIphController::OnSplitTabChanged(const SplitTabChange& change) {
  if (change.type == SplitTabChange::Type::kAdded &&
      change.GetAddedChange()->reason() !=
          SplitTabChange::SplitTabAddReason::kSplitTabUpdated) {
    BrowserUserEducationInterface::From(browser_window_interface_)
        ->NotifyAdditionalConditionEvent(
            feature_engagement::events::kSplitViewCreated);

    MaybeShowPromo(feature_engagement::kIPHSideBySidePinnableFeature);
  }
}

void SplitViewIphController::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (change.type() == TabStripModelChange::kRemoved) {
    RemoveTabFromTracker(change.GetRemove());
  }

  if (selection.active_tab_changed()) {
    tabs::TabInterface* const active_tab = selection.new_tab;

    if (recent_tabs_.size() < kNumTabsTracked ||
        (recent_tabs_[kMostRecentTabTrackerIndex] != active_tab &&
         recent_tabs_[kLeastRecentTabTrackerIndex] != active_tab)) {
      AddNewTabToTracker(active_tab);
    } else if (++tab_switch_count_ >=
               features::kSideBySideIphTabSwitchCount.Get()) {
      const bool is_split_view_pinned =
          browser_window_interface_->GetProfile()->GetPrefs()->GetBoolean(
              prefs::kPinSplitTabButton);

      // Only attempt to show the promo when the split tabs toolbar button is
      // not pinned, which would indicate the user has already used the split
      // tabs feature.
      if (!is_split_view_pinned) {
        MaybeShowPromo(feature_engagement::kIPHSideBySideTabSwitchFeature);
      }
    }
  }
}

void SplitViewIphController::AddNewTabToTracker(tabs::TabInterface* new_tab) {
  // Replace the oldest recent tab with the new tab.
  if (recent_tabs_.size() >= kNumTabsTracked) {
    recent_tabs_.pop_back();
  }
  recent_tabs_.push_front(new_tab);

  // Reset tab_switch_count since we're entering a new tab
  tab_switch_count_ = 0;
}

void SplitViewIphController::RemoveTabFromTracker(
    const TabStripModelChange::Remove* remove_contents) {
  for (const auto& contents : remove_contents->contents) {
    if (recent_tabs_.empty()) {
      return;
    }

    if (recent_tabs_[kMostRecentTabTrackerIndex] == contents.tab) {
      recent_tabs_.pop_front();
    } else if (recent_tabs_.size() == kNumTabsTracked &&
               recent_tabs_[kLeastRecentTabTrackerIndex] == contents.tab) {
      recent_tabs_.pop_back();
    }
  }
}

ui::TrackedElement* SplitViewIphController::GetTabSwitchIPHAnchor(
    BrowserView* browser_view) {
  TabStripModel* tab_strip_model =
      browser_window_interface_->GetTabStripModel();

  // Default to no tab if tabs have not been switched yet.
  int tab_strip_tab_index = TabStripModel::kNoTab;

  if (recent_tabs_.size() >= kNumTabsTracked) {
    const int inactive_tab_index =
        browser_window_interface_->GetActiveTabInterface() ==
                recent_tabs_[kMostRecentTabTrackerIndex]
            ? kLeastRecentTabTrackerIndex
            : kMostRecentTabTrackerIndex;

    tab_strip_tab_index =
        tab_strip_model->GetIndexOfTab(recent_tabs_[inactive_tab_index]);
  }

  if (tab_strip_tab_index == TabStripModel::kNoTab) {
    return nullptr;
  }

  views::View* tab_view =
      browser_view->tab_strip_view()->GetTabAnchorViewAt(tab_strip_tab_index);
  return tab_view
             ? views::ElementTrackerViews::GetInstance()->GetElementForView(
                   tab_view)
             : nullptr;
}

void SplitViewIphController::MaybeShowPromo(const base::Feature& feature) {
  BrowserUserEducationInterface::From(browser_window_interface_)
      ->MaybeShowFeaturePromo(feature);
}
