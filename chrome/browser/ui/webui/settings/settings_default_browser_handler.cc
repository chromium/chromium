// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/settings_default_browser_handler.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/default_browser/default_browser_controller.h"
#include "chrome/browser/default_browser/default_browser_manager.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_prompt.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_prompt_manager.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_prompt_prefs.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/win/taskbar_manager.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/shell_util.h"
#endif

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

#if BUILDFLAG(IS_WIN)
void PinToTaskbarResult(bool result) {
  base::UmaHistogramBoolean("Windows.TaskbarPinFromSettingsSucceeded", result);
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace

DefaultBrowserHandler::DefaultBrowserHandler() = default;

DefaultBrowserHandler::~DefaultBrowserHandler() = default;

void DefaultBrowserHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "requestDefaultBrowserState",
      base::BindRepeating(&DefaultBrowserHandler::RequestDefaultBrowserState,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "requestUserValueStringsFeatureState",
      base::BindRepeating(
          &DefaultBrowserHandler::HandleRequestUserValueStringsFeatureState,
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

  default_browser_controller_ =
      default_browser::DefaultBrowserManager::CreateControllerFor(
          default_browser::DefaultBrowserEntrypointType::kSettingsPage);
  CHECK(default_browser_controller_);

  default_browser_controller_->OnShown();
  did_user_interact_ = false;
}

void DefaultBrowserHandler::OnJavascriptDisallowed() {
  if (!did_user_interact_) {
    default_browser_controller_->OnIgnored();
  }

  did_user_interact_ = false;
  local_state_pref_registrar_.RemoveAll();
  weak_ptr_factory_.InvalidateWeakPtrs();
  default_browser_controller_.reset();
}

void DefaultBrowserHandler::RequestDefaultBrowserState(
    const base::ListValue& args) {
  AllowJavascript();

  CHECK_EQ(args.size(), 1U);
  auto& callback_id = args[0].GetString();

  default_browser::DefaultBrowserManager::From(g_browser_process)
      ->GetDefaultBrowserState(
          base::BindOnce(&DefaultBrowserHandler::OnDefaultBrowserWorkerFinished,
                         weak_ptr_factory_.GetWeakPtr(), callback_id));
}

void DefaultBrowserHandler::HandleRequestUserValueStringsFeatureState(
    const base::ListValue& args) {
  AllowJavascript();

  CHECK_EQ(args.size(), 1U);
  auto& callback_id = args[0].GetString();

  bool is_enabled =
      base::FeatureList::IsEnabled(features::kUserValueDefaultBrowserStrings);
  ResolveJavascriptCallback(callback_id, base::Value(is_enabled));
}
void DefaultBrowserHandler::SetAsDefaultBrowser(const base::ListValue& args) {
  CHECK(!DefaultBrowserIsDisabledByPolicy());
  AllowJavascript();
  RecordSetAsDefaultUMA();

#if BUILDFLAG(IS_WIN)
  if (!args.empty() && args[0].GetBool()) {
    browser_util::PinAppToTaskbar(
        ShellUtil::GetBrowserModelId(InstallUtil::IsPerUserInstall()),
        browser_util::PinAppToTaskbarChannel::kSettingsPage,
        base::BindOnce(&PinToTaskbarResult));
  }
#endif  // BUILDFLAG(IS_WIN)

  did_user_interact_ = true;
  default_browser_controller_->OnAccepted(
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
  default_browser::DefaultBrowserManager::From(g_browser_process)
      ->GetDefaultBrowserState(
          base::BindOnce(&DefaultBrowserHandler::OnDefaultBrowserWorkerFinished,
                         weak_ptr_factory_.GetWeakPtr(), std::nullopt));
}

void DefaultBrowserHandler::RecordSetAsDefaultUMA() {
  base::RecordAction(base::UserMetricsAction("Options_SetAsDefaultBrowser"));
  UMA_HISTOGRAM_COUNTS("Settings.StartSetAsDefault", true);
}

void DefaultBrowserHandler::OnCanPinToTaskbarResult(
    const std::optional<std::string>& js_callback_id,
    shell_integration::DefaultWebClientState state,
    bool can_pin) {
  OnDefaultCheckFinished(js_callback_id, can_pin, state);
}

void DefaultBrowserHandler::OnDefaultBrowserWorkerFinished(
    const std::optional<std::string>& js_callback_id,
    shell_integration::DefaultWebClientState state) {
  if (state == shell_integration::IS_DEFAULT) {
    // Notify the user in the future if Chrome ceases to be the user's chosen
    // default browser.
    chrome::startup::default_prompt::ResetPromptPrefs(
        Profile::FromWebUI(web_ui()));
  } else {
#if BUILDFLAG(IS_WIN)
    if (base::FeatureList::IsEnabled(features::kOfferPinToTaskbarInSettings)) {
      browser_util::ShouldOfferToPin(
          ShellUtil::GetBrowserModelId(InstallUtil::IsPerUserInstall()),
          browser_util::PinAppToTaskbarChannel::kSettingsPage,
          base::BindOnce(&DefaultBrowserHandler::OnCanPinToTaskbarResult,
                         weak_ptr_factory_.GetWeakPtr(), js_callback_id,
                         state));
      return;
    }
#endif  // BUILDFLAG(IS_WIN)
  }
  OnDefaultCheckFinished(js_callback_id, /*can_pin=*/false, state);
}

void DefaultBrowserHandler::OnDefaultCheckFinished(
    const std::optional<std::string>& js_callback_id,
    bool can_pin,
    shell_integration::DefaultWebClientState state) {
  base::DictValue dict;
  dict.Set("isDefault", state == shell_integration::IS_DEFAULT);
  dict.Set("canPin", can_pin);
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
