// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_ECHE_APP_UI_ECHE_PRESENCE_MANAGER_H_
#define CHROMEOS_COMPONENTS_ECHE_APP_UI_ECHE_PRESENCE_MANAGER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chromeos/components/eche_app_ui/eche_feature_status_provider.h"
#include "chromeos/components/eche_app_ui/eche_message_receiver.h"
#include "chromeos/components/eche_app_ui/feature_status_provider.h"

namespace chromeos {

namespace device_sync {

class DeviceSyncClient;

}  // namespace device_sync

namespace multidevice_setup {

class MultiDeviceSetupClient;

}  // namespace multidevice_setup

namespace secure_channel {

class PresenceMonitorClient;

}  // namespace secure_channel

namespace eche_app {

class EcheConnector;

// Control presence monitoring and the sending of keepalives.
class EchePresenceManager : public FeatureStatusProvider::Observer,
                            public EcheMessageReceiver::Observer {
 public:
  EchePresenceManager(
      EcheFeatureStatusProvider* eche_feature_status_provider,
      device_sync::DeviceSyncClient* device_sync_client,
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
      std::unique_ptr<secure_channel::PresenceMonitorClient>
          presence_monitor_client,
      EcheConnector* eche_connector,
      EcheMessageReceiver* eche_message_receiver);
  ~EchePresenceManager() override;

  EchePresenceManager(const EchePresenceManager&) = delete;
  EchePresenceManager& operator=(const EchePresenceManager&) = delete;

 private:
  // FeatureStatusProvider::Observer:
  void OnFeatureStatusChanged() override;
  // EcheMessageReceiver::Observer:
  void OnStatusChange(proto::StatusChangeType status_change_type) override;
  void OnSendAppsSetupResponseReceived(
      proto::SendAppsSetupResponse apps_setup_response) override {}
  void OnGetAppsAccessStateResponseReceived(
      proto::GetAppsAccessStateResponse apps_access_state_response) override {}

  void OnReady();
  void OnDeviceSeen();

  void UpdateMonitoringStatus();
  void StartMonitoring();
  void StopMonitoring();
  void OnTimerExpired();

  EcheFeatureStatusProvider* eche_feature_status_provider_;
  device_sync::DeviceSyncClient* device_sync_client_;
  multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client_;
  std::unique_ptr<secure_channel::PresenceMonitorClient>
      presence_monitor_client_;
  EcheConnector* eche_connector_;
  EcheMessageReceiver* eche_message_receiver_;
  base::RepeatingTimer timer_;

  bool stream_running_ = false;
  bool is_monitoring_ = false;
  base::TimeTicks device_last_seen_time_;

  base::WeakPtrFactory<EchePresenceManager> weak_ptr_factory_{this};
};

}  // namespace eche_app
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_ECHE_APP_UI_ECHE_PRESENCE_MANAGER_H_
