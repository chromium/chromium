// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/recent_apps_interaction_handler.h"
#include "chromeos/ash/components/phonehub/notification.h"

namespace ash::phonehub {

RecentAppsInteractionHandler::RecentAppsInteractionHandler() = default;

RecentAppsInteractionHandler::~RecentAppsInteractionHandler() = default;

void RecentAppsInteractionHandler::AddRecentAppClickObserver(
    RecentAppClickObserver* observer) {
  recent_app_click_observer_list_.AddObserver(observer);
}

void RecentAppsInteractionHandler::RemoveRecentAppClickObserver(
    RecentAppClickObserver* observer) {
  recent_app_click_observer_list_.RemoveObserver(observer);
}

void RecentAppsInteractionHandler::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void RecentAppsInteractionHandler::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void RecentAppsInteractionHandler::NotifyRecentAppsViewUiStateUpdated() {
  for (auto& observer : observer_list_) {
    observer.OnRecentAppsUiStateUpdated();
  }
}

}  // namespace ash::phonehub
