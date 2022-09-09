// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/captions_handler.h"

#include "base/bind.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
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

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
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
    const base::Value::List& args) {
  AllowJavascript();
}

void CaptionsHandler::HandleOpenSystemCaptionsDialog(
    const base::Value::List& args) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  captions::CaptionSettingsDialog::ShowCaptionSettingsDialog();
#endif
}

void CaptionsHandler::OnSodaInstalled(speech::LanguageCode language_code) {
  if (!base::FeatureList::IsEnabled(media::kLiveCaptionMultiLanguage) &&
      soda_available_) {
    // If multi-language is disabled and the language code received is not for
    // Live Caption (perhaps it is downloading because another feature, such as
    // dictation on ChromeOS, has a different language selected), then return
    // early. We do not check for a matching language if multi-language is
    // enabled because we show all of the languages' download status in the UI,
    // even ones that are not currently selected.
    if (!prefs::IsLanguageCodeForLiveCaption(language_code, prefs_))
      return;
    speech::SodaInstaller::GetInstance()->RemoveObserver(this);
  }

  FireWebUIListener("soda-download-progress-changed",
                    base::Value(l10n_util::GetStringUTF16(
                        IDS_SETTINGS_CAPTIONS_LIVE_CAPTION_DOWNLOAD_COMPLETE)),
                    base::Value(speech::GetLanguageName(language_code)));
}

void CaptionsHandler::OnSodaInstallError(
    speech::LanguageCode language_code,
    speech::SodaInstaller::ErrorCode error_code) {
  // If multi-language is disabled and the language code received is not for
  // Live Caption (perhaps it is downloading because another feature, such as
  // dictation on ChromeOS, has a different language selected), then return
  // early. We do not check for a matching language if multi-language is
  // enabled because we show all of the languages' download status in the UI,
  // even ones that are not currently selected.
  // Check that language code matches the selected language for Live Caption
  // or is LanguageCode::kNone (signifying the SODA binary failed).
  if (!base::FeatureList::IsEnabled(media::kLiveCaptionMultiLanguage) &&
      !prefs::IsLanguageCodeForLiveCaption(language_code, prefs_) &&
      language_code != speech::LanguageCode::kNone) {
    return;
  }

  std::u16string error_message;
  switch (error_code) {
    case speech::SodaInstaller::ErrorCode::kUnspecifiedError: {
      error_message = l10n_util::GetStringUTF16(
          IDS_SETTINGS_CAPTIONS_LIVE_CAPTION_DOWNLOAD_ERROR);
      break;
    }
    case speech::SodaInstaller::ErrorCode::kNeedsReboot: {
      error_message = l10n_util::GetStringUTF16(
          IDS_SETTINGS_CAPTIONS_LIVE_CAPTION_DOWNLOAD_ERROR_REBOOT_REQUIRED);
      break;
    }
  }

  FireWebUIListener("soda-download-progress-changed",
                    base::Value(error_message),
                    base::Value(speech::GetLanguageName(language_code)));
}

void CaptionsHandler::OnSodaProgress(speech::LanguageCode language_code,
                                     int progress) {
  if (!base::FeatureList::IsEnabled(media::kLiveCaptionMultiLanguage) &&
      soda_available_) {
    // If multi-language is disabled and the language code received is not for
    // Live Caption (perhaps it is downloading because another feature, such as
    // dictation on ChromeOS, has a different language selected), then return
    // early. We do not check for a matching language if multi-language is
    // enabled because we show all of the languages' download status in the UI,
    // even ones that are not currently selected.
    // Check that language code matches the selected language for Live Caption
    // or is LanguageCode::kNone (signifying the SODA binary progress).
    if (!prefs::IsLanguageCodeForLiveCaption(language_code, prefs_) &&
        language_code != speech::LanguageCode::kNone) {
      return;
    }
  }
  // If the language code is kNone, this means that only the SODA binary has
  // begun downloading. Therefore we pass the Live Caption language along to the
  // WebUI, since that is the language which will begin downloading.
  if (language_code == speech::LanguageCode::kNone) {
    language_code =
        speech::GetLanguageCode(prefs::GetLiveCaptionLanguageCode(prefs_));
  }
  FireWebUIListener(
      "soda-download-progress-changed",
      base::Value(l10n_util::GetStringFUTF16Int(
          IDS_SETTINGS_CAPTIONS_LIVE_CAPTION_DOWNLOAD_PROGRESS, progress)),
      base::Value(speech::GetLanguageName(language_code)));
}

}  // namespace settings
