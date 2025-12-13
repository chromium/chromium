
// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/shared_tab_group_feedback_controller.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/toolbar/pinned_action_toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_controller.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/views/view_class_properties.h"

namespace tab_groups {

SharedTabGroupFeedbackController::SharedTabGroupFeedbackController(
    BrowserWindowInterface* browser)
    : browser_(browser),
      tab_group_sync_service_(
          TabGroupSyncServiceFactory::GetForProfile(browser_->GetProfile())) {
  CHECK(tab_group_sync_service_);
  tab_group_sync_observer_.Observe(tab_group_sync_service_);
  browser_->GetTabStripModel()->AddObserver(this);

  active_tab_change_subscriptions_.push_back(
      browser_->RegisterActiveTabDidChange(
          base::BindRepeating(&SharedTabGroupFeedbackController::MaybeShowIPH,
                              base::Unretained(this))));
}

SharedTabGroupFeedbackController::~SharedTabGroupFeedbackController() = default;

void SharedTabGroupFeedbackController::Init() {
  MaybeShowFeedbackActionInToolbar();
}

void SharedTabGroupFeedbackController::TearDown() {
  browser_->GetTabStripModel()->RemoveObserver(this);
  browser_ = nullptr;
}

void SharedTabGroupFeedbackController::UpdateFeedbackButtonVisibility(
    bool should_show_button) {
  PinnedToolbarActionsController* controller =
      browser_->GetFeatures().pinned_toolbar_actions_controller();
  if (!controller) {
    // Can be null when dragging a tab / group into a new window.
    return;
  }

  if (should_show_button ==
      controller->IsActionPoppedOut(kActionSendSharedTabGroupFeedback)) {
    // Do nothing if the button is already in the correct state.
    return;
  }

  controller->ShowActionEphemerallyInToolbar(kActionSendSharedTabGroupFeedback,
                                             should_show_button);

  if (should_show_button) {
    PinnedActionToolbarButton* button =
        controller->GetButtonFor(kActionSendSharedTabGroupFeedback);
    CHECK(button);

    // Add the ElementIdentifier so the IPH system can find the button.
    button->SetProperty(views::kElementIdentifierKey,
                        kSharedTabGroupFeedbackElementId);
  }
}

void SharedTabGroupFeedbackController::MaybeShowFeedbackActionInToolbar() {
  // Show the feedback button if there are any shared tab groups open.
  bool should_show_button = false;
  std::vector<TabGroupId> local_group_ids =
      browser_->GetTabStripModel()->group_model()->ListTabGroups();
  for (const TabGroupId& local_group_id : local_group_ids) {
    std::optional<SavedTabGroup> saved_group =
        tab_group_sync_service_->GetGroup(local_group_id);
    if (saved_group && saved_group->is_shared_tab_group()) {
      should_show_button = true;
      break;
    }
  }

  UpdateFeedbackButtonVisibility(should_show_button);
}

void SharedTabGroupFeedbackController::MaybeShowIPH(
    BrowserWindowInterface* browser_window_interface) {
  tabs::TabInterface* tab_interface =
      browser_window_interface->GetActiveTabInterface();
  std::optional<TabGroupId> group_id = tab_interface->GetGroup();
  if (!group_id) {
    return;
  }

  std::optional<SavedTabGroup> saved_group =
      tab_group_sync_service_->GetGroup(group_id.value());
  if (!saved_group) {
    return;
  }

  if (!saved_group->is_shared_tab_group()) {
    return;
  }

  BrowserUserEducationInterface::From(browser_window_interface)
      ->MaybeShowFeaturePromo(
          feature_engagement::kIPHTabGroupsSharedTabFeedbackFeature);
}

void SharedTabGroupFeedbackController::OnInitialized() {
  Init();
}

void SharedTabGroupFeedbackController::OnTabGroupUpdated(
    const SavedTabGroup& group,
    TriggerSource source) {
  MaybeShowFeedbackActionInToolbar();
}

void SharedTabGroupFeedbackController::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  MaybeShowFeedbackActionInToolbar();
}

}  // namespace tab_groups
