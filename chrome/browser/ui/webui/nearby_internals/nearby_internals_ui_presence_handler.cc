// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/nearby_internals/nearby_internals_ui_presence_handler.h"
#include "chrome/browser/ash/nearby/presence/nearby_presence_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/nearby/presence/credentials/prefs.h"
#include "chromeos/ash/components/nearby/presence/nearby_presence_service.h"
#include "components/cross_device/logging/logging.h"
#include "components/prefs/pref_service.h"

namespace {

// Keys in the JSON representation of a device.
const char kDeviceNameKey[] = "name";
const char kDeviceIdKey[] = "id";
const char kTypeKey[] = "type";
const char kEndpointKey[] = "endpoint_id";
const char kActionsKey[] = "actions";

// ActionType strings representations.
const char kActiveUnlockAction[] = "Active Unlock";
const char kNearbyShareAction[] = "Nearby Share";
const char kInstantTetheringAction[] = "Instant Tethering";
const char kPhoneHubAction[] = "Phone Hub";
const char kPresenceManagerAction[] = "Presence Manager";
const char kFinderAction[] = "Finder";
const char kFastPairSassAction[] = "Fast Pair Sass";
const char kTapToTransferAction[] = "Tap To Transfer";
const char kLastAction[] = "Invalid Action";

std::string PresenceActionToString(
    ash::nearby::presence::NearbyPresenceService::Action action_enum) {
  switch (action_enum) {
    case ash::nearby::presence::NearbyPresenceService::Action::kActiveUnlock:
      return kActiveUnlockAction;
    case ash::nearby::presence::NearbyPresenceService::Action::kNearbyShare:
      return kNearbyShareAction;
    case ash::nearby::presence::NearbyPresenceService::Action::
        kInstantTethering:
      return kInstantTetheringAction;
    case ash::nearby::presence::NearbyPresenceService::Action::kPhoneHub:
      return kPhoneHubAction;
    case ash::nearby::presence::NearbyPresenceService::Action::kPresenceManager:
      return kPresenceManagerAction;
    case ash::nearby::presence::NearbyPresenceService::Action::kFinder:
      return kFinderAction;
    case ash::nearby::presence::NearbyPresenceService::Action::kFastPairSass:
      return kFastPairSassAction;
    case ash::nearby::presence::NearbyPresenceService::Action::kTapToTransfer:
      return kTapToTransferAction;
    case ash::nearby::presence::NearbyPresenceService::Action::kLast:
      return kLastAction;
  }
}

// Converts |presence_device| to a raw dictionary value used as a JSON argument
// to JavaScript functions.
base::Value::Dict PresenceDeviceToDictionary(
    const ash::nearby::presence::NearbyPresenceService::PresenceDevice&
        presence_device) {
  base::Value::Dict dictionary;
  dictionary.Set(kDeviceNameKey, presence_device.GetName());
  // TODO(b/277820435): add other device type options.
  if (presence_device.GetType() ==
      nearby::internal::DeviceType::DEVICE_TYPE_PHONE) {
    dictionary.Set(kTypeKey, "DEVICE_TYPE_PHONE");
  }
  if (presence_device.GetStableId().has_value()) {
    dictionary.Set(kDeviceIdKey, presence_device.GetStableId().value());
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
    CD_LOG(VERBOSE, Feature::NP)
        << __func__ << ": NearbyPresenceService was retrieved successfully";
    ash::nearby::presence::NearbyPresenceService::ScanFilter filter(
        ash::nearby::presence::NearbyPresenceService::IdentityType::kPublic,
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
    CD_LOG(VERBOSE, Feature::NP)
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
    CD_LOG(VERBOSE, Feature::NP)
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
    ash::nearby::presence::NearbyPresenceService::StatusCode status) {
  if (status ==
      ash::nearby::presence::NearbyPresenceService::StatusCode::kAbslOk) {
    scan_session_ = std::move(scan_session);
    CD_LOG(VERBOSE, Feature::NP)
        << __func__ << ": ScanSession remote successfully returned and bound.";
  } else {
    // TODO(b/276307539): Pass error status back to WebUI.
    return;
  }
}

void NearbyInternalsPresenceHandler::
    OnNearbyPresenceCredentialManagerInitialized() {
  CD_LOG(VERBOSE, Feature::NP) << __func__;
}

void NearbyInternalsPresenceHandler::OnPresenceDeviceFound(
    const ash::nearby::presence::NearbyPresenceService::PresenceDevice&
        presence_device) {
  FireWebUIListener("presence-device-found",
                    PresenceDeviceToDictionary(presence_device));
}

void NearbyInternalsPresenceHandler::OnPresenceDeviceChanged(
    const ash::nearby::presence::NearbyPresenceService::PresenceDevice&
        presence_device) {
  FireWebUIListener("presence-device-changed",
                    PresenceDeviceToDictionary(presence_device));
}

void NearbyInternalsPresenceHandler::OnPresenceDeviceLost(
    const ash::nearby::presence::NearbyPresenceService::PresenceDevice&
        presence_device) {
  FireWebUIListener("presence-device-lost",
                    PresenceDeviceToDictionary(presence_device));
}

void NearbyInternalsPresenceHandler::OnScanSessionInvalidated() {
  scan_session_.reset();
  HandleStartPresenceScan(/*args=*/{});
}

void NearbyInternalsPresenceHandler::HandleConnectToPresenceDevice(
    const base::Value::List& args) {
  // TODO(b/276642472): Add connect functionality.
  CD_LOG(VERBOSE, Feature::NP)
      << __func__ << ": Connection attempt for device with endpoint id: "
      << args[0].GetString();
}
