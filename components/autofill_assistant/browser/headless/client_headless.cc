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
#include "components/autofill_assistant/browser/features.h"
#include "components/autofill_assistant/browser/public/ui_state.h"
#include "components/autofill_assistant/browser/service/access_token_fetcher.h"
#include "components/autofill_assistant/browser/switches.h"
#include "components/autofill_assistant/browser/website_login_manager_impl.h"
#include "components/password_manager/core/browser/password_change_success_tracker.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/tts_controller.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "url/gurl.h"

namespace autofill_assistant {

ClientHeadless::ClientHeadless(content::WebContents* web_contents)
    : web_contents_(web_contents) {}

ClientHeadless::~ClientHeadless() = default;

bool ClientHeadless::Start(
    const GURL& url,
    std::unique_ptr<TriggerContext> trigger_context,
    password_manager::PasswordManagerClient* password_manager_client,
    password_manager::PasswordChangeSuccessTracker*
        password_change_success_tracker) {
  password_manager_client_ = password_manager_client;
  password_change_success_tracker_ = password_change_success_tracker;
  website_login_manager_ = std::make_unique<WebsiteLoginManagerImpl>(
      password_manager_client_, GetWebContents());
  CreateController();

  DCHECK(!trigger_context->GetDirectAction());
  if (VLOG_IS_ON(2)) {
    DVLOG(2) << "Starting autofill assistant in headless with parameters:";
    DVLOG(2) << "\ttarget_url: " << url;
    DVLOG(2) << "\texperiment_ids: " << trigger_context->GetExperimentIds();
    DVLOG(2) << "\tparameters:";
    auto parameters = trigger_context->GetScriptParameters().ToProto();
    for (const auto& param : parameters) {
      DVLOG(2) << "\t\t" << param.name() << ": " << param.value();
    }
  }
  return controller_->Start(url, std::move(trigger_context));
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
  // TODO(b/201964911): Inject on instantiation.
  return version_info::Channel::DEV;
}

std::string ClientHeadless::GetEmailAddressForAccessTokenAccount() const {
  // TODO(b/201964911): return the Chrome signed in user.
  return "";
}

std::string ClientHeadless::GetChromeSignedInEmailAddress() const {
  // TODO(b/201964911): return the Chrome signed in user.
  return "";
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
  // TODO(b/201964911): get access token via native.
  return nullptr;
}

autofill::PersonalDataManager* ClientHeadless::GetPersonalDataManager() const {
  // TODO(b/201964911): support PersonalDataManager.
  return nullptr;
}

WebsiteLoginManager* ClientHeadless::GetWebsiteLoginManager() const {
  return website_login_manager_.get();
}

password_manager::PasswordChangeSuccessTracker*
ClientHeadless::GetPasswordChangeSuccessTracker() const {
  return password_change_success_tracker_;
}

std::string ClientHeadless::GetLocale() const {
  // TODO(b/201964911): get locale via native.
  return "en-us";
}

std::string ClientHeadless::GetCountryCode() const {
  // TODO(b/201964911): get country code via native.
  return "us";
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

void ClientHeadless::RecordDropOut(Metrics::DropOutReason reason) {
  // TODO(b/201964911): Add metrics.
}

bool ClientHeadless::HasHadUI() const {
  return false;
}

ScriptExecutorUiDelegate* ClientHeadless::GetScriptExecutorUiDelegate() {
  // TODO(b/201964911): Create headless ScriptExecutorUiDelegate.
  return nullptr;
}

void ClientHeadless::Shutdown(Metrics::DropOutReason reason) {
  if (!controller_)
    return;

  // Shutdown in a separate task. This avoids tricky ordering issues when
  // Shutdown is called from the controller.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&ClientHeadless::SafeDestroyController,
                                weak_ptr_factory_.GetWeakPtr(), reason));
}

void ClientHeadless::SafeDestroyController(Metrics::DropOutReason reason) {
  DestroyController();
}

void ClientHeadless::FetchAccessToken(
    base::OnceCallback<void(bool, const std::string&)> callback) {
  // TODO(b/201964911): get access token via native.
  std::move(callback).Run(false, "");
}

void ClientHeadless::InvalidateAccessToken(const std::string& access_token) {
  // TODO(b/201964911): get access token via native.
}

void ClientHeadless::CreateController() {
  // TODO(b/201964911): add a check to RuntimeManager to make sure only one
  // instance of the Controller exists per WebContents across the different
  // instances of Clients.
  DestroyController();
  controller_ = std::make_unique<Controller>(
      GetWebContents(), /* client= */ this,
      base::DefaultTickClock::GetInstance(),
      RuntimeManager::GetForWebContents(GetWebContents())->GetWeakPtr(),
      /* service= */ nullptr, ukm::UkmRecorder::Get(),
      /*annotate_dom_model_service= */ nullptr);
}

void ClientHeadless::DestroyController() {
  controller_.reset();
}

}  // namespace autofill_assistant
