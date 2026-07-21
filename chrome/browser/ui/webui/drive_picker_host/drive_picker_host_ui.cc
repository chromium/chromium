// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/drive_picker_host/drive_picker_host_ui.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/webui/drive_picker_host/untrusted/drive_picker_host_untrusted_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/drive_picker_host_resources.h"
#include "chrome/grit/drive_picker_host_resources_map.h"
#include "components/contextual_search/consent_kit/consent_kit_url_builder.h"
#include "components/contextual_search/consent_kit/proto/iframe_interface.pb.h"
#include "components/contextual_search/input_state_model.h"
#include "components/contextual_search/pref_names.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/oauth_consumer_id.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "google_apis/google_api_keys.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/native_theme/native_theme.h"
#include "ui/webui/webui_util.h"
#include "url/origin.h"

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

int GetSessionIndexForPrimaryAccount(
    signin::IdentityManager* identity_manager) {
  CoreAccountId primary_account_id =
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  if (primary_account_id.empty()) {
    return 0;
  }
  signin::AccountsInCookieJarInfo accounts_in_jar =
      identity_manager->GetAccountsInCookieJar();
  const auto& signed_in_accounts =
      accounts_in_jar.GetPotentiallyInvalidSignedInAccounts();
  for (size_t i = 0; i < signed_in_accounts.size(); ++i) {
    if (signed_in_accounts[i].id == primary_account_id) {
      return static_cast<int>(i);
    }
  }
  return 0;
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

base::WeakPtr<DrivePickerUntrustedHostUI::Delegate>
DrivePickerHostUI::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

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
      untrusted_ui->SetDelegate(GetWeakPtr());
    }
  }
}

void DrivePickerHostUI::TriggerDrivePickerHost(
    std::unique_ptr<drive_picker_host::DrivePickerHostRequest> request) {
  if (untrusted_bridge_remote_.is_bound() &&
      untrusted_bridge_remote_.is_connected()) {
    if (request->type() ==
        drive_picker_host::DrivePickerHostRequest::RequestType::kPickerUi) {
      FetchTokenAndShowPicker(request->TakeResultHandler());
    } else if (request->type() == drive_picker_host::DrivePickerHostRequest::
                                      RequestType::kConsentDialog) {
      ShowConsentKitDialog(request->TakeResultHandler());
    }
  } else {
    if (pending_request_ && pending_request_->has_result_handler()) {
      mojo::Remote<drive_picker_host::mojom::DrivePickerResultHandler>(
          pending_request_->TakeResultHandler())
          ->OnCancel();
    }
    pending_request_ = std::move(request);
  }
}

void DrivePickerHostUI::SetBridge(
    mojo::PendingRemote<drive_picker_host_untrusted::mojom::DrivePickerBridge>
        untrusted_bridge) {
  untrusted_bridge_remote_.reset();
  untrusted_bridge_remote_.Bind(std::move(untrusted_bridge));
  untrusted_bridge_remote_.set_disconnect_handler(base::BindOnce(
      [](base::WeakPtr<DrivePickerHostUI> self) {
        if (self && self->pending_request_ &&
            self->pending_request_->has_result_handler()) {
          mojo::Remote<drive_picker_host::mojom::DrivePickerResultHandler>(
              self->pending_request_->TakeResultHandler())
              ->OnError(drive_picker_host::mojom::DrivePickerError::
                            kMojoDisconnected);
        }
      },
      weak_ptr_factory_.GetWeakPtr()));

  if (pending_request_) {
    if (pending_request_->type() ==
        drive_picker_host::DrivePickerHostRequest::RequestType::kPickerUi) {
      FetchTokenAndShowPicker(pending_request_->TakeResultHandler());
    } else if (pending_request_->type() ==
               drive_picker_host::DrivePickerHostRequest::RequestType::
                   kConsentDialog) {
      ShowConsentKitDialog(pending_request_->TakeResultHandler());
    }
  }
}

void DrivePickerHostUI::FetchTokenAndShowPicker(
    mojo::PendingRemote<drive_picker_host::mojom::DrivePickerResultHandler>
        result_handler) {
  VLOG(1) << "[DrivePickerHostUI] FetchTokenAndShowPicker called";
  Profile* profile = Profile::FromWebUI(web_ui());
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);

  if (access_token_fetcher_ || !identity_manager ||
      !identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    mojo::Remote<drive_picker_host::mojom::DrivePickerResultHandler> handler(
        std::move(result_handler));
    handler->OnError(
        drive_picker_host::mojom::DrivePickerError::kTokenFetchFailure);
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
  VLOG(1) << "[DrivePickerHostUI] OnAccessTokenFetched completed. ErrorState: "
          << error.state();
  access_token_fetcher_.reset();

  if (error.state() != GoogleServiceAuthError::NONE) {
    mojo::Remote<drive_picker_host::mojom::DrivePickerResultHandler> handler(
        std::move(result_handler));
    handler->OnError(
        drive_picker_host::mojom::DrivePickerError::kTokenFetchFailure);
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
    mojo::Remote<drive_picker_host::mojom::DrivePickerResultHandler> handler(
        std::move(result_handler));
    handler->OnError(
        drive_picker_host::mojom::DrivePickerError::kMojoDisconnected);
  }
}

void DrivePickerHostUI::BindInterface(
    mojo::PendingReceiver<drive_picker_host::mojom::DrivePickerHostHandler>
        receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
}

void DrivePickerHostUI::LoadConsentKitUrl(const GURL& consent_kit_url) {
  if (untrusted_bridge_remote_.is_bound() &&
      untrusted_bridge_remote_.is_connected()) {
    untrusted_bridge_remote_->LoadConsentKitUrl(consent_kit_url);
  }
}

void DrivePickerHostUI::OnConsentKitIframeMessage(
    mojo_base::ProtoWrapper message_wrapper) {
  VLOG(1) << "[DrivePickerHostUI] OnConsentKitIframeMessage received";
  std::optional<identity_consent::IframeMessage> message =
      message_wrapper.As<identity_consent::IframeMessage>();
  if (!message.has_value()) {
    DLOG(ERROR) << "Failed to parse IframeMessage proto";
    mojo::ReportBadMessage("Failed to parse IframeMessage proto");
    return;
  }

  if (message->event() == identity_consent::Event::DECISION_RESPONSE_EVENT) {
    if (message->has_privacy_flow_result()) {
      HandlePrivacyFlowResult(message->privacy_flow_result());
    }
  } else if (message->event() == identity_consent::Event::TERMINATE_EVENT) {
    if (consent_result_handler_) {
      consent_result_handler_->OnCancel();
      consent_result_handler_.reset();
    }
  }
}

void DrivePickerHostUI::OnConsentKitPrivacyFlowResult(
    mojo_base::ProtoWrapper result_wrapper) {
  VLOG(1) << "[DrivePickerHostUI] OnConsentKitPrivacyFlowResult received";
  std::optional<identity_consent::PrivacyFlowResult> result =
      result_wrapper.As<identity_consent::PrivacyFlowResult>();
  if (!result.has_value()) {
    DLOG(ERROR) << "Failed to parse PrivacyFlowResult proto";
    mojo::ReportBadMessage("Failed to parse PrivacyFlowResult proto");
    return;
  }

  HandlePrivacyFlowResult(*result);
}

void DrivePickerHostUI::OnConsentKitError(const std::string& error_message) {
  DLOG(ERROR) << "ConsentKit error: " << error_message;
  if (consent_result_handler_) {
    consent_result_handler_->OnError(
        drive_picker_host::mojom::DrivePickerError::kUnknown);
    consent_result_handler_.reset();
  }
}

void DrivePickerHostUI::HandlePrivacyFlowResult(
    const identity_consent::PrivacyFlowResult& result) {
  VLOG(1)
      << "[DrivePickerHostUI] HandlePrivacyFlowResult entry. flow_completed: "
      << result.has_flow_completed()
      << ", flow_not_completed: " << result.has_flow_not_completed();
  if (result.has_flow_id() &&
      result.flow_id().value() !=
          omnibox::kComposeboxDriveConsentFlowId.Get()) {
    DLOG(WARNING) << "Unexpected or missing flow_id";
    OnConsentKitError("Unexpected or missing flow_id");
    return;
  }

  if (result.has_flow_completed()) {
    VLOG(1) << "[DrivePickerHostUI] Handling flow_completed. "
               "Transitioning to Picker.";
    Profile* profile = Profile::FromWebUI(web_ui());
    profile->GetPrefs()->SetInteger(
        contextual_search::kDriveConsentState,
        static_cast<int>(contextual_search::DriveConsentState::kConsent));

    if (delegate_) {
      delegate_->OnTransitionToPicker();
    }

    // Transition straight to the Drive Picker UI.
    if (consent_result_handler_) {
      FetchTokenAndShowPicker(consent_result_handler_.Unbind());
    }
  } else if (result.has_flow_not_completed()) {
    if (consent_result_handler_) {
      consent_result_handler_->OnCancel();
      consent_result_handler_.reset();
    }
  }
}

void DrivePickerHostUI::ShowConsentKitDialog(
    mojo::PendingRemote<drive_picker_host::mojom::DrivePickerResultHandler>
        result_handler) {
  Profile* profile = Profile::FromWebUI(web_ui());
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);

  drive::ConsentKitUrlBuilder builder;
  if (identity_manager) {
    builder.SetSessionIndex(GetSessionIndexForPrimaryAccount(identity_manager));
  }
  builder.SetLocale(g_browser_process->GetApplicationLocale());
  builder.SetHostOrigins(
      {url::Origin::Create(GURL(chrome::kChromeUIDrivePickerHostUntrustedURL))
           .Serialize(),
       url::Origin::Create(GURL(chrome::kChromeUIDrivePickerHostURL))
           .Serialize()});
  builder.SetFlowId(omnibox::kComposeboxDriveConsentFlowId.Get());
  builder.SetProductId(omnibox::kComposeboxDriveConsentProductId.Get());
  builder.SetEntrypointId(omnibox::kComposeboxDriveConsentEntrypointId.Get());
  builder.SetDarkMode(
      ui::NativeTheme::GetInstanceForWeb()->preferred_color_scheme() ==
      ui::NativeTheme::PreferredColorScheme::kDark);

  GURL consent_kit_url = builder.Build();
  consent_result_handler_.reset();
  consent_result_handler_.Bind(std::move(result_handler));
  LoadConsentKitUrl(consent_kit_url);
}

WEB_UI_CONTROLLER_TYPE_IMPL(DrivePickerHostUI)
