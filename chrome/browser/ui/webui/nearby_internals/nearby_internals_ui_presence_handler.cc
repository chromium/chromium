// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/nearby_internals/nearby_internals_ui_presence_handler.h"
#include "chrome/browser/ash/nearby/presence/nearby_presence_service_factory.h"
#include "chrome/browser/nearby_sharing/logging/logging.h"
#include "chromeos/ash/components/nearby/presence/nearby_presence_service.h"

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
    ash::nearby::presence::NearbyPresenceService::ScanFilter filter;
    service->StartScan(filter, this);
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

void NearbyInternalsPresenceHandler::OnPresenceDeviceFound(
    const ash::nearby::presence::NearbyPresenceService::PresenceDevice&
        presence_device) {
  NS_LOG(VERBOSE) << __func__;
}

void NearbyInternalsPresenceHandler::OnPresenceDeviceChanged(
    const ash::nearby::presence::NearbyPresenceService::PresenceDevice&
        presence_device) {}

void NearbyInternalsPresenceHandler::OnPresenceDeviceLost(
    const ash::nearby::presence::NearbyPresenceService::PresenceDevice&
        presence_device) {}
