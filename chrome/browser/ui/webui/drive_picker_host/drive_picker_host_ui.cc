// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/drive_picker_host/drive_picker_host_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/webui/drive_picker_host/untrusted/drive_picker_host_untrusted_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/drive_picker_host_resources.h"
#include "chrome/grit/drive_picker_host_resources_map.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "google_apis/google_api_keys.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/webui/webui_util.h"

namespace {

// Extracts the numeric Cloud Project Number from a full OAuth2 Client ID string
// (e.g., extracts "77185425430" from "77185425430.apps.googleusercontent.com").
//
// This extraction is necessary because the Google Picker API's `setAppId`
// method specifically requires the numeric "Cloud project number" as its input,
// rather than the full OAuth2 Client ID string used by the identity service.
//
// Providing the correct numeric Project Number (App ID) is required for the
// Picker to perform a "PreOpen" request to the backend. This request
// authorizes the specific Cloud project to interact with the selected file,
// which is a prerequisite for using the limited `drive.readonly` OAuth scope.
std::string ExtractProjectNumber(const std::string& client_id) {
  size_t dot_pos = client_id.find('.');
  if (dot_pos != std::string::npos) {
    return client_id.substr(0, dot_pos);
  }
  return client_id;
}

}  // namespace

DrivePickerHostUIConfig::DrivePickerHostUIConfig()
    : DefaultTopChromeWebUIConfig(content::kChromeUIScheme,
                                  chrome::kChromeUIDrivePickerHostHost) {}

bool DrivePickerHostUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(
      omnibox::kComposeboxDriveContextMenuOption);
}

DrivePickerHostUI::DrivePickerHostUI(content::WebUI* web_ui)
    : TopChromeWebUIController(web_ui),
      content::WebContentsObserver(web_ui->GetWebContents()) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui), chrome::kChromeUIDrivePickerHostHost);

  webui::SetupWebUIDataSource(source, kDrivePickerHostResources,
                              IDR_DRIVE_PICKER_HOST_DRIVE_PICKER_HOST_HTML);

  // Allow iframing the untrusted drive picker.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FrameSrc,
      "frame-src chrome-untrusted://drive-picker-host/;");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ChildSrc,
      "child-src 'self' chrome-untrusted://drive-picker-host/;");

  web_ui->AddRequestableScheme(content::kChromeUIUntrustedScheme);
}

DrivePickerHostUI::~DrivePickerHostUI() = default;

void DrivePickerHostUI::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  MaybeBindUntrustedBridge(render_frame_host);
}

void DrivePickerHostUI::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->HasCommitted()) {
    MaybeBindUntrustedBridge(navigation_handle->GetRenderFrameHost());
  }
}

void DrivePickerHostUI::MaybeBindUntrustedBridge(
    content::RenderFrameHost* render_frame_host) {
  if (untrusted_bridge_remote_.is_bound() &&
      untrusted_bridge_remote_.is_connected()) {
    return;
  }

  if (render_frame_host && render_frame_host->GetWebUI()) {
    auto* untrusted_ui = render_frame_host->GetWebUI()
                             ->GetController()
                             ->GetAs<DrivePickerUntrustedHostUI>();
    if (untrusted_ui) {
      mojo::PendingRemote<drive_picker_host_untrusted::mojom::DrivePickerBridge>
          bridge;
      untrusted_ui->BindInterface(bridge.InitWithNewPipeAndPassReceiver());
      SetBridge(std::move(bridge));
    }
  }
}

void DrivePickerHostUI::TriggerDrivePickerHost(
    mojo::PendingRemote<drive_picker_host::mojom::DrivePickerResultHandler>
        result_handler) {
  if (untrusted_bridge_remote_.is_bound() &&
      untrusted_bridge_remote_.is_connected()) {
    FetchTokenAndShowPicker(std::move(result_handler));
  } else {
    // Only the most recent request is kept if the bridge is not yet ready or
    // has been disconnected.
    pending_result_handler_ = std::move(result_handler);
  }
}

void DrivePickerHostUI::FetchTokenAndShowPicker(
    mojo::PendingRemote<drive_picker_host::mojom::DrivePickerResultHandler>
        result_handler) {
  if (access_token_fetcher_) {
    return;
  }

  Profile* profile = Profile::FromWebUI(web_ui());
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager) {
    return;
  }

  // Drive is only available for users if they are (1) signed into Chrome and
  // (2) the browser identity matches the AIM identity. We can only get to this
  // point into the flow is these conditions are met, so we can assume that the
  // OAuth token is available.
  access_token_fetcher_ =
      std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
          signin::OAuthConsumerId::kDrivePickerHost, identity_manager,
          base::BindOnce(&DrivePickerHostUI::OnAccessTokenFetched,
                         weak_ptr_factory_.GetWeakPtr(),
                         std::move(result_handler)),
          signin::PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable,
          signin::ConsentLevel::kSignin);
}

void DrivePickerHostUI::OnAccessTokenFetched(
    mojo::PendingRemote<drive_picker_host::mojom::DrivePickerResultHandler>
        result_handler,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  access_token_fetcher_.reset();
  if (error.state() != GoogleServiceAuthError::NONE) {
    return;
  }

  if (untrusted_bridge_remote_.is_bound() &&
      untrusted_bridge_remote_.is_connected()) {
    drive_picker_host_untrusted::mojom::DrivePickerKeysPtr keys =
        drive_picker_host_untrusted::mojom::DrivePickerKeys::New();
    keys->oauth_token = access_token_info.token;
    keys->api_key = google_apis::GetAPIKey();
    keys->app_id = ExtractProjectNumber(
        google_apis::GetOAuth2ClientID(google_apis::OAuth2Client::CLIENT_MAIN));

    untrusted_bridge_remote_->ShowDrivePicker(std::move(result_handler),
                                              std::move(keys));
  } else {
    pending_result_handler_ = std::move(result_handler);
  }
}

void DrivePickerHostUI::SetBridge(
    mojo::PendingRemote<drive_picker_host_untrusted::mojom::DrivePickerBridge>
        untrusted_bridge) {
  untrusted_bridge_remote_.reset();
  untrusted_bridge_remote_.Bind(std::move(untrusted_bridge));

  if (pending_result_handler_) {
    FetchTokenAndShowPicker(std::move(pending_result_handler_));
  }
}

void DrivePickerHostUI::BindInterface(
    mojo::PendingReceiver<drive_picker_host::mojom::DrivePickerHostHandler>
        receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(DrivePickerHostUI)
