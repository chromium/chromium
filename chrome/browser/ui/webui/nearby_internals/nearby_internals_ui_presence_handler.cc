// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/nearby_internals/nearby_internals_ui_presence_handler.h"
#include "chrome/browser/ash/nearby/presence/nearby_presence_service_factory.h"
#include "chrome/browser/nearby_sharing/logging/logging.h"
#include "chromeos/ash/components/nearby/presence/nearby_presence_service.h"

namespace {

// Keys in the JSON representation of a device.
const char kDeviceNameKey[] = "name";
const char kDeviceIdKey[] = "id";
const char kTypeKey[] = "type";
const char kEndpointKey[] = "endpoint_id";

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
      "SyncPresenceCredentials",
      base::BindRepeating(
          &NearbyInternalsPresenceHandler::HandleSyncPresenceCredentials,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "FirstTimePresenceFlow",
      base::BindRepeating(
          &NearbyInternalsPresenceHandler::HandleFirstTimePresenceFlow,
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
    NS_LOG(VERBOSE) << __func__
                    << ": NearbyPresenceService was retrieved successfully";
    ash::nearby::presence::NearbyPresenceService::ScanFilter filter(
        ash::nearby::presence::NearbyPresenceService::IdentityType::kPrivate,
        /*actions=*/{});
    service->StartScan(
        filter, /*scan_delegate=*/this,
        base::BindOnce(&NearbyInternalsPresenceHandler::OnScanStarted,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void NearbyInternalsPresenceHandler::HandleSyncPresenceCredentials(
    const base::Value::List& args) {
  ash::nearby::presence::NearbyPresenceService* service =
      ash::nearby::presence::NearbyPresenceServiceFactory::GetForBrowserContext(
          context_);
  if (service) {
    NS_LOG(VERBOSE) << __func__
                    << ": NearbyPresenceService was retrieved successfully";
    // TODO(b/276307539): Call NPS function to sync credentials.
  }
}

void NearbyInternalsPresenceHandler::HandleFirstTimePresenceFlow(
    const base::Value::List& args) {
  ash::nearby::presence::NearbyPresenceService* service =
      ash::nearby::presence::NearbyPresenceServiceFactory::GetForBrowserContext(
          context_);
  if (service) {
    NS_LOG(VERBOSE) << __func__
                    << ": NearbyPresenceService was retrieved successfully";
    // TODO(b/276307539): Call NPS function to initiate first time flow.
  }
}

void NearbyInternalsPresenceHandler::OnScanStarted(
    std::unique_ptr<ash::nearby::presence::NearbyPresenceService::ScanSession>
        scan_session,
    ash::nearby::presence::mojom::StatusCode status) {
  if (status == ash::nearby::presence::mojom::StatusCode::kOk) {
    scan_session_ = std::move(scan_session);
    NS_LOG(VERBOSE) << __func__
                    << ": ScanSession remote successfully returned and bound.";
  } else {
    // TODO(b/276307539): Pass error status back to WebUI.
    return;
  }
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
