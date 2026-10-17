// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CAPTIONS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CAPTIONS_HANDLER_H_

#include <set>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chrome/browser/speech/speech_recognition_small_expert_model_installer.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "components/soda/constants.h"
#include "components/soda/soda_installer.h"

class PrefService;

namespace settings {

// Settings handler for the captions settings subpage.
class CaptionsHandler
    : public SettingsPageUIHandler,
      public speech::SodaInstaller::Observer,
      public speech::SpeechRecognitionSmallExpertModelInstaller::Observer {
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
  void HandleLiveCaptionSectionReady(const base::ListValue& args);
  void HandleOpenSystemCaptionsDialog(const base::ListValue& args);
  void HandleGetAvailableLanguagePacks(const base::ListValue& args);
  void HandleGetInstalledLanguagePacks(const base::ListValue& args);
  void HandleRemoveLanguagePacks(const base::ListValue& args);
  void HandleInstallLanguagePacks(const base::ListValue& args);
  void HandleInstallSpeechRecognitionSmallExpertModel(
      const base::ListValue& args);
  void HandleDeleteSpeechRecognitionSmallExpertModel(
      const base::ListValue& args);

  base::ListValue GetAvailableLanguagePacks();
  base::ListValue GetInstalledLanguagePacks();

  // SodaInstaller::Observer overrides:
  void OnSodaInstalled(speech::LanguageCode language_code) override;
  void OnSodaInstallError(speech::LanguageCode language_code,
                          speech::SodaInstaller::ErrorCode error_code) override;
  void OnSodaProgress(speech::LanguageCode language_code,
                      int progress) override;
  void OnSodaInstallCleanProgress();

  // SpeechRecognitionSmallExpertModelInstaller::Observer overrides:
  void OnSpeechRecognitionSmallExpertModelProgress(
      int progress,
      double bytes_per_sec,
      base::TimeDelta eta) override;
  void OnSpeechRecognitionSmallExpertModelStateChanged(
      speech::SpeechRecognitionSmallExpertModelInstaller::
          SpeechRecognitionSmallExpertModelState state) override;

  std::set<speech::LanguageCode> newly_installed_languages_;

  base::OneShotTimer installed_string_timer_;

  raw_ptr<PrefService> prefs_;
  bool soda_available_ = true;

  base::ScopedObservation<
      speech::SpeechRecognitionSmallExpertModelInstaller,
      speech::SpeechRecognitionSmallExpertModelInstaller::Observer>
      small_expert_model_observation_{this};

  base::WeakPtrFactory<CaptionsHandler> weak_factory_{this};
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CAPTIONS_HANDLER_H_
