// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/printing/printing_section.h"

#include "ash/constants/ash_features.h"
#include "ash/webui/settings/public/constants/routes.mojom-forward.h"
#include "base/no_destructor.h"
#include "chrome/browser/ui/webui/ash/settings/pages/printing/cups_printers_handler.h"
#include "chrome/browser/ui/webui/ash/settings/search/search_tag_registry.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/printing/printer_configuration.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

namespace ash::settings {

namespace mojom {
using ::chromeos::settings::mojom::kDeviceSectionPath;
using ::chromeos::settings::mojom::kPrintingDetailsSubpagePath;
using ::chromeos::settings::mojom::kPrintingSectionPath;
using ::chromeos::settings::mojom::Section;
using ::chromeos::settings::mojom::Setting;
using ::chromeos::settings::mojom::Subpage;
}  // namespace mojom

namespace {

const std::vector<SearchConcept>& GetPrintingSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_PRINTING_ADD_PRINTER,
       mojom::kPrintingDetailsSubpagePath,
       mojom::SearchResultIcon::kPrinter,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kAddPrinter}},
      {IDS_OS_SETTINGS_TAG_PRINTING,
       mojom::kPrintingDetailsSubpagePath,
       mojom::SearchResultIcon::kPrinter,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kPrintingDetails},
       {IDS_OS_SETTINGS_TAG_PRINTING_ALT1, IDS_OS_SETTINGS_TAG_PRINTING_ALT2,
        SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetSavedPrintersSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_PRINTING_SAVED_PRINTERS,
       mojom::kPrintingDetailsSubpagePath,
       mojom::SearchResultIcon::kPrinter,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kSavedPrinters}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetPrintingManagementSearchConcepts() {
  const char* url_path = ash::features::IsOsSettingsRevampWayfindingEnabled()
                             ? mojom::kPrintingDetailsSubpagePath
                             : mojom::kPrintingSectionPath;
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_PRINT_MANAGEMENT,
       url_path,
       mojom::SearchResultIcon::kPrinter,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kPrintJobs},
       {IDS_OS_SETTINGS_TAG_PRINT_MANAGEMENT_ALT1,
        IDS_OS_SETTINGS_TAG_PRINT_MANAGEMENT_ALT2, SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetScanningAppSearchConcepts(
    const char* section_path) {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_SCANNING_APP,
       section_path,
       ash::features::IsOsSettingsRevampWayfindingEnabled()
           ? mojom::SearchResultIcon::kScanner
           : mojom::SearchResultIcon::kPrinter,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kScanningApp}},
  });
  return *tags;
}

}  // namespace

PrintingSection::PrintingSection(Profile* profile,
                                 SearchTagRegistry* search_tag_registry,
                                 CupsPrintersManager* printers_manager)
    : OsSettingsSection(profile, search_tag_registry),
      printers_manager_(printers_manager) {
  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();
  updater.AddSearchTags(GetPrintingSearchConcepts());
  updater.AddSearchTags(GetPrintingManagementSearchConcepts());
  updater.AddSearchTags(GetScanningAppSearchConcepts(GetSectionPath()));

  // Saved Printers search tags are added/removed dynamically.
  if (printers_manager_) {
    printers_manager_->AddObserver(this);
    UpdateSavedPrintersSearchTags();
  }
}

PrintingSection::~PrintingSection() {
  if (printers_manager_) {
    printers_manager_->RemoveObserver(this);
  }
}

void PrintingSection::AddLoadTimeData(content::WebUIDataSource* html_source) {
  const bool kIsRevampEnabled =
      ash::features::IsOsSettingsRevampWayfindingEnabled();

  webui::LocalizedString kLocalizedStrings[] = {
      {"printingPageTitle", IDS_SETTINGS_PRINT_AND_SCAN},
      {"cupsPrintTitle", kIsRevampEnabled
                             ? IDS_OS_SETTINGS_REVAMP_PRINTING_CUPS_PRINT_TITLE
                             : IDS_SETTINGS_PRINTING_CUPS_PRINTERS},
      {"cupsPrintDescription",
       IDS_OS_SETTINGS_REVAMP_PRINTING_CUPS_PRINT_DESCRIPTION},
      {"cupsPrintersLearnMoreLabel",
       IDS_SETTINGS_PRINTING_CUPS_PRINTERS_LEARN_MORE_LABEL},
      {"addCupsPrinterManually",
       IDS_SETTINGS_PRINTING_CUPS_PRINTERS_ADD_PRINTER_MANUALLY},
      {"editPrinter", IDS_SETTINGS_PRINTING_CUPS_PRINTERS_EDIT},
      {"viewPrinter", IDS_SETTINGS_PRINTING_CUPS_PRINTERS_VIEW},
      {"removePrinter", IDS_SETTINGS_PRINTING_CUPS_PRINTERS_REMOVE},
      {"cupsPrintersViewPpd", IDS_SETTINGS_PRINTING_CUPS_PRINTERS_VIEW_PPD},
      {"savePrinter", IDS_SETTINGS_PRINTING_CUPS_PRINTER_SAVE_BUTTON},
      {"searchLabel", IDS_SETTINGS_PRINTING_CUPS_SEARCH_LABEL},
      {"noSearchResults", IDS_SEARCH_NO_RESULTS},
      {"printJobsTitle",
       IDS_SETTINGS_PRINTING_PRINT_JOBS_LAUNCH_APP_TITLE_LABEL},
      {"printJobsSublabel",
       IDS_SETTINGS_PRINTING_PRINT_JOBS_LAUNCH_APP_SUBLABEL},
      {"scanAppTitle", IDS_SETTINGS_PRINTING_SCANNING_LAUNCH_APP_TITLE_LABEL},
      {"scanAppSublabel", IDS_SETTINGS_PRINTING_SCANNING_LAUNCH_APP_SUBLABEL},
      {"printerDetailsTitle", IDS_SETTINGS_PRINTING_CUPS_PRINTER_DETAILS_TITLE},
      {"printerName", IDS_SETTINGS_PRINTING_CUPS_PRINTER_DETAILS_NAME},
      {"printerModel", IDS_SETTINGS_PRINTING_CUPS_PRINTER_DETAILS_MODEL},
      {"printerQueue", IDS_SETTINGS_PRINTING_CUPS_PRINTER_DETAILS_QUEUE},
      {"savedPrintersTitle", IDS_SETTINGS_PRINTING_CUPS_SAVED_PRINTERS_TITLE},
      {"savedPrintersSubtext",
       IDS_SETTINGS_PRINTING_CUPS_SAVED_PRINTERS_SUBTEXT},
      {"savedPrintersCountMany",
       IDS_SETTINGS_PRINTING_CUPS_PRINTERS_SAVED_PRINTERS_COUNT_MANY},
      {"savedPrintersCountOne",
       IDS_SETTINGS_PRINTING_CUPS_PRINTERS_SAVED_PRINTERS_COUNT_ONE},
      {"savedPrintersCountNone",
       IDS_SETTINGS_PRINTING_CUPS_PRINTERS_SAVED_PRINTERS_COUNT_NONE},
      {"noSavedPrinters", IDS_SETTINGS_PRINTING_CUPS_NO_SAVED_PRINTERS},
      {"helpSectionTitle", IDS_SETTINGS_PRINTING_CUPS_HELP_SECTION_TITLE},
      {"helpSectionDescription",
       IDS_SETTINGS_PRINTING_CUPS_HELP_SECTION_DESCRIPTION},
      {"showMorePrinters", IDS_SETTINGS_PRINTING_CUPS_SHOW_MORE},
      {"addPrintersNearbyTitle",
       IDS_SETTINGS_PRINTING_CUPS_ADD_PRINTERS_NEARBY_TITLE},
      {"addPrintersManuallyTitle",
       IDS_SETTINGS_PRINTING_CUPS_ADD_PRINTERS_MANUALLY_TITLE},
      {"manufacturerAndModelDialogTitle",
       IDS_SETTINGS_PRINTING_CUPS_SELECT_MANUFACTURER_AND_MODEL_TITLE},
      {"availablePrintersReadyTitle",
       IDS_SETTINGS_PRINTING_CUPS_PRINTERS_AVAILABLE_PRINTERS_READY},
      {"availablePrintersReadySubtext",
       IDS_SETTINGS_PRINTING_CUPS_PRINTERS_AVAILABLE_PRINTERS_READY_SUBTEXT},
      {"nearbyPrintersCountMany",
       IDS_SETTINGS_PRINTING_CUPS_PRINTERS_AVAILABLE_PRINTERS_COUNT_MANY},
      {"nearbyPrintersCountOne",
       IDS_SETTINGS_PRINTING_CUPS_PRINTERS_AVAILABLE_PRINTER_COUNT_ONE},
      {"nearbyPrintersCountNone",
       IDS_SETTINGS_PRINTING_CUPS_PRINTERS_AVAILABLE_PRINTER_COUNT_NONE},
      {"enterprisePrintersTitle",
       IDS_SETTINGS_PRINTING_ENTERPRISE_PRINTERS_TITLE},
      {"enterprisePrintersCountMany",
       IDS_SETTINGS_PRINTING_ENTERPRISE_PRINTERS_AVAILABLE_PRINTERS_COUNT_MANY},
      {"enterprisePrintersCountOne",
       IDS_SETTINGS_PRINTING_ENTERPRISE_PRINTERS_AVAILABLE_PRINTER_COUNT_ONE},
      {"enterprisePrintersCountNone",
       IDS_SETTINGS_PRINTING_ENTERPRISE_PRINTERS_AVAILABLE_PRINTER_COUNT_NONE},
      {"manufacturerAndModelAdditionalInformation",
       IDS_SETTINGS_PRINTING_CUPS_MANUFACTURER_MODEL_ADDITIONAL_INFORMATION},
      {"addPrinterButtonText",
       IDS_SETTINGS_PRINTING_CUPS_ADD_PRINTER_BUTTON_ADD},
      {"printerDetailsAdvanced", IDS_SETTINGS_PRINTING_CUPS_PRINTER_ADVANCED},
      {"printerDetailsA11yLabel",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_ADVANCED_ACCESSIBILITY_LABEL},
      {"printerAddress", IDS_SETTINGS_PRINTING_CUPS_PRINTER_ADVANCED_ADDRESS},
      {"printerProtocol", IDS_SETTINGS_PRINTING_CUPS_PRINTER_ADVANCED_PROTOCOL},
      {"printerURI", IDS_SETTINGS_PRINTING_CUPS_PRINTER_ADVANCED_URI},
      {"manuallyAddPrinterButtonText",
       IDS_SETTINGS_PRINTING_CUPS_ADD_PRINTER_BUTTON_MANUAL_ADD},
      {"discoverPrintersButtonText",
       IDS_SETTINGS_PRINTING_CUPS_ADD_PRINTER_BUTTON_DISCOVER_PRINTERS},
      {"printerProtocolIpp", IDS_SETTINGS_PRINTING_CUPS_PRINTER_PROTOCOL_IPP},
      {"printerProtocolIpps", IDS_SETTINGS_PRINTING_CUPS_PRINTER_PROTOCOL_IPPS},
      {"printerProtocolHttp", IDS_SETTINGS_PRINTING_CUPS_PRINTER_PROTOCOL_HTTP},
      {"printerProtocolHttps",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_PROTOCOL_HTTPS},
      {"printerProtocolAppSocket",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_PROTOCOL_APP_SOCKET},
      {"printerProtocolLpd", IDS_SETTINGS_PRINTING_CUPS_PRINTER_PROTOCOL_LPD},
      {"printerProtocolUsb", IDS_SETTINGS_PRINTING_CUPS_PRINTER_PROTOCOL_USB},
      {"printerProtocolIppUsb",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_PROTOCOL_IPPUSB},
      {"printerConfiguringMessage",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_CONFIGURING_MESSAGE},
      {"printerManufacturer", IDS_SETTINGS_PRINTING_CUPS_PRINTER_MANUFACTURER},
      {"selectDriver", IDS_SETTINGS_PRINTING_CUPS_PRINTER_SELECT_DRIVER},
      {"advancedConfigSelectDriver",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_ADVANCED_CONFIG_SELECT_DRIVER},
      {"selectDriverButtonText",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_BUTTON_SELECT_DRIVER},
      {"selectDriverButtonAriaLabel",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_BUTTON_SELECT_DRIVER_ARIA_LABEL},
      {"selectDriverErrorMessage",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_INVALID_DRIVER},
      {"printerAddedSuccessfulMessage",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_ADDED_PRINTER_DONE_MESSAGE},
      {"printerEditedSuccessfulMessage",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_EDITED_PRINTER_DONE_MESSAGE},
      {"printerUnavailableMessage",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_UNAVAILABLE_MESSAGE},
      {"noPrinterNearbyMessage",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_NO_PRINTER_NEARBY},
      {"searchingNearbyPrinters",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_SEARCHING_NEARBY_PRINTER},
      {"printerAddedFailedMessage",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_ADDED_PRINTER_ERROR_MESSAGE},
      {"printerAddedFatalErrorMessage",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_ADDED_PRINTER_FATAL_ERROR_MESSAGE},
      {"printerAddedUnreachableMessage",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_ADDED_PRINTER_PRINTER_UNREACHABLE_MESSAGE},
      {"printerAddedPpdTooLargeMessage",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_ADDED_PRINTER_PPD_TOO_LARGE_MESSAGE},
      {"printerAddedInvalidPpdMessage",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_ADDED_PRINTER_INVALID_PPD_MESSAGE},
      {"printerAddedPpdNotFoundMessage",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_ADDED_PRINTER_PPD_NOT_FOUND},
      {"printerAddedPpdUnretrievableMessage",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_ADDED_PRINTER_PPD_UNRETRIEVABLE},
      {"printerAddedNativePrintersNotAllowedMessage",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_ADDED_NATIVE_PRINTERS_NOT_ALLOWED_MESSAGE},
      {"editPrinterInvalidPrinterUpdate",
       IDS_SETTINGS_PRINTING_CUPS_EDIT_PRINTER_INVALID_PRINTER_UPDATE},
      {"requireNetworkMessage",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_REQUIRE_INTERNET_MESSAGE},
      {"checkNetworkMessage",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_CHECK_CONNECTION_MESSAGE},
      {"noInternetConnection",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_NO_INTERNET_CONNECTION},
      {"checkNetworkAndTryAgain",
       IDS_SETTINGS_PRINTING_CUPS_PRINTER_CONNECT_TO_NETWORK_SUBTEXT},
      {"editPrinterDialogTitle",
       IDS_SETTINGS_PRINTING_CUPS_EDIT_PRINTER_DIALOG_TITLE},
      {"viewPrinterDialogTitle",
       IDS_SETTINGS_PRINTING_CUPS_VIEW_PRINTER_DIALOG_TITLE},
      {"editPrinterButtonText", IDS_SETTINGS_PRINTING_CUPS_EDIT_PRINTER_BUTTON},
      {"currentPpdMessage",
       IDS_SETTINGS_PRINTING_CUPS_EDIT_PRINTER_CURRENT_PPD_MESSAGE},
      {"printerEulaNotice", IDS_SETTINGS_PRINTING_CUPS_EULA_NOTICE},
      {"ippPrinterUnreachable", IDS_SETTINGS_PRINTING_CUPS_IPP_URI_UNREACHABLE},
      {"generalPrinterDialogError",
       IDS_SETTINGS_PRINTING_CUPS_DIALOG_GENERAL_ERROR},
      {"printServerButtonText", IDS_SETTINGS_PRINTING_CUPS_PRINT_SERVER},
      {"addPrintServerTitle",
       IDS_SETTINGS_PRINTING_CUPS_ADD_PRINT_SERVER_TITLE},
      {"printServerAddress", IDS_SETTINGS_PRINTING_CUPS_PRINT_SERVER_ADDRESS},
      {"printServerFoundZeroPrinters",
       IDS_SETTINGS_PRINTING_CUPS_PRINT_SERVER_FOUND_ZERO_PRINTERS},
      {"printServerFoundOnePrinter",
       IDS_SETTINGS_PRINTING_CUPS_PRINT_SERVER_FOUND_ONE_PRINTER},
      {"printServerFoundManyPrinters",
       IDS_SETTINGS_PRINTING_CUPS_PRINT_SERVER_FOUND_MANY_PRINTERS},
      {"printServerInvalidUrlAddress",
       IDS_SETTINGS_PRINTING_CUPS_PRINT_SERVER_INVALID_URL_ADDRESS},
      {"printServerConnectionError",
       IDS_SETTINGS_PRINTING_CUPS_PRINT_SERVER_CONNECTION_ERROR},
      {"printServerConfigurationErrorMessage",
       IDS_SETTINGS_PRINTING_CUPS_PRINT_SERVER_REACHABLE_BUT_CANNOT_ADD},
      {"printerStatusDeviceError",
       IDS_SETTINGS_PRINTING_PRINTER_STATUS_DEVICE_ERROR},
      {"printerStatusDoorOpen", IDS_SETTINGS_PRINTING_PRINTER_STATUS_DOOR_OPEN},
      {"printerStatusLowOnInk",
       IDS_SETTINGS_PRINTING_PRINTER_STATUS_LOW_ON_INK},
      {"printerStatusLowOnPaper",
       IDS_SETTINGS_PRINTING_PRINTER_STATUS_LOW_ON_PAPER},
      {"printerStatusOutOfInk",
       IDS_SETTINGS_PRINTING_PRINTER_STATUS_OUT_OF_INK},
      {"printerStatusOutOfPaper",
       IDS_SETTINGS_PRINTING_PRINTER_STATUS_OUT_OF_PAPER},
      {"printerStatusOutputAlmostFull",
       IDS_SETTINGS_PRINTING_PRINTER_STATUS_OUPUT_ALMOST_FULL},
      {"printerStatusOutputFull",
       IDS_SETTINGS_PRINTING_PRINTER_STATUS_OUPUT_FULL},
      {"printerStatusPaperJam", IDS_SETTINGS_PRINTING_PRINTER_STATUS_PAPER_JAM},
      {"printerStatusPaused", IDS_SETTINGS_PRINTING_PRINTER_STATUS_PAUSED},
      {"printerStatusPrinterQueueFull",
       IDS_SETTINGS_PRINTING_PRINTER_STATUS_PRINTER_QUEUE_FULL},
      {"printerStatusPrinterUnreachable",
       IDS_SETTINGS_PRINTING_PRINTER_STATUS_PRINTER_UNREACHABLE},
      {"printerStatusStopped", IDS_SETTINGS_PRINTING_PRINTER_STATUS_STOPPED},
      {"printerStatusTrayMissing",
       IDS_SETTINGS_PRINTING_PRINTER_STATUS_TRAY_MISSING},
      {"printerEntryAriaLabel", IDS_SETTINGS_PRINTING_PRINTER_ENTRY_ARIA_LABEL},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  html_source->AddString("printingCUPSPrintLearnMoreUrl",
                         GetHelpUrlWithBoard(chrome::kCupsPrintLearnMoreURL));
  html_source->AddString(
      "printingCUPSPrintPpdLearnMoreUrl",
      GetHelpUrlWithBoard(chrome::kCupsPrintPPDLearnMoreURL));
}

void PrintingSection::AddHandlers(content::WebUI* web_ui) {
  web_ui->AddMessageHandler(
      std::make_unique<CupsPrintersHandler>(profile(), printers_manager_));
}

int PrintingSection::GetSectionNameMessageId() const {
  return IDS_SETTINGS_PRINT_AND_SCAN;
}

mojom::Section PrintingSection::GetSection() const {
  return ash::features::IsOsSettingsRevampWayfindingEnabled()
             ? mojom::Section::kDevice
             : mojom::Section::kPrinting;
}

mojom::SearchResultIcon PrintingSection::GetSectionIcon() const {
  return mojom::SearchResultIcon::kPrinter;
}

const char* PrintingSection::GetSectionPath() const {
  return ash::features::IsOsSettingsRevampWayfindingEnabled()
             ? mojom::kDeviceSectionPath
             : mojom::kPrintingSectionPath;
}

bool PrintingSection::LogMetric(mojom::Setting setting,
                                base::Value& value) const {
  // Unimplemented.
  return false;
}

void PrintingSection::RegisterHierarchy(HierarchyGenerator* generator) const {
  generator->RegisterTopLevelSetting(mojom::Setting::kPrintJobs);
  generator->RegisterTopLevelSetting(mojom::Setting::kScanningApp);

  // Printing details.
  generator->RegisterTopLevelSubpage(IDS_SETTINGS_PRINTING_CUPS_PRINTERS,
                                     mojom::Subpage::kPrintingDetails,
                                     mojom::SearchResultIcon::kPrinter,
                                     mojom::SearchResultDefaultRank::kMedium,
                                     mojom::kPrintingDetailsSubpagePath);
  static constexpr mojom::Setting kPrintingDetailsSettings[] = {
      mojom::Setting::kAddPrinter,
      mojom::Setting::kSavedPrinters,
      mojom::Setting::kRemovePrinter,
  };
  RegisterNestedSettingBulk(mojom::Subpage::kPrintingDetails,
                            kPrintingDetailsSettings, generator);
}

void PrintingSection::OnPrintersChanged(
    chromeos::PrinterClass printer_class,
    const std::vector<chromeos::Printer>& printers) {
  UpdateSavedPrintersSearchTags();
}

void PrintingSection::UpdateSavedPrintersSearchTags() {
  // Start with no saved printers search tags.
  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();
  updater.RemoveSearchTags(GetSavedPrintersSearchConcepts());

  std::vector<chromeos::Printer> saved_printers =
      printers_manager_->GetPrinters(chromeos::PrinterClass::kSaved);
  if (!saved_printers.empty()) {
    updater.AddSearchTags(GetSavedPrintersSearchConcepts());
  }
}

}  // namespace ash::settings
