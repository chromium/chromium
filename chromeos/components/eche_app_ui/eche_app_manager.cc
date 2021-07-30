// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/eche_app_ui/eche_app_manager.h"

#include "base/system/sys_info.h"
#include "chromeos/components/eche_app_ui/eche_signaler.h"
#include "chromeos/components/eche_app_ui/eche_uid_provider.h"
#include "chromeos/components/eche_app_ui/system_info.h"
#include "chromeos/components/eche_app_ui/system_info_provider.h"
#include "chromeos/components/phonehub/phone_hub_manager.h"
#include "chromeos/services/secure_channel/public/cpp/client/connection_manager_impl.h"

namespace chromeos {
namespace {
const char kSecureChannelFeatureName[] = "eche";
const char kMetricNameResult[] = "Eche.Connection.Result";
const char kMetricNameDuration[] = "Eche.Connection.Duration";
const char kMetricNameLatency[] = "Eche.Connectivity.Latency";
}  // namespace
namespace eche_app {

EcheAppManager::EcheAppManager(
    PrefService* pref_service,
    std::unique_ptr<SystemInfo> system_info,
    phonehub::PhoneHubManager* phone_hub_manager,
    device_sync::DeviceSyncClient* device_sync_client,
    multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
    secure_channel::SecureChannelClient* secure_channel_client,
    EcheNotificationClickHandler::LaunchEcheAppFunction
        launch_eche_app_function,
    EcheNotificationClickHandler::CloseEcheAppFunction close_eche_app_function,
    EcheRecentAppClickHandler::LaunchEcheAppFunction
        recent_app_launch_eche_app_function)
    : connection_manager_(
          std::make_unique<secure_channel::ConnectionManagerImpl>(
              multidevice_setup_client,
              device_sync_client,
              secure_channel_client,
              kSecureChannelFeatureName,
              kMetricNameResult,
              kMetricNameLatency,
              kMetricNameDuration)),
      feature_status_provider_(std::make_unique<EcheFeatureStatusProvider>(
          phone_hub_manager,
          device_sync_client,
          multidevice_setup_client,
          connection_manager_.get())),
      eche_notification_click_handler_(
          std::make_unique<EcheNotificationClickHandler>(
              phone_hub_manager,
              feature_status_provider_.get(),
              launch_eche_app_function,
              close_eche_app_function)),
      eche_connector_(
          std::make_unique<EcheConnector>(feature_status_provider_.get(),
                                          connection_manager_.get())),
      signaler_(std::make_unique<EcheSignaler>(eche_connector_.get(),
                                               connection_manager_.get())),
      system_info_provider_(
          std::make_unique<SystemInfoProvider>(std::move(system_info))),
      uid_(std::make_unique<EcheUidProvider>(pref_service)),
      eche_recent_app_click_handler_(
          std::make_unique<EcheRecentAppClickHandler>(
              phone_hub_manager,
              feature_status_provider_.get(),
              recent_app_launch_eche_app_function)) {}

EcheAppManager::~EcheAppManager() = default;

void EcheAppManager::BindSignalingMessageExchangerInterface(
    mojo::PendingReceiver<mojom::SignalingMessageExchanger> receiver) {
  signaler_->Bind(std::move(receiver));
}

void EcheAppManager::BindSystemInfoProviderInterface(
    mojo::PendingReceiver<mojom::SystemInfoProvider> receiver) {
  system_info_provider_->Bind(std::move(receiver));
}

void EcheAppManager::BindUidGeneratorInterface(
    mojo::PendingReceiver<mojom::UidGenerator> receiver) {
  uid_->Bind(std::move(receiver));
}

// NOTE: These should be destroyed in the opposite order of how these objects
// are initialized in the constructor.
void EcheAppManager::Shutdown() {
  eche_recent_app_click_handler_.reset();
  uid_.reset();
  system_info_provider_.reset();
  signaler_.reset();
  eche_connector_.reset();
  eche_notification_click_handler_.reset();
  feature_status_provider_.reset();
  connection_manager_.reset();
}

}  // namespace eche_app
}  // namespace chromeos
