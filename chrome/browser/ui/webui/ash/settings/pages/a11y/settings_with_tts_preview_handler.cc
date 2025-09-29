// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/a11y/settings_with_tts_preview_handler.h"

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/extension_api/tts_engine_extension_api.h"
#include "chrome/browser/speech/extension_api/tts_engine_extension_observer_chromeos.h"
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

namespace {

class TtsPreviewEventDelegate : public content::UtteranceEventDelegate {
 public:
  explicit TtsPreviewEventDelegate(
      base::WeakPtr<SettingsWithTtsPreviewHandler> handler)
      : handler_(handler) {}
  ~TtsPreviewEventDelegate() override = default;

  // content::UtteranceEventDelegate:
  void OnTtsEvent(content::TtsUtterance* utterance,
                  content::TtsEventType event_type,
                  int char_index,
                  int length,
                  const std::string& error_message) override {
    if (handler_ && (event_type == content::TTS_EVENT_END ||
                     event_type == content::TTS_EVENT_INTERRUPTED ||
                     event_type == content::TTS_EVENT_ERROR)) {
      handler_->FireTtsPreviewEvent();
    }
  }

 private:
  base::WeakPtr<SettingsWithTtsPreviewHandler> handler_;
};

}  // namespace

SettingsWithTtsPreviewHandler::SettingsWithTtsPreviewHandler() = default;

SettingsWithTtsPreviewHandler::~SettingsWithTtsPreviewHandler() = default;

void SettingsWithTtsPreviewHandler::HandleGetAllTtsVoiceData(
    const base::Value::List& args) {
  OnVoicesChanged();
}

void SettingsWithTtsPreviewHandler::FireTtsPreviewEvent() {
  FireWebUIListener("tts-preview-state-changed",
                    base::Value(/*preview_stopped=*/false));
}

void SettingsWithTtsPreviewHandler::HandlePreviewTtsVoice(
    const base::Value::List& args) {
  DCHECK_EQ(2U, args.size());
  const std::string& text = args[0].GetString();
  const std::string& voice_id = args[1].GetString();

  if (text.empty() || voice_id.empty()) {
    return;
  }

  std::optional<base::Value> json =
      base::JSONReader::Read(voice_id, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  std::string name;
  std::string extension_id;
  if (const std::string* ptr = json->GetDict().FindString("name")) {
    name = *ptr;
  }
  if (const std::string* ptr = json->GetDict().FindString("extension")) {
    extension_id = *ptr;
  }

  std::unique_ptr<content::TtsUtterance> utterance =
      content::TtsUtterance::Create(web_ui()->GetWebContents());
  utterance->SetText(text);
  utterance->SetVoiceName(name);
  utterance->SetEngineId(extension_id);
  utterance->SetSrcUrl(GetSourceURL());
  utterance->SetEventDelegate(
      std::make_unique<TtsPreviewEventDelegate>(weak_factory_.GetWeakPtr()));
  content::TtsController::GetInstance()->Stop();

  FireWebUIListener("tts-preview-state-changed",
                    base::Value(/*preview_started=*/true));
  content::TtsController::GetInstance()->SpeakOrEnqueue(std::move(utterance));
}

void SettingsWithTtsPreviewHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getAllTtsVoiceData",
      base::BindRepeating(
          &SettingsWithTtsPreviewHandler::HandleGetAllTtsVoiceData,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "refreshTtsVoices",
      base::BindRepeating(&SettingsWithTtsPreviewHandler::RefreshTtsVoices,
                          base::Unretained(this)));
}

void SettingsWithTtsPreviewHandler::OnJavascriptAllowed() {
  tts_observation_.Observe(content::TtsController::GetInstance());
}

void SettingsWithTtsPreviewHandler::OnJavascriptDisallowed() {
  tts_observation_.Reset();
}

void SettingsWithTtsPreviewHandler::RefreshTtsVoices(
    const base::Value::List& args) {
  content::TtsController::GetInstance()->RefreshVoices();
}

}  // namespace ash::settings
