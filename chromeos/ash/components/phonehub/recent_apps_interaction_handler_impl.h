// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_RECENT_APPS_INTERACTION_HANDLER_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_RECENT_APPS_INTERACTION_HANDLER_IMPL_H_

#include <stdint.h>
#include <memory>

#include "base/gtest_prod_util.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "chromeos/ash/components/phonehub/multidevice_feature_access_manager.h"
#include "chromeos/ash/components/phonehub/notification.h"
#include "chromeos/ash/components/phonehub/proto/phonehub_api.pb.h"
#include "chromeos/ash/components/phonehub/recent_app_click_observer.h"
#include "chromeos/ash/components/phonehub/recent_apps_interaction_handler.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/multidevice_setup_client.h"

class PrefRegistrySimple;
class PrefService;

namespace ash {
namespace phonehub {

// The handler that exposes APIs to interact with Phone Hub Recent Apps.
class RecentAppsInteractionHandlerImpl
    : public RecentAppsInteractionHandler,
      public multidevice_setup::MultiDeviceSetupClient::Observer,
      public MultideviceFeatureAccessManager::Observer {
 public:
  static void RegisterPrefs(PrefRegistrySimple* registry);

  explicit RecentAppsInteractionHandlerImpl(
      PrefService* pref_service,
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
      MultideviceFeatureAccessManager* multidevice_feature_access_manager);
  ~RecentAppsInteractionHandlerImpl() override;

  // RecentAppsInteractionHandler:
  void NotifyRecentAppClicked(
      const Notification::AppMetadata& app_metadata,
      ash::eche_app::mojom::AppStreamLaunchEntryPoint entrypoint) override;
  void AddRecentAppClickObserver(RecentAppClickObserver* observer) override;
  void RemoveRecentAppClickObserver(RecentAppClickObserver* observer) override;
  void NotifyRecentAppAddedOrUpdated(
      const Notification::AppMetadata& app_metadata,
      base::Time last_accessed_timestamp) override;
  std::vector<Notification::AppMetadata> FetchRecentAppMetadataList() override;

  // MultiDeviceSetupClient::Observer:
  void OnFeatureStatesChanged(
      const multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
          feature_states_map) override;
  void OnHostStatusChanged(
      const multidevice_setup::MultiDeviceSetupClient::HostStatusWithDevice&
          host_device_with_status) override;

  // MultideviceFeatureAccessManager::Observer:
  void OnNotificationAccessChanged() override;
  void OnAppsAccessChanged() override;

  void SetStreamableApps(
      const std::vector<Notification::AppMetadata>& streamable_apps) override;

  std::vector<std::pair<Notification::AppMetadata, base::Time>>*
  recent_app_metadata_list_for_testing() {
    return &recent_app_metadata_list_;
  }

 private:
  friend class RecentAppsInteractionHandlerTest;

  void LoadRecentAppMetadataListFromPrefIfNeed();
  void SaveRecentAppMetadataListToPref();
  void ComputeAndUpdateUiState();
  void ClearRecentAppMetadataListAndPref();
  base::flat_set<int64_t> GetUserIdsWithDisplayRecentApps();

  // Whether this class has finished loading |recent_app_metadata_list_| from
  // pref.
  bool has_loaded_prefs_ = false;

  base::ObserverList<RecentAppClickObserver> observer_list_;
  std::vector<std::pair<Notification::AppMetadata, base::Time>>
      recent_app_metadata_list_;
  PrefService* pref_service_;
  multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client_;
  MultideviceFeatureAccessManager* multidevice_feature_access_manager_;

  base::WeakPtrFactory<RecentAppsInteractionHandlerImpl> weak_ptr_factory_{
      this};
};

}  // namespace phonehub
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_RECENT_APPS_INTERACTION_HANDLER_IMPL_H_
