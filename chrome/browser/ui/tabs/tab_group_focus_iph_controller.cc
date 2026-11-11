// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_group_focus_iph_controller.h"

#include <utility>

#include "chrome/browser/ui/browser_init_state.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/tabs/public/tab_group.h"

DEFINE_USER_DATA(TabGroupFocusIphController);

TabGroupFocusIphController::TabGroupFocusIphController(
    BrowserWindowInterface* interface)
    : browser_window_interface_(interface),
      scoped_data_(interface->GetUnownedUserDataHost(), *this) {
  if (const auto* const init_state = BrowserInitState::From(interface)) {
    is_restoring_focused_group_ =
        init_state->initial_focused_tab_group_id().has_value();
  }
  auto* const tab_strip_model = browser_window_interface_->GetTabStripModel();
  CHECK(tab_strip_model);
  CHECK(tab_strip_model->SupportsTabGroups());
  tab_strip_model->AddObserver(this);
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
    MaybeScheduleGroupFocusPromo();
  }
}

void TabGroupFocusIphController::TabGroupedStateChanged(
    TabStripModel* tab_strip_model,
    std::optional<tab_groups::TabGroupId> old_group,
    std::optional<tab_groups::TabGroupId> new_group,
    tabs::TabHandle tab) {
  if (new_group.has_value() &&
      tab_strip_model->GetFocusedGroup() == new_group) {
    MaybeShowNonGroupPromo();
  }
}

void TabGroupFocusIphController::OnTabGroupAdded(
    const tab_groups::TabGroupId& group_id) {
  auto* const tab_strip_model = browser_window_interface_->GetTabStripModel();
  if (tab_strip_model->GetFocusedGroup() == group_id &&
      !tab_strip_model->IsEphemeralTabGroup(group_id)) {
    if (auto* const user_ed =
            BrowserUserEducationInterface::From(browser_window_interface_)) {
      user_ed->NotifyFeaturePromoFeatureUsed(
          feature_engagement::kIPHTabGroupsNonGroupFocusingFeature,
          FeaturePromoFeatureUsedAction::kClosePromoIfPresent);
    }
    return;
  }
  MaybeScheduleGroupFocusPromo();
}

void TabGroupFocusIphController::OnTabGroupFocusChanged(
    std::optional<tab_groups::TabGroupId> new_focused_group_id,
    std::optional<tab_groups::TabGroupId> old_focused_group_id) {
  if (new_focused_group_id.has_value()) {
    promo_timer_.Stop();
    if (std::exchange(is_restoring_focused_group_, false)) {
      return;
    }
  }

  // May be null in tests.
  auto* const user_ed =
      BrowserUserEducationInterface::From(browser_window_interface_);
  if (!user_ed) {
    return;
  }

  if (new_focused_group_id.has_value()) {
    auto* const tab_strip_model = browser_window_interface_->GetTabStripModel();
    if (tab_strip_model->IsEphemeralTabGroup(*new_focused_group_id)) {
      user_ed->NotifyNewBadgeFeatureUsed(features::kNonGroupFocus);
    } else {
      user_ed->NotifyFeaturePromoFeatureUsed(
          feature_engagement::kIPHTabGroupsFocusingFeature,
          FeaturePromoFeatureUsedAction::kClosePromoIfPresent);
      user_ed->NotifyNewBadgeFeatureUsed(features::kTabGroupsFocusing);
    }
    user_ed->MaybeShowFeaturePromo(
        feature_engagement::kIPHTabGroupsUnfocusingFeature);
    MaybeShowNonGroupPromo();
  } else if (old_focused_group_id.has_value()) {
    user_ed->NotifyFeaturePromoFeatureUsed(
        feature_engagement::kIPHTabGroupsUnfocusingFeature,
        FeaturePromoFeatureUsedAction::kClosePromoIfPresent);
    user_ed->NotifyFeaturePromoFeatureUsed(
        feature_engagement::kIPHTabGroupsNonGroupFocusingFeature,
        FeaturePromoFeatureUsedAction::kClosePromoIfPresent);
  }
}

void TabGroupFocusIphController::MaybeScheduleGroupFocusPromo() {
  auto* const tab_strip_model = browser_window_interface_->GetTabStripModel();
  TabGroupModel* const group_model = tab_strip_model->group_model();
  CHECK(group_model);

  if (is_restoring_focused_group_ ||
      tab_strip_model->GetFocusedGroup().has_value() ||
      group_model->ListTabGroups().empty() ||
      tab_strip_model->count() <=
          features::kTabGroupsFocusingIphMinTabs.Get()) {
    return;
  }

  promo_timer_.Start(FROM_HERE, features::kTabGroupsFocusingIphPromoDelay.Get(),
                     this, &TabGroupFocusIphController::ShowGroupFocusPromo);
}

void TabGroupFocusIphController::ShowGroupFocusPromo() {
  if (auto* user_ed =
          BrowserUserEducationInterface::From(browser_window_interface_)) {
    user_ed->MaybeShowFeaturePromo(
        feature_engagement::kIPHTabGroupsFocusingFeature);
  }
}

void TabGroupFocusIphController::MaybeShowNonGroupPromo() {
  auto* const tab_strip_model = browser_window_interface_->GetTabStripModel();
  const std::optional<tab_groups::TabGroupId> focused_group =
      tab_strip_model->GetFocusedGroup();
  if (!focused_group.has_value() ||
      !tab_strip_model->IsEphemeralTabGroup(*focused_group)) {
    return;
  }

  TabGroupModel* const group_model = tab_strip_model->group_model();
  CHECK(group_model);
  CHECK(group_model->ContainsTabGroup(*focused_group));

  if (group_model->GetTabGroup(*focused_group)->tab_count() <
      features::kNonGroupFocusIphMinTabs.Get()) {
    return;
  }

  if (auto* const user_ed =
          BrowserUserEducationInterface::From(browser_window_interface_)) {
    user_ed->MaybeShowFeaturePromo(
        feature_engagement::kIPHTabGroupsNonGroupFocusingFeature);
  }
}
