// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_RECENT_APPS_INTERACTION_HANDLER_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_RECENT_APPS_INTERACTION_HANDLER_IMPL_H_

#include <stdint.h>
#include <memory>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
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

namespace ash::phonehub {

// The handler that exposes APIs to interact with Phone Hub Recent Apps.
class RecentAppsInteractionHandlerImpl
    : public RecentAppsInteractionHandler,
      public multidevice_setup::MultiDeviceSetupClient::Observer,
      public MultideviceFeatureAccessManager::Observer,
      public eche_app::EcheConnectionStatusHandler::Observer {
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
  void SetConnectionStatusHandler(eche_app::EcheConnectionStatusHandler*
                                      eche_connection_status_handler) override;

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

  // eche_app::EcheConnectionStatusHandler::Observer:
  void OnConnectionStatusForUiChanged(
      eche_app::mojom::ConnectionStatus connection_status) override;

  void SetStreamableApps(
      const std::vector<Notification::AppMetadata>& streamable_apps) override;

  void RemoveStreamableApp(const proto::App streamable_app) override;

  std::vector<std::pair<Notification::AppMetadata, base::Time>>*
  recent_app_metadata_list_for_testing() {
    return &recent_app_metadata_list_;
  }

  eche_app::mojom::ConnectionStatus connection_status_for_testing() {
    return connection_status_;
  }

  void set_connection_status_for_testing(
      eche_app::mojom::ConnectionStatus connection_status) {
    connection_status_ = connection_status;
  }

 private:
  friend class RecentAppsInteractionHandlerTest;

  void LoadRecentAppMetadataListFromPrefIfNeed();
  void SaveRecentAppMetadataListToPref();
  void ComputeAndUpdateUiState();
  void ClearRecentAppMetadataListAndPref();
  RecentAppsUiState GetUiStateFromConnectionStatus();
  base::flat_set<int64_t> GetUserIdsWithDisplayRecentApps();

  // Whether this class has finished loading |recent_app_metadata_list_| from
  // pref.
  bool has_loaded_prefs_ = false;

  eche_app::mojom::ConnectionStatus connection_status_ =
      eche_app::mojom::ConnectionStatus::kConnectionStatusDisconnected;
  base::ObserverList<RecentAppClickObserver> observer_list_;
  std::vector<std::pair<Notification::AppMetadata, base::Time>>
      recent_app_metadata_list_;
  raw_ptr<PrefService> pref_service_;
  raw_ptr<multidevice_setup::MultiDeviceSetupClient> multidevice_setup_client_;
  raw_ptr<MultideviceFeatureAccessManager> multidevice_feature_access_manager_;
  raw_ptr<eche_app::EcheConnectionStatusHandler>
      eche_connection_status_handler_ = nullptr;

  base::WeakPtrFactory<RecentAppsInteractionHandlerImpl> weak_ptr_factory_{
      this};
};

}  // namespace ash::phonehub

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_RECENT_APPS_INTERACTION_HANDLER_IMPL_H_
