// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/tts_handler.h"

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/extension_api/tts_engine_extension_api.h"
#include "chrome/browser/speech/extension_api/tts_engine_extension_observer.h"
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

namespace settings {
TtsHandler::TtsHandler() {}

TtsHandler::~TtsHandler() {
  content::TtsController::GetInstance()->RemoveVoicesChangedDelegate(this);
}

void TtsHandler::HandleGetAllTtsVoiceData(const base::ListValue* args) {
  OnVoicesChanged();
}

void TtsHandler::HandleGetTtsExtensions(const base::ListValue* args) {
  // Ensure the built in tts engine is loaded to be able to respond to messages.
  WakeTtsEngine(nullptr);

  base::ListValue responses;
  Profile* profile = Profile::FromWebUI(web_ui());
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile);

  const std::set<std::string> extensions =
      TtsEngineExtensionObserver::GetInstance(profile)->GetTtsExtensions();
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
    base::DictionaryValue response;
    response.SetString("name", extension->name());
    response.SetString("extensionId", extension_id);
    if (extensions::OptionsPageInfo::HasOptionsPage(extension)) {
      response.SetString(
          "optionsPage",

          extensions::OptionsPageInfo::GetOptionsPage(extension).spec());
    }
    responses.Append(std::move(response));
  }

  FireWebUIListener("tts-extensions-updated", responses);
}

void TtsHandler::OnVoicesChanged() {
  content::TtsController* tts_controller =
      content::TtsController::GetInstance();
  std::vector<content::VoiceData> voices;
  tts_controller->GetVoices(Profile::FromWebUI(web_ui()), &voices);
  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  base::ListValue responses;
  for (const auto& voice : voices) {
    base::DictionaryValue response;
    int language_score = GetVoiceLangMatchScore(&voice, app_locale);
    std::string language_code;
    if (voice.lang.empty()) {
      language_code = "noLanguageCode";
      response.SetString(
          "displayLanguage",
          l10n_util::GetStringUTF8(IDS_TEXT_TO_SPEECH_SETTINGS_NO_LANGUAGE));
    } else {
      language_code = l10n_util::GetLanguage(voice.lang);
      response.SetString(
          "displayLanguage",
          l10n_util::GetDisplayNameForLocale(
              language_code, g_browser_process->GetApplicationLocale(), true));
    }
    response.SetString("name", voice.name);
    response.SetString("languageCode", language_code);
    response.SetString("fullLanguageCode", voice.lang);
    response.SetInteger("languageScore", language_score);
    response.SetString("extensionId", voice.engine_id);
    responses.Append(std::move(response));
  }
  AllowJavascript();
  FireWebUIListener("all-voice-data-updated", responses);

  // Also refresh the TTS extensions in case they have changed.
  HandleGetTtsExtensions(nullptr);
}

void TtsHandler::OnTtsEvent(content::TtsUtterance* utterance,
                            content::TtsEventType event_type,
                            int char_index,
                            int length,
                            const std::string& error_message) {
  if (event_type == content::TTS_EVENT_END ||
      event_type == content::TTS_EVENT_INTERRUPTED ||
      event_type == content::TTS_EVENT_ERROR) {
    base::Value result(false /* preview stopped */);
    FireWebUIListener("tts-preview-state-changed", result);
  }
}

void TtsHandler::HandlePreviewTtsVoice(const base::ListValue* args) {
  DCHECK_EQ(2U, args->GetSize());
  std::string text;
  std::string voice_id;
  args->GetString(0, &text);
  args->GetString(1, &voice_id);

  if (text.empty() || voice_id.empty())
    return;

  std::unique_ptr<base::DictionaryValue> json =
      base::DictionaryValue::From(base::JSONReader::ReadDeprecated(voice_id));
  std::string name;
  std::string extension_id;
  json->GetString("name", &name);
  json->GetString("extension", &extension_id);

  std::unique_ptr<content::TtsUtterance> utterance =
      content::TtsUtterance::Create((Profile::FromWebUI(web_ui())));
  utterance->SetText(text);
  utterance->SetVoiceName(name);
  utterance->SetEngineId(extension_id);
  utterance->SetSrcUrl(
      GURL(chrome::GetOSSettingsUrl("manageAccessibility/tts")));
  utterance->SetEventDelegate(this);
  content::TtsController::GetInstance()->Stop();

  base::Value result(true /* preview started */);
  FireWebUIListener("tts-preview-state-changed", result);
  content::TtsController::GetInstance()->SpeakOrEnqueue(std::move(utterance));
}

void TtsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getAllTtsVoiceData",
      base::BindRepeating(&TtsHandler::HandleGetAllTtsVoiceData,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getTtsExtensions",
      base::BindRepeating(&TtsHandler::HandleGetTtsExtensions,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "previewTtsVoice", base::BindRepeating(&TtsHandler::HandlePreviewTtsVoice,
                                             base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "wakeTtsEngine",
      base::BindRepeating(&TtsHandler::WakeTtsEngine, base::Unretained(this)));
}

void TtsHandler::OnJavascriptAllowed() {
  content::TtsController::GetInstance()->AddVoicesChangedDelegate(this);
}

void TtsHandler::OnJavascriptDisallowed() {
  content::TtsController::GetInstance()->RemoveVoicesChangedDelegate(this);
  content::TtsController::GetInstance()->RemoveUtteranceEventDelegate(this);
}

int TtsHandler::GetVoiceLangMatchScore(const content::VoiceData* voice,
                                       const std::string& app_locale) {
  if (voice->lang.empty() || app_locale.empty())
    return 0;
  if (voice->lang == app_locale)
    return 2;
  return l10n_util::GetLanguage(voice->lang) ==
                 l10n_util::GetLanguage(app_locale)
             ? 1
             : 0;
}

void TtsHandler::WakeTtsEngine(const base::ListValue* args) {
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

}  // namespace settings
