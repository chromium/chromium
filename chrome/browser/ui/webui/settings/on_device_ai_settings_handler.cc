// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/on_device_ai_settings_handler.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/feedback/show_feedback_page.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/model_execution/model_execution_util.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"

using optimization_guide::model_execution::prefs::localstate::
    kOnDeviceAiUserSettingsEnabled;

namespace features {
// Feature flag for "On-device AI" toggle on chrome://settings page.
BASE_FEATURE(kShowOnDeviceAiSettings, base::FEATURE_ENABLED_BY_DEFAULT);
}  // namespace features

namespace settings {

OnDeviceAiSettingsHandler::OnDeviceAiSettingsHandler() = default;

OnDeviceAiSettingsHandler::~OnDeviceAiSettingsHandler() = default;

void OnDeviceAiSettingsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getOnDeviceAiEnabled",
      base::BindRepeating(
          &OnDeviceAiSettingsHandler::HandleGetOnDeviceAiEnabled,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setOnDeviceAiEnabled",
      base::BindRepeating(
          &OnDeviceAiSettingsHandler::HandleSetOnDeviceAiEnabled,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "openOnDeviceAiFeedbackDialog",
      base::BindRepeating(&OnDeviceAiSettingsHandler::HandleOpenFeedbackDialog,
                          base::Unretained(this)));
}

void OnDeviceAiSettingsHandler::OnJavascriptAllowed() {
  pref_member_ = std::make_unique<BooleanPrefMember>();
  pref_member_->Init(
      kOnDeviceAiUserSettingsEnabled, g_browser_process->local_state(),
      base::BindRepeating(&OnDeviceAiSettingsHandler::OnPrefChange,
                          base::Unretained(this)));
}

void OnDeviceAiSettingsHandler::OnJavascriptDisallowed() {
  pref_member_.reset();
}

void OnDeviceAiSettingsHandler::OnPrefChange() {
  SendOnDeviceAiEnabledChange();
}

base::DictValue OnDeviceAiSettingsHandler::GetOnDeviceAiState() {
  PrefService* local_state = g_browser_process->local_state();
  base::DictValue result;
  result.Set("enabled",
             local_state->GetBoolean(kOnDeviceAiUserSettingsEnabled));
  using optimization_guide::model_execution::prefs::
      GenAILocalFoundationalModelEnterprisePolicySettings;
  bool allowedByPolicy =
      optimization_guide::
          GetGenAILocalFoundationalModelEnterprisePolicySettings(local_state) ==
      GenAILocalFoundationalModelEnterprisePolicySettings::kAllowed;
  result.Set("allowedByPolicy", allowedByPolicy);
  return result;
}

void OnDeviceAiSettingsHandler::HandleGetOnDeviceAiEnabled(
    const base::ListValue& args) {
  AllowJavascript();
  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];

  ResolveJavascriptCallback(callback_id, GetOnDeviceAiState());
}

void OnDeviceAiSettingsHandler::HandleSetOnDeviceAiEnabled(
    const base::ListValue& args) {
  CHECK_EQ(1U, args.size());

  PrefService* local_state = g_browser_process->local_state();
  using optimization_guide::model_execution::prefs::
      GenAILocalFoundationalModelEnterprisePolicySettings;
  bool disallowed_by_policy =
      optimization_guide::
          GetGenAILocalFoundationalModelEnterprisePolicySettings(local_state) ==
      GenAILocalFoundationalModelEnterprisePolicySettings::kDisallowed;
  if (disallowed_by_policy) {
    LOG(ERROR) << "Cannot set on-device AI user setting when disallowed "
                  "by policy.";
    return;
  }

  bool enabled = args[0].GetBool();
  local_state->SetBoolean(kOnDeviceAiUserSettingsEnabled, enabled);
}

void OnDeviceAiSettingsHandler::HandleOpenFeedbackDialog(
    const base::ListValue& args) {
  CHECK_EQ(0U, args.size());

  BrowserWindowInterface* browser =
      webui::GetBrowserWindowInterface(web_ui()->GetWebContents());
  DCHECK(browser);
  std::string unused;
  chrome::ShowFeedbackPage(browser, feedback::kFeedbackSourceOnDeviceAI, unused,
                           unused, "on_device_ai", unused);
}

void OnDeviceAiSettingsHandler::SendOnDeviceAiEnabledChange() {
  FireWebUIListener("on-device-ai-enabled-changed", GetOnDeviceAiState());
}

}  // namespace settings
