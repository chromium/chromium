// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/accessibility_handler.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_ui.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "ui/accessibility/accessibility_switches.h"
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
  if (::switches::IsExperimentalAccessibilityDictationOfflineEnabled())
    speech::SodaInstaller::GetInstance()->RemoveObserver(this);
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

  FireWebUIListener(
      "initial-data-ready",
      base::Value(AccessibilityManager::Get()->GetStartupSoundEnabled()));

  MaybeAddSodaInstallerObserver();
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
  if (::switches::IsExperimentalAccessibilityDictationOfflineEnabled()) {
    if (speech::SodaInstaller::GetInstance()->IsSodaInstalled())
      OnSodaInstalled();
    else
      speech::SodaInstaller::GetInstance()->AddObserver(this);
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

}  // namespace settings
}  // namespace chromeos
