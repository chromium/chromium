// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DRIVE_PICKER_HOST_DRIVE_PICKER_HOST_UI_H_
#define CHROME_BROWSER_UI_WEBUI_DRIVE_PICKER_HOST_DRIVE_PICKER_HOST_UI_H_

#include <memory>
#include <string_view>

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/drive_picker_host/drive_picker_result_handler.mojom.h"
#include "chrome/browser/ui/webui/drive_picker_host/drive_picker_host.mojom.h"
#include "chrome/browser/ui/webui/drive_picker_host/untrusted/drive_picker_host_untrusted.mojom.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_config.h"
#include "chrome/common/webui_url_constants.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "content/public/browser/web_contents_observer.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace signin {
class PrimaryAccountAccessTokenFetcher;
}  // namespace signin

class DrivePickerHostUI;

class DrivePickerHostUIConfig
    : public DefaultTopChromeWebUIConfig<DrivePickerHostUI> {
 public:
  DrivePickerHostUIConfig();

  // content::WebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// The WebUI controller for chrome://drive-picker-host.
// It implements DrivePickerHostHandler for communication from the Trusted JS.
class DrivePickerHostUI
    : public TopChromeWebUIController,
      public drive_picker_host::mojom::DrivePickerHostHandler,
      public content::WebContentsObserver {
 public:
  explicit DrivePickerHostUI(content::WebUI* web_ui);
  ~DrivePickerHostUI() override;

  DrivePickerHostUI(const DrivePickerHostUI&) = delete;
  DrivePickerHostUI& operator=(const DrivePickerHostUI&) = delete;

  static std::string_view GetWebUIName() { return "DrivePickerHost"; }

  // Triggers the Drive Picker host logic to display the picker UI and relay
  // results to `result_handler`.
  virtual void TriggerDrivePickerHost(
      mojo::PendingRemote<drive_picker_host::mojom::DrivePickerResultHandler>
          result_handler);

  // Sets the untrusted bridge that will be used to display the picker UI and
  // handle communication with the picker UI.
  void SetBridge(
      mojo::PendingRemote<drive_picker_host_untrusted::mojom::DrivePickerBridge>
          untrusted_bridge);

  void BindInterface(
      mojo::PendingReceiver<drive_picker_host::mojom::DrivePickerHostHandler>
          receiver);

 private:
  // Callback for the access token fetcher.
  void OnAccessTokenFetched(
      mojo::PendingRemote<drive_picker_host::mojom::DrivePickerResultHandler>
          result_handler,
      GoogleServiceAuthError error,
      signin::AccessTokenInfo access_token_info);

  // Initiates the OAuth token fetch and subsequent picker display.
  void FetchTokenAndShowPicker(
      mojo::PendingRemote<drive_picker_host::mojom::DrivePickerResultHandler>
          result_handler);

  // Both RenderFrameCreated and DidFinishNavigation are necessary to establish
  // the bridge as early as possible. RenderFrameCreated is the earliest signal,
  // but GetWebUI() may still be null. DidFinishNavigation serves as a reliable
  // fallback where the WebUI is guaranteed to be associated.
  // content::WebContentsObserver:
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // Helper to establish the Mojo bridge with the untrusted WebUI controller.
  // This uses both RenderFrameCreated and DidFinishNavigation to ensure the
  // bridge is established as early as possible. RenderFrameCreated is the
  // earliest signal but GetWebUI() may still be null; DidFinishNavigation is a
  // reliable fallback where the WebUI is guaranteed to be associated.
  void MaybeBindUntrustedBridge(content::RenderFrameHost* render_frame_host);

  // Stores a single request that arrived before the untrusted bridge was bound.
  mojo::PendingRemote<drive_picker_host::mojom::DrivePickerResultHandler>
      pending_result_handler_;

  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      access_token_fetcher_;

  mojo::Remote<drive_picker_host_untrusted::mojom::DrivePickerBridge>
      untrusted_bridge_remote_;
  mojo::Receiver<drive_picker_host::mojom::DrivePickerHostHandler> receiver_{
      this};

  base::WeakPtrFactory<DrivePickerHostUI> weak_ptr_factory_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_DRIVE_PICKER_HOST_DRIVE_PICKER_HOST_UI_H_
