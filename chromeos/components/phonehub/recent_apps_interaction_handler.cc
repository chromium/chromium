// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/recent_apps_interaction_handler.h"
#include "chromeos/components/phonehub/notification.h"

namespace chromeos {
namespace phonehub {

RecentAppsInteractionHandler::RecentAppsInteractionHandler() = default;

RecentAppsInteractionHandler::~RecentAppsInteractionHandler() = default;

void RecentAppsInteractionHandler::AddRecentAppClickObserver(
    RecentAppClickObserver* observer) {
  observer_list_.AddObserver(observer);
}

void RecentAppsInteractionHandler::RemoveRecentAppClickObserver(
    RecentAppClickObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void RecentAppsInteractionHandler::NotifyRecentAppClicked(
    const Notification::AppMetadata& app_metadata) {
  for (auto& observer : observer_list_)
    observer.OnRecentAppClicked(app_metadata);
}

}  // namespace phonehub
}  // namespace chromeos
