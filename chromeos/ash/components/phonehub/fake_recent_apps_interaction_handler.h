// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_FAKE_RECENT_APPS_INTERACTION_HANDLER_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_FAKE_RECENT_APPS_INTERACTION_HANDLER_H_

#include <stdint.h>

#include "ash/webui/eche_app_ui/eche_connection_status_handler.h"
#include "base/time/time.h"
#include "chromeos/ash/components/phonehub/notification.h"
#include "chromeos/ash/components/phonehub/recent_apps_interaction_handler.h"
#include "chromeos/ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"

namespace ash::phonehub {

class FakeRecentAppsInteractionHandler : public RecentAppsInteractionHandler {
 public:
  FakeRecentAppsInteractionHandler();
  FakeRecentAppsInteractionHandler(const FakeRecentAppsInteractionHandler&) =
      delete;
  FakeRecentAppsInteractionHandler* operator=(
      const FakeRecentAppsInteractionHandler&) = delete;
  ~FakeRecentAppsInteractionHandler() override;

  void OnFeatureStateChanged(
      multidevice_setup::mojom::FeatureState feature_state);

  size_t HandledRecentAppsCount(const std::string& package_name) const {
    return package_name_to_click_count_.at(package_name);
  }

  size_t recent_app_click_observer_count() const {
    return recent_app_click_observer_count_;
  }

  size_t eche_connection_status_handler_count() const {
    return eche_connection_status_handler_count_;
  }

  void NotifyRecentAppClicked(
      const Notification::AppMetadata& app_metadata,
      eche_app::mojom::AppStreamLaunchEntryPoint entrypoint) override;
  void AddRecentAppClickObserver(RecentAppClickObserver* observer) override;
  void RemoveRecentAppClickObserver(RecentAppClickObserver* observer) override;
  void SetConnectionStatusHandler(eche_app::EcheConnectionStatusHandler*
                                      eche_connection_status_handler) override;
  void NotifyRecentAppAddedOrUpdated(
      const Notification::AppMetadata& app_metadata,
      base::Time last_accessed_timestamp) override;
  std::vector<Notification::AppMetadata> FetchRecentAppMetadataList() override;
  void SetStreamableApps(
      const std::vector<Notification::AppMetadata>& streamable_apps) override;
  void RemoveStreamableApp(proto::App app_to_remove) override;

 private:
  void ComputeAndUpdateUiState();

  size_t recent_app_click_observer_count_ = 0;
  size_t eche_connection_status_handler_count_ = 0;
  multidevice_setup::mojom::FeatureState feature_state_ =
      multidevice_setup::mojom::FeatureState::kDisabledByUser;

  std::vector<std::pair<Notification::AppMetadata, base::Time>>
      recent_apps_metadata_;
  std::map<std::string, size_t> package_name_to_click_count_;
};

}  // namespace ash::phonehub

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_FAKE_RECENT_APPS_INTERACTION_HANDLER_H_
