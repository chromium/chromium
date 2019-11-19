// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/arc_playstore_shortcut_launcher_item_controller.h"

#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_launcher.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "components/arc/metrics/arc_metrics_constants.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/image/image_skia.h"

ArcPlaystoreShortcutLauncherItemController::
    ArcPlaystoreShortcutLauncherItemController()
    : AppShortcutLauncherItemController(ash::ShelfID(arc::kPlayStoreAppId)) {}

ArcPlaystoreShortcutLauncherItemController::
    ~ArcPlaystoreShortcutLauncherItemController() {}

void ArcPlaystoreShortcutLauncherItemController::ItemSelected(
    std::unique_ptr<ui::Event> event,
    int64_t display_id,
    ash::ShelfLaunchSource source,
    ItemSelectedCallback callback) {
  // Report |callback| now, once Play Store launch request may cause inline
  // replacement of this controller to deferred launch controller and |callback|
  // will never be delivered to ash.
  std::move(callback).Run(ash::SHELF_ACTION_NONE, {});
  if (!playstore_launcher_) {
    // Play Store launch request has never been scheduled.
    std::unique_ptr<ArcAppLauncher> playstore_launcher =
        std::make_unique<ArcAppLauncher>(
            ChromeLauncherController::instance()->profile(),
            arc::kPlayStoreAppId,
            base::Optional<std::string>() /* launch_intent */,
            true /* deferred_launch_allowed */, display_id,
            arc::UserInteractionType::APP_STARTED_FROM_SHELF);
    // ArcAppLauncher may launch Play Store in case it exists already. In this
    // case this instance of ArcPlaystoreShortcutLauncherItemController may be
    // deleted. If Play Store does not exist at this moment, then let
    // |playstore_launcher_| wait until it appears.
    if (!playstore_launcher->app_launched())
      playstore_launcher_ = std::move(playstore_launcher);
  }
}
