// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_A11Y_SETTINGS_WITH_TTS_PREVIEW_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_A11Y_SETTINGS_WITH_TTS_PREVIEW_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "content/public/browser/tts_controller.h"

namespace ash::settings {

// Parent Chrome OS TTS-related settings page handler.
class SettingsWithTtsPreviewHandler : public ::settings::SettingsPageUIHandler,
                                      public content::VoicesChangedDelegate,
                                      public content::UtteranceEventDelegate {
 public:
  SettingsWithTtsPreviewHandler();

  SettingsWithTtsPreviewHandler(const SettingsWithTtsPreviewHandler&) = delete;
  SettingsWithTtsPreviewHandler& operator=(
      const SettingsWithTtsPreviewHandler&) = delete;

  ~SettingsWithTtsPreviewHandler() override;

  void HandleGetAllTtsVoiceData(const base::Value::List& args);
  void HandlePreviewTtsVoice(const base::Value::List& args);

  // SettingsPageUIHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // UtteranceEventDelegate implementation.
  void OnTtsEvent(content::TtsUtterance* utterance,
                  content::TtsEventType event_type,
                  int char_index,
                  int length,
                  const std::string& error_message) override;

  void RefreshTtsVoices(const base::Value::List& args);
  void RemoveTtsControllerDelegates();
  virtual GURL GetSourceURL() const = 0;

 private:
  base::WeakPtrFactory<SettingsWithTtsPreviewHandler> weak_factory_{this};
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_A11Y_SETTINGS_WITH_TTS_PREVIEW_HANDLER_H_
