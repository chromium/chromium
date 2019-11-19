// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/first_run/first_run_ui.h"

#include <memory>

#include "ash/public/cpp/shelf_prefs.h"
#include "base/command_line.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/first_run/first_run_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_switches.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace {

const char kFirstRunJSPath[] = "first_run.js";
const char kShelfAlignmentBottom[] = "bottom";
const char kShelfAlignmentLeft[] = "left";
const char kShelfAlignmentRight[] = "right";

void SetLocalizedStrings(Profile* profile,
                         base::DictionaryValue* localized_strings) {
  localized_strings->SetString(
      "appListHeader",
      l10n_util::GetStringUTF16(IDS_FIRST_RUN_APP_LIST_STEP_HEADER));
  localized_strings->SetString(
      "appListText1",
      l10n_util::GetStringUTF16(IDS_FIRST_RUN_APP_LIST_STEP_TEXT_1));
  localized_strings->SetString(
      "appListText2",
      l10n_util::GetStringUTF16(IDS_FIRST_RUN_APP_LIST_STEP_TEXT_2));
  localized_strings->SetString(
      "trayHeader", l10n_util::GetStringUTF16(IDS_FIRST_RUN_TRAY_STEP_HEADER));
  localized_strings->SetString(
      "trayText", l10n_util::GetStringUTF16(IDS_FIRST_RUN_TRAY_STEP_TEXT));
  localized_strings->SetString(
      "helpHeader", l10n_util::GetStringUTF16(IDS_FIRST_RUN_HELP_STEP_HEADER));
  localized_strings->SetString(
      "helpText", l10n_util::GetStringUTF16(IDS_FIRST_RUN_HELP_STEP_TEXT));
  localized_strings->SetString(
      "helpKeepExploringButton",
      l10n_util::GetStringUTF16(IDS_FIRST_RUN_HELP_STEP_KEEP_EXPLORING_BUTTON));
  localized_strings->SetString(
      "helpFinishButton",
      l10n_util::GetStringUTF16(IDS_FIRST_RUN_HELP_STEP_FINISH_BUTTON));
  localized_strings->SetString(
      "nextButton", l10n_util::GetStringUTF16(IDS_FIRST_RUN_NEXT_BUTTON));
  localized_strings->SetBoolean(
      "transitionsEnabled",
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kEnableFirstRunUITransitions));
  localized_strings->SetString(
      "accessibleTitle",
      l10n_util::GetStringUTF16(IDS_FIRST_RUN_STEP_ACCESSIBLE_TITLE));
  ash::ShelfAlignment alignment = ash::GetShelfAlignmentPref(
      profile->GetPrefs(),
      display::Screen::GetScreen()->GetPrimaryDisplay().id());
  std::string shelf_alignment;
  switch (alignment) {
    case ash::SHELF_ALIGNMENT_BOTTOM:
    case ash::SHELF_ALIGNMENT_BOTTOM_LOCKED:
      shelf_alignment = kShelfAlignmentBottom;
      break;
    case ash::SHELF_ALIGNMENT_LEFT:
      shelf_alignment = kShelfAlignmentLeft;
      break;
    case ash::SHELF_ALIGNMENT_RIGHT:
      shelf_alignment = kShelfAlignmentRight;
      break;
  }
  localized_strings->SetString("shelfAlignment", shelf_alignment);
}

content::WebUIDataSource* CreateDataSource(Profile* profile) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIFirstRunHost);
  source->UseStringsJs();
  source->SetDefaultResource(IDR_FIRST_RUN_HTML);
  source->AddResourcePath(kFirstRunJSPath, IDR_FIRST_RUN_JS);
  base::DictionaryValue localized_strings;
  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  webui::SetLoadTimeDataDefaults(app_locale, &localized_strings);
  SetLocalizedStrings(profile, &localized_strings);
  source->AddLocalizedStrings(localized_strings);
  return source;
}

}  // anonymous namespace

namespace chromeos {

FirstRunUI::FirstRunUI(content::WebUI* web_ui)
    : WebUIController(web_ui),
      actor_(NULL) {
  auto handler = std::make_unique<FirstRunHandler>();
  actor_ = handler.get();
  web_ui->AddMessageHandler(std::move(handler));
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, CreateDataSource(profile));
}

}  // namespace chromeos
