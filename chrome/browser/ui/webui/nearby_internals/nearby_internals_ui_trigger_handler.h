// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEARBY_INTERNALS_NEARBY_INTERNALS_UI_TRIGGER_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_NEARBY_INTERNALS_NEARBY_INTERNALS_UI_TRIGGER_HANDLER_H_

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service.h"
#include "chrome/browser/nearby_sharing/share_target_discovered_callback.h"
#include "chrome/browser/nearby_sharing/transfer_update_callback.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace content {
class BrowserContext;
}  // namespace content

// WebUIMessageHandler for to trigger NearbySharingService UI events.
class NearbyInternalsUiTriggerHandler : public content::WebUIMessageHandler,
                                        public TransferUpdateCallback,
                                        public ShareTargetDiscoveredCallback {
 public:
  explicit NearbyInternalsUiTriggerHandler(content::BrowserContext* context);
  NearbyInternalsUiTriggerHandler(const NearbyInternalsUiTriggerHandler&) =
      delete;
  NearbyInternalsUiTriggerHandler& operator=(
      const NearbyInternalsUiTriggerHandler&) = delete;
  ~NearbyInternalsUiTriggerHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

  // TransferUpdateCallback:
  void OnTransferUpdate(const ShareTarget& share_target,
                        const TransferMetadata& transfer_metadata) override;

  // ShareTargetDiscoveryCallback:
  void OnShareTargetDiscovered(ShareTarget share_target) override;
  void OnShareTargetLost(ShareTarget share_target) override;

 private:
  // Triggers WebUIListener event after corresponding event is triggered in
  // NearbyShareService to pass callback to JavaScript to eventually be
  // displayed.
  void OnAcceptCalled(NearbySharingService::StatusCodes status_codes);
  void OnOpenCalled(NearbySharingService::StatusCodes status_codes);
  void OnRejectCalled(NearbySharingService::StatusCodes status_codes);
  void OnCancelCalled(NearbySharingService::StatusCodes status_codes);

  // Message handler callback that initializes JavaScript.
  void InitializeContents(const base::Value::List& args);

  // Message handler callback that triggers SendText in the NearbyShareService.
  void SendText(const base::Value::List& args);

  // Message handler callback that triggers Accept in the NearbyShareService.
  void Accept(const base::Value::List& args);

  // Message handler callback that triggers Reject in the NearbyShareService.
  void Reject(const base::Value::List& args);

  // Message handler callback that triggers Cancel in the NearbyShareService.
  void Cancel(const base::Value::List& args);

  // Message handler callback that triggers Open in the NearbyShareService.
  void Open(const base::Value::List& args);

  // Message handler callback that calls RegisterSendSurface in the
  // NearbySharingService with a Foreground SendSurfaceState.
  // The NearbyInternalsUiTriggerHandler instance acts as the send surface,
  // playing the role of both the TransferUpdateCallback and the
  // ShareTargetDiscoveredCallback.
  void RegisterSendSurfaceForeground(const base::Value::List& args);

  // Message handler callback that calls RegisterSendSurface in the
  // NearbySharingService with a Background SendSurfaceState.
  // The NearbyInternalsUiTriggerHandler instance acts as the send surface,
  // playing the role of both the TransferUpdateCallback and the
  // ShareTargetDiscoveredCallback.
  void RegisterSendSurfaceBackground(const base::Value::List& args);

  // Message handler callback that calls UnregisterSendSurface in the
  // NearbySharingService. The NearbyInternalsUiTriggerHandler instance acts as
  // the send surface to be unregistered.
  void UnregisterSendSurface(const base::Value::List& args);

  // Message handler callback that calls RegisterReceiveSurface in the
  // NearbySharingService with a Foreground receive surface state.
  // The NearbyInternalsUiTriggerHandler instance acts as the receive surface,
  // playing the role of both the TransferUpdateCallback and the
  // ShareTargetDiscoveredCallback.
  void RegisterReceiveSurfaceForeground(const base::Value::List& args);

  // Message handler callback that calls RegisterReceiveSurface in the
  // NearbySharingService with a Background receive surface state.
  // The NearbyInternalsUiTriggerHandler instance acts as the receive surface,
  // playing the role of both the TransferUpdateCallback and the
  // ShareTargetDiscoveredCallback.
  void RegisterReceiveSurfaceBackground(const base::Value::List& args);

  // Message handler callback that calls UnregisterReceiveSurface in the
  // NearbySharingService. The NearbyInternalsUiTriggerHandler instance acts as
  // the receive surface to be unregistered.
  void UnregisterReceiveSurface(const base::Value::List& args);

  // Message handler callback that calls ShowSuccess in the
  // NearbySharingService's NearbyNotificationManager.
  void ShowReceivedNotification(const base::Value::List& args);

  // Message handler callback that calls IsScanning, IsTransferring,
  // IsReceivingFile, IsSendingFile, IsConnecting, and IsInHighVisibility in the
  // NearbySharingService and passes booleans to JavaScript to eventually be
  // displayed.
  void GetState(const base::Value::List& args);

  const raw_ptr<content::BrowserContext> context_;
  base::flat_map<std::string, ShareTarget> id_to_share_target_map_;
  base::WeakPtrFactory<NearbyInternalsUiTriggerHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_NEARBY_INTERNALS_NEARBY_INTERNALS_UI_TRIGGER_HANDLER_H_
