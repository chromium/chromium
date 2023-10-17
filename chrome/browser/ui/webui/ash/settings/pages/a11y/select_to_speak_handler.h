// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_A11Y_SELECT_TO_SPEAK_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_A11Y_SELECT_TO_SPEAK_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/ash/settings/pages/a11y/settings_with_tts_preview_handler.h"

namespace ash::settings {

// ChromeOS "/textToSpeech/selectToSpeak/*" settings page UI handler.
class SelectToSpeakHandler : public SettingsWithTtsPreviewHandler {
 public:
  SelectToSpeakHandler();

  SelectToSpeakHandler(const SelectToSpeakHandler&) = delete;
  SelectToSpeakHandler& operator=(const SelectToSpeakHandler&) = delete;

  ~SelectToSpeakHandler() override;

  void HandleGetAllTtsVoiceData(const base::Value::List& args);
  void HandleGetAppLocale(const base::Value::List& args);
  void HandlePreviewTtsVoice(const base::Value::List& args);

  // SettingsPageUIHandler implementation.
  void RegisterMessages() override;

  // VoicesChangedDelegate implementation.
  void OnVoicesChanged() override;

  // SettingsWithTtsPreviewHandler implementation.
  GURL GetSourceURL() const override;

 private:
  base::WeakPtrFactory<SelectToSpeakHandler> weak_factory_{this};
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_A11Y_SELECT_TO_SPEAK_HANDLER_H_
