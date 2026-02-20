// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/updater/updater_ui.h"

#include <cstdint>
#include <initializer_list>
#include <memory>
#include <string_view>
#include <utility>
#include <variant>

#include "base/check.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/plural_string_handler.h"
#include "chrome/browser/ui/webui/updater/updater_page_handler.h"
#include "chrome/browser/ui/webui/updater/updater_ui.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/updater_resources.h"
#include "chrome/grit/updater_resources_map.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "ui/webui/webui_util.h"

namespace {

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// Associates an app name with one or more AppIds in the load-time data made
// available to the front-end. `app_name` may be a localized string identified
// by an IDS code or a provided string.
void AddKnownApp(content::WebUIDataSource& source,
                 int index,
                 std::variant<int, std::string_view> app_name,
                 std::initializer_list<std::string_view> app_ids) {
  std::visit(
      absl::Overload{
          [&](int app_name_ids) {
            source.AddLocalizedString(
                base::StrCat({"knownAppName", base::NumberToString(index)}),
                app_name_ids);
          },
          [&](std::string_view app_name) {
            source.AddString(
                base::StrCat({"knownAppName", base::NumberToString(index)}),
                app_name);
          },
      },
      app_name);
  source.AddString(base::StrCat({"knownAppIds", base::NumberToString(index)}),
                   base::JoinString(app_ids, ","));
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

}  // namespace

bool UpdaterUIConfig::IsWebUIEnabled(content::BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(features::kUpdaterUI);
}

// enable_chrome_send is needed for plural_string_handler.
UpdaterUI::UpdaterUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, /*enable_chrome_send=*/true) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUIUpdaterHost);
  source->AddLocalizedStrings({
      {"activationFailed", IDS_UPDATER_ACTIVATION_FAILED},
      {"activationSucceeded", IDS_UPDATER_ACTIVATION_SUCCEEDED},
      {"addFilter", IDS_UPDATER_ADD_FILTER},
      {"app", IDS_UPDATER_APP},
      {"appColumn", IDS_UPDATER_APP_COLUMN},
      {"appNameOrId", IDS_UPDATER_APP_NAME_OR_ID},
      {"appPolicies", IDS_UPDATER_APP_POLICIES},
      {"apply", IDS_UPDATER_APPLY},
      {"appStatesQueryFailed", IDS_UPDATER_APP_STATES_QUERY_FAILED},
      {"cancel", IDS_UPDATER_CANCEL},
      {"clearAllFilters", IDS_UPDATER_CLEAR_ALL_FILTERS},
      {"collapseAll", IDS_UPDATER_COLLAPSE_ALL},
      {"commandLine", IDS_UPDATER_COMMAND_LINE},
      {"commandOutcome", IDS_UPDATER_COMMAND_OUTCOME},
      {"common", IDS_UPDATER_COMMON},
      {"date", IDS_UPDATER_DATE},
      {"dateFilterAfter", IDS_UPDATER_DATE_FILTER_AFTER},
      {"dateFilterBefore", IDS_UPDATER_DATE_FILTER_BEFORE},
      {"dateFilterRange", IDS_UPDATER_DATE_FILTER_RANGE},
      {"displayedEventsCount", IDS_UPDATER_DISPLAYED_EVENTS_COUNT},
      {"duration", IDS_UPDATER_DURATION},
      {"endDate", IDS_UPDATER_END_DATE},
      {"updaterError-accessDenied", IDS_UPDATER_ERROR_ACCESS_DENIED},
      {"updaterError-corrupt", IDS_UPDATER_ERROR_CORRUPT},
      {"updaterError-disabled", IDS_UPDATER_ERROR_DISABLED},
      {"updaterError-diskFull", IDS_UPDATER_ERROR_DISK_FULL},
      {"updaterError-download", IDS_UPDATER_ERROR_DOWNLOAD},
      {"updaterError-install", IDS_UPDATER_ERROR_INSTALL},
      {"updaterError-installerFailed", IDS_UPDATER_ERROR_INSTALLER_FAILED},
      {"updaterError-network", IDS_UPDATER_ERROR_NETWORK},
      {"updaterError-restricted", IDS_UPDATER_ERROR_RESTRICTED},
      {"updaterError-unpack", IDS_UPDATER_ERROR_UNPACK},
      {"updaterError-unknown", IDS_UPDATER_ERROR_UNKNOWN},
      {"updaterError-updateCheck", IDS_UPDATER_ERROR_UPDATE_CHECK},
      {"eventListTitle", IDS_UPDATER_EVENT_LIST_TITLE},
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
      {"expandAll", IDS_UPDATER_EXPAND_ALL},
      {"filterChipApp", IDS_UPDATER_FILTER_CHIP_APP},
      {"filterChipDate", IDS_UPDATER_FILTER_CHIP_DATE},
      {"filterChipEventType", IDS_UPDATER_FILTER_CHIP_EVENT_TYPE},
      {"filterChipUpdateOutcome", IDS_UPDATER_FILTER_CHIP_UPDATE_OUTCOME},
      {"filterChipUpdaterScope", IDS_UPDATER_FILTER_CHIP_UPDATER_SCOPE},
      {"inactiveVersions", IDS_UPDATER_INACTIVE_VERSIONS_LABEL},
      {"installPath", IDS_UPDATER_INSTALL_PATH_LABEL},
      {"installSummary", IDS_UPDATER_INSTALL_SUMMARY},
      {"installedAppsTitle", IDS_UPDATER_INSTALLED_APPS_TITLE},
      {"internal", IDS_UPDATER_INTERNAL},
      {"lastChecked", IDS_UPDATER_LAST_CHECKED_LABEL},
      {"lastStarted", IDS_UPDATER_LAST_STARTED_LABEL},
      {"never", IDS_UPDATER_NEVER},
      {"nextVersion", IDS_UPDATER_NEXT_VERSION},
      {"noAppsFound", IDS_UPDATER_NO_APPS_FOUND},
      {"noPolicies", IDS_UPDATER_NO_POLICIES},
      {"noUpdate", IDS_UPDATER_NO_UPDATE},
      {"noUpdaterFound", IDS_UPDATER_NO_UPDATER_FOUND},
      {"omahaRequest", IDS_UPDATER_OMAHA_REQUEST},
      {"omahaResponse", IDS_UPDATER_OMAHA_RESPONSE},
      {"other", IDS_UPDATER_OTHER},
      {"outcome", IDS_UPDATER_OUTCOME},
      {"outcomeUnknown", IDS_UPDATER_OUTCOME_UNKNOWN},
      {"persistedDataSummary", IDS_UPDATER_PERSISTED_DATA_SUMMARY},
      {"policyConflictWarning", IDS_UPDATER_POLICY_CONFLICT_WARNING},
      {"policyDetails", IDS_UPDATER_EFFECTIVE_POLICY_SET},
      {"policyIgnored", IDS_UPDATER_POLICY_IGNORED},
      {"policyName", IDS_UPDATER_POLICY_NAME},
      {"policyOk", IDS_UPDATER_POLICY_OK},
      {"policySource", IDS_UPDATER_POLICY_SOURCE},
      {"policyStatus", IDS_UPDATER_POLICY_STATUS},
      {"policyValue", IDS_UPDATER_POLICY_VALUE},
      {"processSummary", IDS_UPDATER_PROCESS_SUMMARY},
      {"qualificationFailed", IDS_UPDATER_QUALIFICATION_FAILED},
      {"qualificationSucceeded", IDS_UPDATER_QUALIFICATION_SUCCEEDED},
      {"removeFilter", IDS_UPDATER_REMOVE_FILTER},
      {"scope", IDS_UPDATER_SCOPE},
      {"scopeSystem", IDS_UPDATER_SCOPE_SYSTEM},
      {"scopeUser", IDS_UPDATER_SCOPE_USER},
      {"startDate", IDS_UPDATER_START_DATE},
      {"title", IDS_UPDATER_PAGE_TITLE},
      {"uninstallSummary", IDS_UPDATER_UNINSTALL_SUMMARY},
      {"updateError", IDS_UPDATER_UPDATE_ERROR},
      {"updateOutcome", IDS_UPDATER_UPDATE_OUTCOME},
      {"updateOutcomeNO_UPDATE", IDS_UPDATER_UPDATE_OUTCOME_NO_UPDATE},
      {"updateOutcomeUPDATED", IDS_UPDATER_UPDATE_OUTCOME_UPDATED},
      {"updateOutcomeUPDATE_ERROR", IDS_UPDATER_UPDATE_OUTCOME_UPDATE_ERROR},
      {"updatedTo", IDS_UPDATER_UPDATED_TO},
      {"updaterPolicies", IDS_UPDATER_UPDATER_POLICIES},
      {"updaterStateQueryFailed", IDS_UPDATER_QUERY_FAILED},
      {"updaterStateTitle", IDS_UPDATER_STATE_TITLE},
      {"updaterVersion", IDS_UPDATER_UPDATER_VERSION},
      {"versionColumn", IDS_UPDATER_VERSION_COLUMN},
      {"versionLabel", IDS_UPDATER_VERSION_LABEL},
      {"viewRawDetails", IDS_UPDATER_VIEW_RAW_DETAILS},
  });

  // Add a handler to provide pluralized strings.
  auto plural_string_handler = std::make_unique<PluralStringHandler>();
  plural_string_handler->AddLocalizedString("parseErrorEvents",
                                            IDS_UPDATER_PARSE_ERROR_EVENTS);
  plural_string_handler->AddLocalizedString("undatedEvents",
                                            IDS_UPDATER_UNDATED_EVENTS);
  web_ui->AddMessageHandler(std::move(plural_string_handler));

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
  AddKnownApp(*source, num_known_apps++, "Google Updater",
              {"{44FC7FE2-65CE-487C-93F4-EDEE46EEAAAB}"});
  AddKnownApp(*source, num_known_apps++, "Chrome Enterprise Companion App",
              {"{85EEDF37-756C-4972-9399-5A12A4BEE148}"});
  source->AddLocalizedString("defaultAppFilters", IDS_PRODUCT_NAME);
#else
  source->AddString("defaultAppFilters", "");
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
  source->AddInteger("numKnownApps", num_known_apps);

  webui::SetupWebUIDataSource(source, kUpdaterResources,
                              IDR_UPDATER_UPDATER_HTML);
}

WEB_UI_CONTROLLER_TYPE_IMPL(UpdaterUI)

UpdaterUI::~UpdaterUI() = default;

void UpdaterUI::BindInterface(
    mojo::PendingReceiver<updater_ui::mojom::PageHandlerFactory> receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void UpdaterUI::CreatePageHandler(
    mojo::PendingRemote<updater_ui::mojom::Page> page,
    mojo::PendingReceiver<updater_ui::mojom::PageHandler> receiver) {
  CHECK(page);
  page_handler_ = std::make_unique<UpdaterPageHandler>(
      Profile::FromWebUI(web_ui()), std::move(receiver), std::move(page));
}
