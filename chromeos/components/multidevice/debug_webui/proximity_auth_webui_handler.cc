// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/multidevice/debug_webui/proximity_auth_webui_handler.h"

#include <algorithm>
#include <memory>
#include <sstream>
#include <utility>

#include "base/base64url.h"
#include "base/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "base/values.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/components/multidevice/software_feature_state.h"
#include "chromeos/components/proximity_auth/messenger.h"
#include "chromeos/components/proximity_auth/remote_device_life_cycle_impl.h"
#include "chromeos/components/proximity_auth/remote_status_update.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/services/device_sync/proto/enum_util.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

namespace chromeos {

namespace multidevice {

namespace {

constexpr const multidevice::SoftwareFeature kAllSoftareFeatures[] = {
    multidevice::SoftwareFeature::kBetterTogetherHost,
    multidevice::SoftwareFeature::kBetterTogetherClient,
    multidevice::SoftwareFeature::kSmartLockHost,
    multidevice::SoftwareFeature::kSmartLockClient,
    multidevice::SoftwareFeature::kInstantTetheringHost,
    multidevice::SoftwareFeature::kInstantTetheringClient,
    multidevice::SoftwareFeature::kMessagesForWebHost,
    multidevice::SoftwareFeature::kMessagesForWebClient};

// Keys in the JSON representation of a log message.
const char kLogMessageTextKey[] = "text";
const char kLogMessageTimeKey[] = "time";
const char kLogMessageFileKey[] = "file";
const char kLogMessageLineKey[] = "line";
const char kLogMessageSeverityKey[] = "severity";

// Keys in the JSON representation of a SyncState object for enrollment or
// device sync.
const char kSyncStateLastSuccessTime[] = "lastSuccessTime";
const char kSyncStateNextRefreshTime[] = "nextRefreshTime";
const char kSyncStateRecoveringFromFailure[] = "recoveringFromFailure";
const char kSyncStateOperationInProgress[] = "operationInProgress";

// Converts |log_message| to a raw dictionary value used as a JSON argument to
// JavaScript functions.
std::unique_ptr<base::DictionaryValue> LogMessageToDictionary(
    const multidevice::LogBuffer::LogMessage& log_message) {
  std::unique_ptr<base::DictionaryValue> dictionary(
      new base::DictionaryValue());
  dictionary->SetString(kLogMessageTextKey, log_message.text);
  dictionary->SetString(
      kLogMessageTimeKey,
      base::TimeFormatTimeOfDayWithMilliseconds(log_message.time));
  dictionary->SetString(kLogMessageFileKey, log_message.file);
  dictionary->SetInteger(kLogMessageLineKey, log_message.line);
  dictionary->SetInteger(kLogMessageSeverityKey,
                         static_cast<int>(log_message.severity));
  return dictionary;
}

// Keys in the JSON representation of an ExternalDeviceInfo proto.
const char kExternalDevicePublicKey[] = "publicKey";
const char kExternalDevicePublicKeyTruncated[] = "publicKeyTruncated";
const char kExternalDeviceFriendlyName[] = "friendlyDeviceName";
const char kExternalDeviceNoPiiName[] = "noPiiName";
const char kExternalDeviceUnlockKey[] = "unlockKey";
const char kExternalDeviceMobileHotspot[] = "hasMobileHotspot";
const char kExternalDeviceConnectionStatus[] = "connectionStatus";
const char kExternalDeviceFeatureStates[] = "featureStates";
const char kExternalDeviceRemoteState[] = "remoteState";

// The possible values of the |kExternalDeviceConnectionStatus| field.
const char kExternalDeviceConnected[] = "connected";
const char kExternalDeviceDisconnected[] = "disconnected";
const char kExternalDeviceConnecting[] = "connecting";

// Creates a SyncState JSON object that can be passed to the WebUI.
std::unique_ptr<base::DictionaryValue> CreateSyncStateDictionary(
    double last_success_time,
    double next_refresh_time,
    bool is_recovering_from_failure,
    bool is_enrollment_in_progress) {
  std::unique_ptr<base::DictionaryValue> sync_state(
      new base::DictionaryValue());
  sync_state->SetDouble(kSyncStateLastSuccessTime, last_success_time);
  sync_state->SetDouble(kSyncStateNextRefreshTime, next_refresh_time);
  sync_state->SetBoolean(kSyncStateRecoveringFromFailure,
                         is_recovering_from_failure);
  sync_state->SetBoolean(kSyncStateOperationInProgress,
                         is_enrollment_in_progress);
  return sync_state;
}

std::string GenerateFeaturesString(const multidevice::RemoteDeviceRef& device) {
  std::stringstream ss;
  ss << "{";

  bool logged_feature = false;
  for (const auto& software_feature : kAllSoftareFeatures) {
    multidevice::SoftwareFeatureState state =
        device.GetSoftwareFeatureState(software_feature);

    // Only log features with values.
    if (state == multidevice::SoftwareFeatureState::kNotSupported)
      continue;

    logged_feature = true;
    ss << software_feature << ": " << state << ", ";
  }

  if (logged_feature)
    ss.seekp(-2, ss.cur);  // Remove last ", " from the stream.

  ss << "}";
  return ss.str();
}

}  // namespace

ProximityAuthWebUIHandler::ProximityAuthWebUIHandler(
    device_sync::DeviceSyncClient* device_sync_client,
    secure_channel::SecureChannelClient* secure_channel_client)
    : device_sync_client_(device_sync_client),
      secure_channel_client_(secure_channel_client),
      web_contents_initialized_(false) {}

ProximityAuthWebUIHandler::~ProximityAuthWebUIHandler() {
  multidevice::LogBuffer::GetInstance()->RemoveObserver(this);

  device_sync_client_->RemoveObserver(this);
}

void ProximityAuthWebUIHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "onWebContentsInitialized",
      base::BindRepeating(&ProximityAuthWebUIHandler::OnWebContentsInitialized,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "clearLogBuffer",
      base::BindRepeating(&ProximityAuthWebUIHandler::ClearLogBuffer,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "getLogMessages",
      base::BindRepeating(&ProximityAuthWebUIHandler::GetLogMessages,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "toggleUnlockKey",
      base::BindRepeating(&ProximityAuthWebUIHandler::ToggleUnlockKey,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "findEligibleUnlockDevices",
      base::BindRepeating(&ProximityAuthWebUIHandler::FindEligibleUnlockDevices,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "getLocalState",
      base::BindRepeating(&ProximityAuthWebUIHandler::GetLocalState,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "forceEnrollment",
      base::BindRepeating(&ProximityAuthWebUIHandler::ForceEnrollment,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "forceDeviceSync",
      base::BindRepeating(&ProximityAuthWebUIHandler::ForceDeviceSync,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "toggleConnection",
      base::BindRepeating(&ProximityAuthWebUIHandler::ToggleConnection,
                          base::Unretained(this)));
}

void ProximityAuthWebUIHandler::OnLogMessageAdded(
    const multidevice::LogBuffer::LogMessage& log_message) {
  std::unique_ptr<base::DictionaryValue> dictionary =
      LogMessageToDictionary(log_message);
  web_ui()->CallJavascriptFunctionUnsafe("LogBufferInterface.onLogMessageAdded",
                                         *dictionary);
}

void ProximityAuthWebUIHandler::OnLogBufferCleared() {
  web_ui()->CallJavascriptFunctionUnsafe(
      "LogBufferInterface.onLogBufferCleared");
}

void ProximityAuthWebUIHandler::OnEnrollmentFinished() {
  // OnGetDebugInfo() will call NotifyOnEnrollmentFinished() with the enrollment
  // state info.
  enrollment_update_waiting_for_debug_info_ = true;
  device_sync_client_->GetDebugInfo(
      base::BindOnce(&ProximityAuthWebUIHandler::OnGetDebugInfo,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ProximityAuthWebUIHandler::OnNewDevicesSynced() {
  // OnGetDebugInfo() will call NotifyOnSyncFinished() with the device sync
  // state info.
  sync_update_waiting_for_debug_info_ = true;
  device_sync_client_->GetDebugInfo(
      base::BindOnce(&ProximityAuthWebUIHandler::OnGetDebugInfo,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ProximityAuthWebUIHandler::OnWebContentsInitialized(
    const base::ListValue* args) {
  if (!web_contents_initialized_) {
    device_sync_client_->AddObserver(this);
    multidevice::LogBuffer::GetInstance()->AddObserver(this);
    web_contents_initialized_ = true;
  }
}

void ProximityAuthWebUIHandler::GetLogMessages(const base::ListValue* args) {
  base::ListValue json_logs;
  for (const auto& log : *multidevice::LogBuffer::GetInstance()->logs()) {
    json_logs.Append(LogMessageToDictionary(log));
  }
  web_ui()->CallJavascriptFunctionUnsafe("LogBufferInterface.onGotLogMessages",
                                         json_logs);
}

void ProximityAuthWebUIHandler::ClearLogBuffer(const base::ListValue* args) {
  // The OnLogBufferCleared() observer function will be called after the buffer
  // is cleared.
  multidevice::LogBuffer::GetInstance()->Clear();
}

void ProximityAuthWebUIHandler::ToggleUnlockKey(const base::ListValue* args) {
  std::string public_key_b64, public_key;
  bool make_unlock_key;
  if (args->GetSize() != 2 || !args->GetString(0, &public_key_b64) ||
      !args->GetBoolean(1, &make_unlock_key) ||
      !base::Base64UrlDecode(public_key_b64,
                             base::Base64UrlDecodePolicy::REQUIRE_PADDING,
                             &public_key)) {
    PA_LOG(ERROR) << "Invalid arguments to toggleUnlockKey";
    return;
  }

  device_sync_client_->SetSoftwareFeatureState(
      public_key, multidevice::SoftwareFeature::kSmartLockHost,
      true /* enabled */, true /* is_exclusive */,
      base::BindOnce(&ProximityAuthWebUIHandler::OnSetSoftwareFeatureState,
                     weak_ptr_factory_.GetWeakPtr(), public_key));
}

void ProximityAuthWebUIHandler::FindEligibleUnlockDevices(
    const base::ListValue* args) {
  device_sync_client_->FindEligibleDevices(
      multidevice::SoftwareFeature::kSmartLockHost,
      base::BindOnce(&ProximityAuthWebUIHandler::OnFindEligibleDevices,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ProximityAuthWebUIHandler::ForceEnrollment(const base::ListValue* args) {
  device_sync_client_->ForceEnrollmentNow(
      base::BindOnce(&ProximityAuthWebUIHandler::OnForceEnrollmentNow,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ProximityAuthWebUIHandler::ForceDeviceSync(const base::ListValue* args) {
  device_sync_client_->ForceSyncNow(
      base::BindOnce(&ProximityAuthWebUIHandler::OnForceSyncNow,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ProximityAuthWebUIHandler::ToggleConnection(const base::ListValue* args) {
  std::string b64_public_key;
  std::string public_key;
  if (!args->GetSize() || !args->GetString(0, &b64_public_key) ||
      !base::Base64UrlDecode(b64_public_key,
                             base::Base64UrlDecodePolicy::REQUIRE_PADDING,
                             &public_key)) {
    PA_LOG(ERROR) << "Unlock key (" << b64_public_key << ") not found";
    return;
  }

  std::string selected_device_public_key;
  if (selected_remote_device_)
    selected_device_public_key = selected_remote_device_->public_key();

  for (const auto& remote_device : device_sync_client_->GetSyncedDevices()) {
    if (remote_device.public_key() != public_key)
      continue;

    if (life_cycle_ && selected_device_public_key == public_key) {
      CleanUpRemoteDeviceLifeCycle();
      return;
    }

    StartRemoteDeviceLifeCycle(remote_device);
  }
}

void ProximityAuthWebUIHandler::GetLocalState(const base::ListValue* args) {
  // OnGetDebugInfo() will call NotifyGotLocalState() with the enrollment and
  // device sync state info.
  get_local_state_update_waiting_for_debug_info_ = true;
  device_sync_client_->GetDebugInfo(
      base::BindOnce(&ProximityAuthWebUIHandler::OnGetDebugInfo,
                     weak_ptr_factory_.GetWeakPtr()));
}

std::unique_ptr<base::Value>
ProximityAuthWebUIHandler::GetTruncatedLocalDeviceId() {
  base::Optional<multidevice::RemoteDeviceRef> local_device_metadata =
      device_sync_client_->GetLocalDeviceMetadata();

  std::string device_id =
      local_device_metadata
          ? local_device_metadata->GetTruncatedDeviceIdForLogs()
          : "Missing Device ID";

  return std::make_unique<base::Value>(device_id);
}

std::unique_ptr<base::ListValue>
ProximityAuthWebUIHandler::GetRemoteDevicesList() {
  std::unique_ptr<base::ListValue> devices_list_value(new base::ListValue());

  for (const auto& remote_device : device_sync_client_->GetSyncedDevices())
    devices_list_value->Append(RemoteDeviceToDictionary(remote_device));

  return devices_list_value;
}

void ProximityAuthWebUIHandler::StartRemoteDeviceLifeCycle(
    multidevice::RemoteDeviceRef remote_device) {
  base::Optional<multidevice::RemoteDeviceRef> local_device;
  local_device = device_sync_client_->GetLocalDeviceMetadata();

  selected_remote_device_ = remote_device;
  life_cycle_.reset(new proximity_auth::RemoteDeviceLifeCycleImpl(
      *selected_remote_device_, local_device, secure_channel_client_));
  life_cycle_->AddObserver(this);
  life_cycle_->Start();
}

void ProximityAuthWebUIHandler::CleanUpRemoteDeviceLifeCycle() {
  if (selected_remote_device_) {
    PA_LOG(VERBOSE) << "Cleaning up connection to "
                    << selected_remote_device_->name();
  }
  life_cycle_.reset();
  selected_remote_device_ = base::nullopt;
  last_remote_status_update_.reset();
  web_ui()->CallJavascriptFunctionUnsafe(
      "LocalStateInterface.onRemoteDevicesChanged", *GetRemoteDevicesList());
}

std::unique_ptr<base::DictionaryValue>
ProximityAuthWebUIHandler::RemoteDeviceToDictionary(
    const multidevice::RemoteDeviceRef& remote_device) {
  // Set the fields in the ExternalDeviceInfo proto.
  std::unique_ptr<base::DictionaryValue> dictionary(
      new base::DictionaryValue());
  dictionary->SetString(kExternalDevicePublicKey, remote_device.GetDeviceId());
  dictionary->SetString(kExternalDevicePublicKeyTruncated,
                        remote_device.GetTruncatedDeviceIdForLogs());
  dictionary->SetString(kExternalDeviceFriendlyName, remote_device.name());
  dictionary->SetString(kExternalDeviceNoPiiName,
                        remote_device.pii_free_name());
  dictionary->SetBoolean(kExternalDeviceUnlockKey,
                         remote_device.GetSoftwareFeatureState(
                             multidevice::SoftwareFeature::kSmartLockHost) ==
                             multidevice::SoftwareFeatureState::kEnabled);
  dictionary->SetBoolean(
      kExternalDeviceMobileHotspot,
      remote_device.GetSoftwareFeatureState(
          multidevice::SoftwareFeature::kInstantTetheringHost) ==
          multidevice::SoftwareFeatureState::kSupported);
  dictionary->SetString(kExternalDeviceConnectionStatus,
                        kExternalDeviceDisconnected);
  dictionary->SetString(kExternalDeviceFeatureStates,
                        GenerateFeaturesString(remote_device));

  // TODO(crbug.com/852836): Add kExternalDeviceIsArcPlusPlusEnrollment and
  // kExternalDeviceIsPixelPhone values to the dictionary once RemoteDevice
  // carries those relevant fields.

  std::string selected_device_public_key;
  if (selected_remote_device_)
    selected_device_public_key = selected_remote_device_->public_key();

  // If it's the selected remote device, combine the already-populated
  // dictionary with corresponding local device data (e.g. connection status and
  // remote status updates).
  if (selected_device_public_key != remote_device.public_key())
    return dictionary;

  // Fill in the current Bluetooth connection status.
  std::string connection_status = kExternalDeviceDisconnected;
  if (life_cycle_ && life_cycle_->GetState() ==
                         proximity_auth::RemoteDeviceLifeCycle::State::
                             SECURE_CHANNEL_ESTABLISHED) {
    connection_status = kExternalDeviceConnected;
  } else if (life_cycle_) {
    connection_status = kExternalDeviceConnecting;
  }
  dictionary->SetString(kExternalDeviceConnectionStatus, connection_status);

  // Fill the remote status dictionary.
  if (last_remote_status_update_) {
    std::unique_ptr<base::DictionaryValue> status_dictionary(
        new base::DictionaryValue());
    status_dictionary->SetInteger("userPresent",
                                  last_remote_status_update_->user_presence);
    status_dictionary->SetInteger(
        "secureScreenLock",
        last_remote_status_update_->secure_screen_lock_state);
    status_dictionary->SetInteger(
        "trustAgent", last_remote_status_update_->trust_agent_state);
    dictionary->Set(kExternalDeviceRemoteState, std::move(status_dictionary));
  }

  return dictionary;
}

void ProximityAuthWebUIHandler::OnLifeCycleStateChanged(
    proximity_auth::RemoteDeviceLifeCycle::State old_state,
    proximity_auth::RemoteDeviceLifeCycle::State new_state) {
  // Do not re-attempt to find a connection after the first one fails--just
  // abort.
  if ((old_state != proximity_auth::RemoteDeviceLifeCycle::State::STOPPED &&
       new_state ==
           proximity_auth::RemoteDeviceLifeCycle::State::FINDING_CONNECTION) ||
      new_state ==
          proximity_auth::RemoteDeviceLifeCycle::State::AUTHENTICATION_FAILED) {
    // Clean up the life cycle asynchronously, because we are currently in the
    // call stack of |life_cycle_|.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&ProximityAuthWebUIHandler::CleanUpRemoteDeviceLifeCycle,
                       weak_ptr_factory_.GetWeakPtr()));
  } else if (new_state == proximity_auth::RemoteDeviceLifeCycle::State::
                              SECURE_CHANNEL_ESTABLISHED) {
    life_cycle_->GetMessenger()->AddObserver(this);
  }

  web_ui()->CallJavascriptFunctionUnsafe(
      "LocalStateInterface.onRemoteDevicesChanged", *GetRemoteDevicesList());
}

void ProximityAuthWebUIHandler::OnRemoteStatusUpdate(
    const proximity_auth::RemoteStatusUpdate& status_update) {
  PA_LOG(VERBOSE) << "Remote status update:"
                  << "\n  user_presence: "
                  << static_cast<int>(status_update.user_presence)
                  << "\n  secure_screen_lock_state: "
                  << static_cast<int>(status_update.secure_screen_lock_state)
                  << "\n  trust_agent_state: "
                  << static_cast<int>(status_update.trust_agent_state);

  last_remote_status_update_.reset(
      new proximity_auth::RemoteStatusUpdate(status_update));
  std::unique_ptr<base::ListValue> synced_devices = GetRemoteDevicesList();
  web_ui()->CallJavascriptFunctionUnsafe(
      "LocalStateInterface.onRemoteDevicesChanged", *synced_devices);
}

void ProximityAuthWebUIHandler::OnForceEnrollmentNow(bool success) {
  PA_LOG(VERBOSE) << "Force enrollment result: " << success;
}

void ProximityAuthWebUIHandler::OnForceSyncNow(bool success) {
  PA_LOG(VERBOSE) << "Force sync result: " << success;
}

void ProximityAuthWebUIHandler::OnSetSoftwareFeatureState(
    const std::string public_key,
    device_sync::mojom::NetworkRequestResult result_code) {
  std::string device_id = RemoteDevice::GenerateDeviceId(public_key);

  if (result_code == device_sync::mojom::NetworkRequestResult::kSuccess) {
    PA_LOG(VERBOSE) << "Successfully set SoftwareFeature state for device: "
                    << device_id;
  } else {
    PA_LOG(ERROR) << "Failed to set SoftwareFeature state for device: "
                  << device_id << ", error code: " << result_code;
  }
}

void ProximityAuthWebUIHandler::OnFindEligibleDevices(
    device_sync::mojom::NetworkRequestResult result_code,
    multidevice::RemoteDeviceRefList eligible_devices,
    multidevice::RemoteDeviceRefList ineligible_devices) {
  if (result_code != device_sync::mojom::NetworkRequestResult::kSuccess) {
    PA_LOG(ERROR) << "Failed to find eligible devices: " << result_code;
    return;
  }

  base::ListValue eligible_devices_list_value;
  for (const auto& device : eligible_devices) {
    eligible_devices_list_value.Append(RemoteDeviceToDictionary(device));
  }

  base::ListValue ineligible_devices_list_value;
  for (const auto& device : ineligible_devices) {
    ineligible_devices_list_value.Append(RemoteDeviceToDictionary(device));
  }

  PA_LOG(VERBOSE) << "Found " << eligible_devices_list_value.GetSize()
                  << " eligible devices and "
                  << ineligible_devices_list_value.GetSize()
                  << " ineligible devices.";
  web_ui()->CallJavascriptFunctionUnsafe(
      "CryptAuthInterface.onGotEligibleDevices", eligible_devices_list_value,
      ineligible_devices_list_value);
}

void ProximityAuthWebUIHandler::OnGetDebugInfo(
    device_sync::mojom::DebugInfoPtr debug_info_ptr) {
  // If enrollment is not yet complete, no debug information is available.
  if (!debug_info_ptr)
    return;

  if (enrollment_update_waiting_for_debug_info_) {
    enrollment_update_waiting_for_debug_info_ = false;
    NotifyOnEnrollmentFinished(
        true /* success */,
        CreateSyncStateDictionary(
            debug_info_ptr->last_enrollment_time.ToJsTime(),
            debug_info_ptr->time_to_next_enrollment_attempt.InMillisecondsF(),
            debug_info_ptr->is_recovering_from_enrollment_failure,
            debug_info_ptr->is_enrollment_in_progress));
  }

  if (sync_update_waiting_for_debug_info_) {
    sync_update_waiting_for_debug_info_ = false;
    NotifyOnSyncFinished(
        true /* was_sync_successful */, true /* changed */,
        CreateSyncStateDictionary(
            debug_info_ptr->last_sync_time.ToJsTime(),
            debug_info_ptr->time_to_next_sync_attempt.InMillisecondsF(),
            debug_info_ptr->is_recovering_from_sync_failure,
            debug_info_ptr->is_sync_in_progress));
  }

  if (get_local_state_update_waiting_for_debug_info_) {
    get_local_state_update_waiting_for_debug_info_ = false;
    NotifyGotLocalState(
        GetTruncatedLocalDeviceId(),
        CreateSyncStateDictionary(
            debug_info_ptr->last_enrollment_time.ToJsTime(),
            debug_info_ptr->time_to_next_enrollment_attempt.InMillisecondsF(),
            debug_info_ptr->is_recovering_from_enrollment_failure,
            debug_info_ptr->is_enrollment_in_progress),
        CreateSyncStateDictionary(
            debug_info_ptr->last_sync_time.ToJsTime(),
            debug_info_ptr->time_to_next_sync_attempt.InMillisecondsF(),
            debug_info_ptr->is_recovering_from_sync_failure,
            debug_info_ptr->is_sync_in_progress),
        GetRemoteDevicesList());
  }
}

void ProximityAuthWebUIHandler::NotifyOnEnrollmentFinished(
    bool success,
    std::unique_ptr<base::DictionaryValue> enrollment_state) {
  PA_LOG(VERBOSE) << "Enrollment attempt completed with success=" << success
                  << ":\n"
                  << *enrollment_state;
  web_ui()->CallJavascriptFunctionUnsafe(
      "LocalStateInterface.onEnrollmentStateChanged", *enrollment_state);
}

void ProximityAuthWebUIHandler::NotifyOnSyncFinished(
    bool was_sync_successful,
    bool changed,
    std::unique_ptr<base::DictionaryValue> device_sync_state) {
  PA_LOG(VERBOSE) << "Device sync completed with result=" << was_sync_successful
                  << ":\n"
                  << *device_sync_state;
  web_ui()->CallJavascriptFunctionUnsafe(
      "LocalStateInterface.onDeviceSyncStateChanged", *device_sync_state);

  if (changed) {
    std::unique_ptr<base::ListValue> synced_devices = GetRemoteDevicesList();
    PA_LOG(VERBOSE) << "New unlock keys obtained after device sync:\n"
                    << *synced_devices;
    web_ui()->CallJavascriptFunctionUnsafe(
        "LocalStateInterface.onRemoteDevicesChanged", *synced_devices);
  }
}

void ProximityAuthWebUIHandler::NotifyGotLocalState(
    std::unique_ptr<base::Value> truncated_local_device_id,
    std::unique_ptr<base::DictionaryValue> enrollment_state,
    std::unique_ptr<base::DictionaryValue> device_sync_state,
    std::unique_ptr<base::ListValue> synced_devices) {
  PA_LOG(VERBOSE) << "==== Got Local State ====\n"
                  << "Device ID (truncated): " << *truncated_local_device_id
                  << "\nEnrollment State: \n"
                  << *enrollment_state << "Device Sync State: \n"
                  << *device_sync_state << "Synced devices: \n"
                  << *synced_devices;
  web_ui()->CallJavascriptFunctionUnsafe(
      "LocalStateInterface.onGotLocalState", *truncated_local_device_id,
      *enrollment_state, *device_sync_state, *synced_devices);
}

}  // namespace multidevice

}  // namespace chromeos
