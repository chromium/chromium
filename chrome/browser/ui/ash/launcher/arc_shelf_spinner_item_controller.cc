// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/arc_shelf_spinner_item_controller.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/launcher/shelf_spinner_controller.h"
#include "components/arc/metrics/arc_metrics_constants.h"

ArcShelfSpinnerItemController::ArcShelfSpinnerItemController(
    const std::string& arc_app_id,
    int event_flags,
    arc::UserInteractionType user_interaction_type,
    int64_t display_id)
    : ShelfSpinnerItemController(arc_app_id),
      event_flags_(event_flags),
      user_interaction_type_(user_interaction_type),
      display_id_(display_id) {
  arc::ArcSessionManager* arc_session_manager = arc::ArcSessionManager::Get();
  // arc::ArcSessionManager might not be set in tests.
  if (arc_session_manager)
    arc_session_manager->AddObserver(this);
}

ArcShelfSpinnerItemController::~ArcShelfSpinnerItemController() {
  if (observed_profile_)
    ArcAppListPrefs::Get(observed_profile_)->RemoveObserver(this);
  arc::ArcSessionManager* arc_session_manager = arc::ArcSessionManager::Get();
  // arc::ArcSessionManager may be released first.
  if (arc_session_manager)
    arc_session_manager->RemoveObserver(this);
}

void ArcShelfSpinnerItemController::SetHost(
    const base::WeakPtr<ShelfSpinnerController>& controller) {
  DCHECK(!observed_profile_);
  observed_profile_ = controller->OwnerProfile();
  ArcAppListPrefs::Get(observed_profile_)->AddObserver(this);

  ShelfSpinnerItemController::SetHost(controller);
}

void ArcShelfSpinnerItemController::OnAppStatesChanged(
    const std::string& arc_app_id,
    const ArcAppListPrefs::AppInfo& app_info) {
  if (app_id() != arc_app_id)
    return;

  // App was suspended. Launch is no longer available, close controller.
  if (app_info.suspended) {
    Close();
    return;
  }

  if (!app_info.ready)
    return;

  // Close() destroys this object, so start launching the app first.
  arc::LaunchApp(observed_profile_, arc_app_id, event_flags_,
                 user_interaction_type_, display_id_);
  Close();
}

void ArcShelfSpinnerItemController::OnAppRemoved(
    const std::string& removed_app_id) {
  Close();
}

void ArcShelfSpinnerItemController::OnArcPlayStoreEnabledChanged(bool enabled) {
  if (enabled)
    return;
  // If ARC was disabled, remove the deferred launch request.
  Close();
}
