// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/print_preview_ui.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/id_map.h"
#include "base/feature_list.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/printing/background_printing_manager.h"
#include "chrome/browser/printing/print_preview_data_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_pages.h"
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
#include "components/printing/common/print_messages.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/content_features.h"
#include "extensions/common/constants.h"
#include "printing/page_size_margins.h"
#include "printing/print_job_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/web_dialogs/web_dialog_delegate.h"
#include "ui/web_dialogs/web_dialog_ui.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#elif defined(OS_WIN)
#include "base/win/win_util.h"
#endif

using content::WebContents;
using printing::PageSizeMargins;

namespace {

#if defined(OS_MACOSX)
// U+0028 U+21E7 U+2318 U+0050 U+0029 in UTF8
const char kBasicPrintShortcut[] = "\x28\xE2\x8c\xA5\xE2\x8C\x98\x50\x29";
#elif !defined(OS_CHROMEOS)
const char kBasicPrintShortcut[] = "(Ctrl+Shift+P)";
#endif

PrintPreviewUI::TestingDelegate* g_testing_delegate = nullptr;

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

// PrintPreviewUI serves data for chrome://print requests.
//
// The format for requesting PDF data is as follows:
// chrome://print/<PrintPreviewUIID>/<PageIndex>/print.pdf
//
// Parameters (< > required):
//    <PrintPreviewUIID> = PrintPreview UI ID
//    <PageIndex> = Page index is zero-based or
//                  |printing::COMPLETE_PREVIEW_DOCUMENT_INDEX| to represent
//                  a print ready PDF.
//
// Example:
//    chrome://print/123/10/print.pdf
//
// Requests to chrome://print with paths not ending in /print.pdf are used
// to return the markup or other resources for the print preview page itself.
bool HandleRequestCallback(
    const std::string& path,
    const content::WebUIDataSource::GotDataCallback& callback) {
  // ChromeWebUIDataSource handles most requests except for the print preview
  // data.
  std::string file_path = path.substr(0, path.find_first_of('?'));
  if (!base::EndsWith(file_path, "/print.pdf", base::CompareCase::SENSITIVE))
    return false;

  // Print Preview data.
  scoped_refptr<base::RefCountedMemory> data;
  std::vector<std::string> url_substr = base::SplitString(
      path, "/", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  int preview_ui_id = -1;
  int page_index = 0;
  if (url_substr.size() == 3 &&
      base::StringToInt(url_substr[0], &preview_ui_id),
      base::StringToInt(url_substr[1], &page_index) &&
      preview_ui_id >= 0) {
    PrintPreviewDataService::GetInstance()->GetDataEntry(
        preview_ui_id, page_index, &data);
  }
  if (data.get()) {
    callback.Run(data.get());
    return true;
  }
  // Invalid request.
  auto empty_bytes = base::MakeRefCounted<base::RefCountedBytes>();
  callback.Run(empty_bytes.get());
  return true;
}

void AddPrintPreviewStrings(content::WebUIDataSource* source) {
  source->AddLocalizedString("title", IDS_PRINT_PREVIEW_TITLE);
  source->AddLocalizedString("learnMore", IDS_LEARN_MORE);
  source->AddLocalizedString("loading", IDS_PRINT_PREVIEW_LOADING);
  source->AddLocalizedString("noPlugin", IDS_PRINT_PREVIEW_NO_PLUGIN);
  source->AddLocalizedString("launchNativeDialog",
                             IDS_PRINT_PREVIEW_NATIVE_DIALOG);
  source->AddLocalizedString("previewFailed", IDS_PRINT_PREVIEW_FAILED);
  source->AddLocalizedString("invalidPrinterSettings",
                             IDS_PRINT_INVALID_PRINTER_SETTINGS);
  source->AddLocalizedString("unsupportedCloudPrinter",
                             IDS_PRINT_PREVIEW_UNSUPPORTED_CLOUD_PRINTER);
  source->AddLocalizedString("printButton", IDS_PRINT_PREVIEW_PRINT_BUTTON);
  source->AddLocalizedString("saveButton", IDS_PRINT_PREVIEW_SAVE_BUTTON);
  source->AddLocalizedString("printing", IDS_PRINT_PREVIEW_PRINTING);
  source->AddLocalizedString("saving", IDS_PRINT_PREVIEW_SAVING);
  source->AddLocalizedString("destinationLabel",
                             IDS_PRINT_PREVIEW_DESTINATION_LABEL);
  source->AddLocalizedString("copiesLabel", IDS_PRINT_PREVIEW_COPIES_LABEL);
  source->AddLocalizedString("scalingLabel", IDS_PRINT_PREVIEW_SCALING_LABEL);
  source->AddLocalizedString("pagesPerSheetLabel",
                             IDS_PRINT_PREVIEW_PAGES_PER_SHEET_LABEL);

  source->AddLocalizedString("examplePageRangeText",
                             IDS_PRINT_PREVIEW_EXAMPLE_PAGE_RANGE_TEXT);
  source->AddLocalizedString("layoutLabel", IDS_PRINT_PREVIEW_LAYOUT_LABEL);
  source->AddLocalizedString("optionAllPages",
                             IDS_PRINT_PREVIEW_OPTION_ALL_PAGES);
  source->AddLocalizedString("optionCustomPages",
                             IDS_PRINT_PREVIEW_OPTION_CUSTOM_PAGES);
  source->AddLocalizedString("optionBw", IDS_PRINT_PREVIEW_OPTION_BW);
  source->AddLocalizedString("optionCollate", IDS_PRINT_PREVIEW_OPTION_COLLATE);
  source->AddLocalizedString("optionColor", IDS_PRINT_PREVIEW_OPTION_COLOR);
  source->AddLocalizedString("optionLandscape",
                             IDS_PRINT_PREVIEW_OPTION_LANDSCAPE);
  source->AddLocalizedString("optionPortrait",
                             IDS_PRINT_PREVIEW_OPTION_PORTRAIT);
  source->AddLocalizedString("optionTwoSided",
                             IDS_PRINT_PREVIEW_OPTION_TWO_SIDED);
  source->AddLocalizedString("pagesLabel", IDS_PRINT_PREVIEW_PAGES_LABEL);
  source->AddLocalizedString("pageRangeTextBox",
                             IDS_PRINT_PREVIEW_PAGE_RANGE_TEXT);
  source->AddLocalizedString("pageRangeRadio",
                             IDS_PRINT_PREVIEW_PAGE_RANGE_RADIO);
  source->AddLocalizedString("printToPDF", IDS_PRINT_PREVIEW_PRINT_TO_PDF);
  source->AddLocalizedString("printPreviewSummaryFormatShort",
                             IDS_PRINT_PREVIEW_SUMMARY_FORMAT_SHORT);
  source->AddLocalizedString("printPreviewSummaryFormatLong",
                             IDS_PRINT_PREVIEW_SUMMARY_FORMAT_LONG);
  source->AddLocalizedString("printPreviewSheetsLabelSingular",
                             IDS_PRINT_PREVIEW_SHEETS_LABEL_SINGULAR);
  source->AddLocalizedString("printPreviewSheetsLabelPlural",
                             IDS_PRINT_PREVIEW_SHEETS_LABEL_PLURAL);
  source->AddLocalizedString("printPreviewPageLabelSingular",
                             IDS_PRINT_PREVIEW_PAGE_LABEL_SINGULAR);
  source->AddLocalizedString("printPreviewPageLabelPlural",
                             IDS_PRINT_PREVIEW_PAGE_LABEL_PLURAL);
  source->AddLocalizedString("selectButton",
                             IDS_PRINT_PREVIEW_BUTTON_SELECT);
  source->AddLocalizedString("goBackButton",
                             IDS_PRINT_PREVIEW_BUTTON_GO_BACK);
  source->AddLocalizedString(
      "resolveExtensionUSBDialogTitle",
      IDS_PRINT_PREVIEW_RESOLVE_EXTENSION_USB_DIALOG_TITLE);
  source->AddLocalizedString(
      "resolveExtensionUSBPermissionMessage",
      IDS_PRINT_PREVIEW_RESOLVE_EXTENSION_USB_PERMISSION_MESSAGE);
  source->AddLocalizedString(
      "resolveExtensionUSBErrorMessage",
      IDS_PRINT_PREVIEW_RESOLVE_EXTENSION_USB_ERROR_MESSAGE);
  source->AddString(
      "printWithCloudPrintWait",
      l10n_util::GetStringFUTF16(
          IDS_PRINT_PREVIEW_PRINT_WITH_CLOUD_PRINT_WAIT,
          l10n_util::GetStringUTF16(IDS_GOOGLE_CLOUD_PRINT)));
  source->AddString(
      "noDestsPromoLearnMoreUrl",
      chrome::kCloudPrintNoDestinationsLearnMoreURL);
  source->AddString(
      "settingsPrintingPage",
      chrome::GetSettingsUrl(chrome::kPrintingSettingsSubPage).spec());
  source->AddString("gcpCertificateErrorLearnMoreURL",
                    chrome::kCloudPrintCertificateErrorLearnMoreURL);
  source->AddLocalizedString("pageRangeLimitInstruction",
                             IDS_PRINT_PREVIEW_PAGE_RANGE_LIMIT_INSTRUCTION);
  source->AddLocalizedString(
      "pageRangeLimitInstructionWithValue",
      IDS_PRINT_PREVIEW_PAGE_RANGE_LIMIT_INSTRUCTION_WITH_VALUE);
  source->AddLocalizedString("pageRangeSyntaxInstruction",
                             IDS_PRINT_PREVIEW_PAGE_RANGE_SYNTAX_INSTRUCTION);
  source->AddLocalizedString("copiesInstruction",
                             IDS_PRINT_PREVIEW_COPIES_INSTRUCTION);
  source->AddLocalizedString("scalingInstruction",
                             IDS_PRINT_PREVIEW_SCALING_INSTRUCTION);
  source->AddLocalizedString("printPagesLabel",
                             IDS_PRINT_PREVIEW_PRINT_PAGES_LABEL);
  source->AddLocalizedString("optionsLabel", IDS_PRINT_PREVIEW_OPTIONS_LABEL);
  source->AddLocalizedString("optionHeaderFooter",
                             IDS_PRINT_PREVIEW_OPTION_HEADER_FOOTER);
  source->AddLocalizedString("optionFitToPage",
                             IDS_PRINT_PREVIEW_OPTION_FIT_TO_PAGE);
  source->AddLocalizedString(
      "optionBackgroundColorsAndImages",
      IDS_PRINT_PREVIEW_OPTION_BACKGROUND_COLORS_AND_IMAGES);
  source->AddLocalizedString("optionSelectionOnly",
                             IDS_PRINT_PREVIEW_OPTION_SELECTION_ONLY);
  source->AddLocalizedString("optionRasterize",
                             IDS_PRINT_PREVIEW_OPTION_RASTERIZE);
  source->AddLocalizedString("marginsLabel", IDS_PRINT_PREVIEW_MARGINS_LABEL);
  source->AddLocalizedString("defaultMargins",
                             IDS_PRINT_PREVIEW_DEFAULT_MARGINS);
  source->AddLocalizedString("noMargins", IDS_PRINT_PREVIEW_NO_MARGINS);
  source->AddLocalizedString("customMargins", IDS_PRINT_PREVIEW_CUSTOM_MARGINS);
  source->AddLocalizedString("minimumMargins",
                             IDS_PRINT_PREVIEW_MINIMUM_MARGINS);
  source->AddLocalizedString("top", IDS_PRINT_PREVIEW_TOP_MARGIN_LABEL);
  source->AddLocalizedString("bottom", IDS_PRINT_PREVIEW_BOTTOM_MARGIN_LABEL);
  source->AddLocalizedString("left", IDS_PRINT_PREVIEW_LEFT_MARGIN_LABEL);
  source->AddLocalizedString("right", IDS_PRINT_PREVIEW_RIGHT_MARGIN_LABEL);
  source->AddLocalizedString("mediaSizeLabel",
                             IDS_PRINT_PREVIEW_MEDIA_SIZE_LABEL);
  source->AddLocalizedString("dpiLabel", IDS_PRINT_PREVIEW_DPI_LABEL);
  source->AddLocalizedString("dpiItemLabel", IDS_PRINT_PREVIEW_DPI_ITEM_LABEL);
  source->AddLocalizedString("nonIsotropicDpiItemLabel",
                             IDS_PRINT_PREVIEW_NON_ISOTROPIC_DPI_ITEM_LABEL);
  source->AddLocalizedString("destinationSearchTitle",
                             IDS_PRINT_PREVIEW_DESTINATION_SEARCH_TITLE);
  source->AddLocalizedString("accountSelectTitle",
                             IDS_PRINT_PREVIEW_ACCOUNT_SELECT_TITLE);
  source->AddLocalizedString("addAccountTitle",
                             IDS_PRINT_PREVIEW_ADD_ACCOUNT_TITLE);
  source->AddLocalizedString("cloudPrintPromotion",
                             IDS_PRINT_PREVIEW_CLOUD_PRINT_PROMOTION);
  source->AddLocalizedString("searchBoxPlaceholder",
                             IDS_PRINT_PREVIEW_SEARCH_BOX_PLACEHOLDER);
  source->AddLocalizedString("noDestinationsMessage",
                             IDS_PRINT_PREVIEW_NO_DESTINATIONS_MESSAGE);
  source->AddLocalizedString("showAllButtonText",
                             IDS_PRINT_PREVIEW_SHOW_ALL_BUTTON_TEXT);
  source->AddLocalizedString("destinationCount",
                             IDS_PRINT_PREVIEW_DESTINATION_COUNT);
  source->AddLocalizedString("recentDestinationsTitle",
                             IDS_PRINT_PREVIEW_RECENT_DESTINATIONS_TITLE);
  source->AddLocalizedString("printDestinationsTitle",
                             IDS_PRINT_PREVIEW_PRINT_DESTINATIONS_TITLE);
  source->AddLocalizedString("manage", IDS_PRINT_PREVIEW_MANAGE);
  source->AddLocalizedString("changeDestination",
                             IDS_PRINT_PREVIEW_CHANGE_DESTINATION);
  source->AddLocalizedString("offlineForYear",
                             IDS_PRINT_PREVIEW_OFFLINE_FOR_YEAR);
  source->AddLocalizedString("offlineForMonth",
                             IDS_PRINT_PREVIEW_OFFLINE_FOR_MONTH);
  source->AddLocalizedString("offlineForWeek",
                             IDS_PRINT_PREVIEW_OFFLINE_FOR_WEEK);
  source->AddLocalizedString("offline", IDS_PRINT_PREVIEW_OFFLINE);
  source->AddLocalizedString("noLongerSupportedFragment",
                             IDS_PRINT_PREVIEW_NO_LONGER_SUPPORTED_FRAGMENT);
  source->AddLocalizedString("noLongerSupported",
                             IDS_PRINT_PREVIEW_NO_LONGER_SUPPORTED);
  source->AddLocalizedString("couldNotPrint",
                             IDS_PRINT_PREVIEW_COULD_NOT_PRINT);
  source->AddLocalizedString("registerPromoButtonText",
                             IDS_PRINT_PREVIEW_REGISTER_PROMO_BUTTON_TEXT);
  source->AddLocalizedString(
      "extensionDestinationIconTooltip",
      IDS_PRINT_PREVIEW_EXTENSION_DESTINATION_ICON_TOOLTIP);
  source->AddLocalizedString(
      "advancedSettingsSearchBoxPlaceholder",
      IDS_PRINT_PREVIEW_ADVANCED_SETTINGS_SEARCH_BOX_PLACEHOLDER);
  source->AddLocalizedString("advancedSettingsDialogTitle",
                             IDS_PRINT_PREVIEW_ADVANCED_SETTINGS_DIALOG_TITLE);
  source->AddLocalizedString(
      "noAdvancedSettingsMatchSearchHint",
      IDS_PRINT_PREVIEW_NO_ADVANCED_SETTINGS_MATCH_SEARCH_HINT);
  source->AddLocalizedString(
      "advancedSettingsDialogConfirm",
      IDS_PRINT_PREVIEW_ADVANCED_SETTINGS_DIALOG_CONFIRM);
  source->AddLocalizedString("cancel", IDS_CANCEL);
  source->AddLocalizedString("advancedOptionsLabel",
                             IDS_PRINT_PREVIEW_ADVANCED_OPTIONS_LABEL);
  source->AddLocalizedString("showAdvancedOptions",
                             IDS_PRINT_PREVIEW_SHOW_ADVANCED_OPTIONS);
  source->AddLocalizedString("newShowAdvancedOptions",
                             IDS_PRINT_PREVIEW_NEW_SHOW_ADVANCED_OPTIONS);

  source->AddLocalizedString("accept", IDS_PRINT_PREVIEW_ACCEPT_INVITE);
  source->AddLocalizedString(
      "acceptForGroup", IDS_PRINT_PREVIEW_ACCEPT_GROUP_INVITE);
  source->AddLocalizedString("reject", IDS_PRINT_PREVIEW_REJECT_INVITE);
  source->AddLocalizedString(
      "groupPrinterSharingInviteText", IDS_PRINT_PREVIEW_GROUP_INVITE_TEXT);
  source->AddLocalizedString(
      "printerSharingInviteText", IDS_PRINT_PREVIEW_INVITE_TEXT);
  source->AddLocalizedString("registerPrinterInformationMessage",
                             IDS_CLOUD_PRINT_REGISTER_PRINTER_INFORMATION);
  source->AddLocalizedString("moreOptionsLabel", IDS_MORE_OPTIONS_LABEL);
  source->AddLocalizedString("lessOptionsLabel", IDS_LESS_OPTIONS_LABEL);
  source->AddLocalizedString("managedOption",
                             IDS_PRINT_PREVIEW_MANAGED_OPTION_TEXT);
#if defined(OS_CHROMEOS)
  source->AddLocalizedString("configuringInProgressText",
                             IDS_PRINT_CONFIGURING_IN_PROGRESS_TEXT);
  source->AddLocalizedString("configuringFailedText",
                             IDS_PRINT_CONFIGURING_FAILED_TEXT);
#else
  const base::string16 shortcut_text(base::UTF8ToUTF16(kBasicPrintShortcut));
  source->AddString("systemDialogOption",
                    l10n_util::GetStringFUTF16(
                        IDS_PRINT_PREVIEW_SYSTEM_DIALOG_OPTION, shortcut_text));
#endif
#if defined(OS_MACOSX)
  source->AddLocalizedString("openingPDFInPreview",
                             IDS_PRINT_PREVIEW_OPENING_PDF_IN_PREVIEW_APP);
  source->AddLocalizedString("openPdfInPreviewOption",
                             IDS_PRINT_PREVIEW_OPEN_PDF_IN_PREVIEW_APP);
#endif
}

void AddPrintPreviewImages(content::WebUIDataSource* source) {
  source->AddResourcePath("images/1x/printer.png",
                          IDR_PRINT_PREVIEW_IMAGES_1X_PRINTER);
  source->AddResourcePath("images/2x/printer.png",
                          IDR_PRINT_PREVIEW_IMAGES_2X_PRINTER);
  source->AddResourcePath("images/1x/printer_shared.png",
                          IDR_PRINT_PREVIEW_IMAGES_1X_PRINTER_SHARED);
  source->AddResourcePath("images/2x/printer_shared.png",
                          IDR_PRINT_PREVIEW_IMAGES_2X_PRINTER_SHARED);
  source->AddResourcePath("images/business.svg",
                          IDR_PRINT_PREVIEW_IMAGES_ENTERPRISE_PRINTER);
  source->AddResourcePath("images/google_doc.png",
                          IDR_PRINT_PREVIEW_IMAGES_GOOGLE_DOC);
  source->AddResourcePath("images/pdf.png", IDR_PRINT_PREVIEW_IMAGES_PDF);
  source->AddResourcePath("images/mobile.png", IDR_PRINT_PREVIEW_IMAGES_MOBILE);
  source->AddResourcePath("images/mobile_shared.png",
                          IDR_PRINT_PREVIEW_IMAGES_MOBILE_SHARED);
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
  enterprise_managed = base::win::IsEnterpriseManaged();
#endif
  source->AddBoolean("isEnterpriseManaged", enterprise_managed);

  bool nup_printing_enabled =
      base::FeatureList::IsEnabled(features::kNupPrinting);
  source->AddBoolean("pagesPerSheetEnabled", nup_printing_enabled);

  bool cloud_printer_handler_enabled =
      base::FeatureList::IsEnabled(features::kCloudPrinterHandler);
  source->AddBoolean("cloudPrinterHandlerEnabled",
                     cloud_printer_handler_enabled);
}

void SetupPrintPreviewPlugin(content::WebUIDataSource* source) {
  source->AddResourcePath("pdf/index.html", IDR_PDF_INDEX_HTML);
  source->AddResourcePath("pdf/index.css", IDR_PDF_INDEX_CSS);
  source->AddResourcePath("pdf/main.js", IDR_PDF_MAIN_JS);
  source->AddResourcePath("pdf/pdf_viewer.js", IDR_PDF_PDF_VIEWER_JS);
  source->AddResourcePath("pdf/toolbar_manager.js", IDR_PDF_UI_MANAGER_JS);
  source->AddResourcePath("pdf/pdf_fitting_type.js",
                          IDR_PDF_PDF_FITTING_TYPE_JS);
  source->AddResourcePath("pdf/viewport.js", IDR_PDF_VIEWPORT_JS);
  source->AddResourcePath("pdf/open_pdf_params_parser.js",
                          IDR_PDF_OPEN_PDF_PARAMS_PARSER_JS);
  source->AddResourcePath("pdf/navigator.js", IDR_PDF_NAVIGATOR_JS);
  source->AddResourcePath("pdf/viewport_scroller.js",
                          IDR_PDF_VIEWPORT_SCROLLER_JS);
  source->AddResourcePath("pdf/pdf_scripting_api.js",
                          IDR_PDF_PDF_SCRIPTING_API_JS);
  source->AddResourcePath("pdf/zoom_manager.js", IDR_PDF_ZOOM_MANAGER_JS);
  source->AddResourcePath("pdf/gesture_detector.js",
                          IDR_PDF_GESTURE_DETECTOR_JS);
  source->AddResourcePath("pdf/browser_api.js", IDR_PDF_BROWSER_API_JS);
  source->AddResourcePath("pdf/metrics.js", IDR_PDF_METRICS_JS);
  source->AddResourcePath("pdf/coords_transformer.js",
                          IDR_PDF_COORDS_TRANSFORMER_JS);

  source->AddResourcePath("pdf/elements/shared-vars.html",
                          IDR_PDF_SHARED_VARS_HTML);
  source->AddResourcePath("pdf/elements/icons.html", IDR_PDF_ICONS_HTML);
  source->AddResourcePath("pdf/elements/viewer-bookmark/viewer-bookmark.html",
                          IDR_PDF_VIEWER_BOOKMARK_HTML);
  source->AddResourcePath("pdf/elements/viewer-bookmark/viewer-bookmark.js",
                          IDR_PDF_VIEWER_BOOKMARK_JS);
  source->AddResourcePath(
      "pdf/elements/viewer-bookmarks-content/viewer-bookmarks-content.html",
      IDR_PDF_VIEWER_BOOKMARKS_CONTENT_HTML);
  source->AddResourcePath(
      "pdf/elements/viewer-bookmarks-content/viewer-bookmarks-content.js",
      IDR_PDF_VIEWER_BOOKMARKS_CONTENT_JS);
  source->AddResourcePath(
      "pdf/elements/viewer-error-screen/viewer-error-screen.html",
      IDR_PDF_VIEWER_ERROR_SCREEN_HTML);
  source->AddResourcePath(
      "pdf/elements/viewer-error-screen/viewer-error-screen.js",
      IDR_PDF_VIEWER_ERROR_SCREEN_JS);
  source->AddResourcePath(
      "pdf/elements/viewer-page-indicator/viewer-page-indicator.html",
      IDR_PDF_VIEWER_PAGE_INDICATOR_HTML);
  source->AddResourcePath(
      "pdf/elements/viewer-page-indicator/viewer-page-indicator.js",
      IDR_PDF_VIEWER_PAGE_INDICATOR_JS);
  source->AddResourcePath(
      "pdf/elements/viewer-page-selector/viewer-page-selector.html",
      IDR_PDF_VIEWER_PAGE_SELECTOR_HTML);
  source->AddResourcePath(
      "pdf/elements/viewer-page-selector/viewer-page-selector.js",
      IDR_PDF_VIEWER_PAGE_SELECTOR_JS);
  source->AddResourcePath(
      "pdf/elements/viewer-password-screen/viewer-password-screen.html",
      IDR_PDF_VIEWER_PASSWORD_SCREEN_HTML);
  source->AddResourcePath(
      "pdf/elements/viewer-password-screen/viewer-password-screen.js",
      IDR_PDF_VIEWER_PASSWORD_SCREEN_JS);
  source->AddResourcePath(
      "pdf/elements/viewer-pdf-toolbar/viewer-pdf-toolbar.html",
      IDR_PDF_VIEWER_PDF_TOOLBAR_HTML);
  source->AddResourcePath(
      "pdf/elements/viewer-pdf-toolbar/viewer-pdf-toolbar.js",
      IDR_PDF_VIEWER_PDF_TOOLBAR_JS);
  source->AddResourcePath(
      "pdf/elements/viewer-toolbar-dropdown/viewer-toolbar-dropdown.html",
      IDR_PDF_VIEWER_TOOLBAR_DROPDOWN_HTML);
  source->AddResourcePath(
      "pdf/elements/viewer-toolbar-dropdown/viewer-toolbar-dropdown.js",
      IDR_PDF_VIEWER_TOOLBAR_DROPDOWN_JS);
  source->AddResourcePath(
      "pdf/elements/viewer-zoom-toolbar/viewer-zoom-button.html",
      IDR_PDF_VIEWER_ZOOM_BUTTON_HTML);
  source->AddResourcePath(
      "pdf/elements/viewer-zoom-toolbar/viewer-zoom-button.js",
      IDR_PDF_VIEWER_ZOOM_BUTTON_JS);
  source->AddResourcePath(
      "pdf/elements/viewer-zoom-toolbar/viewer-zoom-toolbar.html",
      IDR_PDF_VIEWER_ZOOM_SELECTOR_HTML);
  source->AddResourcePath(
      "pdf/elements/viewer-zoom-toolbar/viewer-zoom-toolbar.js",
      IDR_PDF_VIEWER_ZOOM_SELECTOR_JS);

  source->SetRequestFilter(base::BindRepeating(&HandleRequestCallback));
  source->OverrideContentSecurityPolicyChildSrc("child-src 'self';");
  source->DisableDenyXFrameOptions();
  source->OverrideContentSecurityPolicyObjectSrc("object-src 'self';");
}

content::WebUIDataSource* CreateNewPrintPreviewUISource(Profile* profile) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIPrintHost);
  AddPrintPreviewStrings(source);
  AddPrintPreviewImages(source);
  source->SetJsonPath("strings.js");
#if BUILDFLAG(OPTIMIZE_WEBUI)
  source->AddResourcePath("crisper.js", IDR_PRINT_PREVIEW_CRISPER_JS);
  source->SetDefaultResource(IDR_PRINT_PREVIEW_VULCANIZED_HTML);
  source->SetDefaultResource(
      base::FeatureList::IsEnabled(features::kWebUIPolymer2) ?
          IDR_PRINT_PREVIEW_VULCANIZED_P2_HTML :
          IDR_PRINT_PREVIEW_VULCANIZED_HTML);
#else
  for (size_t i = 0; i < kPrintPreviewResourcesSize; ++i) {
    source->AddResourcePath(kPrintPreviewResources[i].name,
                            kPrintPreviewResources[i].value);
  }
  source->SetDefaultResource(IDR_PRINT_PREVIEW_NEW_HTML);
#endif
  SetupPrintPreviewPlugin(source);
  AddPrintPreviewFlags(source, profile);
  return source;
}

content::WebUIDataSource* CreatePrintPreviewUISource(Profile* profile) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIPrintHost);
  AddPrintPreviewStrings(source);
  source->SetJsonPath("strings.js");
  source->AddResourcePath("print_preview.js", IDR_PRINT_PREVIEW_JS);
  AddPrintPreviewImages(source);
  source->SetDefaultResource(IDR_PRINT_PREVIEW_HTML);
  SetupPrintPreviewPlugin(source);
  AddPrintPreviewFlags(source, profile);
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
      id_(g_print_preview_ui_id_map.Get().Add(this)),
      handler_(handler.get()) {
  web_ui->AddMessageHandler(std::move(handler));

  g_print_preview_request_id_map.Get().Set(id_, -1);
}

PrintPreviewUI::PrintPreviewUI(content::WebUI* web_ui)
    : ConstrainedWebDialogUI(web_ui),
      initial_preview_start_time_(base::TimeTicks::Now()),
      id_(g_print_preview_ui_id_map.Get().Add(this)),
      handler_(CreatePrintPreviewHandlers(web_ui)) {
  // Set up the chrome://print/ data source.
  Profile* profile = Profile::FromWebUI(web_ui);

  bool new_print_preview_enabled =
      base::FeatureList::IsEnabled(features::kNewPrintPreview) ||
      base::FeatureList::IsEnabled(features::kExperimentalUi);
  if (new_print_preview_enabled) {
    content::WebUIDataSource::Add(profile,
                                  CreateNewPrintPreviewUISource(profile));
  } else {
    content::WebUIDataSource::Add(profile, CreatePrintPreviewUISource(profile));
  }

  // Set up the chrome://theme/ source.
  content::URLDataSource::Add(profile, std::make_unique<ThemeSource>(profile));

  g_print_preview_request_id_map.Get().Set(id_, -1);
}

PrintPreviewUI::~PrintPreviewUI() {
  PrintPreviewDataService::GetInstance()->RemoveEntry(id_);
  g_print_preview_request_id_map.Get().Erase(id_);
  g_print_preview_ui_id_map.Get().Remove(id_);
}

void PrintPreviewUI::GetPrintPreviewDataForIndex(
    int index,
    scoped_refptr<base::RefCountedMemory>* data) const {
  PrintPreviewDataService::GetInstance()->GetDataEntry(id_, index, data);
}

void PrintPreviewUI::SetPrintPreviewDataForIndex(
    int index,
    scoped_refptr<base::RefCountedMemory> data) {
  PrintPreviewDataService::GetInstance()->SetDataEntry(id_, index,
                                                       std::move(data));
}

void PrintPreviewUI::ClearAllPreviewData() {
  PrintPreviewDataService::GetInstance()->RemoveEntry(id_);
}

void PrintPreviewUI::SetInitiatorTitle(
    const base::string16& job_title) {
  initiator_title_ = job_title;
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
  print_preview_ui->source_is_modifiable_ = params.is_modifiable;
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

int32_t PrintPreviewUI::GetIDForPrintPreviewUI() const {
  return id_;
}

void PrintPreviewUI::OnPrintPreviewDialogClosed() {
  WebContents* preview_dialog = web_ui()->GetWebContents();
  printing::BackgroundPrintingManager* background_printing_manager =
      g_browser_process->background_printing_manager();
  if (background_printing_manager->HasPrintPreviewDialog(preview_dialog))
    return;
  OnClosePrintPreviewDialog();
}

void PrintPreviewUI::OnInitiatorClosed() {
  // Should only get here if the initiator was still tracked by the Print
  // Preview Dialog Controller, so the print job has not yet been sent.
  WebContents* preview_dialog = web_ui()->GetWebContents();
  printing::BackgroundPrintingManager* background_printing_manager =
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
  g_print_preview_request_id_map.Get().Set(id_, request_id);
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

  if (g_testing_delegate)
    g_testing_delegate->DidGetPreviewPageCount(params.page_count);
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
  layout.SetDouble(printing::kSettingMarginTop, page_layout.margin_top);
  layout.SetDouble(printing::kSettingMarginLeft, page_layout.margin_left);
  layout.SetDouble(printing::kSettingMarginBottom, page_layout.margin_bottom);
  layout.SetDouble(printing::kSettingMarginRight, page_layout.margin_right);
  layout.SetDouble(printing::kSettingContentWidth, page_layout.content_width);
  layout.SetDouble(printing::kSettingContentHeight, page_layout.content_height);
  layout.SetInteger(printing::kSettingPrintableAreaX, printable_area.x());
  layout.SetInteger(printing::kSettingPrintableAreaY, printable_area.y());
  layout.SetInteger(printing::kSettingPrintableAreaWidth,
                    printable_area.width());
  layout.SetInteger(printing::kSettingPrintableAreaHeight,
                    printable_area.height());
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

  if (g_testing_delegate)
    g_testing_delegate->DidRenderPreviewPage(web_ui()->GetWebContents());
  handler_->SendPagePreviewReady(page_number, id_, preview_request_id);
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

  SetPrintPreviewDataForIndex(printing::COMPLETE_PREVIEW_DOCUMENT_INDEX,
                              std::move(data));

  handler_->OnPrintPreviewReady(id_, preview_request_id);
}

void PrintPreviewUI::OnCancelPendingPreviewRequest() {
  g_print_preview_request_id_map.Get().Set(id_, -1);
}

void PrintPreviewUI::OnPrintPreviewFailed(int request_id) {
  handler_->OnPrintPreviewFailed(request_id);
}

void PrintPreviewUI::OnInvalidPrinterSettings(int request_id) {
  handler_->OnInvalidPrinterSettings(request_id);
}

void PrintPreviewUI::OnHidePreviewDialog() {
  WebContents* preview_dialog = web_ui()->GetWebContents();
  printing::BackgroundPrintingManager* background_printing_manager =
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
void PrintPreviewUI::SetDelegateForTesting(TestingDelegate* delegate) {
  g_testing_delegate = delegate;
}

void PrintPreviewUI::SetSelectedFileForTesting(const base::FilePath& path) {
  handler_->FileSelectedForTesting(path, 0, nullptr);
}

void PrintPreviewUI::SetPdfSavedClosureForTesting(
    const base::Closure& closure) {
  handler_->SetPdfSavedClosureForTesting(closure);
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
