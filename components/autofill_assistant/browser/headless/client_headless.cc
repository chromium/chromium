// Copyright 2022 The Chromium Authors. All rights reserved.
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
#include "base/time/default_tick_clock.h"
#include "components/autofill_assistant/browser/autofill_assistant_tts_controller.h"
#include "components/autofill_assistant/browser/controller.h"
#include "components/autofill_assistant/browser/display_strings_util.h"
#include "components/autofill_assistant/browser/empty_website_login_manager_impl.h"
#include "components/autofill_assistant/browser/features.h"
#include "components/autofill_assistant/browser/public/ui_state.h"
#include "components/autofill_assistant/browser/service/access_token_fetcher.h"
#include "components/autofill_assistant/browser/switches.h"
#include "components/autofill_assistant/browser/website_login_manager_impl.h"
#include "components/password_manager/content/browser/password_change_success_tracker_factory.h"
#include "components/password_manager/core/browser/password_change_success_tracker.h"
#include "components/password_manager/core/browser/password_manager_client.h"
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
    ExternalActionDelegate* action_extension_delegate)
    : web_contents_(web_contents), common_dependencies_(common_dependencies) {
  auto* password_manager_client =
      common_dependencies_->GetPasswordManagerClient(web_contents);
  if (password_manager_client) {
    website_login_manager_ = std::make_unique<WebsiteLoginManagerImpl>(
        password_manager_client, web_contents);
  } else {
    website_login_manager_ = std::make_unique<EmptyWebsiteLoginManagerImpl>();
  }
  headless_ui_controller_ =
      std::make_unique<HeadlessUiController>(action_extension_delegate);
}

ClientHeadless::~ClientHeadless() = default;

void ClientHeadless::Start(const GURL& url,
                           std::unique_ptr<TriggerContext> trigger_context,
                           ControllerObserver* observer) {
  controller_ = std::make_unique<Controller>(
      web_contents_, /* client= */ this, base::DefaultTickClock::GetInstance(),
      RuntimeManager::GetForWebContents(web_contents_)->GetWeakPtr(),
      /* service= */ nullptr, ukm::UkmRecorder::Get(),
      /* annotate_dom_model_service= */
      common_dependencies_->GetOrCreateAnnotateDomModelService(
          GetWebContents()->GetBrowserContext()));
  controller_->AddObserver(observer);
  controller_->Start(url, std::move(trigger_context));
}

bool ClientHeadless::IsRunning() const {
  // TODO(b/201964911): Use the runtime manager to check whether a controller is
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
  return common_dependencies_->GetSignedInEmail(GetWebContents());
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
  // TODO(b/201964911): support payments client.
  std::move(callback).Run("");
}

AccessTokenFetcher* ClientHeadless::GetAccessTokenFetcher() {
  return this;
}

autofill::PersonalDataManager* ClientHeadless::GetPersonalDataManager() const {
  return common_dependencies_->GetPersonalDataManager();
}

WebsiteLoginManager* ClientHeadless::GetWebsiteLoginManager() const {
  // TODO(b/201964911): return instance.
  return nullptr;
}

password_manager::PasswordChangeSuccessTracker*
ClientHeadless::GetPasswordChangeSuccessTracker() const {
  return password_manager::PasswordChangeSuccessTrackerFactory::
      GetForBrowserContext(GetWebContents()->GetBrowserContext());
}

std::string ClientHeadless::GetLocale() const {
  return common_dependencies_->GetLocale();
}

std::string ClientHeadless::GetCountryCode() const {
  return common_dependencies_->GetCountryCode();
}

DeviceContext ClientHeadless::GetDeviceContext() const {
  return DeviceContext();
}

bool ClientHeadless::IsAccessibilityEnabled() const {
  return false;
}

bool ClientHeadless::IsSpokenFeedbackAccessibilityServiceEnabled() const {
  return false;
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

void ClientHeadless::Shutdown(Metrics::DropOutReason reason) {}

void ClientHeadless::FetchAccessToken(
    base::OnceCallback<void(bool, const std::string&)> callback) {
  DCHECK(!fetch_access_token_callback_);
  fetch_access_token_callback_ = std::move(callback);
  auto* identity_manager = common_dependencies_->GetIdentityManager(
      GetWebContents()->GetBrowserContext());
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
  auto* identity_manager = common_dependencies_->GetIdentityManager(
      GetWebContents()->GetBrowserContext());
  identity_manager->RemoveAccessTokenFromCache(
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSync),
      {kOAuth2Scope}, access_token);
}

}  // namespace autofill_assistant
