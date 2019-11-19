// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_TTS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_TTS_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "content/public/browser/tts_controller.h"

class Profile;

namespace settings {

// Chrome "/manageAccessibility/tts/*" settings page UI handler.
class TtsHandler : public SettingsPageUIHandler,
                   public content::VoicesChangedDelegate,
                   public content::UtteranceEventDelegate {
 public:
  TtsHandler();
  ~TtsHandler() override;

  void HandleGetAllTtsVoiceData(const base::ListValue* args);
  void HandleGetTtsExtensions(const base::ListValue* args);
  void HandlePreviewTtsVoice(const base::ListValue* args);

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
  void WakeTtsEngine(const base::ListValue* args);
  void OnTtsEngineAwake(bool success);
  int GetVoiceLangMatchScore(const content::VoiceData* voice,
                             const std::string& app_locale);

  base::WeakPtrFactory<TtsHandler> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(TtsHandler);
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_TTS_HANDLER_H_
