// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/a11y/select_to_speak_handler.h"

#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/extension_api/tts_engine_extension_api.h"
#include "chrome/browser/speech/extension_api/tts_engine_extension_observer_chromeos.h"
#include "chrome/browser/speech/extension_api/tts_extension_api.h"
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

SelectToSpeakHandler::SelectToSpeakHandler() = default;

SelectToSpeakHandler::~SelectToSpeakHandler() = default;

void SelectToSpeakHandler::HandleGetAppLocale(const base::Value::List& args) {
  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  AllowJavascript();
  FireWebUIListener("app-locale-updated", base::Value(app_locale));
}

void SelectToSpeakHandler::OnVoicesChanged() {
  content::TtsController* tts_controller =
      content::TtsController::GetInstance();
  std::vector<content::VoiceData> voices;
  tts_controller->GetVoices(
      Profile::FromWebUI(web_ui()),
      GURL(chrome::GetOSSettingsUrl(
          chromeos::settings::mojom::kSelectToSpeakSubpagePath)),
      &voices);
  base::Value::List responses;
  for (const auto& voice : voices) {
    base::Value::Dict response;
    base::Value::List event_types;
    std::string language_code;
    std::string language_and_country_code = voice.lang;
    if (!language_and_country_code.empty()) {
      // Normalize underscores to hyphens because enhanced voices use
      // underscores, and l10n_util::GetLanguage uses hyphens.
      std::replace(language_and_country_code.begin(),
                   language_and_country_code.end(), '_', '-');
      language_code = l10n_util::GetLanguage(language_and_country_code);
      response.Set(
          "displayLanguage",
          l10n_util::GetDisplayNameForLocale(
              language_code, g_browser_process->GetApplicationLocale(), true));
      response.Set("displayLanguageAndCountry",
                   l10n_util::GetDisplayNameForLocale(
                       language_and_country_code,
                       g_browser_process->GetApplicationLocale(), true));
    }
    for (auto& event : voice.events) {
      const char* event_name_constant = TtsEventTypeToString(event);
      event_types.Append(event_name_constant);
    }
    response.Set("eventTypes", std::move(event_types));
    response.Set("extensionId", voice.engine_id);
    response.Set("voiceName", voice.name);
    response.Set("lang", voice.lang);
    responses.Append(std::move(response));
  }
  AllowJavascript();
  FireWebUIListener("all-sts-voice-data-updated", responses);
}

void SelectToSpeakHandler::RegisterMessages() {
  SettingsWithTtsPreviewHandler::RegisterMessages();
  web_ui()->RegisterMessageCallback(
      "getAllTtsVoiceDataForSts",
      base::BindRepeating(
          &SettingsWithTtsPreviewHandler::HandleGetAllTtsVoiceData,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getAppLocale",
      base::BindRepeating(&SelectToSpeakHandler::HandleGetAppLocale,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "previewTtsVoiceForSts",
      base::BindRepeating(&SettingsWithTtsPreviewHandler::HandlePreviewTtsVoice,
                          base::Unretained(this)));
}

GURL SelectToSpeakHandler::GetSourceURL() const {
  return GURL(chrome::GetOSSettingsUrl(
      chromeos::settings::mojom::kSelectToSpeakSubpagePath));
}

}  // namespace ash::settings
