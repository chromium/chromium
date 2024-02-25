// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_A11Y_TTS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_A11Y_TTS_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/ash/settings/pages/a11y/settings_with_tts_preview_handler.h"
#include "content/public/browser/tts_controller.h"

namespace ash::settings {

// ChromeOS "/manageAccessibility/tts/*" settings page UI handler.
class TtsHandler : public SettingsWithTtsPreviewHandler {
 public:
  TtsHandler();

  TtsHandler(const TtsHandler&) = delete;
  TtsHandler& operator=(const TtsHandler&) = delete;

  ~TtsHandler() override;

  void HandleGetAllTtsVoiceData(const base::Value::List& args);
  void HandleGetTtsExtensions(const base::Value::List& args);
  void HandleGetDisplayNameForLocale(const base::Value::List& args);
  void HandleGetApplicationLocale(const base::Value::List& args);

  // SettingsPageUIHandler implementation.
  void RegisterMessages() override;

  // VoicesChangedDelegate implementation.
  void OnVoicesChanged() override;

  // SettingsWithTtsPreviewHandler implementation.
  GURL GetSourceURL() const override;

 private:
  void WakeTtsEngine(const base::Value::List& args);
  void OnTtsEngineAwake(bool success);
  int GetVoiceLangMatchScore(const content::VoiceData* voice,
                             const std::string& app_locale);

  base::WeakPtrFactory<TtsHandler> weak_factory_{this};
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_A11Y_TTS_HANDLER_H_
