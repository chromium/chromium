// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/a11y/accessibility_handler.h"

#include <set>
#include <string_view>

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/dictation.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/components/kiosk/kiosk_utils.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/common/locale_util.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/manifest_handlers/options_page_info.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/ime/ash/input_method_util.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash::settings {
namespace {

void RecordShowShelfNavigationButtonsValueChange(bool enabled) {
  base::UmaHistogramBoolean(
      "Accessibility.CrosShelfNavigationButtonsInTabletModeChanged."
      "OsSettings",
      enabled);
}

}  // namespace

AccessibilityHandler::AccessibilityHandler(Profile* profile)
    : profile_(profile) {}

AccessibilityHandler::~AccessibilityHandler() {
  if (a11y_nav_buttons_toggle_metrics_reporter_timer_.IsRunning()) {
    a11y_nav_buttons_toggle_metrics_reporter_timer_.FireNow();
  }
}

void AccessibilityHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "showBrowserAppearanceSettings",
      base::BindRepeating(
          &AccessibilityHandler::HandleShowBrowserAppearanceSettings,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setStartupSoundEnabled",
      base::BindRepeating(&AccessibilityHandler::HandleSetStartupSoundEnabled,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "recordSelectedShowShelfNavigationButtonValue",
      base::BindRepeating(
          &AccessibilityHandler::
              HandleRecordSelectedShowShelfNavigationButtonsValue,
          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "manageA11yPageReady",
      base::BindRepeating(&AccessibilityHandler::HandleManageA11yPageReady,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "showChromeVoxTutorial",
      base::BindRepeating(&AccessibilityHandler::HandleShowChromeVoxTutorial,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "updateBluetoothBrailleDisplayAddress",
      base::BindRepeating(
          &AccessibilityHandler::HandleUpdateBluetoothBrailleDisplayAddress,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getStartupSoundEnabled",
      base::BindRepeating(&AccessibilityHandler::HandleGetStartupSoundEnabled,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "previewFlashNotification",
      base::BindRepeating(&AccessibilityHandler::HandlePreviewFlashNotification,
                          base::Unretained(this)));
}

void AccessibilityHandler::HandleShowBrowserAppearanceSettings(
    const base::Value::List& args) {
  ash::NewWindowDelegate::GetPrimary()->OpenUrl(
      GURL(chrome::kChromeUISettingsURL).Resolve(chrome::kAppearanceSubPage),
      ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      ash::NewWindowDelegate::Disposition::kSwitchToTab);
}

void AccessibilityHandler::HandleSetStartupSoundEnabled(
    const base::Value::List& args) {
  DCHECK_EQ(1U, args.size());
  bool enabled = false;
  if (args[0].is_bool()) {
    enabled = args[0].GetBool();
  }
  AccessibilityManager::Get()->SetStartupSoundEnabled(enabled);
  base::UmaHistogramBoolean(
      "ChromeOS.Settings.Accessibility.OOBEStartupSound.Enabled", enabled);
}

void AccessibilityHandler::HandleRecordSelectedShowShelfNavigationButtonsValue(
    const base::Value::List& args) {
  DCHECK_EQ(1U, args.size());
  bool enabled = false;
  if (args[0].is_bool()) {
    enabled = args[0].GetBool();
  }

  a11y_nav_buttons_toggle_metrics_reporter_timer_.Start(
      FROM_HERE, base::Seconds(10),
      base::BindOnce(&RecordShowShelfNavigationButtonsValueChange, enabled));
}

void AccessibilityHandler::HandleManageA11yPageReady(
    const base::Value::List& args) {
  AllowJavascript();
}

void AccessibilityHandler::OnJavascriptAllowed() {
  FireWebUIListener(
      "initial-data-ready",
      base::Value(AccessibilityManager::Get()->GetStartupSoundEnabled()));
  MaybeAddSodaInstallerObserver();
  MaybeAddDictationLocales();
}

void AccessibilityHandler::OnJavascriptDisallowed() {
  if (features::IsDictationOfflineAvailable()) {
    soda_observation_.Reset();
  }
}

void AccessibilityHandler::HandleShowChromeVoxTutorial(
    const base::Value::List& args) {
  AccessibilityManager::Get()->ShowChromeVoxTutorial();
}

void AccessibilityHandler::HandleUpdateBluetoothBrailleDisplayAddress(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  const std::string address = args[0].GetString();
  AccessibilityManager::Get()->UpdateBluetoothBrailleDisplayAddress(address);
}

void AccessibilityHandler::HandleGetStartupSoundEnabled(
    const base::Value::List& args) {
  AllowJavascript();
  FireWebUIListener(
      "startup-sound-setting-retrieved",
      base::Value(AccessibilityManager::Get()->GetStartupSoundEnabled()));
}

void AccessibilityHandler::HandlePreviewFlashNotification(
    const base::Value::List& args) {
  AccessibilityManager::Get()->PreviewFlashNotification();
}

void AccessibilityHandler::OpenExtensionOptionsPage(const char extension_id[]) {
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile_)
          ->enabled_extensions()
          .GetByID(extension_id);
  if (!extension) {
    return;
  }

  extensions::ExtensionTabUtil::OpenOptionsPage(
      extension, chrome::FindBrowserWithTab(web_ui()->GetWebContents()));
}

void AccessibilityHandler::MaybeAddSodaInstallerObserver() {
  if (!features::IsDictationOfflineAvailable()) {
    return;
  }

  speech::SodaInstaller* soda_installer = speech::SodaInstaller::GetInstance();
  if (!soda_installer->IsSodaInstalled(GetDictationLocale())) {
    // Add self as an observer. If this was a page refresh we don't want to
    // get added twice.
    soda_observation_.Observe(soda_installer);
  }
}

// SodaInstaller::Observer:
void AccessibilityHandler::OnSodaInstalled(speech::LanguageCode language_code) {
  if (language_code != GetDictationLocale()) {
    return;
  }

  // Only show the success message if both the SODA binary and the language pack
  // matching the Dictation locale have been downloaded.
  FireWebUIListener(
      "dictation-locale-menu-subtitle-changed",
      base::Value(l10n_util::GetStringFUTF16(
          IDS_SETTINGS_ACCESSIBILITY_DICTATION_LOCALE_SUB_LABEL_OFFLINE,
          GetDictationLocaleDisplayName())));
}

void AccessibilityHandler::OnSodaProgress(speech::LanguageCode language_code,
                                          int progress) {
  if (language_code != speech::LanguageCode::kNone &&
      language_code != GetDictationLocale()) {
    return;
  }

  // Only show the progress message if either the Dictation locale or the SODA
  // binary has progress (encoded by LanguageCode::kNone).
  FireWebUIListener(
      "dictation-locale-menu-subtitle-changed",
      base::Value(l10n_util::GetStringFUTF16Int(
          IDS_SETTINGS_ACCESSIBILITY_DICTATION_SUBTITLE_SODA_DOWNLOAD_PROGRESS,
          progress)));
}

void AccessibilityHandler::OnSodaInstallError(
    speech::LanguageCode language_code,
    speech::SodaInstaller::ErrorCode error_code) {
  if (language_code != speech::LanguageCode::kNone &&
      language_code != GetDictationLocale()) {
    return;
  }

  // Show the failed message if either the Dictation locale failed or the SODA
  // binary failed (encoded by LanguageCode::kNone).
  FireWebUIListener(
      "dictation-locale-menu-subtitle-changed",
      base::Value(l10n_util::GetStringFUTF16(
          IDS_SETTINGS_ACCESSIBILITY_DICTATION_SUBTITLE_SODA_DOWNLOAD_ERROR,
          GetDictationLocaleDisplayName())));
}

void AccessibilityHandler::MaybeAddDictationLocales() {
  base::flat_map<std::string, Dictation::LocaleData> locales =
      Dictation::GetAllSupportedLocales();

  // Get application locale.
  std::string application_locale = g_browser_process->GetApplicationLocale();
  std::pair<std::string_view, std::string_view> application_lang_and_locale =
      language::SplitIntoMainAndTail(application_locale);

  // Get IME locales
  input_method::InputMethodManager* ime_manager =
      input_method::InputMethodManager::Get();
  std::vector<std::string> input_method_ids =
      ime_manager->GetActiveIMEState()->GetEnabledInputMethodIds();
  std::vector<std::string> ime_languages;
  ime_manager->GetInputMethodUtil()->GetLanguageCodesFromInputMethodIds(
      input_method_ids, &ime_languages);

  // Get enabled preferred UI languages.
  std::string preferred_languages =
      profile_->GetPrefs()->GetString(language::prefs::kPreferredLanguages);
  std::vector<std::string_view> enabled_languages =
      base::SplitStringPiece(preferred_languages, ",", base::TRIM_WHITESPACE,
                             base::SPLIT_WANT_NONEMPTY);

  // Combine these into one set for recommending Dication languages.
  std::set<std::string_view> ui_languages;
  ui_languages.insert(application_lang_and_locale.first);
  for (auto& ime_language : ime_languages) {
    ui_languages.insert(language::SplitIntoMainAndTail(ime_language).first);
  }
  for (auto& enabled_language : enabled_languages) {
    ui_languages.insert(language::SplitIntoMainAndTail(enabled_language).first);
  }

  base::Value::List locales_list;
  for (auto& locale : locales) {
    base::Value::Dict option;
    option.Set("value", locale.first);
    option.Set("name",
               l10n_util::GetDisplayNameForLocale(
                   locale.first, application_locale, /*is_for_ui=*/true));
    option.Set("worksOffline", locale.second.works_offline);
    option.Set("installed", locale.second.installed);

    // We can recommend languages that match the current application
    // locale, IME languages or enabled preferred languages.
    std::pair<std::string_view, std::string_view> lang_and_locale =
        language::SplitIntoMainAndTail(locale.first);
    bool is_recommended = base::Contains(ui_languages, lang_and_locale.first);

    option.Set("recommended", is_recommended);
    locales_list.Append(std::move(option));
  }

  FireWebUIListener("dictation-locales-set", locales_list);
}

// Returns the Dictation locale as a language code.
speech::LanguageCode AccessibilityHandler::GetDictationLocale() {
  const std::string dictation_locale =
      profile_->GetPrefs()->GetString(prefs::kAccessibilityDictationLocale);
  return speech::GetLanguageCode(dictation_locale);
}

std::u16string AccessibilityHandler::GetDictationLocaleDisplayName() {
  const std::string dictation_locale =
      profile_->GetPrefs()->GetString(prefs::kAccessibilityDictationLocale);

  return l10n_util::GetDisplayNameForLocale(
      /*locale=*/dictation_locale,
      /*display_locale=*/g_browser_process->GetApplicationLocale(),
      /*is_ui=*/true);
}

}  // namespace ash::settings
