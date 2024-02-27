// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/fake_recent_apps_interaction_handler.h"

#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/time/time.h"
#include "chromeos/ash/components/phonehub/notification.h"

namespace ash::phonehub {

using FeatureState = multidevice_setup::mojom::FeatureState;

FakeRecentAppsInteractionHandler::FakeRecentAppsInteractionHandler() = default;

FakeRecentAppsInteractionHandler::~FakeRecentAppsInteractionHandler() = default;

void FakeRecentAppsInteractionHandler::NotifyRecentAppClicked(
    const Notification::AppMetadata& app_metadata,
    eche_app::mojom::AppStreamLaunchEntryPoint entrypoint) {
  if (base::Contains(package_name_to_click_count_, app_metadata.package_name)) {
    package_name_to_click_count_.at(app_metadata.package_name)++;
    return;
  }
  package_name_to_click_count_[app_metadata.package_name] = 1;
}

void FakeRecentAppsInteractionHandler::AddRecentAppClickObserver(
    RecentAppClickObserver* observer) {
  recent_app_click_observer_count_++;
}

void FakeRecentAppsInteractionHandler::RemoveRecentAppClickObserver(
    RecentAppClickObserver* observer) {
  recent_app_click_observer_count_--;
}

void FakeRecentAppsInteractionHandler::SetConnectionStatusHandler(
    eche_app::EcheConnectionStatusHandler* eche_connection_status_handler) {
  eche_connection_status_handler_count_++;
}

void FakeRecentAppsInteractionHandler::OnFeatureStateChanged(
    FeatureState feature_state) {
  feature_state_ = feature_state;
  ComputeAndUpdateUiState();
}

void FakeRecentAppsInteractionHandler::NotifyRecentAppAddedOrUpdated(
    const Notification::AppMetadata& app_metadata,
    base::Time last_accessed_timestamp) {
  recent_apps_metadata_.emplace(recent_apps_metadata_.begin(), app_metadata,
                                last_accessed_timestamp);
}

std::vector<Notification::AppMetadata>
FakeRecentAppsInteractionHandler::FetchRecentAppMetadataList() {
  std::vector<Notification::AppMetadata> app_metadata_list;
  for (const auto& recent_app_metadata : recent_apps_metadata_) {
    app_metadata_list.emplace_back(recent_app_metadata.first);
  }
  return app_metadata_list;
}

void FakeRecentAppsInteractionHandler::SetStreamableApps(
    const std::vector<Notification::AppMetadata>& streamable_apps) {
  recent_apps_metadata_.clear();
  for (const auto& app_metadata : streamable_apps) {
    recent_apps_metadata_.emplace_back(app_metadata, base::Time::UnixEpoch());
  }
}

void FakeRecentAppsInteractionHandler::RemoveStreamableApp(
    proto::App app_to_remove) {
  std::erase_if(
      recent_apps_metadata_,
      [&app_to_remove](
          const std::pair<Notification::AppMetadata, base::Time>& app) {
        return app.first.package_name == app_to_remove.package_name();
      });
}

void FakeRecentAppsInteractionHandler::ComputeAndUpdateUiState() {
  if (feature_state_ != FeatureState::kEnabledByUser) {
    ui_state_ = RecentAppsUiState::HIDDEN;
    return;
  }
  ui_state_ = recent_apps_metadata_.empty()
                  ? RecentAppsUiState::PLACEHOLDER_VIEW
                  : RecentAppsUiState::ITEMS_VISIBLE;
}

}  // namespace ash::phonehub
