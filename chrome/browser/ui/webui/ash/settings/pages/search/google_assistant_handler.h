// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_SEARCH_GOOGLE_ASSISTANT_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_SEARCH_GOOGLE_ASSISTANT_HANDLER_H_

#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"

namespace ash::settings {

class GoogleAssistantHandler : public ::settings::SettingsPageUIHandler,
                               CrasAudioHandler::AudioObserver {
 public:
  GoogleAssistantHandler();

  GoogleAssistantHandler(const GoogleAssistantHandler&) = delete;
  GoogleAssistantHandler& operator=(const GoogleAssistantHandler&) = delete;

  ~GoogleAssistantHandler() override;

  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // CrasAudioHandler::AudioObserver overrides
  void OnAudioNodesChanged() override;

 private:
  // WebUI call to launch into the Google Assistant app settings.
  void HandleShowGoogleAssistantSettings(const base::Value::List& args);
  // WebUI call to retrain Assistant voice model.
  void HandleRetrainVoiceModel(const base::Value::List& args);
  // WebUI call to sync Assistant voice model status.
  void HandleSyncVoiceModelStatus(const base::Value::List& args);
  // WebUI call to signal js side is ready.
  void HandleInitialized(const base::Value::List& args);

  bool pending_hotword_update_ = false;

  base::WeakPtrFactory<GoogleAssistantHandler> weak_factory_{this};
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_SEARCH_GOOGLE_ASSISTANT_HANDLER_H_
