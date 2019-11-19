// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/print_preview_ui.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/containers/id_map.h"
#include "base/feature_list.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/printing/background_printing_manager.h"
#include "chrome/browser/printing/print_preview_data_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/webui/localized_string.h"
#include "chrome/browser/ui/webui/metrics_handler.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_handler.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/component_extension_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/print_preview_resources.h"
#include "chrome/grit/print_preview_resources_map.h"
#include "components/prefs/pref_service.h"
#include "components/printing/browser/print_manager_utils.h"
#include "components/printing/common/print_messages.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "extensions/common/constants.h"
#include "printing/page_size_margins.h"
#include "printing/print_job_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/resources/grit/webui_resources.h"
#include "ui/web_dialogs/web_dialog_delegate.h"
#include "ui/web_dialogs/web_dialog_ui.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#elif defined(OS_WIN)
#include "base/enterprise_util.h"
#endif

#if !BUILDFLAG(OPTIMIZE_WEBUI)
#include "chrome/browser/ui/webui/managed_ui_handler.h"
#endif

using content::WebContents;

namespace printing {

namespace {

#if defined(OS_MACOSX)
// U+0028 U+21E7 U+2318 U+0050 U+0029 in UTF8
const char kBasicPrintShortcut[] = "\x28\xE2\x8c\xA5\xE2\x8C\x98\x50\x29";
#elif !defined(OS_CHROMEOS)
const char kBasicPrintShortcut[] = "(Ctrl+Shift+P)";
#endif

#if !BUILDFLAG(OPTIMIZE_WEBUI)
constexpr char kGeneratedPath[] =
    "@out_folder@/gen/chrome/browser/resources/print_preview/";
#endif

PrintPreviewUI::TestDelegate* g_test_delegate = nullptr;

// Thread-safe wrapper around a std::map to keep track of mappings from
// PrintPreviewUI IDs to most recent print preview request IDs.
class PrintPreviewRequestIdMapWithLock {
 public:
  PrintPreviewRequestIdMapWithLock() {}
  ~PrintPreviewRequestIdMapWithLock() {}

  // Gets the value for |preview_id|.
  // Returns true and sets |out_value| on success.
  bool Get(int32_t preview_id, int* out_value) {
    base::AutoLock lock(lock_);
    PrintPreviewRequestIdMap::const_iterator it = map_.find(preview_id);
    if (it == map_.end())
      return false;
    *out_value = it->second;
    return true;
  }

  // Sets the |value| for |preview_id|.
  void Set(int32_t preview_id, int value) {
    base::AutoLock lock(lock_);
    map_[preview_id] = value;
  }

  // Erases the entry for |preview_id|.
  void Erase(int32_t preview_id) {
    base::AutoLock lock(lock_);
    map_.erase(preview_id);
  }

 private:
  // Mapping from PrintPreviewUI ID to print preview request ID.
  typedef std::map<int, int> PrintPreviewRequestIdMap;

  PrintPreviewRequestIdMap map_;
  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(PrintPreviewRequestIdMapWithLock);
};

// Written to on the UI thread, read from any thread.
base::LazyInstance<PrintPreviewRequestIdMapWithLock>::DestructorAtExit
    g_print_preview_request_id_map = LAZY_INSTANCE_INITIALIZER;

// PrintPreviewUI IDMap used to avoid exposing raw pointer addresses to WebUI.
// Only accessed on the UI thread.
base::LazyInstance<base::IDMap<PrintPreviewUI*>>::DestructorAtExit
    g_print_preview_ui_id_map = LAZY_INSTANCE_INITIALIZER;

bool ShouldHandleRequestCallback(const std::string& path) {
  // ChromeWebUIDataSource handles most requests except for the print preview
  // data.
  return PrintPreviewUI::ParseDataPath(path, nullptr, nullptr);
}

// Get markup or other resources for the print preview page.
void HandleRequestCallback(
    const std::string& path,
    const content::WebUIDataSource::GotDataCallback& callback) {
  // ChromeWebUIDataSource handles most requests except for the print preview
  // data.
  int preview_ui_id;
  int page_index;
  CHECK(PrintPreviewUI::ParseDataPath(path, &preview_ui_id, &page_index));

  scoped_refptr<base::RefCountedMemory> data;
  PrintPreviewDataService::GetInstance()->GetDataEntry(preview_ui_id,
                                                       page_index, &data);
  if (data.get()) {
    callback.Run(data.get());
    return;
  }
  // Invalid request.
  auto empty_bytes = base::MakeRefCounted<base::RefCountedBytes>();
  callback.Run(empty_bytes.get());
}

void AddPrintPreviewStrings(content::WebUIDataSource* source) {
  static constexpr LocalizedString kLocalizedStrings[] = {
    {"accept", IDS_PRINT_PREVIEW_ACCEPT_INVITE},
    {"acceptForGroup", IDS_PRINT_PREVIEW_ACCEPT_GROUP_INVITE},
    {"accountSelectTitle", IDS_PRINT_PREVIEW_ACCOUNT_SELECT_TITLE},
    {"addAccountTitle", IDS_PRINT_PREVIEW_ADD_ACCOUNT_TITLE},
    {"advancedSettingsDialogConfirm",
     IDS_PRINT_PREVIEW_ADVANCED_SETTINGS_DIALOG_CONFIRM},
    {"advancedSettingsDialogTitle",
     IDS_PRINT_PREVIEW_ADVANCED_SETTINGS_DIALOG_TITLE},
    {"advancedSettingsSearchBoxPlaceholder",
     IDS_PRINT_PREVIEW_ADVANCED_SETTINGS_SEARCH_BOX_PLACEHOLDER},
    {"bottom", IDS_PRINT_PREVIEW_BOTTOM_MARGIN_LABEL},
    {"cancel", IDS_CANCEL},
    {"cloudPrintPromotion", IDS_PRINT_PREVIEW_CLOUD_PRINT_PROMOTION},
    {"copiesInstruction", IDS_PRINT_PREVIEW_COPIES_INSTRUCTION},
    {"copiesLabel", IDS_PRINT_PREVIEW_COPIES_LABEL},
    {"couldNotPrint", IDS_PRINT_PREVIEW_COULD_NOT_PRINT},
    {"customMargins", IDS_PRINT_PREVIEW_CUSTOM_MARGINS},
    {"defaultMargins", IDS_PRINT_PREVIEW_DEFAULT_MARGINS},
    {"destinationLabel", IDS_PRINT_PREVIEW_DESTINATION_LABEL},
    {"destinationSearchTitle", IDS_PRINT_PREVIEW_DESTINATION_SEARCH_TITLE},
    {"dpiItemLabel", IDS_PRINT_PREVIEW_DPI_ITEM_LABEL},
    {"dpiLabel", IDS_PRINT_PREVIEW_DPI_LABEL},
    {"examplePageRangeText", IDS_PRINT_PREVIEW_EXAMPLE_PAGE_RANGE_TEXT},
    {"extensionDestinationIconTooltip",
     IDS_PRINT_PREVIEW_EXTENSION_DESTINATION_ICON_TOOLTIP},
    {"goBackButton", IDS_PRINT_PREVIEW_BUTTON_GO_BACK},
    {"groupPrinterSharingInviteText", IDS_PRINT_PREVIEW_GROUP_INVITE_TEXT},
    {"invalidPrinterSettings", IDS_PRINT_PREVIEW_INVALID_PRINTER_SETTINGS},
    {"layoutLabel", IDS_PRINT_PREVIEW_LAYOUT_LABEL},
    {"learnMore", IDS_LEARN_MORE},
    {"left", IDS_PRINT_PREVIEW_LEFT_MARGIN_LABEL},
    {"loading", IDS_PRINT_PREVIEW_LOADING},
    {"manage", IDS_PRINT_PREVIEW_MANAGE},
    {"managedSettings", IDS_PRINT_PREVIEW_MANAGED_SETTINGS_TEXT},
    {"marginsLabel", IDS_PRINT_PREVIEW_MARGINS_LABEL},
    {"mediaSizeLabel", IDS_PRINT_PREVIEW_MEDIA_SIZE_LABEL},
    {"minimumMargins", IDS_PRINT_PREVIEW_MINIMUM_MARGINS},
    {"moreOptionsLabel", IDS_MORE_OPTIONS_LABEL},
    {"newShowAdvancedOptions", IDS_PRINT_PREVIEW_NEW_SHOW_ADVANCED_OPTIONS},
    {"noAdvancedSettingsMatchSearchHint",
     IDS_PRINT_PREVIEW_NO_ADVANCED_SETTINGS_MATCH_SEARCH_HINT},
    {"noDestinationsMessage", IDS_PRINT_PREVIEW_NO_DESTINATIONS_MESSAGE},
    {"noLongerSupported", IDS_PRINT_PREVIEW_NO_LONGER_SUPPORTED},
    {"noLongerSupportedFragment",
     IDS_PRINT_PREVIEW_NO_LONGER_SUPPORTED_FRAGMENT},
    {"noMargins", IDS_PRINT_PREVIEW_NO_MARGINS},
    {"noPlugin", IDS_PRINT_PREVIEW_NO_PLUGIN},
    {"nonIsotropicDpiItemLabel",
     IDS_PRINT_PREVIEW_NON_ISOTROPIC_DPI_ITEM_LABEL},
    {"offline", IDS_PRINT_PREVIEW_OFFLINE},
    {"offlineForMonth", IDS_PRINT_PREVIEW_OFFLINE_FOR_MONTH},
    {"offlineForWeek", IDS_PRINT_PREVIEW_OFFLINE_FOR_WEEK},
    {"offlineForYear", IDS_PRINT_PREVIEW_OFFLINE_FOR_YEAR},
    {"optionAllPages", IDS_PRINT_PREVIEW_OPTION_ALL_PAGES},
    {"optionBackgroundColorsAndImages",
     IDS_PRINT_PREVIEW_OPTION_BACKGROUND_COLORS_AND_IMAGES},
    {"optionBw", IDS_PRINT_PREVIEW_OPTION_BW},
    {"optionCollate", IDS_PRINT_PREVIEW_OPTION_COLLATE},
    {"optionColor", IDS_PRINT_PREVIEW_OPTION_COLOR},
    {"optionCustomPages", IDS_PRINT_PREVIEW_OPTION_CUSTOM_PAGES},
    {"optionCustomScaling", IDS_PRINT_PREVIEW_OPTION_CUSTOM_SCALING},
    {"optionDefaultScaling", IDS_PRINT_PREVIEW_OPTION_DEFAULT_SCALING},
    {"optionFitToPage", IDS_PRINT_PREVIEW_OPTION_FIT_TO_PAGE},
    {"optionFitToPaper", IDS_PRINT_PREVIEW_OPTION_FIT_TO_PAPER},
    {"optionHeaderFooter", IDS_PRINT_PREVIEW_OPTION_HEADER_FOOTER},
    {"optionLandscape", IDS_PRINT_PREVIEW_OPTION_LANDSCAPE},
    {"optionLongEdge", IDS_PRINT_PREVIEW_OPTION_LONG_EDGE},
    {"optionPortrait", IDS_PRINT_PREVIEW_OPTION_PORTRAIT},
    {"optionRasterize", IDS_PRINT_PREVIEW_OPTION_RASTERIZE},
    {"optionSelectionOnly", IDS_PRINT_PREVIEW_OPTION_SELECTION_ONLY},
    {"optionShortEdge", IDS_PRINT_PREVIEW_OPTION_SHORT_EDGE},
    {"optionTwoSided", IDS_PRINT_PREVIEW_OPTION_TWO_SIDED},
    {"optionsLabel", IDS_PRINT_PREVIEW_OPTIONS_LABEL},
    {"pageRangeLimitInstructionWithValue",
     IDS_PRINT_PREVIEW_PAGE_RANGE_LIMIT_INSTRUCTION_WITH_VALUE},
    {"pageRangeSyntaxInstruction",
     IDS_PRINT_PREVIEW_PAGE_RANGE_SYNTAX_INSTRUCTION},
    {"pagesLabel", IDS_PRINT_PREVIEW_PAGES_LABEL},
    {"pagesPerSheetLabel", IDS_PRINT_PREVIEW_PAGES_PER_SHEET_LABEL},
    {"previewFailed", IDS_PRINT_PREVIEW_FAILED},
    {"printOnBothSidesLabel", IDS_PRINT_PREVIEW_PRINT_ON_BOTH_SIDES_LABEL},
    {"printButton", IDS_PRINT_PREVIEW_PRINT_BUTTON},
    {"printDestinationsTitle", IDS_PRINT_PREVIEW_PRINT_DESTINATIONS_TITLE},
    {"printPagesLabel", IDS_PRINT_PREVIEW_PRINT_PAGES_LABEL},
    {"printPreviewPageLabelPlural", IDS_PRINT_PREVIEW_PAGE_LABEL_PLURAL},
    {"printPreviewPageLabelSingular", IDS_PRINT_PREVIEW_PAGE_LABEL_SINGULAR},
    {"printPreviewNewSummaryFormatShort",
     IDS_PRINT_PREVIEW_NEW_SUMMARY_FORMAT_SHORT},
    {"printPreviewSheetsLabelPlural", IDS_PRINT_PREVIEW_SHEETS_LABEL_PLURAL},
    {"printPreviewSheetsLabelSingular",
     IDS_PRINT_PREVIEW_SHEETS_LABEL_SINGULAR},
    {"printPreviewSummaryFormatShort", IDS_PRINT_PREVIEW_SUMMARY_FORMAT_SHORT},
    {"printToGoogleDrive", IDS_PRINT_PREVIEW_PRINT_TO_GOOGLE_DRIVE},
    {"printToPDF", IDS_PRINT_PREVIEW_PRINT_TO_PDF},
    {"printerSharingInviteText", IDS_PRINT_PREVIEW_INVITE_TEXT},
    {"printing", IDS_PRINT_PREVIEW_PRINTING},
    {"recentDestinationsTitle", IDS_PRINT_PREVIEW_RECENT_DESTINATIONS_TITLE},
    {"registerPrinterInformationMessage",
     IDS_CLOUD_PRINT_REGISTER_PRINTER_INFORMATION},
    {"reject", IDS_PRINT_PREVIEW_REJECT_INVITE},
    {"resolveExtensionUSBDialogTitle",
     IDS_PRINT_PREVIEW_RESOLVE_EXTENSION_USB_DIALOG_TITLE},
    {"resolveExtensionUSBErrorMessage",
     IDS_PRINT_PREVIEW_RESOLVE_EXTENSION_USB_ERROR_MESSAGE},
    {"resolveExtensionUSBPermissionMessage",
     IDS_PRINT_PREVIEW_RESOLVE_EXTENSION_USB_PERMISSION_MESSAGE},
    {"right", IDS_PRINT_PREVIEW_RIGHT_MARGIN_LABEL},
    {"saveButton", IDS_PRINT_PREVIEW_SAVE_BUTTON},
    {"saving", IDS_PRINT_PREVIEW_SAVING},
    {"scalingInstruction", IDS_PRINT_PREVIEW_SCALING_INSTRUCTION},
    {"scalingLabel", IDS_PRINT_PREVIEW_SCALING_LABEL},
    {"searchBoxPlaceholder", IDS_PRINT_PREVIEW_SEARCH_BOX_PLACEHOLDER},
    {"selectButton", IDS_PRINT_PREVIEW_BUTTON_SELECT},
    {"seeMore", IDS_PRINT_PREVIEW_SEE_MORE},
    {"seeMoreDestinationsLabel", IDS_PRINT_PREVIEW_SEE_MORE_DESTINATIONS_LABEL},
    {"title", IDS_PRINT_PREVIEW_TITLE},
    {"top", IDS_PRINT_PREVIEW_TOP_MARGIN_LABEL},
    {"unsupportedCloudPrinter", IDS_PRINT_PREVIEW_UNSUPPORTED_CLOUD_PRINTER},
#if defined(OS_CHROMEOS)
    {"configuringFailedText", IDS_PRINT_CONFIGURING_FAILED_TEXT},
    {"configuringInProgressText", IDS_PRINT_CONFIGURING_IN_PROGRESS_TEXT},
    {"optionPin", IDS_PRINT_PREVIEW_OPTION_PIN},
    {"pinErrorMessage", IDS_PRINT_PREVIEW_PIN_ERROR_MESSAGE},
    {"pinPlaceholder", IDS_PRINT_PREVIEW_PIN_PLACEHOLDER},
#endif
#if defined(OS_MACOSX)
    {"openPdfInPreviewOption", IDS_PRINT_PREVIEW_OPEN_PDF_IN_PREVIEW_APP},
    {"openingPDFInPreview", IDS_PRINT_PREVIEW_OPENING_PDF_IN_PREVIEW_APP},
#endif
  };
  AddLocalizedStringsBulk(source, kLocalizedStrings,
                          base::size(kLocalizedStrings));

  source->AddString("gcpCertificateErrorLearnMoreURL",
                    chrome::kCloudPrintCertificateErrorLearnMoreURL);

#if !defined(OS_CHROMEOS)
  const base::string16 shortcut_text(base::UTF8ToUTF16(kBasicPrintShortcut));
  source->AddString("systemDialogOption",
                    l10n_util::GetStringFUTF16(
                        IDS_PRINT_PREVIEW_SYSTEM_DIALOG_OPTION, shortcut_text));
#endif
}

void AddPrintPreviewFlags(content::WebUIDataSource* source, Profile* profile) {
#if defined(OS_CHROMEOS)
  source->AddBoolean("useSystemDefaultPrinter", false);
#else
  bool system_default_printer = profile->GetPrefs()->GetBoolean(
      prefs::kPrintPreviewUseSystemDefaultPrinter);
  source->AddBoolean("useSystemDefaultPrinter", system_default_printer);
#endif

  bool enterprise_managed = false;
#if defined(OS_CHROMEOS)
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  enterprise_managed = connector->IsEnterpriseManaged();
#elif defined(OS_WIN)
  enterprise_managed = base::IsMachineExternallyManaged();
#endif
  source->AddBoolean("isEnterpriseManaged", enterprise_managed);

  bool cloud_printer_handler_enabled =
      base::FeatureList::IsEnabled(features::kCloudPrinterHandler);
  source->AddBoolean("cloudPrinterHandlerEnabled",
                     cloud_printer_handler_enabled);
}

void SetupPrintPreviewPlugin(content::WebUIDataSource* source) {
  static constexpr struct {
    const char* path;
    int id;
  } kPdfResources[] = {
    {"pdf/browser_api.js", IDR_PDF_BROWSER_API_JS},
    {"pdf/controller.js", IDR_PDF_CONTROLLER_JS},
    {"pdf/elements/icons.js", IDR_PDF_ICONS_JS},
    {"pdf/elements/shared-vars.js", IDR_PDF_SHARED_VARS_JS},
    {"pdf/elements/viewer-bookmark.js", IDR_PDF_VIEWER_BOOKMARK_JS},
    {"pdf/elements/viewer-error-screen.js", IDR_PDF_VIEWER_ERROR_SCREEN_JS},
#if defined(OS_CHROMEOS)
    {"pdf/elements/viewer-ink-host.js", IDR_PDF_VIEWER_INK_HOST_JS},
#endif
    {"pdf/elements/viewer-page-indicator.js", IDR_PDF_VIEWER_PAGE_INDICATOR_JS},
    {"pdf/elements/viewer-page-selector.js", IDR_PDF_VIEWER_PAGE_SELECTOR_JS},
    {"pdf/elements/viewer-password-screen.js",
     IDR_PDF_VIEWER_PASSWORD_SCREEN_JS},
    {"pdf/elements/viewer-pdf-toolbar.js", IDR_PDF_VIEWER_PDF_TOOLBAR_JS},
#if defined(OS_CHROMEOS)
    {"pdf/elements/viewer-form-warning.js", IDR_PDF_VIEWER_FORM_WARNING_JS},
    {"pdf/elements/viewer-pen-options.js", IDR_PDF_VIEWER_PEN_OPTIONS_JS},
#endif
    {"pdf/elements/viewer-toolbar-dropdown.js",
     IDR_PDF_VIEWER_TOOLBAR_DROPDOWN_JS},
    {"pdf/elements/viewer-zoom-button.js", IDR_PDF_VIEWER_ZOOM_BUTTON_JS},
    {"pdf/elements/viewer-zoom-toolbar.js", IDR_PDF_VIEWER_ZOOM_SELECTOR_JS},
    {"pdf/gesture_detector.js", IDR_PDF_GESTURE_DETECTOR_JS},
    {"pdf/index.css", IDR_PDF_INDEX_CSS},
    {"pdf/index.html", IDR_PDF_INDEX_HTML},
    {"pdf/main.js", IDR_PDF_MAIN_JS},
    {"pdf/metrics.js", IDR_PDF_METRICS_JS},
    {"pdf/navigator.js", IDR_PDF_NAVIGATOR_JS},
    {"pdf/open_pdf_params_parser.js", IDR_PDF_OPEN_PDF_PARAMS_PARSER_JS},
    {"pdf/pdf_fitting_type.js", IDR_PDF_PDF_FITTING_TYPE_JS},
    {"pdf/pdf_scripting_api.js", IDR_PDF_PDF_SCRIPTING_API_JS},
    {"pdf/pdf_viewer.js", IDR_PDF_PDF_VIEWER_JS},
    {"pdf/toolbar_manager.js", IDR_PDF_TOOLBAR_MANAGER_JS},
    {"pdf/viewport.js", IDR_PDF_VIEWPORT_JS},
    {"pdf/viewport_scroller.js", IDR_PDF_VIEWPORT_SCROLLER_JS},
    {"pdf/zoom_manager.js", IDR_PDF_ZOOM_MANAGER_JS},
  };
  for (const auto& resource : kPdfResources) {
    source->AddResourcePath(resource.path, resource.id);
  }

  source->SetRequestFilter(base::BindRepeating(&ShouldHandleRequestCallback),
                           base::BindRepeating(&HandleRequestCallback));
  source->OverrideContentSecurityPolicyChildSrc("child-src 'self';");
  source->DisableDenyXFrameOptions();
  source->OverrideContentSecurityPolicyObjectSrc("object-src 'self';");
}

content::WebUIDataSource* CreatePrintPreviewUISource(Profile* profile) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIPrintHost);
  source->OverrideContentSecurityPolicyScriptSrc(
      "script-src chrome://resources chrome://test 'self';");
  AddPrintPreviewStrings(source);
  source->UseStringsJs();
  source->EnableReplaceI18nInJS();
#if BUILDFLAG(OPTIMIZE_WEBUI)
  source->AddResourcePath("print_preview.js",
                          IDR_PRINT_PREVIEW_PRINT_PREVIEW_ROLLUP_JS);
  source->SetDefaultResource(IDR_PRINT_PREVIEW_VULCANIZED_HTML);
#else
  // Add all Print Preview resources.
  for (size_t i = 0; i < kPrintPreviewResourcesSize; ++i) {
    std::string path = kPrintPreviewResources[i].name;
    if (path.rfind(kGeneratedPath, 0) == 0) {
      path = path.substr(sizeof(kGeneratedPath) - 1);
    }

    source->AddResourcePath(path, kPrintPreviewResources[i].value);
  }
  source->SetDefaultResource(IDR_PRINT_PREVIEW_HTML);
#endif
  SetupPrintPreviewPlugin(source);
  AddPrintPreviewFlags(source, profile);
  source->AddResourcePath("test_loader.js", IDR_WEBUI_JS_TEST_LOADER);
  source->AddResourcePath("test_loader.html", IDR_WEBUI_HTML_TEST_LOADER);
  return source;
}

PrintPreviewHandler* CreatePrintPreviewHandlers(content::WebUI* web_ui) {
  auto handler = std::make_unique<PrintPreviewHandler>();
  PrintPreviewHandler* handler_ptr = handler.get();
  web_ui->AddMessageHandler(std::move(handler));
  web_ui->AddMessageHandler(std::make_unique<MetricsHandler>());
  return handler_ptr;
}

}  // namespace

PrintPreviewUI::PrintPreviewUI(content::WebUI* web_ui,
                               std::unique_ptr<PrintPreviewHandler> handler)
    : ConstrainedWebDialogUI(web_ui),
      initial_preview_start_time_(base::TimeTicks::Now()),
      handler_(handler.get()) {
  web_ui->AddMessageHandler(std::move(handler));
}

PrintPreviewUI::PrintPreviewUI(content::WebUI* web_ui)
    : ConstrainedWebDialogUI(web_ui),
      initial_preview_start_time_(base::TimeTicks::Now()),
      handler_(CreatePrintPreviewHandlers(web_ui)) {
  // Set up the chrome://print/ data source.
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* source = CreatePrintPreviewUISource(profile);
#if !BUILDFLAG(OPTIMIZE_WEBUI)
  // For the Polymer 3 demo page.
  ManagedUIHandler::Initialize(web_ui, source);
#endif
  content::WebUIDataSource::Add(profile, source);

  // Set up the chrome://theme/ source.
  content::URLDataSource::Add(profile, std::make_unique<ThemeSource>(profile));
}

PrintPreviewUI::~PrintPreviewUI() {
  ClearPreviewUIId();
}

void PrintPreviewUI::ClearPreviewUIId() {
  if (!id_)
    return;

  PrintPreviewDataService::GetInstance()->RemoveEntry(*id_);
  g_print_preview_request_id_map.Get().Erase(*id_);
  g_print_preview_ui_id_map.Get().Remove(*id_);
  id_.reset();
}

void PrintPreviewUI::GetPrintPreviewDataForIndex(
    int index,
    scoped_refptr<base::RefCountedMemory>* data) const {
  PrintPreviewDataService::GetInstance()->GetDataEntry(*id_, index, data);
}

void PrintPreviewUI::SetPrintPreviewDataForIndex(
    int index,
    scoped_refptr<base::RefCountedMemory> data) {
  PrintPreviewDataService::GetInstance()->SetDataEntry(*id_, index,
                                                       std::move(data));
}

// static
bool PrintPreviewUI::ParseDataPath(const std::string& path,
                                   int* ui_id,
                                   int* page_index) {
  std::string file_path = path.substr(0, path.find_first_of('?'));
  if (!base::EndsWith(file_path, "/print.pdf", base::CompareCase::SENSITIVE))
    return false;

  std::vector<std::string> url_substr =
      base::SplitString(path, "/", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (url_substr.size() != 3)
    return false;

  int preview_ui_id = -1;
  if (!base::StringToInt(url_substr[0], &preview_ui_id) || preview_ui_id < 0)
    return false;

  int preview_page_index = 0;
  if (!base::StringToInt(url_substr[1], &preview_page_index))
    return false;

  if (ui_id)
    *ui_id = preview_ui_id;
  if (page_index)
    *page_index = preview_page_index;
  return true;
}

void PrintPreviewUI::ClearAllPreviewData() {
  PrintPreviewDataService::GetInstance()->RemoveEntry(*id_);
}

void PrintPreviewUI::SetInitiatorTitle(
    const base::string16& job_title) {
  initiator_title_ = job_title;
}

bool PrintPreviewUI::ShouldCompositeDocumentUsingIndividualPages() const {
  return printing::IsOopifEnabled() && source_is_modifiable_;
}

bool PrintPreviewUI::LastPageComposited(int page_number) const {
  if (pages_to_render_.empty())
    return false;

  return page_number == pages_to_render_.back();
}

int PrintPreviewUI::GetPageToNupConvertIndex(int page_number) const {
  for (size_t index = 0; index < pages_to_render_.size(); ++index) {
    if (page_number == pages_to_render_[index])
      return index;
  }
  return -1;
}

std::vector<base::ReadOnlySharedMemoryRegion>
PrintPreviewUI::TakePagesForNupConvert() {
  return std::move(pages_for_nup_convert_);
}

void PrintPreviewUI::AddPdfPageForNupConversion(
    base::ReadOnlySharedMemoryRegion pdf_page) {
  pages_for_nup_convert_.push_back(std::move(pdf_page));
}

// static
void PrintPreviewUI::SetInitialParams(
    content::WebContents* print_preview_dialog,
    const PrintHostMsg_RequestPrintPreview_Params& params) {
  if (!print_preview_dialog || !print_preview_dialog->GetWebUI())
    return;
  PrintPreviewUI* print_preview_ui = static_cast<PrintPreviewUI*>(
      print_preview_dialog->GetWebUI()->GetController());
  print_preview_ui->source_is_arc_ = params.is_from_arc;
  print_preview_ui->source_is_modifiable_ = params.is_modifiable;
  print_preview_ui->source_is_pdf_ = params.is_pdf;
  print_preview_ui->source_has_selection_ = params.has_selection;
  print_preview_ui->print_selection_only_ = params.selection_only;
}

// static
bool PrintPreviewUI::ShouldCancelRequest(const PrintHostMsg_PreviewIds& ids) {
  int current_id = -1;
  if (!g_print_preview_request_id_map.Get().Get(ids.ui_id, &current_id))
    return true;
  return ids.request_id != current_id;
}

base::Optional<int32_t> PrintPreviewUI::GetIDForPrintPreviewUI() const {
  return id_;
}

void PrintPreviewUI::OnPrintPreviewDialogClosed() {
  WebContents* preview_dialog = web_ui()->GetWebContents();
  BackgroundPrintingManager* background_printing_manager =
      g_browser_process->background_printing_manager();
  if (background_printing_manager->HasPrintPreviewDialog(preview_dialog))
    return;
  OnClosePrintPreviewDialog();
}

void PrintPreviewUI::OnInitiatorClosed() {
  // Should only get here if the initiator was still tracked by the Print
  // Preview Dialog Controller, so the print job has not yet been sent.
  WebContents* preview_dialog = web_ui()->GetWebContents();
  BackgroundPrintingManager* background_printing_manager =
      g_browser_process->background_printing_manager();
  if (background_printing_manager->HasPrintPreviewDialog(preview_dialog)) {
    // Dialog is hidden but is still generating the preview. Cancel the print
    // request as it can't be completed.
    background_printing_manager->OnPrintRequestCancelled(preview_dialog);
    handler_->OnPrintRequestCancelled();
  } else {
    // Initiator was closed while print preview dialog was still open.
    OnClosePrintPreviewDialog();
  }
}

void PrintPreviewUI::OnPrintPreviewCancelled(int request_id) {
  handler_->OnPrintPreviewCancelled(request_id);
}

void PrintPreviewUI::OnPrintPreviewRequest(int request_id) {
  if (!initial_preview_start_time_.is_null()) {
    UMA_HISTOGRAM_TIMES("PrintPreview.InitializationTime",
                        base::TimeTicks::Now() - initial_preview_start_time_);
  }
  g_print_preview_request_id_map.Get().Set(*id_, request_id);
}

void PrintPreviewUI::OnDidStartPreview(
    const PrintHostMsg_DidStartPreview_Params& params,
    int request_id) {
  DCHECK_GT(params.page_count, 0);
  DCHECK(!params.pages_to_render.empty());

  pages_to_render_ = params.pages_to_render;
  pages_to_render_index_ = 0;
  pages_per_sheet_ = params.pages_per_sheet;
  page_size_ = params.page_size;
  ClearAllPreviewData();

  if (g_test_delegate)
    g_test_delegate->DidGetPreviewPageCount(params.page_count);
  handler_->SendPageCountReady(params.page_count, params.fit_to_page_scaling,
                               request_id);
}

void PrintPreviewUI::OnDidGetDefaultPageLayout(
    const PageSizeMargins& page_layout,
    const gfx::Rect& printable_area,
    bool has_custom_page_size_style,
    int request_id) {
  if (page_layout.margin_top < 0 || page_layout.margin_left < 0 ||
      page_layout.margin_bottom < 0 || page_layout.margin_right < 0 ||
      page_layout.content_width < 0 || page_layout.content_height < 0 ||
      printable_area.width() <= 0 || printable_area.height() <= 0) {
    NOTREACHED();
    return;
  }
  // Save printable_area information for N-up conversion.
  printable_area_ = printable_area;

  base::DictionaryValue layout;
  layout.SetDouble(kSettingMarginTop, page_layout.margin_top);
  layout.SetDouble(kSettingMarginLeft, page_layout.margin_left);
  layout.SetDouble(kSettingMarginBottom, page_layout.margin_bottom);
  layout.SetDouble(kSettingMarginRight, page_layout.margin_right);
  layout.SetDouble(kSettingContentWidth, page_layout.content_width);
  layout.SetDouble(kSettingContentHeight, page_layout.content_height);
  layout.SetInteger(kSettingPrintableAreaX, printable_area.x());
  layout.SetInteger(kSettingPrintableAreaY, printable_area.y());
  layout.SetInteger(kSettingPrintableAreaWidth, printable_area.width());
  layout.SetInteger(kSettingPrintableAreaHeight, printable_area.height());
  handler_->SendPageLayoutReady(layout, has_custom_page_size_style, request_id);
}

bool PrintPreviewUI::OnPendingPreviewPage(int page_number) {
  if (pages_to_render_index_ >= pages_to_render_.size())
    return false;

  bool matched = page_number == pages_to_render_[pages_to_render_index_];
  ++pages_to_render_index_;
  return matched;
}

void PrintPreviewUI::OnDidPreviewPage(
    int page_number,
    scoped_refptr<base::RefCountedMemory> data,
    int preview_request_id) {
  DCHECK_GE(page_number, 0);

  SetPrintPreviewDataForIndex(page_number, std::move(data));

  if (g_test_delegate)
    g_test_delegate->DidRenderPreviewPage(web_ui()->GetWebContents());
  handler_->SendPagePreviewReady(page_number, *id_, preview_request_id);
}

void PrintPreviewUI::OnPreviewDataIsAvailable(
    int expected_pages_count,
    scoped_refptr<base::RefCountedMemory> data,
    int preview_request_id) {
  VLOG(1) << "Print preview request finished with "
          << expected_pages_count << " pages";

  if (!initial_preview_start_time_.is_null()) {
    UMA_HISTOGRAM_TIMES("PrintPreview.InitialDisplayTime",
                        base::TimeTicks::Now() - initial_preview_start_time_);
    UMA_HISTOGRAM_COUNTS_1M("PrintPreview.PageCount.Initial",
                            expected_pages_count);
    UMA_HISTOGRAM_COUNTS_1M(
        "PrintPreview.RegeneratePreviewRequest.BeforeFirstData",
        handler_->regenerate_preview_request_count());
    initial_preview_start_time_ = base::TimeTicks();
  }

  SetPrintPreviewDataForIndex(COMPLETE_PREVIEW_DOCUMENT_INDEX, std::move(data));

  handler_->OnPrintPreviewReady(*id_, preview_request_id);
}

void PrintPreviewUI::OnCancelPendingPreviewRequest() {
  g_print_preview_request_id_map.Get().Set(*id_, -1);
}

void PrintPreviewUI::OnPrintPreviewFailed(int request_id) {
  handler_->OnPrintPreviewFailed(request_id);
}

void PrintPreviewUI::OnInvalidPrinterSettings(int request_id) {
  handler_->OnInvalidPrinterSettings(request_id);
}

void PrintPreviewUI::OnHidePreviewDialog() {
  WebContents* preview_dialog = web_ui()->GetWebContents();
  BackgroundPrintingManager* background_printing_manager =
      g_browser_process->background_printing_manager();
  if (background_printing_manager->HasPrintPreviewDialog(preview_dialog))
    return;

  ConstrainedWebDialogDelegate* delegate = GetConstrainedDelegate();
  if (!delegate)
    return;
  std::unique_ptr<content::WebContents> preview_contents =
      delegate->ReleaseWebContents();
  DCHECK_EQ(preview_dialog, preview_contents.get());
  background_printing_manager->OwnPrintPreviewDialog(
      std::move(preview_contents));
  OnClosePrintPreviewDialog();
}

void PrintPreviewUI::OnClosePrintPreviewDialog() {
  if (dialog_closed_)
    return;
  dialog_closed_ = true;
  ConstrainedWebDialogDelegate* delegate = GetConstrainedDelegate();
  if (!delegate)
    return;
  delegate->GetWebDialogDelegate()->OnDialogClosed(std::string());
  delegate->OnDialogCloseFromWebUI();
}

void PrintPreviewUI::OnSetOptionsFromDocument(
    const PrintHostMsg_SetOptionsFromDocument_Params& params,
    int request_id) {
  handler_->SendPrintPresetOptions(params.is_scaling_disabled, params.copies,
                                   params.duplex, request_id);
}

// static
void PrintPreviewUI::SetDelegateForTesting(TestDelegate* delegate) {
  g_test_delegate = delegate;
}

void PrintPreviewUI::SetSelectedFileForTesting(const base::FilePath& path) {
  handler_->FileSelectedForTesting(path, 0, nullptr);
}

void PrintPreviewUI::SetPdfSavedClosureForTesting(base::OnceClosure closure) {
  handler_->SetPdfSavedClosureForTesting(std::move(closure));
}

void PrintPreviewUI::SendEnableManipulateSettingsForTest() {
  handler_->SendEnableManipulateSettingsForTest();
}

void PrintPreviewUI::SendManipulateSettingsForTest(
    const base::DictionaryValue& settings) {
  handler_->SendManipulateSettingsForTest(settings);
}

void PrintPreviewUI::SetPrintPreviewDataForIndexForTest(
    int index,
    scoped_refptr<base::RefCountedMemory> data) {
  SetPrintPreviewDataForIndex(index, data);
}

void PrintPreviewUI::ClearAllPreviewDataForTest() {
  ClearAllPreviewData();
}

void PrintPreviewUI::SetPreviewUIId() {
  DCHECK(!id_);
  id_ = g_print_preview_ui_id_map.Get().Add(this);
  g_print_preview_request_id_map.Get().Set(*id_, -1);
}

}  // namespace printing
