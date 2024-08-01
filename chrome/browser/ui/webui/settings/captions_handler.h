// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CAPTIONS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CAPTIONS_HANDLER_H_

#include <set>

#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
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
  void HandleLiveCaptionSectionReady(const base::Value::List& args);
  void HandleOpenSystemCaptionsDialog(const base::Value::List& args);
  void HandleGetAvailableLanguagePacks(const base::Value::List& args);
  void HandleGetInstalledLanguagePacks(const base::Value::List& args);
  void HandleRemoveLanguagePacks(const base::Value::List& args);
  void HandleInstallLanguagePacks(const base::Value::List& args);

  base::Value::List GetAvailableLanguagePacks();
  base::Value::List GetInstalledLanguagePacks();

  // SodaInstaller::Observer overrides:
  void OnSodaInstalled(speech::LanguageCode language_code) override;
  void OnSodaInstallError(speech::LanguageCode language_code,
                          speech::SodaInstaller::ErrorCode error_code) override;
  void OnSodaProgress(speech::LanguageCode language_code,
                      int progress) override;
  void OnSodaInstallCleanProgress();

  std::set<speech::LanguageCode> newly_installed_languages_;

  base::OneShotTimer installed_string_timer_;

  raw_ptr<PrefService> prefs_;
  bool soda_available_ = true;
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CAPTIONS_HANDLER_H_
