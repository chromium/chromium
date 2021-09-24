// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/recent_apps_interaction_handler.h"
#include "chromeos/components/phonehub/notification.h"

namespace chromeos {
namespace phonehub {

const size_t kMaxMostRecentApps = 5;

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
    const std::string& package_name,
    const std::u16string& visible_name) {
  for (auto& observer : observer_list_)
    observer.OnRecentAppClicked(package_name, visible_name);
}

void RecentAppsInteractionHandler::NotifyRecentAppAddedOrUpdated(
    const Notification::AppMetadata& app_metadata,
    base::Time last_accessed_timestamp) {
  // Each element of |recent_app_metadata_list_| has a unique |package_name|.
  for (auto it = recent_app_metadata_list_.begin();
       it != recent_app_metadata_list_.end(); ++it) {
    if (it->first.package_name == app_metadata.package_name) {
      recent_app_metadata_list_.erase(it);
      break;
    }
  }

  recent_app_metadata_list_.emplace_back(app_metadata, last_accessed_timestamp);
}

std::vector<Notification::AppMetadata>
RecentAppsInteractionHandler::FetchRecentAppMetadataList() {
  // Sort |recent_app_metadata_list_| from most recently visited to least
  // recently visited.
  std::sort(recent_app_metadata_list_.begin(), recent_app_metadata_list_.end(),
            [](const std::pair<Notification::AppMetadata, base::Time>& a,
               const std::pair<Notification::AppMetadata, base::Time>& b) {
              // More recently visited apps should come before earlier visited
              // apps.
              return a.second > b.second;
            });

  // At most |kMaxMostRecentApps| recent apps can be displayed.
  size_t num_recent_apps_to_display =
      std::min(recent_app_metadata_list_.size(), kMaxMostRecentApps);
  std::vector<Notification::AppMetadata> app_metadata_list;
  for (size_t i = 0; i < num_recent_apps_to_display; ++i) {
    app_metadata_list.push_back(recent_app_metadata_list_[i].first);
  }
  return app_metadata_list;
}

}  // namespace phonehub
}  // namespace chromeos
