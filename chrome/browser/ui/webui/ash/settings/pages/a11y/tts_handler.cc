// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/a11y/tts_handler.h"

#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/json/json_reader.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/extension_api/tts_engine_extension_api.h"
#include "chrome/browser/speech/extension_api/tts_engine_extension_observer_chromeos.h"
#include "chrome/browser/speech/extension_api/tts_engine_extension_observer_chromeos_factory.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/tts_controller.h"
#include "content/public/browser/web_ui.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest_handlers/options_page_info.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash::settings {

TtsHandler::TtsHandler() = default;

TtsHandler::~TtsHandler() = default;

void TtsHandler::HandleGetTtsExtensions(const base::Value::List& args) {
  // Ensure the built in tts engine is loaded to be able to respond to messages.
  WakeTtsEngine(base::Value::List());

  base::Value::List responses;
  Profile* profile = Profile::FromWebUI(web_ui());
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile);

  const std::set<std::string> extensions =
      TtsEngineExtensionObserverChromeOSFactory::GetForProfile(profile)
          ->engine_extension_ids();
  std::set<std::string>::const_iterator iter;
  for (iter = extensions.begin(); iter != extensions.end(); ++iter) {
    const std::string extension_id = *iter;
    const extensions::Extension* extension =
        registry->GetInstalledExtension(extension_id);
    if (!extension) {
      // The extension is still loading from OnVoicesChange call to
      // TtsController::GetVoices(). Don't do any work, voices will
      // be updated again after extension load.
      continue;
    }
    base::Value::Dict response;
    response.Set("name", extension->name());
    response.Set("extensionId", extension_id);
    if (extensions::OptionsPageInfo::HasOptionsPage(extension)) {
      response.Set(
          "optionsPage",
          extensions::OptionsPageInfo::GetOptionsPage(extension).spec());
    }
    responses.Append(std::move(response));
  }

  FireWebUIListener("tts-extensions-updated", responses);
}

void TtsHandler::HandleGetDisplayNameForLocale(const base::Value::List& args) {
  CHECK_EQ(2U, args.size());
  const std::string callback_id = args[0].GetString();
  const std::string locale = args[1].GetString();

  const std::u16string display_name = l10n_util::GetDisplayNameForLocale(
      locale, g_browser_process->GetApplicationLocale(), true);

  AllowJavascript();
  ResolveJavascriptCallback(callback_id, base::UTF16ToUTF8(display_name));
}

void TtsHandler::HandleGetApplicationLocale(const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  const std::string callback_id = args[0].GetString();

  const std::string& application_locale =
      g_browser_process->GetApplicationLocale();

  AllowJavascript();
  ResolveJavascriptCallback(callback_id, application_locale);
}

void TtsHandler::OnVoicesChanged() {
  content::TtsController* tts_controller =
      content::TtsController::GetInstance();
  std::vector<content::VoiceData> voices;
  tts_controller->GetVoices(Profile::FromWebUI(web_ui()), GURL(), &voices);
  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  base::Value::List responses;
  for (const auto& voice : voices) {
    base::Value::Dict response;
    int language_score = GetVoiceLangMatchScore(&voice, app_locale);
    std::string language_code;
    if (voice.lang.empty()) {
      language_code = "noLanguageCode";
      response.Set(
          "displayLanguage",
          l10n_util::GetStringUTF8(IDS_TEXT_TO_SPEECH_SETTINGS_NO_LANGUAGE));
    } else {
      language_code = l10n_util::GetLanguage(voice.lang);
      response.Set(
          "displayLanguage",
          l10n_util::GetDisplayNameForLocale(
              language_code, g_browser_process->GetApplicationLocale(), true));
    }
    response.Set("name", voice.name);
    response.Set("remote", voice.remote);
    response.Set("languageCode", language_code);
    response.Set("fullLanguageCode", voice.lang);
    response.Set("languageScore", language_score);
    response.Set("extensionId", voice.engine_id);
    responses.Append(std::move(response));
  }
  AllowJavascript();
  FireWebUIListener("all-voice-data-updated", responses);

  // Also refresh the TTS extensions in case they have changed.
  HandleGetTtsExtensions(base::Value::List());
}

void TtsHandler::RegisterMessages() {
  SettingsWithTtsPreviewHandler::RegisterMessages();
  web_ui()->RegisterMessageCallback(
      "getTtsExtensions",
      base::BindRepeating(&TtsHandler::HandleGetTtsExtensions,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getDisplayNameForLocale",
      base::BindRepeating(&TtsHandler::HandleGetDisplayNameForLocale,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getApplicationLocale",
      base::BindRepeating(&TtsHandler::HandleGetApplicationLocale,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "previewTtsVoice",
      base::BindRepeating(&SettingsWithTtsPreviewHandler::HandlePreviewTtsVoice,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "wakeTtsEngine",
      base::BindRepeating(&TtsHandler::WakeTtsEngine, base::Unretained(this)));
}

int TtsHandler::GetVoiceLangMatchScore(const content::VoiceData* voice,
                                       const std::string& app_locale) {
  if (voice->lang.empty() || app_locale.empty()) {
    return 0;
  }
  if (voice->lang == app_locale) {
    return 2;
  }
  return l10n_util::GetLanguage(voice->lang) ==
                 l10n_util::GetLanguage(app_locale)
             ? 1
             : 0;
}

void TtsHandler::WakeTtsEngine(const base::Value::List& args) {
  Profile* profile = Profile::FromWebUI(web_ui());
  TtsExtensionEngine::GetInstance()->LoadBuiltInTtsEngine(profile);
  extensions::ProcessManager::Get(profile)->WakeEventPage(
      extension_misc::kGoogleSpeechSynthesisExtensionId,
      base::BindOnce(&TtsHandler::OnTtsEngineAwake,
                     weak_factory_.GetWeakPtr()));
}

void TtsHandler::OnTtsEngineAwake(bool success) {
  OnVoicesChanged();
}

GURL TtsHandler::GetSourceURL() const {
  return GURL(chrome::GetOSSettingsUrl(
      chromeos::settings::mojom::kTextToSpeechSubpagePath));
}

}  // namespace ash::settings
