// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_MULTIDEVICE_DEBUG_WEBUI_PROXIMITY_AUTH_WEBUI_HANDLER_H_
#define CHROMEOS_COMPONENTS_MULTIDEVICE_DEBUG_WEBUI_PROXIMITY_AUTH_WEBUI_HANDLER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/values.h"
#include "chromeos/components/multidevice/logging/log_buffer.h"
#include "chromeos/components/multidevice/remote_device_ref.h"
#include "chromeos/components/proximity_auth/messenger_observer.h"
#include "chromeos/components/proximity_auth/proximity_auth_client.h"
#include "chromeos/components/proximity_auth/remote_device_life_cycle.h"
#include "chromeos/services/device_sync/public/cpp/device_sync_client.h"
#include "chromeos/services/secure_channel/public/cpp/client/secure_channel_client.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class ListValue;
}

namespace proximity_auth {
struct RemoteStatusUpdate;
}  // namespace proximity_auth

namespace chromeos {

namespace multidevice {

// Handles messages from the chrome://proximity-auth page.
class ProximityAuthWebUIHandler
    : public content::WebUIMessageHandler,
      public multidevice::LogBuffer::Observer,
      public device_sync::DeviceSyncClient::Observer,
      public proximity_auth::RemoteDeviceLifeCycle::Observer,
      public proximity_auth::MessengerObserver {
 public:
  ProximityAuthWebUIHandler(
      device_sync::DeviceSyncClient* device_sync_client,
      secure_channel::SecureChannelClient* secure_channel_client);
  ~ProximityAuthWebUIHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

 private:
  // multidevice::LogBuffer::Observer:
  void OnLogMessageAdded(
      const multidevice::LogBuffer::LogMessage& log_message) override;
  void OnLogBufferCleared() override;

  // device_sync::DeviceSyncClient::Observer:
  void OnEnrollmentFinished() override;
  void OnNewDevicesSynced() override;

  // Message handler callbacks.
  void OnWebContentsInitialized(const base::ListValue* args);
  void GetLogMessages(const base::ListValue* args);
  void ClearLogBuffer(const base::ListValue* args);
  void ToggleUnlockKey(const base::ListValue* args);
  void FindEligibleUnlockDevices(const base::ListValue* args);
  void GetLocalState(const base::ListValue* args);
  void ForceEnrollment(const base::ListValue* args);
  void ForceDeviceSync(const base::ListValue* args);
  void ToggleConnection(const base::ListValue* args);

  void StartRemoteDeviceLifeCycle(multidevice::RemoteDeviceRef remote_device);
  void CleanUpRemoteDeviceLifeCycle();

  std::unique_ptr<base::DictionaryValue> RemoteDeviceToDictionary(
      const multidevice::RemoteDeviceRef& remote_device);

  // proximity_auth::RemoteDeviceLifeCycle::Observer:
  void OnLifeCycleStateChanged(
      proximity_auth::RemoteDeviceLifeCycle::State old_state,
      proximity_auth::RemoteDeviceLifeCycle::State new_state) override;

  // proximity_auth::MessengerObserver:
  void OnRemoteStatusUpdate(
      const proximity_auth::RemoteStatusUpdate& status_update) override;

  void OnForceEnrollmentNow(bool success);
  void OnForceSyncNow(bool success);
  void OnSetSoftwareFeatureState(
      const std::string public_key,
      device_sync::mojom::NetworkRequestResult result_code);
  void OnFindEligibleDevices(
      device_sync::mojom::NetworkRequestResult result_code,
      multidevice::RemoteDeviceRefList eligible_devices,
      multidevice::RemoteDeviceRefList ineligible_devices);
  void OnGetDebugInfo(device_sync::mojom::DebugInfoPtr debug_info_ptr);

  void NotifyOnEnrollmentFinished(
      bool success,
      std::unique_ptr<base::DictionaryValue> enrollment_state);
  void NotifyOnSyncFinished(
      bool was_sync_successful,
      bool changed,
      std::unique_ptr<base::DictionaryValue> device_sync_state);
  void NotifyGotLocalState(
      std::unique_ptr<base::Value> truncated_local_device_id,
      std::unique_ptr<base::DictionaryValue> enrollment_state,
      std::unique_ptr<base::DictionaryValue> device_sync_state,
      std::unique_ptr<base::ListValue> synced_devices);

  std::unique_ptr<base::Value> GetTruncatedLocalDeviceId();
  std::unique_ptr<base::ListValue> GetRemoteDevicesList();

  // The delegate used to fetch dependencies. Must outlive this instance.
  device_sync::DeviceSyncClient* device_sync_client_;
  secure_channel::SecureChannelClient* secure_channel_client_;

  // True if we get a message from the loaded WebContents to know that it is
  // initialized, and we can inject JavaScript.
  bool web_contents_initialized_;

  // Member variables for connecting to and authenticating the remote device.
  // TODO(tengs): Support multiple simultaenous connections.
  base::Optional<multidevice::RemoteDeviceRef> selected_remote_device_;
  std::unique_ptr<proximity_auth::RemoteDeviceLifeCycle> life_cycle_;
  std::unique_ptr<proximity_auth::RemoteStatusUpdate>
      last_remote_status_update_;

  bool enrollment_update_waiting_for_debug_info_ = false;
  bool sync_update_waiting_for_debug_info_ = false;
  bool get_local_state_update_waiting_for_debug_info_ = false;

  base::WeakPtrFactory<ProximityAuthWebUIHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ProximityAuthWebUIHandler);
};

}  // namespace multidevice

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_MULTIDEVICE_DEBUG_WEBUI_PROXIMITY_AUTH_WEBUI_HANDLER_H_
