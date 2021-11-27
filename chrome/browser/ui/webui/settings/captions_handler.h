// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CAPTIONS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CAPTIONS_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "components/soda/constants.h"
#include "components/soda/soda_installer.h"

class PrefService;

namespace settings {

// Settings handler for the captions settings subpage.
class CaptionsHandler : public SettingsPageUIHandler,
                        public speech::SodaInstaller::Observer {
 public:
  explicit CaptionsHandler(PrefService* prefs);
  ~CaptionsHandler() override;
  CaptionsHandler(const CaptionsHandler&) = delete;
  CaptionsHandler& operator=(const CaptionsHandler&) = delete;

  // SettingsPageUIHandler overrides.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  void HandleLiveCaptionSectionReady(const base::ListValue* args);
  void HandleOpenSystemCaptionsDialog(const base::ListValue* args);

  // SodaInstaller::Observer overrides:
  void OnSodaInstalled() override;
  void OnSodaLanguagePackInstalled(speech::LanguageCode language_code) override;
  void OnSodaError() override;
  void OnSodaLanguagePackError(speech::LanguageCode language_code) override;
  void OnSodaProgress(int combined_progress) override;
  void OnSodaLanguagePackProgress(int language_progress,
                                  speech::LanguageCode language_code) override;

  raw_ptr<PrefService> prefs_;
  bool soda_available_ = true;
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CAPTIONS_HANDLER_H_
