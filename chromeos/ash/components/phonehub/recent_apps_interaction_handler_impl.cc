// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/recent_apps_interaction_handler_impl.h"

#include <memory>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/phonehub/notification.h"
#include "chromeos/ash/components/phonehub/pref_names.h"
#include "chromeos/ash/components/phonehub/proto/phonehub_api.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"

namespace ash::phonehub {

using multidevice_setup::mojom::Feature;
using multidevice_setup::mojom::FeatureState;
using multidevice_setup::mojom::HostStatus;
using HostStatusWithDevice =
    multidevice_setup::MultiDeviceSetupClient::HostStatusWithDevice;
using FeatureStatesMap =
    multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap;

const size_t kMaxMostRecentApps = 5;
const size_t kMaxSavedRecentApps = 10;

// static
void RecentAppsInteractionHandlerImpl::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kRecentAppsHistory);
}

RecentAppsInteractionHandlerImpl::RecentAppsInteractionHandlerImpl(
    PrefService* pref_service,
    multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
    MultideviceFeatureAccessManager* multidevice_feature_access_manager)
    : pref_service_(pref_service),
      multidevice_setup_client_(multidevice_setup_client),
      multidevice_feature_access_manager_(multidevice_feature_access_manager) {
  multidevice_setup_client_->AddObserver(this);
  multidevice_feature_access_manager_->AddObserver(this);
}

RecentAppsInteractionHandlerImpl::~RecentAppsInteractionHandlerImpl() {
  if (features::IsEcheNetworkConnectionStateEnabled() &&
      eche_connection_status_handler_) {
    eche_connection_status_handler_->RemoveObserver(this);
  }

  multidevice_setup_client_->RemoveObserver(this);
  multidevice_feature_access_manager_->RemoveObserver(this);
}

void RecentAppsInteractionHandlerImpl::AddRecentAppClickObserver(
    RecentAppClickObserver* observer) {
  observer_list_.AddObserver(observer);
}

void RecentAppsInteractionHandlerImpl::RemoveRecentAppClickObserver(
    RecentAppClickObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void RecentAppsInteractionHandlerImpl::NotifyRecentAppClicked(
    const Notification::AppMetadata& app_metadata,
    eche_app::mojom::AppStreamLaunchEntryPoint entrypoint) {
  for (auto& observer : observer_list_)
    observer.OnRecentAppClicked(app_metadata, entrypoint);
}

// Load the |recent_app_metadata_list_| from |pref_service_| if there is a
// history of |recent_app_metadata_list_| exist in |pref_service_|. Then add or
// update |app_metadata| into |recent_app_metadata_list_|. Also update
// this |app_metadata| back to |pref_service_|.
void RecentAppsInteractionHandlerImpl::NotifyRecentAppAddedOrUpdated(
    const Notification::AppMetadata& app_metadata,
    base::Time last_accessed_timestamp) {
  LoadRecentAppMetadataListFromPrefIfNeed();

  // Each element of |recent_app_metadata_list_| has a unique |package_name| and
  // |user_id|.
  for (auto it = recent_app_metadata_list_.begin();
       it != recent_app_metadata_list_.end(); ++it) {
    if (it->first.package_name == app_metadata.package_name &&
        it->first.user_id == app_metadata.user_id) {
      recent_app_metadata_list_.erase(it);
      break;
    }
  }

  recent_app_metadata_list_.emplace(recent_app_metadata_list_.begin(),
                                    app_metadata, last_accessed_timestamp);

  SaveRecentAppMetadataListToPref();
  ComputeAndUpdateUiState();
}

void RecentAppsInteractionHandlerImpl::SetConnectionStatusHandler(
    eche_app::EcheConnectionStatusHandler* eche_connection_status_handler) {
  if (!features::IsEcheNetworkConnectionStateEnabled()) {
    return;
  }

  if (eche_connection_status_handler_) {
    eche_connection_status_handler_->RemoveObserver(this);
  }

  eche_connection_status_handler_ = eche_connection_status_handler;

  if (eche_connection_status_handler_) {
    eche_connection_status_handler_->AddObserver(this);
  }
}

base::flat_set<int64_t>
RecentAppsInteractionHandlerImpl::GetUserIdsWithDisplayRecentApps() {
  base::flat_set<int64_t> user_ids;
  for (auto& user : user_states()) {
    if (user.is_enabled) {
      user_ids.emplace(user.user_id);
    }
  }
  // Skip filtering recent apps when not receiving user states.
  if (user_ids.empty()) {
    for (auto const& it : recent_app_metadata_list_) {
      if (!user_ids.contains(it.first.user_id)) {
        user_ids.emplace(it.first.user_id);
      }
    }
  }
  return user_ids;
}

std::vector<Notification::AppMetadata>
RecentAppsInteractionHandlerImpl::FetchRecentAppMetadataList() {
  LoadRecentAppMetadataListFromPrefIfNeed();

  base::flat_set<int64_t> active_user_ids = GetUserIdsWithDisplayRecentApps();
  std::vector<Notification::AppMetadata> app_metadata_list;

  for (auto const& it : recent_app_metadata_list_) {
    if (active_user_ids.contains(it.first.user_id)) {
      app_metadata_list.push_back(it.first);
      // At most |kMaxMostRecentApps| recent apps can be displayed.
      if (app_metadata_list.size() == kMaxMostRecentApps)
        break;
    }
  }
  return app_metadata_list;
}

void RecentAppsInteractionHandlerImpl::
    LoadRecentAppMetadataListFromPrefIfNeed() {
  if (!has_loaded_prefs_) {
    PA_LOG(INFO) << "LoadRecentAppMetadataListFromPref";
    const base::Value::List& recent_apps_history_pref =
        pref_service_->GetList(prefs::kRecentAppsHistory);
    for (const auto& value : recent_apps_history_pref) {
      DCHECK(value.is_dict());
      recent_app_metadata_list_.emplace_back(
          Notification::AppMetadata::FromValue(value.GetDict()),
          base::Time::FromSecondsSinceUnixEpoch(0));
    }
    has_loaded_prefs_ = true;
  }
}

void RecentAppsInteractionHandlerImpl::SaveRecentAppMetadataListToPref() {
  PA_LOG(INFO) << "SaveRecentAppMetadataListToPref";
  size_t num_recent_apps_to_save =
      std::min(recent_app_metadata_list_.size(), kMaxSavedRecentApps);
  base::Value::List app_metadata_value_list;
  for (size_t i = 0; i < num_recent_apps_to_save; ++i) {
    app_metadata_value_list.Append(
        recent_app_metadata_list_[i].first.ToValue());
  }
  pref_service_->SetList(prefs::kRecentAppsHistory,
                         std::move(app_metadata_value_list));
  has_loaded_prefs_ = true;
}

void RecentAppsInteractionHandlerImpl::OnFeatureStatesChanged(
    const FeatureStatesMap& feature_states_map) {
  ComputeAndUpdateUiState();
}

void RecentAppsInteractionHandlerImpl::OnHostStatusChanged(
    const HostStatusWithDevice& host_device_with_status) {
  if (host_device_with_status.first != HostStatus::kHostVerified) {
    PA_LOG(INFO) << "ClearRecentAppMetadataListAndPref";
    ClearRecentAppMetadataListAndPref();
  }
}

void RecentAppsInteractionHandlerImpl::OnNotificationAccessChanged() {
  ComputeAndUpdateUiState();
}

void RecentAppsInteractionHandlerImpl::OnAppsAccessChanged() {
  ComputeAndUpdateUiState();
}

void RecentAppsInteractionHandlerImpl::OnConnectionStatusForUiChanged(
    eche_app::mojom::ConnectionStatus connection_status) {
  if (features::IsEcheNetworkConnectionStateEnabled() &&
      connection_status_ != connection_status) {
    connection_status_ = connection_status;
    ComputeAndUpdateUiState();
  }
}

void RecentAppsInteractionHandlerImpl::SetStreamableApps(
    const std::vector<Notification::AppMetadata>& streamable_apps) {
  PA_LOG(INFO) << "ClearRecentAppMetadataListAndPref to update the list of "
               << streamable_apps.size() << " items.";
  ClearRecentAppMetadataListAndPref();

  // TODO(b/260015890): Save at most 6 apps.
  for (const auto& app : streamable_apps) {
    recent_app_metadata_list_.emplace_back(
        app, base::Time::FromSecondsSinceUnixEpoch(0));
  }

  SaveRecentAppMetadataListToPref();
  ComputeAndUpdateUiState();
}

void RecentAppsInteractionHandlerImpl::RemoveStreamableApp(
    const proto::App app_to_remove) {
  std::erase_if(
      recent_app_metadata_list_,
      [&app_to_remove](
          const std::pair<Notification::AppMetadata, base::Time>& app) {
        return app.first.package_name == app_to_remove.package_name();
      });

  SaveRecentAppMetadataListToPref();
  ComputeAndUpdateUiState();
}

void RecentAppsInteractionHandlerImpl::ComputeAndUpdateUiState() {
  ui_state_ = RecentAppsUiState::HIDDEN;

  LoadRecentAppMetadataListFromPrefIfNeed();

  // There are five cases we need to handle:
  // 1. If no recent app in list and necessary permission be granted, the
  // placeholder view will be shown.
  // 2. If some recent apps in list and streaming is allowed, the loading view
  // will show when determining if the connection can be bootstrapped.
  // 3. If some recent apps in list and streaming is allowed, the connection
  // error view will be shown.
  // 4. If some recent apps in list, streaming is allowed and the booststrap
  // connection was successful, then recent apps view will be shown.
  // 5. Otherwise, no recent apps view will be shown.
  bool allow_streaming = multidevice_setup_client_->GetFeatureState(
                             Feature::kEche) == FeatureState::kEnabledByUser;

  bool is_apps_access_required =
      features::IsEcheSWAEnabled() &&
      multidevice_feature_access_manager_->GetAppsAccessStatus() ==
          phonehub::MultideviceFeatureAccessManager::AccessStatus::
              kAvailableButNotGranted;

  if (!allow_streaming || is_apps_access_required) {
    NotifyRecentAppsViewUiStateUpdated();
    return;
  }

  if (features::IsEcheNetworkConnectionStateEnabled()) {
    ui_state_ = GetUiStateFromConnectionStatus();
    NotifyRecentAppsViewUiStateUpdated();
    return;
  }

  if (recent_app_metadata_list_.empty()) {
    bool notifications_enabled =
        multidevice_setup_client_->GetFeatureState(
            Feature::kPhoneHubNotifications) == FeatureState::kEnabledByUser;
    bool grant_notification_access_on_host =
        multidevice_feature_access_manager_->GetNotificationAccessStatus() ==
        phonehub::MultideviceFeatureAccessManager::AccessStatus::kAccessGranted;
    if (notifications_enabled && grant_notification_access_on_host) {
      ui_state_ = RecentAppsUiState::PLACEHOLDER_VIEW;
    }
  } else {
    ui_state_ = RecentAppsUiState::ITEMS_VISIBLE;
  }
  NotifyRecentAppsViewUiStateUpdated();
}

void RecentAppsInteractionHandlerImpl::ClearRecentAppMetadataListAndPref() {
  recent_app_metadata_list_.clear();
  pref_service_->ClearPref(prefs::kRecentAppsHistory);
  has_loaded_prefs_ = false;
}

RecentAppsInteractionHandler::RecentAppsUiState
RecentAppsInteractionHandlerImpl::GetUiStateFromConnectionStatus() {
  RecentAppsUiState ui_state = RecentAppsUiState::HIDDEN;
  switch (connection_status_) {
    case eche_app::mojom::ConnectionStatus::kConnectionStatusDisconnected:
      [[fallthrough]];
    case eche_app::mojom::ConnectionStatus::kConnectionStatusConnecting:
      ui_state = RecentAppsUiState::LOADING;
      break;
    case eche_app::mojom::ConnectionStatus::kConnectionStatusConnected:
      ui_state = RecentAppsUiState::ITEMS_VISIBLE;
      break;
    case eche_app::mojom::ConnectionStatus::kConnectionStatusFailed:
      ui_state = RecentAppsUiState::CONNECTION_FAILED;
      break;
  }
  return ui_state;
}

}  // namespace ash::phonehub
