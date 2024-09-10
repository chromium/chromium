// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/captions_handler.h"

#include <string>
#include <unordered_set>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/live_caption/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/soda/constants.h"
#include "components/soda/soda_installer.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "content/public/browser/web_ui.h"
#include "media/base/media_switches.h"
#include "ui/base/l10n/l10n_util.h"
#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#include "chrome/browser/accessibility/caption_settings_dialog.h"
#endif

namespace {
constexpr char kCodeKey[] = "code";
constexpr char kDisplayNameKey[] = "displayName";
constexpr char kNativeDisplayNameKey[] = "nativeDisplayName";

base::Value::List SortByDisplayName(
    std::vector<base::Value::Dict> language_packs) {
  std::sort(language_packs.begin(), language_packs.end(),
            [](const base::Value::Dict& a, const base::Value::Dict& b) {
              return *(a.Find(kDisplayNameKey)->GetIfString()) <
                     *(b.Find(kDisplayNameKey)->GetIfString());
            });

  base::Value::List sorted_language_packs;
  for (base::Value::Dict& language_pack : language_packs) {
    sorted_language_packs.Append(std::move(language_pack));
  }

  return sorted_language_packs;
}

}  // namespace

namespace settings {

CaptionsHandler::CaptionsHandler(PrefService* prefs) : prefs_(prefs) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  soda_available_ =
      base::FeatureList::IsEnabled(ash::features::kOnDeviceSpeechRecognition);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  soda_available_ = false;
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
  web_ui()->RegisterMessageCallback(
      "getInstalledLanguagePacks",
      base::BindRepeating(&CaptionsHandler::HandleGetInstalledLanguagePacks,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getAvailableLanguagePacks",
      base::BindRepeating(&CaptionsHandler::HandleGetAvailableLanguagePacks,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "removeLanguagePack",
      base::BindRepeating(&CaptionsHandler::HandleRemoveLanguagePacks,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "installLanguagePacks",
      base::BindRepeating(&CaptionsHandler::HandleInstallLanguagePacks,
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

void CaptionsHandler::HandleGetAvailableLanguagePacks(
    const base::Value::List& args) {
  CHECK_EQ(args.size(), 1U);
  AllowJavascript();
  const base::Value& callback_id = args[0];
  ResolveJavascriptCallback(callback_id, GetAvailableLanguagePacks());
}

void CaptionsHandler::HandleGetInstalledLanguagePacks(
    const base::Value::List& args) {
  CHECK_EQ(args.size(), 1U);
  AllowJavascript();
  const base::Value& callback_id = args[0];
  ResolveJavascriptCallback(callback_id, GetInstalledLanguagePacks());
}

void CaptionsHandler::HandleRemoveLanguagePacks(const base::Value::List& args) {
  CHECK_GT(args.size(), 0U);
  AllowJavascript();
  for (const base::Value& arg : args) {
    const std::string* language_code = arg.GetIfString();
    speech::SodaInstaller::GetInstance()->UninstallLanguage(
        *language_code, g_browser_process->local_state());
  }
}

void CaptionsHandler::HandleInstallLanguagePacks(
    const base::Value::List& args) {
  CHECK_GT(args.size(), 0U);
  AllowJavascript();
  for (const base::Value& arg : args) {
    const std::string* language_code = arg.GetIfString();
    speech::SodaInstaller::GetInstance()->InstallLanguage(
        *language_code, g_browser_process->local_state());
  }
}

base::Value::List CaptionsHandler::GetAvailableLanguagePacks() {
  std::vector<std::string> enabled_and_available_languages;
  std::vector<base::Value::Dict> available_language_packs;
  {
    auto enabled_languages =
        speech::SodaInstaller::GetInstance()->GetLiveCaptionEnabledLanguages();
    auto available_languages =
        speech::SodaInstaller::GetInstance()->GetAvailableLanguages();
    auto available_languages_set = std::unordered_set<std::string>(
        available_languages.begin(), available_languages.end());
    for (const auto& enabled_language : enabled_languages) {
      if (available_languages_set.find(enabled_language) !=
          available_languages_set.end()) {
        enabled_and_available_languages.push_back(enabled_language);
      }
    }
  }
  // On ChromeOS we have already checked config availability on disk via the
  // installer, so we don't need to check the speech::kLanguageComponentConfigs
  // list.
#if BUILDFLAG(IS_CHROMEOS)
  for (const auto& language_name : enabled_and_available_languages) {
    base::Value::Dict available_language_pack;
    available_language_pack.Set(kCodeKey, language_name);
    available_language_pack.Set(
        kDisplayNameKey,
        speech::GetLanguageDisplayName(
            language_name, g_browser_process->GetApplicationLocale()));
    available_language_pack.Set(
        kNativeDisplayNameKey,
        speech::GetLanguageDisplayName(language_name, language_name));
    available_language_packs.push_back(std::move(available_language_pack));
  }
#else
  for (const auto& config : speech::kLanguageComponentConfigs) {
    if (config.language_code != speech::LanguageCode::kNone &&
        base::Contains(enabled_and_available_languages, config.language_name)) {
      base::Value::Dict available_language_pack;
      available_language_pack.Set(kCodeKey, config.language_name);
      available_language_pack.Set(
          kDisplayNameKey,
          speech::GetLanguageDisplayName(
              config.language_name, g_browser_process->GetApplicationLocale()));
      available_language_pack.Set(
          kNativeDisplayNameKey,
          speech::GetLanguageDisplayName(config.language_name,
                                         config.language_name));
      available_language_packs.push_back(std::move(available_language_pack));
    }
  }
#endif
  return SortByDisplayName(std::move(available_language_packs));
}

base::Value::List CaptionsHandler::GetInstalledLanguagePacks() {
  std::vector<base::Value::Dict> installed_language_packs;
  for (const auto& language : g_browser_process->local_state()->GetList(
           prefs::kSodaRegisteredLanguagePacks)) {
    base::Value::Dict installed_language_pack;
    const std::optional<speech::SodaLanguagePackComponentConfig> config =
        speech::GetLanguageComponentConfig(language.GetString());
    if (config && config->language_code != speech::LanguageCode::kNone) {
      installed_language_pack.Set(kCodeKey, language.GetString());
      installed_language_pack.Set(
          kDisplayNameKey, speech::GetLanguageDisplayName(
                               config->language_name,
                               g_browser_process->GetApplicationLocale()));
      installed_language_pack.Set(
          kNativeDisplayNameKey,
          speech::GetLanguageDisplayName(config->language_name,
                                         config->language_name));
      installed_language_packs.push_back(std::move(installed_language_pack));
    }
  }

  return SortByDisplayName(std::move(installed_language_packs));
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
  newly_installed_languages_.insert(language_code);

  installed_string_timer_.Start(FROM_HERE, base::Seconds(30), this,
                                &CaptionsHandler::OnSodaInstallCleanProgress);
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

void CaptionsHandler::OnSodaInstallCleanProgress() {
  for (const auto& language_code : newly_installed_languages_) {
    // Update the webui to show an empty str for progress.
    FireWebUIListener("soda-download-progress-changed",
                      base::Value(std::u16string()),
                      base::Value(speech::GetLanguageName(language_code)));
  }
  newly_installed_languages_.clear();
}

}  // namespace settings
