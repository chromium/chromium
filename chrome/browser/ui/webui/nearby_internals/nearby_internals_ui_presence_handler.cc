// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/nearby_internals/nearby_internals_ui_presence_handler.h"

#include "base/containers/contains.h"
#include "chrome/browser/ash/nearby/nearby_process_manager_factory.h"
#include "chrome/browser/ash/nearby/presence/nearby_presence_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/push_notification/push_notification_service_factory.h"
#include "chromeos/ash/components/nearby/presence/credentials/prefs.h"
#include "chromeos/ash/components/nearby/presence/nearby_presence_service.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom.h"
#include "components/cross_device/logging/logging.h"
#include "components/prefs/pref_service.h"
#include "components/push_notification/push_notification_service.h"

namespace {

// Keys in the JSON representation of a device.
const char kDeviceNameKey[] = "name";
const char kTypeKey[] = "type";
const char kEndpointKey[] = "endpoint_id";
const char kActionsKey[] = "actions";

// ActionType strings representations.
const char kCallTransferAction[] = "Call Transfer";
const char kActiveUnlockAction[] = "Active Unlock";
const char kNearbyShareAction[] = "Nearby Share";
const char kInstantTetheringAction[] = "Instant Tethering";
const char kPhoneHubAction[] = "Phone Hub";
const char kPresenceManagerAction[] = "Presence Manager";
const char kFinderAction[] = "Finder";
const char kFastPairSassAction[] = "Fast Pair Sass";
const char kTapToTransferAction[] = "Tap To Transfer";
const char kLastAction[] = "Invalid Action";

// `PushNotificationMessage` key value pairs
const char kNotificationTypeIdKey[] = "type_id";
const char kNotificationClientIdKey[] = "client_id";
const char kNotificationClientIdValue[] = "nearby";

std::string PresenceActionToString(nearby::presence::PresenceAction action) {
  switch (nearby::presence::ActionBit(action.GetActionIdentifier())) {
    case nearby::presence::ActionBit::kCallTransferAction:
      return kCallTransferAction;
    case nearby::presence::ActionBit::kActiveUnlockAction:
      return kActiveUnlockAction;
    case nearby::presence::ActionBit::kNearbyShareAction:
      return kNearbyShareAction;
    case nearby::presence::ActionBit::kInstantTetheringAction:
      return kInstantTetheringAction;
    case nearby::presence::ActionBit::kPhoneHubAction:
      return kPhoneHubAction;
    case nearby::presence::ActionBit::kPresenceManagerAction:
      return kPresenceManagerAction;
    case nearby::presence::ActionBit::kFinderAction:
      return kFinderAction;
    case nearby::presence::ActionBit::kFastPairSassAction:
      return kFastPairSassAction;
    case nearby::presence::ActionBit::kTapToTransferAction:
      return kTapToTransferAction;
    case nearby::presence::ActionBit::kLastAction:
      return kLastAction;
  }
}

// Converts |presence_device| to a raw dictionary value used as a JSON argument
// to JavaScript functions.
base::Value::Dict PresenceDeviceToDictionary(
    nearby::presence::PresenceDevice presence_device) {
  base::Value::Dict dictionary;
  dictionary.Set(kDeviceNameKey,
                 presence_device.GetDeviceIdentityMetadata().device_name());
  // TODO(b/277820435): add other device type options.
  if (presence_device.GetDeviceIdentityMetadata().device_type() ==
      nearby::internal::DeviceType::DEVICE_TYPE_PHONE) {
    dictionary.Set(kTypeKey, "DEVICE_TYPE_PHONE");
  }

  dictionary.Set(kEndpointKey, presence_device.GetEndpointId());
  std::string actions_list;
  for (auto action : presence_device.GetActions()) {
    actions_list += PresenceActionToString(action);
    actions_list += ", ";
  }

  // Remove the trailing comma and whitespace.
  actions_list.pop_back();
  actions_list.pop_back();

  dictionary.Set(kActionsKey, actions_list);
  return dictionary;
}

}  // namespace

NearbyInternalsPresenceHandler::NearbyInternalsPresenceHandler(
    content::BrowserContext* context)
    : context_(context) {}

NearbyInternalsPresenceHandler::~NearbyInternalsPresenceHandler() = default;

void NearbyInternalsPresenceHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "InitializePresenceHandler",
      base::BindRepeating(&NearbyInternalsPresenceHandler::Initialize,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "StartPresenceScan",
      base::BindRepeating(
          &NearbyInternalsPresenceHandler::HandleStartPresenceScan,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "StopPresenceScan",
      base::BindRepeating(
          &NearbyInternalsPresenceHandler::HandleStopPresenceScan,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "SyncPresenceCredentials",
      base::BindRepeating(
          &NearbyInternalsPresenceHandler::HandleSyncPresenceCredentials,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "FirstTimePresenceFlow",
      base::BindRepeating(
          &NearbyInternalsPresenceHandler::HandleFirstTimePresenceFlow,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "ConnectToPresenceDevice",
      base::BindRepeating(
          &NearbyInternalsPresenceHandler::HandleConnectToPresenceDevice,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "SendUpdateCredentialsMessage",
      base::BindRepeating(
          &NearbyInternalsPresenceHandler::HandleSendUpdateCredentialsMessage,
          base::Unretained(this)));
}

void NearbyInternalsPresenceHandler::OnJavascriptAllowed() {}

void NearbyInternalsPresenceHandler::OnJavascriptDisallowed() {}

void NearbyInternalsPresenceHandler::Initialize(const base::Value::List& args) {
  AllowJavascript();
}

void NearbyInternalsPresenceHandler::HandleStartPresenceScan(
    const base::Value::List& args) {
  ash::nearby::presence::NearbyPresenceService* service =
      ash::nearby::presence::NearbyPresenceServiceFactory::GetForBrowserContext(
          context_);
  if (service) {
    CD_LOG(VERBOSE, Feature::NEARBY_INFRA)
        << __func__ << ": NearbyPresenceService was retrieved successfully";
    ash::nearby::presence::NearbyPresenceService::ScanFilter filter(
        nearby::internal::IdentityType::IDENTITY_TYPE_PRIVATE_GROUP,
        /*actions=*/{});
    service->StartScan(
        filter, /*scan_delegate=*/this,
        base::BindOnce(&NearbyInternalsPresenceHandler::OnScanStarted,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void NearbyInternalsPresenceHandler::HandleStopPresenceScan(
    const base::Value::List& args) {
  scan_session_.reset();
}

void NearbyInternalsPresenceHandler::HandleSyncPresenceCredentials(
    const base::Value::List& args) {
  ash::nearby::presence::NearbyPresenceService* service =
      ash::nearby::presence::NearbyPresenceServiceFactory::GetForBrowserContext(
          context_);
  if (service) {
    CD_LOG(VERBOSE, Feature::NEARBY_INFRA)
        << __func__ << ": NearbyPresenceService was retrieved successfully";
    service->UpdateCredentials();
  }
}

void NearbyInternalsPresenceHandler::HandleFirstTimePresenceFlow(
    const base::Value::List& args) {
  ash::nearby::presence::NearbyPresenceService* service =
      ash::nearby::presence::NearbyPresenceServiceFactory::GetForBrowserContext(
          context_);
  if (service) {
    CD_LOG(VERBOSE, Feature::NEARBY_INFRA)
        << __func__ << ": NearbyPresenceService was retrieved successfully";
    auto* pref_service = Profile::FromBrowserContext(context_)->GetPrefs();

    // Reset the state that indicates that first time registration was
    // completed for testing. This will trigger the first time flow in
    // `NearbyPresenceService::Initialize()`, in the case that this was already
    // set on the device for manual testing.
    pref_service->SetBoolean(ash::nearby::presence::prefs::
                                 kNearbyPresenceFirstTimeRegistrationComplete,
                             false);
    service->Initialize(
        base::BindOnce(&NearbyInternalsPresenceHandler::
                           OnNearbyPresenceCredentialManagerInitialized,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void NearbyInternalsPresenceHandler::OnScanStarted(
    std::unique_ptr<ash::nearby::presence::NearbyPresenceService::ScanSession>
        scan_session,
    ash::nearby::presence::enums::StatusCode status) {
  if (status == ash::nearby::presence::enums::StatusCode::kAbslOk) {
    scan_session_ = std::move(scan_session);
    CD_LOG(VERBOSE, Feature::NEARBY_INFRA)
        << __func__ << ": ScanSession remote successfully returned and bound.";
  } else {
    // TODO(b/276307539): Pass error status back to WebUI.
    return;
  }
}

void NearbyInternalsPresenceHandler::
    OnNearbyPresenceCredentialManagerInitialized() {
  CD_LOG(VERBOSE, Feature::NEARBY_INFRA) << __func__;
}

void NearbyInternalsPresenceHandler::OnPresenceDeviceFound(
    nearby::presence::PresenceDevice presence_device) {
  const std::string& endpoint_id = presence_device.GetEndpointId();
  endpoint_id_to_presence_device_map_.emplace(
      endpoint_id, std::make_unique<nearby::presence::PresenceDevice>(
                       std::move(presence_device)));

  FireWebUIListener(
      "presence-device-found",
      PresenceDeviceToDictionary(
          *endpoint_id_to_presence_device_map_.at(endpoint_id).get()));
}

void NearbyInternalsPresenceHandler::OnPresenceDeviceChanged(
    nearby::presence::PresenceDevice presence_device) {
  FireWebUIListener("presence-device-changed",
                    PresenceDeviceToDictionary(presence_device));
}

void NearbyInternalsPresenceHandler::OnPresenceDeviceLost(
    nearby::presence::PresenceDevice presence_device) {
  endpoint_id_to_presence_device_map_.erase(presence_device.GetEndpointId());

  FireWebUIListener("presence-device-lost",
                    PresenceDeviceToDictionary(presence_device));
}

void NearbyInternalsPresenceHandler::OnScanSessionInvalidated() {
  scan_session_.reset();
  HandleStartPresenceScan(/*args=*/{});
}

void NearbyInternalsPresenceHandler::HandleConnectToPresenceDevice(
    const base::Value::List& args) {
  const std::string& endpoint_id = args[0].GetString();
  CD_LOG(VERBOSE, Feature::NEARBY_INFRA)
      << __func__
      << ": Connection attempt for device with endpoint id: " << endpoint_id;

  if (!base::Contains(endpoint_id_to_presence_device_map_, endpoint_id)) {
    CD_LOG(WARNING, Feature::NEARBY_INFRA)
        << __func__ << ": Received endpoint_id for device no longer in map.";
    return;
  }

  auto* presence_connections_manager = GetNearbyPresenceConnectionsManager();
  if (presence_connections_manager) {
    presence_connections_manager->ConnectV3(
        *endpoint_id_to_presence_device_map_.at(endpoint_id).get(),
        nearby_share::mojom::DataUsage::kOffline,
        base::BindOnce(&NearbyInternalsPresenceHandler::OnConnection,
                       weak_ptr_factory_.GetWeakPtr(), endpoint_id));
  }
}

void NearbyInternalsPresenceHandler::OnConnection(
    const std::string& endpoint_id,
    NearbyConnection* connection) {
  // TODO(b/339674618): Add `Connected` / `Failed to Connect` label on device
  // cell in the WebUI.
  if (connection) {
    CD_LOG(VERBOSE, Feature::NEARBY_INFRA)
        << "Successfully connected to endpoint_id=" << endpoint_id;
  } else {
    CD_LOG(WARNING, Feature::NEARBY_INFRA)
        << "Failed to connect to to endpoint_id=" << endpoint_id;
  }
}

void NearbyInternalsPresenceHandler::HandleSendUpdateCredentialsMessage(
    const base::Value::List& args) {
  push_notification::PushNotificationService* service =
      push_notification::PushNotificationServiceFactory::GetForBrowserContext(
          context_);
  CHECK(service);
  push_notification::PushNotificationClientManager::PushNotificationMessage
      message;
  message.data.insert_or_assign(kNotificationTypeIdKey,
                                push_notification::kNearbyPresenceClientId);
  message.data.insert_or_assign(kNotificationClientIdKey,
                                kNotificationClientIdValue);

  service->GetPushNotificationClientManager()
      ->NotifyPushNotificationClientOfMessage(std::move(message));
}

ash::nearby::presence::NearbyPresenceConnectionsManager*
NearbyInternalsPresenceHandler::GetNearbyPresenceConnectionsManager() {
  if (!nearby_presence_connections_manager_) {
    ash::nearby::presence::NearbyPresenceService* service = ash::nearby::
        presence::NearbyPresenceServiceFactory::GetForBrowserContext(context_);
    if (service) {
      CD_LOG(VERBOSE, Feature::NEARBY_INFRA)
          << __func__ << ": NearbyPresenceService was retrieved successfully";
      nearby_presence_connections_manager_ =
          service->CreateNearbyPresenceConnectionsManager();
    }
  }

  return nearby_presence_connections_manager_.get();
}
