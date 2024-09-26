// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/arc_shelf_spinner_item_controller.h"

#include "ash/components/arc/metrics/arc_metrics_constants.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/app_restore/app_restore_arc_task_handler.h"
#include "chrome/browser/ash/app_restore/app_restore_arc_task_handler_factory.h"
#include "chrome/browser/ash/app_restore/arc_app_queue_restore_handler.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/shelf/shelf_spinner_controller.h"
#include "components/app_restore/app_restore_utils.h"
#include "components/services/app_service/public/cpp/intent_util.h"

ArcShelfSpinnerItemController::ArcShelfSpinnerItemController(
    const std::string& arc_app_id,
    apps::IntentPtr intent,
    int event_flags,
    arc::UserInteractionType user_interaction_type,
    arc::mojom::WindowInfoPtr window_info)
    : ShelfSpinnerItemController(arc_app_id),
      intent_(std::move(intent)),
      event_flags_(event_flags),
      user_interaction_type_(user_interaction_type),
      request_time_(base::TimeTicks::Now()),
      window_info_(std::move(window_info)) {
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

void ArcShelfSpinnerItemController::ItemSelected(
    std::unique_ptr<ui::Event> event,
    int64_t display_id,
    ash::ShelfLaunchSource source,
    ItemSelectedCallback callback,
    const ItemFilterPredicate& filter_predicate) {
  if (window_info_ &&
      window_info_->window_id >
          app_restore::kArcSessionIdOffsetForRestoredLaunching) {
    ash::app_restore::AppRestoreArcTaskHandlerFactory::GetForProfile(
        observed_profile_)
        ->GetFullRestoreArcAppQueueRestoreHandler()
        ->LaunchApp(app_id());
    std::move(callback).Run(ash::SHELF_ACTION_NEW_WINDOW_CREATED, {});
    return;
  }

  std::move(callback).Run(ash::SHELF_ACTION_NONE, {});
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

  // If this item is created by full restore, we don't need to launch the app,
  // because the full restore component will launch the app when the app is
  // ready.
  if (IsCreatedByFullRestore())
    return;

  // Close() destroys this object, so start launching the app first.

  // Embed deferred time only for app launches. Don't modify shortcuts.
  // Shortcuts do not have activity so they are not compatible.
  if (!app_info.shortcut) {
    if (!intent_) {
      intent_ = std::make_unique<apps::Intent>(apps_util::kIntentActionMain);
      intent_->categories.push_back(arc::kCategoryLauncher);
      intent_->activity_name = app_info.activity;
    }
    intent_->extras[arc::kRequestDeferredStartTimeParamKey] =
        base::NumberToString(
            (request_time_ - base::TimeTicks()).InMilliseconds());
    arc::LaunchAppWithIntent(observed_profile_, arc_app_id, std::move(intent_),
                             event_flags_, user_interaction_type_,
                             std::move(window_info_));
  } else {
    arc::LaunchApp(observed_profile_, arc_app_id, event_flags_,
                   user_interaction_type_, std::move(window_info_));
  }
  Close();
}

void ArcShelfSpinnerItemController::OnAppRemoved(
    const std::string& removed_app_id) {
  Close();
}

void ArcShelfSpinnerItemController::OnAppConnectionReady() {
  // If this item is created by full restore, start a 1 minute timer to close
  // this item when timeout.
  if (IsCreatedByFullRestore() && !close_timer_) {
    close_timer_ = std::make_unique<base::OneShotTimer>();
    close_timer_->Start(FROM_HERE, ash::app_restore::kStopRestoreDelay,
                        base::BindOnce(&ArcShelfSpinnerItemController::Close,
                                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void ArcShelfSpinnerItemController::OnArcPlayStoreEnabledChanged(bool enabled) {
  if (enabled)
    return;
  // If ARC was disabled, remove the deferred launch request.
  Close();
}

bool ArcShelfSpinnerItemController::IsCreatedByFullRestore() {
  return window_info_ &&
         window_info_->window_id >
             app_restore::kArcSessionIdOffsetForRestoredLaunching;
}
