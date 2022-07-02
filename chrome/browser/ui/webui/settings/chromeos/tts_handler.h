// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_TTS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_TTS_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "content/public/browser/tts_controller.h"

namespace settings {

// Chrome "/manageAccessibility/tts/*" settings page UI handler.
class TtsHandler : public SettingsPageUIHandler,
                   public content::VoicesChangedDelegate,
                   public content::UtteranceEventDelegate {
 public:
  TtsHandler();

  TtsHandler(const TtsHandler&) = delete;
  TtsHandler& operator=(const TtsHandler&) = delete;

  ~TtsHandler() override;

  void HandleGetAllTtsVoiceData(const base::Value::List& args);
  void HandleGetTtsExtensions(const base::Value::List& args);
  void HandlePreviewTtsVoice(const base::Value::List& args);

  // SettingsPageUIHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // VoicesChangedDelegate implementation.
  void OnVoicesChanged() override;

  // UtteranceEventDelegate implementation.
  void OnTtsEvent(content::TtsUtterance* utterance,
                  content::TtsEventType event_type,
                  int char_index,
                  int length,
                  const std::string& error_message) override;

 private:
  void WakeTtsEngine(const base::Value::List& args);
  void OnTtsEngineAwake(bool success);
  void RefreshTtsVoices(const base::Value::List& args);
  int GetVoiceLangMatchScore(const content::VoiceData* voice,
                             const std::string& app_locale);
  void RemoveTtsControllerDelegates();

  base::WeakPtrFactory<TtsHandler> weak_factory_{this};
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_TTS_HANDLER_H_
