// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/chrome_shelf_metrics_provider.h"

#include "ash/public/cpp/shelf_item.h"
#include "ash/public/cpp/shelf_model.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/apps/app_service/metrics/app_service_metrics.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"

ChromeShelfMetricsProvider::ChromeShelfMetricsProvider() = default;

ChromeShelfMetricsProvider::~ChromeShelfMetricsProvider() = default;

void ChromeShelfMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension*) {
  const ChromeShelfController* controller = ChromeShelfController::instance();
  if (!controller) {
    return;
  }

  // The `ChromeShelfController` is a singleton but it's attached profile may
  // change at runtime in a multi-profile setting. Only record metrics if the
  // `controller` has attached to the active profile and is not in a state of
  // transition. Note that this will become obsolete with Lacros.
  const Profile* profile = ProfileManager::GetActiveUserProfile();
  if (!profile || controller->profile() != profile) {
    return;
  }

  ash::ShelfModel* model = controller->shelf_model();
  if (!model) {
    return;
  }

  for (const ash::ShelfItem& item : model->items()) {
    if (item.type == ash::TYPE_PINNED_APP ||
        item.type == ash::TYPE_BROWSER_SHORTCUT) {
      if (const std::optional<apps::DefaultAppName> default_app_name =
              apps::AppIdToName(item.id.app_id)) {
        base::UmaHistogramEnumeration("Ash.Shelf.DefaultApps.Pinned",
                                      default_app_name.value());
      }
    }
  }
}
