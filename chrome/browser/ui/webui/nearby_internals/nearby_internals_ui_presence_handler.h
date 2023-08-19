// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEARBY_INTERNALS_NEARBY_INTERNALS_UI_PRESENCE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_NEARBY_INTERNALS_NEARBY_INTERNALS_UI_PRESENCE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chromeos/ash/components/nearby/presence/nearby_presence_service.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace content {
class BrowserContext;
}  // namespace content

class NearbyInternalsPresenceHandler
    : public content::WebUIMessageHandler,
      ash::nearby::presence::NearbyPresenceService::ScanDelegate {
 public:
  explicit NearbyInternalsPresenceHandler(content::BrowserContext* context);
  NearbyInternalsPresenceHandler(const NearbyInternalsPresenceHandler&) =
      delete;
  NearbyInternalsPresenceHandler& operator=(
      const NearbyInternalsPresenceHandler&) = delete;
  ~NearbyInternalsPresenceHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  // ash::nearby::presence::NearbyPresenceService::ScanDelegate:
  void OnPresenceDeviceFound(
      const ash::nearby::presence::NearbyPresenceService::PresenceDevice&
          presence_device) override;
  void OnPresenceDeviceChanged(
      const ash::nearby::presence::NearbyPresenceService::PresenceDevice&
          presence_device) override;
  void OnPresenceDeviceLost(
      const ash::nearby::presence::NearbyPresenceService::PresenceDevice&
          presence_device) override;
  void OnScanSessionInvalidated() override;

  void Initialize(const base::Value::List& args);
  void HandleStartPresenceScan(const base::Value::List& args);
  void HandleStopPresenceScan(const base::Value::List& args);
  void HandleSyncPresenceCredentials(const base::Value::List& args);
  void HandleFirstTimePresenceFlow(const base::Value::List& args);

  void OnScanStarted(
      std::unique_ptr<ash::nearby::presence::NearbyPresenceService::ScanSession>
          scan_session,
      ash::nearby::presence::mojom::StatusCode status);
  void OnNearbyPresenceCredentialManagerInitialized();

  void HandleConnectToPresenceDevice(const base::Value::List& args);

 private:
  const raw_ptr<content::BrowserContext> context_;
  std::unique_ptr<ash::nearby::presence::NearbyPresenceService::ScanSession>
      scan_session_;

  base::WeakPtrFactory<NearbyInternalsPresenceHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_NEARBY_INTERNALS_NEARBY_INTERNALS_UI_PRESENCE_HANDLER_H_
