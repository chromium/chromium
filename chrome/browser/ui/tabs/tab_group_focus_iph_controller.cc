// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_group_focus_iph_controller.h"

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "components/feature_engagement/public/feature_constants.h"

DEFINE_USER_DATA(TabGroupFocusIphController);

TabGroupFocusIphController::TabGroupFocusIphController(
    BrowserWindowInterface* interface)
    : browser_window_interface_(interface),
      scoped_data_(interface->GetUnownedUserDataHost(), *this) {
  browser_window_interface_->GetTabStripModel()->AddObserver(this);
}

TabGroupFocusIphController::~TabGroupFocusIphController() = default;

// static
TabGroupFocusIphController* TabGroupFocusIphController::From(
    BrowserWindowInterface* interface) {
  return ui::ScopedUnownedUserData<TabGroupFocusIphController>::Get(
      interface->GetUnownedUserDataHost());
}

void TabGroupFocusIphController::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (change.type() == TabStripModelChange::kInserted) {
    ScheduleMaybeShowPromo();
  }
}

void TabGroupFocusIphController::OnTabGroupAdded(
    const tab_groups::TabGroupId& group_id) {
  ScheduleMaybeShowPromo();
}

void TabGroupFocusIphController::OnTabGroupFocusChanged(
    std::optional<tab_groups::TabGroupId> new_focused_group_id,
    std::optional<tab_groups::TabGroupId> old_focused_group_id) {
  if (new_focused_group_id.has_value()) {
    promo_timer_.Stop();
  }

  // May be null in tests.
  auto* const user_ed =
      BrowserUserEducationInterface::From(browser_window_interface_);
  if (!user_ed) {
    return;
  }

  if (new_focused_group_id.has_value()) {
    user_ed->NotifyFeaturePromoFeatureUsed(
        feature_engagement::kIPHTabGroupsFocusingFeature,
        FeaturePromoFeatureUsedAction::kClosePromoIfPresent);
    user_ed->NotifyNewBadgeFeatureUsed(features::kTabGroupsFocusing);
    user_ed->MaybeShowFeaturePromo(
        feature_engagement::kIPHTabGroupsUnfocusingFeature);
  } else if (old_focused_group_id.has_value()) {
    user_ed->NotifyFeaturePromoFeatureUsed(
        feature_engagement::kIPHTabGroupsUnfocusingFeature,
        FeaturePromoFeatureUsedAction::kClosePromoIfPresent);
  }
}

void TabGroupFocusIphController::ScheduleMaybeShowPromo() {
  promo_timer_.Start(FROM_HERE, features::kTabGroupsFocusingIphPromoDelay.Get(),
                     this, &TabGroupFocusIphController::MaybeShowPromo);
}

void TabGroupFocusIphController::MaybeShowPromo() {
  auto* const tab_strip_model = browser_window_interface_->GetTabStripModel();
  if (!tab_strip_model || !tab_strip_model->SupportsTabGroups() ||
      tab_strip_model->GetFocusedGroup().has_value()) {
    return;
  }

  TabGroupModel* const group_model = tab_strip_model->group_model();
  if (!group_model || group_model->ListTabGroups().empty()) {
    return;
  }

  if (tab_strip_model->count() <=
      features::kTabGroupsFocusingIphMinTabs.Get()) {
    return;
  }

  if (auto* user_ed =
          BrowserUserEducationInterface::From(browser_window_interface_)) {
    user_ed->MaybeShowFeaturePromo(
        feature_engagement::kIPHTabGroupsFocusingFeature);
  }
}
