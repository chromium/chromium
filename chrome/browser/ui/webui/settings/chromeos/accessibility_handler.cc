// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/accessibility_handler.h"

#include <set>

#include "ash/constants/ash_pref_names.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/containers/flat_map.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/dictation.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/common/locale_util.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/base/ime/chromeos/input_method_util.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {
namespace settings {
namespace {

using ::ash::AccessibilityManager;

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
  if (a11y_nav_buttons_toggle_metrics_reporter_timer_.IsRunning())
    a11y_nav_buttons_toggle_metrics_reporter_timer_.FireNow();
}

void AccessibilityHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "showChromeVoxSettings",
      base::BindRepeating(&AccessibilityHandler::HandleShowChromeVoxSettings,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "showSelectToSpeakSettings",
      base::BindRepeating(
          &AccessibilityHandler::HandleShowSelectToSpeakSettings,
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
}

void AccessibilityHandler::HandleShowChromeVoxSettings(
    const base::ListValue* args) {
  OpenExtensionOptionsPage(extension_misc::kChromeVoxExtensionId);
}

void AccessibilityHandler::HandleShowSelectToSpeakSettings(
    const base::ListValue* args) {
  OpenExtensionOptionsPage(extension_misc::kSelectToSpeakExtensionId);
}

void AccessibilityHandler::HandleSetStartupSoundEnabled(
    const base::ListValue* args) {
  DCHECK_EQ(1U, args->GetSize());
  bool enabled;
  args->GetBoolean(0, &enabled);
  AccessibilityManager::Get()->SetStartupSoundEnabled(enabled);
}

void AccessibilityHandler::HandleRecordSelectedShowShelfNavigationButtonsValue(
    const base::ListValue* args) {
  DCHECK_EQ(1U, args->GetSize());
  bool enabled;
  args->GetBoolean(0, &enabled);

  a11y_nav_buttons_toggle_metrics_reporter_timer_.Start(
      FROM_HERE, base::TimeDelta::FromSeconds(10),
      base::BindOnce(&RecordShowShelfNavigationButtonsValueChange, enabled));
}

void AccessibilityHandler::HandleManageA11yPageReady(
    const base::ListValue* args) {
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
  if (features::IsDictationOfflineAvailableAndEnabled())
    soda_observation_.Reset();
}

void AccessibilityHandler::HandleShowChromeVoxTutorial(
    const base::ListValue* args) {
  AccessibilityManager::Get()->ShowChromeVoxTutorial();
}

void AccessibilityHandler::OpenExtensionOptionsPage(const char extension_id[]) {
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile_)->GetExtensionById(
          extension_id, extensions::ExtensionRegistry::ENABLED);
  if (!extension)
    return;
  extensions::ExtensionTabUtil::OpenOptionsPage(
      extension,
      chrome::FindBrowserWithWebContents(web_ui()->GetWebContents()));
}

void AccessibilityHandler::MaybeAddSodaInstallerObserver() {
  // TODO(crbug.com/1173135): Don't display SODA status if the Dictation
  // language is not a downloaded or available SODA language.
  if (features::IsDictationOfflineAvailableAndEnabled()) {
    const std::string dictation_locale =
        profile_->GetPrefs()->GetString(prefs::kAccessibilityDictationLocale);
    if (speech::SodaInstaller::GetInstance()->IsSodaInstalled(
            speech::GetLanguageCode(dictation_locale))) {
      OnSodaInstalled();
    } else {
      // Add self as an observer. If this was a page refresh we don't want to
      // get added twice.
      soda_observation_.Observe(speech::SodaInstaller::GetInstance());
    }
  }
}

// SodaInstaller::Observer:
void AccessibilityHandler::OnSodaInstalled() {
  speech::SodaInstaller::GetInstance()->RemoveObserver(this);
  FireWebUIListener(
      "dictation-setting-subtitle-changed",
      base::Value(l10n_util::GetStringUTF16(
          IDS_SETTINGS_ACCESSIBILITY_DICTATION_SUBTITLE_SODA_DOWNLOAD_COMPLETE)));
}

void AccessibilityHandler::OnSodaProgress(int progress) {
  FireWebUIListener(
      "dictation-setting-subtitle-changed",
      base::Value(l10n_util::GetStringFUTF16Int(
          IDS_SETTINGS_ACCESSIBILITY_DICTATION_SUBTITLE_SODA_DOWNLOAD_PROGRESS,
          progress)));
}

void AccessibilityHandler::OnSodaError() {
  FireWebUIListener(
      "dictation-setting-subtitle-changed",
      base::Value(l10n_util::GetStringUTF16(
          IDS_SETTINGS_ACCESSIBILITY_DICTATION_SUBTITLE_SODA_DOWNLOAD_ERROR)));
}

void AccessibilityHandler::MaybeAddDictationLocales() {
  if (!features::IsExperimentalAccessibilityDictationOfflineEnabled())
    return;

  base::flat_map<std::string, bool> locales =
      ash::Dictation::GetAllSupportedLocales();

  // Get application locale.
  std::string application_locale = g_browser_process->GetApplicationLocale();
  std::pair<base::StringPiece, base::StringPiece> application_lang_and_locale =
      language::SplitIntoMainAndTail(application_locale);

  // Get IME locales
  input_method::InputMethodManager* ime_manager =
      input_method::InputMethodManager::Get();
  std::vector<std::string> input_method_ids =
      ime_manager->GetActiveIMEState()->GetActiveInputMethodIds();
  std::vector<std::string> ime_languages;
  ime_manager->GetInputMethodUtil()->GetLanguageCodesFromInputMethodIds(
      input_method_ids, &ime_languages);

  // Get enabled preferred UI languages.
  std::string preferred_languages =
      profile_->GetPrefs()->GetString(language::prefs::kPreferredLanguages);
  std::vector<base::StringPiece> enabled_languages =
      base::SplitStringPiece(preferred_languages, ",", base::TRIM_WHITESPACE,
                             base::SPLIT_WANT_NONEMPTY);

  // Combine these into one set for recommending Dication languages.
  std::set<base::StringPiece> ui_languages;
  ui_languages.insert(application_lang_and_locale.first);
  for (auto& ime_language : ime_languages) {
    ui_languages.insert(language::SplitIntoMainAndTail(ime_language).first);
  }
  for (auto& enabled_language : enabled_languages) {
    ui_languages.insert(language::SplitIntoMainAndTail(enabled_language).first);
  }

  base::Value locales_list(base::Value::Type::LIST);
  for (auto& locale : locales) {
    base::Value option(base::Value::Type::DICTIONARY);
    option.SetKey("value", base::Value(locale.first));
    option.SetKey("name",
                  base::Value(l10n_util::GetDisplayNameForLocale(
                      locale.first, application_locale, /*is_for_ui=*/true)));
    option.SetKey("offline", base::Value(locale.second));

    // We can recommend languages that match the current application
    // locale, IME languages or enabled preferred languages.
    std::pair<base::StringPiece, base::StringPiece> lang_and_locale =
        language::SplitIntoMainAndTail(locale.first);
    bool is_recommended = base::Contains(ui_languages, lang_and_locale.first);

    option.SetKey("recommended", base::Value(is_recommended));
    locales_list.Append(std::move(option));
  }

  FireWebUIListener("dictation-locales-set", locales_list);
}

}  // namespace settings
}  // namespace chromeos
