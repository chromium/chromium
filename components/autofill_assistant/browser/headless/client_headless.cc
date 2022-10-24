// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/headless/client_headless.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/json/json_writer.h"
#include "base/metrics/field_trial_params.h"
#include "components/autofill_assistant/browser/autofill_assistant_tts_controller.h"
#include "components/autofill_assistant/browser/common_dependencies.h"
#include "components/autofill_assistant/browser/controller.h"
#include "components/autofill_assistant/browser/device_context.h"
#include "components/autofill_assistant/browser/display_strings_util.h"
#include "components/autofill_assistant/browser/features.h"
#include "components/autofill_assistant/browser/headless/headless_ui_controller.h"
#include "components/autofill_assistant/browser/public/password_change/empty_website_login_manager_impl.h"
#include "components/autofill_assistant/browser/public/password_change/website_login_manager.h"
#include "components/autofill_assistant/browser/public/password_change/website_login_manager_impl.h"
#include "components/autofill_assistant/browser/public/runtime_manager.h"
#include "components/autofill_assistant/browser/public/ui_state.h"
#include "components/autofill_assistant/browser/service/access_token_fetcher.h"
#include "components/autofill_assistant/browser/service/local_script_store.h"
#include "components/autofill_assistant/browser/service/no_round_trip_service.h"
#include "components/autofill_assistant/browser/switches.h"
#include "components/password_manager/content/browser/password_change_success_tracker_factory.h"
#include "components/password_manager/core/browser/password_change_success_tracker.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/security_state/core/security_state.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/tts_controller.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "url/gurl.h"

namespace autofill_assistant {

const char kOAuth2Scope[] = "https://www.googleapis.com/auth/userinfo.profile";
const char kConsumerName[] = "autofill_assistant";

ClientHeadless::ClientHeadless(
    content::WebContents* web_contents,
    const CommonDependencies* common_dependencies,
    ExternalActionDelegate* action_extension_delegate,
    WebsiteLoginManager* website_login_manager,
    const base::TickClock* tick_clock,
    base::WeakPtr<RuntimeManager> runtime_manager,
    ukm::UkmRecorder* ukm_recorder,
    AnnotateDomModelService* annotate_dom_model_service)
    : web_contents_(web_contents),
      common_dependencies_(common_dependencies),
      website_login_manager_(website_login_manager),
      tick_clock_(tick_clock),
      runtime_manager_(runtime_manager),
      ukm_recorder_(ukm_recorder),
      annotate_dom_model_service_(annotate_dom_model_service) {
  headless_ui_controller_ =
      std::make_unique<HeadlessUiController>(action_extension_delegate);
}

ClientHeadless::~ClientHeadless() = default;

void ClientHeadless::Start(
    const GURL& url,
    std::unique_ptr<TriggerContext> trigger_context,
    std::unique_ptr<Service> service,
    std::unique_ptr<WebController> web_controller,
    base::OnceCallback<void(Metrics::DropOutReason reason)>
        script_ended_callback) {
  // Ignore the call if a script is already running.
  if (script_ended_callback_) {
    return;
  }
  script_ended_callback_ = std::move(script_ended_callback);
  if (trigger_context->GetScriptParameters().GetIsNoRoundtrip().value_or(
          false)) {
    service =
        NoRoundTripService::Create(web_contents_->GetBrowserContext(), this);
  }
  controller_ = std::make_unique<Controller>(
      web_contents_, /* client= */ this, tick_clock_, runtime_manager_,
      std::move(service), std::move(web_controller), ukm_recorder_,
      annotate_dom_model_service_);
  controller_->AddObserver(headless_ui_controller_.get());
  controller_->Start(url, std::move(trigger_context));
}

bool ClientHeadless::IsRunning() const {
  // TODO(b/249979875): Use the runtime manager to check whether a controller is
  // running across all clients.
  return controller_ != nullptr;
}

void ClientHeadless::AttachUI() {}

void ClientHeadless::DestroyUISoon() {}

void ClientHeadless::DestroyUI() {}

version_info::Channel ClientHeadless::GetChannel() const {
  return common_dependencies_->GetChannel();
}

std::string ClientHeadless::GetEmailAddressForAccessTokenAccount() const {
  return GetSignedInEmail();
}

std::string ClientHeadless::GetSignedInEmail() const {
  return common_dependencies_->GetSignedInEmail();
}

absl::optional<std::pair<int, int>> ClientHeadless::GetWindowSize() const {
  return absl::nullopt;
}

ClientContextProto::ScreenOrientation ClientHeadless::GetScreenOrientation()
    const {
  return ClientContextProto::UNDEFINED_ORIENTATION;
}

void ClientHeadless::FetchPaymentsClientToken(
    base::OnceCallback<void(const std::string&)> callback) {
  NOTIMPLEMENTED() << "Payments client is not implemented for headless.";
  std::move(callback).Run("");
}

AccessTokenFetcher* ClientHeadless::GetAccessTokenFetcher() {
  return this;
}

autofill::PersonalDataManager* ClientHeadless::GetPersonalDataManager() const {
  return common_dependencies_->GetPersonalDataManager();
}

WebsiteLoginManager* ClientHeadless::GetWebsiteLoginManager() const {
  return website_login_manager_.get();
}

password_manager::PasswordChangeSuccessTracker*
ClientHeadless::GetPasswordChangeSuccessTracker() const {
  return password_manager::PasswordChangeSuccessTrackerFactory::
      GetForBrowserContext(GetWebContents()->GetBrowserContext());
}

std::string ClientHeadless::GetLocale() const {
  return common_dependencies_->GetLocale();
}

std::string ClientHeadless::GetLatestCountryCode() const {
  return common_dependencies_->GetLatestCountryCode();
}

std::string ClientHeadless::GetStoredPermanentCountryCode() const {
  return common_dependencies_->GetStoredPermanentCountryCode();
}

DeviceContext ClientHeadless::GetDeviceContext() const {
  return DeviceContext();
}

security_state::SecurityLevel ClientHeadless::GetSecurityLevel() const {
  return common_dependencies_->GetSecurityLevel(GetWebContents());
}

bool ClientHeadless::IsAccessibilityEnabled() const {
  return false;
}

bool ClientHeadless::IsSpokenFeedbackAccessibilityServiceEnabled() const {
  return false;
}

bool ClientHeadless::IsXmlSigned(const std::string& xml_string) const {
  return false;
}

const std::vector<std::string> ClientHeadless::ExtractValuesFromSingleTagXml(
    const std::string& xml_string,
    const std::vector<std::string>& keys) const {
  return (const std::vector<std::string>){};
}

content::WebContents* ClientHeadless::GetWebContents() const {
  return web_contents_;
}

void ClientHeadless::RecordDropOut(Metrics::DropOutReason reason) {}

bool ClientHeadless::HasHadUI() const {
  return false;
}

ScriptExecutorUiDelegate* ClientHeadless::GetScriptExecutorUiDelegate() {
  return headless_ui_controller_.get();
}

bool ClientHeadless::MustUseBackendData() const {
  return false;
}

void ClientHeadless::GetAnnotateDomModelVersion(
    base::OnceCallback<void(absl::optional<int64_t>)> callback) const {
  std::move(callback).Run(absl::nullopt);
}

void ClientHeadless::Shutdown(Metrics::DropOutReason reason) {
  // This call can cause Controller to be destroyed. For this reason we delay it
  // to avoid UAF errors in the controller.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&ClientHeadless::NotifyScriptEnded,
                                weak_ptr_factory_.GetWeakPtr(), reason));
}

void ClientHeadless::NotifyScriptEnded(Metrics::DropOutReason reason) {
  if (script_ended_callback_) {
    std::move(script_ended_callback_).Run(reason);
  }

  // This instance can be destroyed by the above call, so nothing should be
  // added here.
}

void ClientHeadless::FetchAccessToken(
    base::OnceCallback<void(bool, const std::string&)> callback) {
  DCHECK(!fetch_access_token_callback_);
  fetch_access_token_callback_ = std::move(callback);
  auto* identity_manager = common_dependencies_->GetIdentityManager();
  access_token_fetcher_ = identity_manager->CreateAccessTokenFetcherForAccount(
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSync),
      kConsumerName, {kOAuth2Scope},
      base::BindOnce(&ClientHeadless::OnAccessTokenFetchComplete,
                     weak_ptr_factory_.GetWeakPtr()),
      signin::AccessTokenFetcher::Mode::kImmediate);
}

void ClientHeadless::OnAccessTokenFetchComplete(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  if (!fetch_access_token_callback_) {
    return;
  }

  if (error.state() != GoogleServiceAuthError::NONE) {
    VLOG(2) << "OAuth2 token request failed. " << error.state() << ": "
            << error.ToString();

    std::move(fetch_access_token_callback_).Run(false, "");
    return;
  }
  std::move(fetch_access_token_callback_).Run(true, access_token_info.token);
}

void ClientHeadless::InvalidateAccessToken(const std::string& access_token) {
  auto* identity_manager = common_dependencies_->GetIdentityManager();
  identity_manager->RemoveAccessTokenFromCache(
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSync),
      {kOAuth2Scope}, access_token);
}

bool ClientHeadless::GetMakeSearchesAndBrowsingBetterEnabled() const {
  return common_dependencies_->GetMakeSearchesAndBrowsingBetterEnabled();
}

bool ClientHeadless::GetMetricsReportingEnabled() const {
  return common_dependencies_->GetMetricsReportingEnabled();
}

}  // namespace autofill_assistant
