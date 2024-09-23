// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/settings_default_browser_handler.h"

#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_prompt.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_prompt_manager.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_prompt_prefs.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"

namespace settings {

namespace {

bool DefaultBrowserIsDisabledByPolicy() {
  const PrefService::Preference* pref =
      g_browser_process->local_state()->FindPreference(
          prefs::kDefaultBrowserSettingEnabled);
  DCHECK(pref);
  DCHECK(pref->GetValue()->is_bool());
  return pref->IsManaged() && !pref->GetValue()->GetBool();
}

}  // namespace

DefaultBrowserHandler::DefaultBrowserHandler() = default;

DefaultBrowserHandler::~DefaultBrowserHandler() = default;

void DefaultBrowserHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "requestDefaultBrowserState",
      base::BindRepeating(&DefaultBrowserHandler::RequestDefaultBrowserState,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setAsDefaultBrowser",
      base::BindRepeating(&DefaultBrowserHandler::SetAsDefaultBrowser,
                          base::Unretained(this)));
}

void DefaultBrowserHandler::OnJavascriptAllowed() {
  PrefService* prefs = g_browser_process->local_state();
  local_state_pref_registrar_.Init(prefs);
  local_state_pref_registrar_.Add(
      prefs::kDefaultBrowserSettingEnabled,
      base::BindRepeating(&DefaultBrowserHandler::OnDefaultBrowserSettingChange,
                          base::Unretained(this)));
  default_browser_worker_ = new shell_integration::DefaultBrowserWorker();
}

void DefaultBrowserHandler::OnJavascriptDisallowed() {
  local_state_pref_registrar_.RemoveAll();
  weak_ptr_factory_.InvalidateWeakPtrs();
  default_browser_worker_ = nullptr;
}

void DefaultBrowserHandler::RequestDefaultBrowserState(
    const base::Value::List& args) {
  AllowJavascript();

  CHECK_EQ(args.size(), 1U);
  auto& callback_id = args[0].GetString();

  default_browser_worker_->StartCheckIsDefault(
      base::BindOnce(&DefaultBrowserHandler::OnDefaultBrowserWorkerFinished,
                     weak_ptr_factory_.GetWeakPtr(), callback_id));
}

void DefaultBrowserHandler::SetAsDefaultBrowser(const base::Value::List& args) {
  CHECK(!DefaultBrowserIsDisabledByPolicy());
  AllowJavascript();
  RecordSetAsDefaultUMA();

  default_browser_worker_->StartSetAsDefault(
      base::BindOnce(&DefaultBrowserHandler::OnDefaultBrowserWorkerFinished,
                     weak_ptr_factory_.GetWeakPtr(), std::nullopt));

  // If the user attempted to make Chrome the default browser, notify
  // them when this changes and close all open prompts.
  chrome::startup::default_prompt::UpdatePrefsForDismissedPrompt(
      Profile::FromWebUI(web_ui()));
  DefaultBrowserPromptManager::GetInstance()->CloseAllPrompts(
      DefaultBrowserPromptManager::CloseReason::kAccept);
}

void DefaultBrowserHandler::OnDefaultBrowserSettingChange() {
  default_browser_worker_->StartCheckIsDefault(
      base::BindOnce(&DefaultBrowserHandler::OnDefaultBrowserWorkerFinished,
                     weak_ptr_factory_.GetWeakPtr(), std::nullopt));
}

void DefaultBrowserHandler::RecordSetAsDefaultUMA() {
  base::RecordAction(base::UserMetricsAction("Options_SetAsDefaultBrowser"));
  UMA_HISTOGRAM_COUNTS("Settings.StartSetAsDefault", true);
}

void DefaultBrowserHandler::OnDefaultBrowserWorkerFinished(
    const std::optional<std::string>& js_callback_id,
    shell_integration::DefaultWebClientState state) {
  if (state == shell_integration::IS_DEFAULT) {
    // Notify the user in the future if Chrome ceases to be the user's chosen
    // default browser.
    chrome::startup::default_prompt::ResetPromptPrefs(
        Profile::FromWebUI(web_ui()));
  }

  base::Value::Dict dict;
  dict.Set("isDefault", state == shell_integration::IS_DEFAULT);
  dict.Set("canBeDefault", shell_integration::CanSetAsDefaultBrowser());
  dict.Set("isUnknownError", state == shell_integration::UNKNOWN_DEFAULT);
  dict.Set("isDisabledByPolicy", DefaultBrowserIsDisabledByPolicy());

  if (js_callback_id) {
    ResolveJavascriptCallback(base::Value(*js_callback_id), dict);
  } else {
    FireWebUIListener("browser-default-state-changed", dict);
  }
}

}  // namespace settings
