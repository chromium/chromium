// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DRIVE_PICKER_HOST_DRIVE_PICKER_HOST_UI_H_
#define CHROME_BROWSER_UI_WEBUI_DRIVE_PICKER_HOST_DRIVE_PICKER_HOST_UI_H_

#include <memory>
#include <string_view>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/drive_picker_host/drive_picker_host.mojom.h"
#include "chrome/browser/ui/webui/drive_picker_host/drive_picker_host_request.h"
#include "chrome/browser/ui/webui/drive_picker_host/untrusted/drive_picker_host_untrusted.mojom.h"
#include "chrome/browser/ui/webui/drive_picker_host/untrusted/drive_picker_host_untrusted_ui.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_config.h"
#include "chrome/common/webui_url_constants.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "content/public/browser/web_contents_observer.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "url/gurl.h"

namespace signin {
class PrimaryAccountAccessTokenFetcher;
}  // namespace signin

namespace identity_consent {
class PrivacyFlowResult;
}  // namespace identity_consent

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
      public content::WebContentsObserver,
      public DrivePickerUntrustedHostUI::Delegate {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual void OnTransitionToPicker() = 0;
  };

  void set_delegate(Delegate* delegate) { delegate_ = delegate; }

  explicit DrivePickerHostUI(content::WebUI* web_ui);
  ~DrivePickerHostUI() override;

  DrivePickerHostUI(const DrivePickerHostUI&) = delete;
  DrivePickerHostUI& operator=(const DrivePickerHostUI&) = delete;

  static std::string_view GetWebUIName() { return "DrivePickerHost"; }

  // Triggers the Drive Picker host logic to display the picker UI and relay
  // results to `result_handler`.
  virtual void TriggerDrivePickerHost(
      std::unique_ptr<drive_picker_host::DrivePickerHostRequest> request);

  // Sets the untrusted bridge that will be used to display the picker UI and
  // handle communication with the picker UI.
  void SetBridge(
      mojo::PendingRemote<drive_picker_host_untrusted::mojom::DrivePickerBridge>
          untrusted_bridge);

  void BindInterface(
      mojo::PendingReceiver<drive_picker_host::mojom::DrivePickerHostHandler>
          receiver);

  // Instructs the untrusted context to load the provided ConsentKit URL
  // within an iframe.
  //
  // `consent_kit_url`: The ConsentKit URL to load.
  void LoadConsentKitUrl(const GURL& consent_kit_url);

  void OnConsentKitIframeMessage(
      mojo_base::ProtoWrapper message_wrapper) override;
  void OnConsentKitPrivacyFlowResult(
      mojo_base::ProtoWrapper result_wrapper) override;
  void OnConsentKitError(const std::string& error_message) override;
  base::WeakPtr<DrivePickerUntrustedHostUI::Delegate> GetWeakPtr() override;

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

  // Constructs the ConsentKit URL and initiates the consent flow.
  void ShowConsentKitDialog(
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
  std::unique_ptr<drive_picker_host::DrivePickerHostRequest> pending_request_;

  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      access_token_fetcher_;

  mojo::Remote<drive_picker_host_untrusted::mojom::DrivePickerBridge>
      untrusted_bridge_remote_;
  mojo::Receiver<drive_picker_host::mojom::DrivePickerHostHandler> receiver_{
      this};

  mojo::Remote<drive_picker_host::mojom::DrivePickerResultHandler>
      consent_result_handler_;

  raw_ptr<Delegate> delegate_ = nullptr;

  base::WeakPtrFactory<DrivePickerHostUI> weak_ptr_factory_{this};

  void HandlePrivacyFlowResult(
      const identity_consent::PrivacyFlowResult& result);

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_DRIVE_PICKER_HOST_DRIVE_PICKER_HOST_UI_H_
