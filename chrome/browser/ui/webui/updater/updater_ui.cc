// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/updater/updater_ui.h"

#include <cstdint>
#include <initializer_list>
#include <string_view>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/updater_resources.h"
#include "chrome/grit/updater_resources_map.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

void AddKnownApp(content::WebUIDataSource& source,
                 int index,
                 int app_name_ids,
                 std::initializer_list<std::string_view> app_ids) {
  source.AddLocalizedString(
      base::StrCat({"knownAppName", base::NumberToString(index)}),
      app_name_ids);
  source.AddString(base::StrCat({"knownAppIds", base::NumberToString(index)}),
                   base::JoinString(app_ids, ","));
}

UpdaterUI::UpdaterUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUIUpdaterHost);
  source->AddLocalizedStrings({
      {"activationFailed", IDS_UPDATER_ACTIVATION_FAILED},
      {"activationSucceeded", IDS_UPDATER_ACTIVATION_SUCCEEDED},
      {"addFilter", IDS_UPDATER_ADD_FILTER},
      {"app", IDS_UPDATER_APP},
      {"appNameOrId", IDS_UPDATER_APP_NAME_OR_ID},
      {"apply", IDS_UPDATER_APPLY},
      {"cancel", IDS_UPDATER_CANCEL},
      {"clearAllFilters", IDS_UPDATER_CLEAR_ALL_FILTERS},
      {"commandLine", IDS_UPDATER_COMMAND_LINE},
      {"commandOutcome", IDS_UPDATER_COMMAND_OUTCOME},
      {"common", IDS_UPDATER_COMMON},
      {"date", IDS_UPDATER_DATE},
      {"dateFilterAfter", IDS_UPDATER_DATE_FILTER_AFTER},
      {"dateFilterBefore", IDS_UPDATER_DATE_FILTER_BEFORE},
      {"dateFilterRange", IDS_UPDATER_DATE_FILTER_RANGE},
      {"endDate", IDS_UPDATER_END_DATE},
      {"errorDetails", IDS_UPDATER_ERROR_DETAILS},
      {"eventType", IDS_UPDATER_EVENT_TYPE},
      {"eventTypeACTIVATE", IDS_UPDATER_EVENT_TYPE_ACTIVATE},
      {"eventTypeAPP_COMMAND", IDS_UPDATER_EVENT_TYPE_APP_COMMAND},
      {"eventTypeINSTALL", IDS_UPDATER_EVENT_TYPE_INSTALL},
      {"eventTypeLOAD_POLICY", IDS_UPDATER_EVENT_TYPE_LOAD_POLICY},
      {"eventTypePERSISTED_DATA", IDS_UPDATER_EVENT_TYPE_PERSISTED_DATA},
      {"eventTypePOST_REQUEST", IDS_UPDATER_EVENT_TYPE_POST_REQUEST},
      {"eventTypeQUALIFY", IDS_UPDATER_EVENT_TYPE_QUALIFY},
      {"eventTypeUNINSTALL", IDS_UPDATER_EVENT_TYPE_UNINSTALL},
      {"eventTypeUPDATE", IDS_UPDATER_EVENT_TYPE_UPDATE},
      {"eventTypeUPDATER_PROCESS", IDS_UPDATER_EVENT_TYPE_UPDATER_PROCESS},
      {"filterChipApp", IDS_UPDATER_FILTER_CHIP_APP},
      {"filterChipDate", IDS_UPDATER_FILTER_CHIP_DATE},
      {"filterChipEventType", IDS_UPDATER_FILTER_CHIP_EVENT_TYPE},
      {"filterChipUpdateOutcome", IDS_UPDATER_FILTER_CHIP_UPDATE_OUTCOME},
      {"installSummary", IDS_UPDATER_INSTALL_SUMMARY},
      {"internal", IDS_UPDATER_INTERNAL},
      {"nextVersion", IDS_UPDATER_NEXT_VERSION},
      {"noUpdate", IDS_UPDATER_NO_UPDATE},
      {"omahaRequest", IDS_UPDATER_OMAHA_REQUEST},
      {"omahaResponse", IDS_UPDATER_OMAHA_RESPONSE},
      {"other", IDS_UPDATER_OTHER},
      {"outcome", IDS_UPDATER_OUTCOME},
      {"outcomeUnknown", IDS_UPDATER_OUTCOME_UNKNOWN},
      {"persistedDataSummary", IDS_UPDATER_PERSISTED_DATA_SUMMARY},
      {"processSummary", IDS_UPDATER_PROCESS_SUMMARY},
      {"qualificationFailed", IDS_UPDATER_QUALIFICATION_FAILED},
      {"qualificationSucceeded", IDS_UPDATER_QUALIFICATION_SUCCEEDED},
      {"removeFilter", IDS_UPDATER_REMOVE_FILTER},
      {"startDate", IDS_UPDATER_START_DATE},
      {"uninstallSummary", IDS_UPDATER_UNINSTALL_SUMMARY},
      {"updateError", IDS_UPDATER_UPDATE_ERROR},
      {"updateOutcome", IDS_UPDATER_UPDATE_OUTCOME},
      {"updateOutcomeNO_UPDATE", IDS_UPDATER_UPDATE_OUTCOME_NO_UPDATE},
      {"updateOutcomeUPDATED", IDS_UPDATER_UPDATE_OUTCOME_UPDATED},
      {"updateOutcomeUPDATE_ERROR", IDS_UPDATER_UPDATE_OUTCOME_UPDATE_ERROR},
      {"updatedTo", IDS_UPDATER_UPDATED_TO},
      {"updaterVersion", IDS_UPDATER_UPDATER_VERSION},
      {"viewRawDetails", IDS_UPDATER_VIEW_RAW_DETAILS},
  });

  int32_t num_known_apps = 0;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  AddKnownApp(*source, num_known_apps++, IDS_PRODUCT_NAME,
              {"{8A69D345-D564-463C-AFF1-A69D9E530F96}", "COM.GOOGLE.CHROME"});
  AddKnownApp(
      *source, num_known_apps++, IDS_SHORTCUT_NAME_BETA,
      {"{8237E44A-0054-442C-B6B6-EA0509993955}", "COM.GOOGLE.CHROME.BETA"});
  AddKnownApp(
      *source, num_known_apps++, IDS_SXS_SHORTCUT_NAME,
      {"{4EA16AC7-FD5A-47C3-875B-DBF4A2008C20}", "COM.GOOGLE.CHROME.CANARY"});
  AddKnownApp(
      *source, num_known_apps++, IDS_SHORTCUT_NAME_DEV,
      {"{401C381F-E0DE-4B85-8BD8-3F3F14FBDA57}", "COM.GOOGLE.CHROME.DEV"});
  source->AddLocalizedString("defaultAppFilters", IDS_PRODUCT_NAME);
#else
  source->AddString("defaultAppFilters", "");
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
  source->AddInteger("numKnownApps", num_known_apps);

  webui::SetupWebUIDataSource(source, kUpdaterResources,
                              IDR_UPDATER_UPDATER_HTML);
}

UpdaterUI::~UpdaterUI() = default;
