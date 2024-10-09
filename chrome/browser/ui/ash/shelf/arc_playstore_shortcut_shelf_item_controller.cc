// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/arc_playstore_shortcut_shelf_item_controller.h"

#include <utility>

#include "ash/components/arc/app/arc_app_constants.h"
#include "ash/components/arc/metrics/arc_metrics_constants.h"
#include "ash/public/cpp/shelf_types.h"
#include "chrome/browser/ash/app_list/arc/arc_app_launcher.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph_factory.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/image/image_skia.h"

ArcPlaystoreShortcutShelfItemController::
    ArcPlaystoreShortcutShelfItemController()
    : AppShortcutShelfItemController(ash::ShelfID(arc::kPlayStoreAppId)) {}

ArcPlaystoreShortcutShelfItemController::
    ~ArcPlaystoreShortcutShelfItemController() = default;

void ArcPlaystoreShortcutShelfItemController::ItemSelected(
    std::unique_ptr<ui::Event> event,
    int64_t display_id,
    ash::ShelfLaunchSource source,
    ItemSelectedCallback callback,
    const ItemFilterPredicate& filter_predicate) {
  Profile* profile = ChromeShelfController::instance()->profile();

  // Launches from app list are covered in `AppListClientImpl::ActivateItem`.
  if (source == ash::ShelfLaunchSource::LAUNCH_FROM_SHELF) {
    scalable_iph::ScalableIph* scalable_iph =
        ScalableIphFactory::GetForBrowserContext(profile);
    if (scalable_iph) {
      scalable_iph->RecordEvent(
          scalable_iph::ScalableIph::Event::kShelfItemActivationGooglePlay);
    }
  }

  // Report |callback| now, once Play Store launch request may cause inline
  // replacement of this controller to deferred launch controller and |callback|
  // will never be delivered to ash.
  std::move(callback).Run(ash::SHELF_ACTION_NONE, {});
  if (!playstore_launcher_) {
    // Play Store launch request has never been scheduled.
    std::unique_ptr<ArcAppLauncher> playstore_launcher =
        std::make_unique<ArcAppLauncher>(
            profile, arc::kPlayStoreAppId, nullptr /* launch_intent */,
            true /* deferred_launch_allowed */, display_id,
            apps::LaunchSource::kFromShelf);
    // ArcAppLauncher may launch Play Store in case it exists already. In this
    // case this instance of ArcPlaystoreShortcutShelfItemController may be
    // deleted. If Play Store does not exist at this moment, then let
    // |playstore_launcher_| wait until it appears.
    if (!playstore_launcher->app_launched())
      playstore_launcher_ = std::move(playstore_launcher);
  }
}
