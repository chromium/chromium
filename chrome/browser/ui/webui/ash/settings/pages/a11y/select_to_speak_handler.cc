// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/a11y/select_to_speak_handler.h"

#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "ash/webui/settings/public/constants/routes_util.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/i18n/legacy_language_tag_helpers.h"
#include "base/json/json_reader.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/extension_api/tts_engine_extension_api.h"
#include "chrome/browser/speech/extension_api/tts_engine_extension_observer_chromeos.h"
#include "chrome/browser/speech/extension_api/tts_extension_api.h"
#include "components/application_locale_storage/application_locale_storage.h"
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

SelectToSpeakHandler::SelectToSpeakHandler(
    const ApplicationLocaleStorage* application_locale_storage)
    : application_locale_storage_(CHECK_DEREF(application_locale_storage)) {}

SelectToSpeakHandler::~SelectToSpeakHandler() = default;

void SelectToSpeakHandler::HandleGetAppLocale(const base::ListValue& args) {
  AllowJavascript();
  FireWebUIListener("app-locale-updated",
                    base::Value(application_locale_storage_->Get()));
}

void SelectToSpeakHandler::OnVoicesChanged() {
  content::TtsController* tts_controller =
      content::TtsController::GetInstance();
  std::vector<content::VoiceData> voices;
  tts_controller->GetVoices(
      Profile::FromWebUI(web_ui()),
      GURL(chromeos::settings::GetOSSettingsUrl(
          chromeos::settings::mojom::kSelectToSpeakSubpagePath)),
      &voices);
  base::ListValue responses;
  for (const auto& voice : voices) {
    base::DictValue response;
    base::ListValue event_types;
    std::string language_code;
    std::string language_and_country_code = voice.lang;
    if (!language_and_country_code.empty()) {
      language_code = base::i18n::GetLanguageSubtagUsingLanguageTag(
          language_and_country_code);
      response.Set(
          "displayLanguage",
          l10n_util::GetDisplayNameForLocale(
              language_code, application_locale_storage_->Get(), true));
      response.Set("displayLanguageAndCountry",
                   l10n_util::GetDisplayNameForLocale(
                       language_and_country_code,
                       application_locale_storage_->Get(), true));
    }
    for (auto& event : voice.events) {
      event_types.Append(TtsEventTypeToString(event));
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
  return GURL(chromeos::settings::GetOSSettingsUrl(
      chromeos::settings::mojom::kSelectToSpeakSubpagePath));
}

}  // namespace ash::settings
