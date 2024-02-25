// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/parent_access/parent_access_ui_handler_impl.h"

#include <optional>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/webui/ash/parent_access/parent_access_callback.pb.h"
#include "chrome/browser/ui/webui/ash/parent_access/parent_access_dialog.h"
#include "chrome/browser/ui/webui/ash/parent_access/parent_access_metrics_utils.h"
#include "chrome/browser/ui/webui/ash/parent_access/parent_access_ui.mojom.h"
#include "components/google/core/common/google_util.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "google_apis/gaia/gaia_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "url/gurl.h"

namespace ash {

namespace {
constexpr char kParentAccessDefaultURL[] =
    "https://families.google.com/parentaccess";
constexpr char kParentAccessSwitch[] = "parent-access-url";

// Returns the caller id to be used for the web widget.  The caller id is
// mapped from the flow type.   When new flow types are added to
// ParentAccessParams, a new case statement should be added here.
std::string GetCallerId(
    parent_access_ui::mojom::ParentAccessParams::FlowType flow_type) {
  switch (flow_type) {
    case parent_access_ui::mojom::ParentAccessParams::FlowType::kWebsiteAccess:
      return "39454505";
    case parent_access_ui::mojom::ParentAccessParams::FlowType::
        kExtensionAccess:
      return "12367dff";
      // NOTE:  Do not add default case here, to ensure that adding new flow
      // types to ParentAccessParams forces this case statement to be updated.
  }
}
}  // namespace

void ParentAccessUiHandlerImpl::RecordParentAccessWidgetError(
    ParentAccessUiHandlerImpl::ParentAccessWidgetError error) {
  if (delegate_) {
    base::UmaHistogramEnumeration(
        parent_access::GetHistogramTitleForFlowType(
            parent_access::kParentAccessWidgetErrorHistogramBase,
            params_->flow_type),
        error);
  }

  // Always record metric for "all" flow type.
  base::UmaHistogramEnumeration(
      parent_access::GetHistogramTitleForFlowType(
          parent_access::kParentAccessWidgetErrorHistogramBase, std::nullopt),
      error);
}

ParentAccessUiHandlerImpl::ParentAccessUiHandlerImpl(
    mojo::PendingReceiver<parent_access_ui::mojom::ParentAccessUiHandler>
        receiver,
    signin::IdentityManager* identity_manager,
    ParentAccessUiHandlerDelegate* delegate)
    : identity_manager_(identity_manager),
      delegate_(delegate),
      receiver_(this, std::move(receiver)),
      params_(delegate_ ? delegate_->CloneParentAccessParams() : nullptr) {
  // ParentAccess state is only tracked when a dialog is created. i.e. not when
  // chrome://parent-access is directly accessed.
  if (delegate_) {
    state_tracker_ = std::make_unique<ParentAccessStateTracker>(
        params_->flow_type, params_->is_disabled);
  }
}

ParentAccessUiHandlerImpl::~ParentAccessUiHandlerImpl() = default;

void ParentAccessUiHandlerImpl::GetOauthToken(GetOauthTokenCallback callback) {
  signin::ScopeSet scopes;
  scopes.insert(GaiaConstants::kParentApprovalOAuth2Scope);
  scopes.insert(GaiaConstants::kProgrammaticChallengeOAuth2Scope);

  if (oauth2_access_token_fetcher_) {
    // Only one GetOauthToken call can happen at a time.
    std::move(callback).Run(
        parent_access_ui::mojom::GetOauthTokenStatus::kOnlyOneFetchAtATime, "");
    return;
  }

  oauth2_access_token_fetcher_ =
      identity_manager_->CreateAccessTokenFetcherForAccount(
          identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSync),
          "parent_access", scopes,
          base::BindOnce(&ParentAccessUiHandlerImpl::OnAccessTokenFetchComplete,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
          signin::AccessTokenFetcher::Mode::kImmediate);
}

void ParentAccessUiHandlerImpl::OnAccessTokenFetchComplete(
    GetOauthTokenCallback callback,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  oauth2_access_token_fetcher_.reset();
  if (error.state() != GoogleServiceAuthError::NONE) {
    DLOG(ERROR) << "ParentAccessUiHandlerImpl: OAuth2 token request failed. "
                << error.state() << ": " << error.ToString();
    RecordParentAccessWidgetError(
        ParentAccessUiHandlerImpl::ParentAccessWidgetError::kOAuthError);
    std::move(callback).Run(
        parent_access_ui::mojom::GetOauthTokenStatus::kError,
        "" /* No token */);
    return;
  }
  std::move(callback).Run(
      parent_access_ui::mojom::GetOauthTokenStatus::kSuccess,
      access_token_info.token);
}

void ParentAccessUiHandlerImpl::GetParentAccessParams(
    GetParentAccessParamsCallback callback) {
  if (!delegate_) {
    LOG(ERROR) << "Delegate not available in ParentAccessUiHandler - WebUI was "
                  "probably created without a dialog";
    RecordParentAccessWidgetError(
        ParentAccessUiHandlerImpl::ParentAccessWidgetError::
            kDelegateNotAvailable);
    std::move(callback).Run(parent_access_ui::mojom::ParentAccessParams::New());
    return;
  }
  std::move(callback).Run(params_->Clone());
  return;
}

void ParentAccessUiHandlerImpl::OnParentAccessDone(
    parent_access_ui::mojom::ParentAccessResult result,
    OnParentAccessDoneCallback callback) {
  if (!delegate_) {
    LOG(ERROR) << "Delegate not available in ParentAccessUiHandler - WebUI was "
                  "probably created without a dialog";
    RecordParentAccessWidgetError(
        ParentAccessUiHandlerImpl::ParentAccessWidgetError::
            kDelegateNotAvailable);
    std::move(callback).Run();
    return;
  }
  switch (result) {
    case parent_access_ui::mojom::ParentAccessResult::kApproved:
      DCHECK(parent_access_token_);
      if (state_tracker_) {
        state_tracker_->OnWebUiStateChanged(
            ParentAccessStateTracker::FlowResult::kAccessApproved);
      }
      delegate_->SetApproved(
          parent_access_token_->token(),
          // Only keep the seconds, not the nanoseconds.
          base::Time::FromSecondsSinceUnixEpoch(
              parent_access_token_->expire_time().seconds()));
      break;
    case parent_access_ui::mojom::ParentAccessResult::kDeclined:
      if (state_tracker_) {
        state_tracker_->OnWebUiStateChanged(
            ParentAccessStateTracker::FlowResult::kAccessDeclined);
      }
      delegate_->SetDeclined();
      break;
    case parent_access_ui::mojom::ParentAccessResult::kCanceled:
      delegate_->SetCanceled();
      break;
    case parent_access_ui::mojom::ParentAccessResult::kError:
      if (state_tracker_) {
        state_tracker_->OnWebUiStateChanged(
            ParentAccessStateTracker::FlowResult::kError);
      }
      delegate_->SetError();
      break;
    case parent_access_ui::mojom::ParentAccessResult::kDisabled:
      delegate_->SetDisabled();
      break;
  }

  std::move(callback).Run();
}

void ParentAccessUiHandlerImpl::GetParentAccessUrl(
    GetParentAccessUrlCallback callback) {
  if (!delegate_) {
    LOG(ERROR) << "Delegate not available in ParentAccessUiHandler - WebUI was "
                  "probably created without a dialog";
    RecordParentAccessWidgetError(
        ParentAccessUiHandlerImpl::ParentAccessWidgetError::
            kDelegateNotAvailable);
    std::move(callback).Run("");
    return;
  }

  std::string platform_version = base::SysInfo::OperatingSystemVersion();
  std::string language_code =
      google_util::GetGoogleLocale(g_browser_process->GetApplicationLocale());

  std::string url;
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kParentAccessSwitch)) {
    url = command_line->GetSwitchValueASCII(kParentAccessSwitch);
  } else {
    url = kParentAccessDefaultURL;
    DCHECK(GURL(url).DomainIs("google.com"));
  }

  const GURL base_url(url);
  GURL::Replacements replacements;
  std::string query_string = base::StringPrintf(
      "callerid=%s&hl=%s&platform_version=%s&cros-origin=chrome://"
      "parent-access",
      GetCallerId(params_->flow_type).c_str(), language_code.c_str(),
      platform_version.c_str());
  replacements.SetQueryStr(query_string);
  const GURL result = base_url.ReplaceComponents(replacements);
  DCHECK(result.is_valid()) << "Invalid URL \"" << url << "\" for switch \""
                            << kParentAccessSwitch << "\"";
  std::move(callback).Run(result.spec());
}

void ParentAccessUiHandlerImpl::OnBeforeScreenDone(
    OnBeforeScreenDoneCallback callback) {
  if (state_tracker_) {
    state_tracker_->OnWebUiStateChanged(
        ParentAccessStateTracker::FlowResult::kParentAuthentication);
  }
  std::move(callback).Run();
}

const kids::platform::parentaccess::client::proto::ParentAccessToken*
ParentAccessUiHandlerImpl::GetParentAccessTokenForTest() {
  return parent_access_token_.get();
}

void ParentAccessUiHandlerImpl::OnParentAccessCallbackReceived(
    const std::string& encoded_parent_access_callback_proto,
    OnParentAccessCallbackReceivedCallback callback) {
  std::string decoded_parent_access_callback;
  parent_access_ui::mojom::ParentAccessServerMessagePtr message =
      parent_access_ui::mojom::ParentAccessServerMessage::New();
  if (!base::Base64Decode(encoded_parent_access_callback_proto,
                          &decoded_parent_access_callback)) {
    LOG(ERROR) << "ParentAccessHandler::ParentAccessResult: Error decoding "
                  "parent_access_result from base64";
    RecordParentAccessWidgetError(
        ParentAccessUiHandlerImpl::ParentAccessWidgetError::kDecodingError);

    message->type =
        parent_access_ui::mojom::ParentAccessServerMessageType::kError;
    std::move(callback).Run(std::move(message));
    return;
  }

  kids::platform::parentaccess::client::proto::ParentAccessCallback
      parent_access_callback;
  if (!parent_access_callback.ParseFromString(decoded_parent_access_callback)) {
    LOG(ERROR) << "ParentAccessHandler::ParentAccessResult: Error parsing "
                  "decoded_parent_access_result to proto";
    RecordParentAccessWidgetError(
        ParentAccessUiHandlerImpl::ParentAccessWidgetError::kParsingError);

    message->type =
        parent_access_ui::mojom::ParentAccessServerMessageType::kError;
    std::move(callback).Run(std::move(message));
    return;
  }

  switch (parent_access_callback.callback_case()) {
    case kids::platform::parentaccess::client::proto::ParentAccessCallback::
        CallbackCase::kOnParentVerified:
      message->type = parent_access_ui::mojom::ParentAccessServerMessageType::
          kParentVerified;
      if (state_tracker_) {
        state_tracker_->OnWebUiStateChanged(
            ParentAccessStateTracker::FlowResult::kApproval);
      }
      if (parent_access_callback.on_parent_verified()
              .verification_proof_case() ==
          kids::platform::parentaccess::client::proto::OnParentVerified::
              VerificationProofCase::kParentAccessToken) {
        DCHECK(!parent_access_token_);
        parent_access_token_ = std::make_unique<
            kids::platform::parentaccess::client::proto::ParentAccessToken>();
        parent_access_token_->CopyFrom(
            parent_access_callback.on_parent_verified().parent_access_token());
      }
      std::move(callback).Run(std::move(message));
      break;
    default:
      VLOG(0) << "ParentAccessHandler::OnParentAccessCallback: Unknown type of "
                 "callback received and ignored: "
              << parent_access_callback.callback_case();
      RecordParentAccessWidgetError(
          ParentAccessUiHandlerImpl::ParentAccessWidgetError::kUnknownCallback);
      message->type =
          parent_access_ui::mojom::ParentAccessServerMessageType::kIgnore;
      std::move(callback).Run(std::move(message));
      break;
  }
}

}  // namespace ash
