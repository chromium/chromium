// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/captions_handler.h"

#include "base/bind.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/live_caption/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/soda/constants.h"
#include "content/public/browser/web_ui.h"
#include "media/base/media_switches.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_WIN) || defined(OS_MAC)
#include "chrome/browser/accessibility/caption_settings_dialog.h"
#endif

namespace settings {

CaptionsHandler::CaptionsHandler(PrefService* prefs) : prefs_(prefs) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  soda_available_ =
      base::FeatureList::IsEnabled(ash::features::kOnDeviceSpeechRecognition);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

CaptionsHandler::~CaptionsHandler() {
  if (soda_available_)
    speech::SodaInstaller::GetInstance()->RemoveObserver(this);
}

void CaptionsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "openSystemCaptionsDialog",
      base::BindRepeating(&CaptionsHandler::HandleOpenSystemCaptionsDialog,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "liveCaptionSectionReady",
      base::BindRepeating(&CaptionsHandler::HandleLiveCaptionSectionReady,
                          base::Unretained(this)));
}

void CaptionsHandler::OnJavascriptAllowed() {
  if (soda_available_)
    speech::SodaInstaller::GetInstance()->AddObserver(this);
}

void CaptionsHandler::OnJavascriptDisallowed() {
  if (soda_available_)
    speech::SodaInstaller::GetInstance()->RemoveObserver(this);
}

void CaptionsHandler::HandleLiveCaptionSectionReady(
    const base::ListValue* args) {
  AllowJavascript();
}

void CaptionsHandler::HandleOpenSystemCaptionsDialog(
    const base::ListValue* args) {
#if defined(OS_WIN) || defined(OS_MAC)
  captions::CaptionSettingsDialog::ShowCaptionSettingsDialog();
#endif
}

void CaptionsHandler::OnSodaInstalled() {
  if (!base::FeatureList::IsEnabled(media::kLiveCaptionMultiLanguage) &&
      soda_available_) {
    speech::SodaInstaller::GetInstance()->RemoveObserver(this);
  }

  FireWebUIListener("soda-download-progress-changed",
                    base::Value(l10n_util::GetStringUTF16(
                        IDS_SETTINGS_CAPTIONS_LIVE_CAPTION_DOWNLOAD_COMPLETE)));
}

void CaptionsHandler::OnSodaLanguagePackInstalled(
    speech::LanguageCode language_code) {
  if (!base::FeatureList::IsEnabled(media::kLiveCaptionMultiLanguage))
    return;

  FireWebUIListener("soda-download-progress-changed",
                    base::Value(l10n_util::GetStringUTF16(
                        IDS_SETTINGS_CAPTIONS_LIVE_CAPTION_DOWNLOAD_COMPLETE)),
                    base::Value(speech::GetLanguageName(language_code)));
}

void CaptionsHandler::OnSodaError() {
  if (!base::FeatureList::IsEnabled(media::kLiveCaptionMultiLanguage)) {
    prefs_->SetBoolean(prefs::kLiveCaptionEnabled, false);
  }

  FireWebUIListener("soda-download-progress-changed",
                    base::Value(l10n_util::GetStringUTF16(
                        IDS_SETTINGS_CAPTIONS_LIVE_CAPTION_DOWNLOAD_ERROR)),
                    base::Value());
}

void CaptionsHandler::OnSodaLanguagePackError(
    speech::LanguageCode language_code) {
  if (!base::FeatureList::IsEnabled(media::kLiveCaptionMultiLanguage))
    return;

  prefs_->SetBoolean(prefs::kLiveCaptionEnabled, false);
  FireWebUIListener("soda-download-progress-changed",
                    base::Value(l10n_util::GetStringUTF16(
                        IDS_SETTINGS_CAPTIONS_LIVE_CAPTION_DOWNLOAD_ERROR)),
                    base::Value(speech::GetLanguageName(language_code)));
}

void CaptionsHandler::OnSodaProgress(int combined_progress) {
  FireWebUIListener("soda-download-progress-changed",
                    base::Value(l10n_util::GetStringFUTF16Int(
                        IDS_SETTINGS_CAPTIONS_LIVE_CAPTION_DOWNLOAD_PROGRESS,
                        combined_progress)),
                    base::Value());
}

void CaptionsHandler::OnSodaLanguagePackProgress(
    int language_progress,
    speech::LanguageCode language_code) {
  if (!base::FeatureList::IsEnabled(media::kLiveCaptionMultiLanguage))
    return;

  FireWebUIListener("soda-download-progress-changed",
                    base::Value(l10n_util::GetStringFUTF16Int(
                        IDS_SETTINGS_CAPTIONS_LIVE_CAPTION_DOWNLOAD_PROGRESS,
                        language_progress)),
                    base::Value(speech::GetLanguageName(language_code)));
}

}  // namespace settings
