// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_A11Y_SETTINGS_WITH_TTS_PREVIEW_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_A11Y_SETTINGS_WITH_TTS_PREVIEW_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "content/public/browser/tts_controller.h"

namespace content {
class TtsController;
}  // namespace content

namespace ash::settings {

// Parent Chrome OS TTS-related settings page handler.
class SettingsWithTtsPreviewHandler : public ::settings::SettingsPageUIHandler,
                                      public content::VoicesChangedDelegate {
 public:
  SettingsWithTtsPreviewHandler();

  SettingsWithTtsPreviewHandler(const SettingsWithTtsPreviewHandler&) = delete;
  SettingsWithTtsPreviewHandler& operator=(
      const SettingsWithTtsPreviewHandler&) = delete;

  ~SettingsWithTtsPreviewHandler() override;

  void FireTtsPreviewEvent();

  void HandleGetAllTtsVoiceData(const base::Value::List& args);
  void HandlePreviewTtsVoice(const base::Value::List& args);

  // SettingsPageUIHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  virtual GURL GetSourceURL() const = 0;

 private:
  void RefreshTtsVoices(const base::Value::List& args);

  base::ScopedObservation<content::TtsController,
                          content::VoicesChangedDelegate>
      tts_observation_{this};

  base::WeakPtrFactory<SettingsWithTtsPreviewHandler> weak_factory_{this};
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_A11Y_SETTINGS_WITH_TTS_PREVIEW_HANDLER_H_
