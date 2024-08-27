// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/print_preview/print_preview_ui.h"

#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/containers/id_map.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted_memory.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/pdf/pdf_extension_util.h"
#include "chrome/browser/printing/background_printing_manager.h"
#include "chrome/browser/printing/pdf_nup_converter_client.h"
#include "chrome/browser/printing/print_compositor_util.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "chrome/browser/printing/print_preview_data_service.h"
#include "chrome/browser/printing/print_preview_dialog_controller.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/printing/printer_query.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/webui/metrics_handler.h"
#include "chrome/browser/ui/webui/plural_string_handler.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_handler.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/pdf_resources_map.h"
#include "chrome/grit/print_preview_resources.h"
#include "chrome/grit/print_preview_resources_map.h"
#include "components/device_event_log/device_event_log.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/prefs/pref_service.h"
#include "components/printing/browser/print_composite_client.h"
#include "components/printing/browser/print_manager_utils.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "printing/buildflags/buildflags.h"
#include "printing/mojom/print.mojom.h"
#include "printing/nup_parameters.h"
#include "printing/print_job_constants.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/web_dialogs/web_dialog_delegate.h"
#include "ui/web_dialogs/web_dialog_ui.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/webui/print_preview/print_preview_handler_chromeos.h"
#include "chrome/common/chrome_features.h"
#endif

#if !BUILDFLAG(OPTIMIZE_WEBUI)
#include "chrome/browser/ui/webui/managed_ui_handler.h"
#endif

#if BUILDFLAG(ENABLE_OOP_PRINTING)
#include "chrome/browser/printing/oop_features.h"
#include "chrome/browser/printing/print_backend_service_manager.h"
#endif

using content::WebContents;

namespace printing {

namespace {

#if BUILDFLAG(IS_MAC)
const char16_t kBasicPrintShortcut[] = u"(⌥⌘P)";
#elif !BUILDFLAG(IS_CHROMEOS)
const char16_t kBasicPrintShortcut[] = u"(Ctrl+Shift+P)";
#endif

constexpr char kInvalidArgsForDidStartPreview[] =
    "Invalid arguments for DidStartPreview";
constexpr char kInvalidPageIndexForDidPreviewPage[] =
    "Invalid page index for DidPreviewPage";
constexpr char kInvalidPageCountForMetafileReadyForPrinting[] =
    "Invalid page count for MetafileReadyForPrinting";

PrintPreviewUI::TestDelegate* g_test_delegate = nullptr;

// Returns true only for the first time it is called.
bool IsFirstInstanceSinceStartup() {
  static bool first_instance = true;
  bool first = first_instance;
  first_instance = false;
  return first;
}

void StopWorker(int document_cookie) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (document_cookie <= 0)
    return;
  scoped_refptr<PrintQueriesQueue> queue =
      g_browser_process->print_job_manager()->queue();
  std::unique_ptr<PrinterQuery> printer_query =
      queue->PopPrinterQuery(document_cookie);
}

bool IsValidPageIndex(uint32_t page_index, uint32_t page_count) {
  return page_index < page_count;
}

WebContents* GetInitiator(content::WebUI* web_ui) {
  auto* dialog_controller = PrintPreviewDialogController::GetInstance();
  CHECK(dialog_controller);
  return dialog_controller->GetInitiator(web_ui->GetWebContents());
}

// Mapping from PrintPreviewUI ID to print preview request ID.
using PrintPreviewRequestIdMap = base::flat_map<int, int>;

PrintPreviewRequestIdMap& GetPrintPreviewRequestIdMap() {
  static base::NoDestructor<PrintPreviewRequestIdMap> map;
  return *map;
}

// PrintPreviewUI IDMap used to avoid exposing raw pointer addresses to WebUI.
// Only accessed on the UI thread.
base::LazyInstance<base::IDMap<PrintPreviewUI*>>::DestructorAtExit
    g_print_preview_ui_id_map = LAZY_INSTANCE_INITIALIZER;

void AddPrintPreviewStrings(content::WebUIDataSource* source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
    {"advancedSettingsDialogConfirm",
     IDS_PRINT_PREVIEW_ADVANCED_SETTINGS_DIALOG_CONFIRM},
    {"advancedSettingsDialogTitle",
     IDS_PRINT_PREVIEW_ADVANCED_SETTINGS_DIALOG_TITLE},
    {"advancedSettingsSearchBoxPlaceholder",
     IDS_PRINT_PREVIEW_ADVANCED_SETTINGS_SEARCH_BOX_PLACEHOLDER},
    {"borderlessLabel", IDS_PRINT_PREVIEW_BORDERLESS_LABEL},
    {"bottom", IDS_PRINT_PREVIEW_BOTTOM_MARGIN_LABEL},
    {"cancel", IDS_CANCEL},
    {"clearSearch", IDS_CLEAR_SEARCH},
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
    {"invalidPrinterSettings", IDS_PRINT_PREVIEW_INVALID_PRINTER_SETTINGS},
    {"layoutLabel", IDS_PRINT_PREVIEW_LAYOUT_LABEL},
    {"left", IDS_PRINT_PREVIEW_LEFT_MARGIN_LABEL},
    {"loading", IDS_PRINT_PREVIEW_LOADING},
    {"manage", IDS_PRINT_PREVIEW_MANAGE},
#if BUILDFLAG(IS_CHROMEOS)
    {"managePrintersLabel", IDS_PRINT_PREVIEW_MANAGE_PRINTERS_LABEL},
#endif
    {"managedSettings", IDS_PRINT_PREVIEW_MANAGED_SETTINGS_TEXT},
    {"marginsLabel", IDS_PRINT_PREVIEW_MARGINS_LABEL},
    {"mediaSizeLabel", IDS_PRINT_PREVIEW_MEDIA_SIZE_LABEL},
    {"mediaTypeLabel", IDS_PRINT_PREVIEW_MEDIA_TYPE_LABEL},
    {"minimumMargins", IDS_PRINT_PREVIEW_MINIMUM_MARGINS},
    {"moreOptionsLabel", IDS_MORE_OPTIONS_LABEL},
    {"newShowAdvancedOptions", IDS_PRINT_PREVIEW_NEW_SHOW_ADVANCED_OPTIONS},
    {"noAdvancedSettingsMatchSearchHint",
     IDS_PRINT_PREVIEW_NO_ADVANCED_SETTINGS_MATCH_SEARCH_HINT},
    {"noDestinationsMessage", IDS_PRINT_PREVIEW_NO_DESTINATIONS_MESSAGE},
    {"noMargins", IDS_PRINT_PREVIEW_NO_MARGINS},
    {"nonIsotropicDpiItemLabel",
     IDS_PRINT_PREVIEW_NON_ISOTROPIC_DPI_ITEM_LABEL},
    {"optionAllPages", IDS_PRINT_PREVIEW_OPTION_ALL_PAGES},
    {"optionBackgroundColorsAndImages",
     IDS_PRINT_PREVIEW_OPTION_BACKGROUND_COLORS_AND_IMAGES},
    {"optionBw", IDS_PRINT_PREVIEW_OPTION_BW},
    {"optionCollate", IDS_PRINT_PREVIEW_OPTION_COLLATE},
    {"optionColor", IDS_PRINT_PREVIEW_OPTION_COLOR},
    {"optionCustomPages", IDS_PRINT_PREVIEW_OPTION_CUSTOM_PAGES},
    {"optionCustomScaling", IDS_PRINT_PREVIEW_OPTION_CUSTOM_SCALING},
    {"optionDefaultScaling", IDS_PRINT_PREVIEW_OPTION_DEFAULT_SCALING},
    {"optionEvenPages", IDS_PRINT_PREVIEW_OPTION_EVEN_PAGES},
    {"optionFitToPage", IDS_PRINT_PREVIEW_OPTION_FIT_TO_PAGE},
    {"optionFitToPaper", IDS_PRINT_PREVIEW_OPTION_FIT_TO_PAPER},
    {"optionHeaderFooter", IDS_PRINT_PREVIEW_OPTION_HEADER_FOOTER},
    {"optionLandscape", IDS_PRINT_PREVIEW_OPTION_LANDSCAPE},
    {"optionLongEdge", IDS_PRINT_PREVIEW_OPTION_LONG_EDGE},
    {"optionOddPages", IDS_PRINT_PREVIEW_OPTION_ODD_PAGES},
    {"optionPortrait", IDS_PRINT_PREVIEW_OPTION_PORTRAIT},
    {"optionRasterize", IDS_PRINT_PREVIEW_OPTION_RASTERIZE},
    {"optionSelectionOnly", IDS_PRINT_PREVIEW_OPTION_SELECTION_ONLY},
    {"optionShortEdge", IDS_PRINT_PREVIEW_OPTION_SHORT_EDGE},
    {"optionTwoSided", IDS_PRINT_PREVIEW_OPTION_TWO_SIDED},
    {"optionsLabel", IDS_PRINT_PREVIEW_OPTIONS_LABEL},
    {"pageDescription", IDS_PRINT_PREVIEW_DESCRIPTION},
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
#if BUILDFLAG(IS_CHROMEOS)
    {"printerSetupInfoMessageDetailNoPrintersText",
     IDS_PRINT_PREVIEW_PRINTER_SETUP_INFO_MESSAGE_DETAIL_NO_PRINTERS_TEXT},
    {"printerSetupInfoMessageDetailPrinterOfflineText",
     IDS_PRINT_PREVIEW_PRINTER_SETUP_INFO_MESSAGE_DETAIL_PRINTER_OFFLINE_TEXT},
    {"printerSetupInfoMessageHeadingNoPrintersText",
     IDS_PRINT_PREVIEW_PRINTER_SETUP_INFO_MESSAGE_HEADING_NO_PRINTERS_TEXT},
    {"printerSetupInfoMessageHeadingPrinterOfflineText",
     IDS_PRINT_PREVIEW_PRINTER_SETUP_INFO_MESSAGE_HEADING_PRINTER_OFFLINE_TEXT},
    {"printToGoogleDrive", IDS_PRINT_PREVIEW_PRINT_TO_GOOGLE_DRIVE},
#endif
    {"printToPDF", IDS_PRINT_PREVIEW_PRINT_TO_PDF},
    {"printing", IDS_PRINT_PREVIEW_PRINTING},
#if BUILDFLAG(IS_CHROMEOS)
    {"resolveExtensionUSBDialogTitle",
     IDS_PRINT_PREVIEW_RESOLVE_EXTENSION_USB_DIALOG_TITLE},
    {"resolveExtensionUSBErrorMessage",
     IDS_PRINT_PREVIEW_RESOLVE_EXTENSION_USB_ERROR_MESSAGE},
    {"resolveExtensionUSBPermissionMessage",
     IDS_PRINT_PREVIEW_RESOLVE_EXTENSION_USB_PERMISSION_MESSAGE},
#endif
    {"right", IDS_PRINT_PREVIEW_RIGHT_MARGIN_LABEL},
    {"saveButton", IDS_PRINT_PREVIEW_SAVE_BUTTON},
    {"saving", IDS_PRINT_PREVIEW_SAVING},
    {"scalingInstruction", IDS_PRINT_PREVIEW_SCALING_INSTRUCTION},
    {"scalingLabel", IDS_PRINT_PREVIEW_SCALING_LABEL},
    {"searchBoxPlaceholder", IDS_PRINT_PREVIEW_SEARCH_BOX_PLACEHOLDER},
    {"searchResultBubbleText", IDS_SEARCH_RESULT_BUBBLE_TEXT},
    {"searchResultsBubbleText", IDS_SEARCH_RESULTS_BUBBLE_TEXT},
    {"selectButton", IDS_PRINT_PREVIEW_BUTTON_SELECT},
    {"seeMore", IDS_PRINT_PREVIEW_SEE_MORE},
    {"seeMoreDestinationsLabel", IDS_PRINT_PREVIEW_SEE_MORE_DESTINATIONS_LABEL},
#if BUILDFLAG(IS_CHROMEOS)
    {"serverSearchBoxPlaceholder",
     IDS_PRINT_PREVIEW_SERVER_SEARCH_BOX_PLACEHOLDER},
#endif
    {"title", IDS_PRINT_PREVIEW_TITLE},
    {"top", IDS_PRINT_PREVIEW_TOP_MARGIN_LABEL},
#if BUILDFLAG(IS_CHROMEOS)
    {"configuringFailedText", IDS_PRINT_CONFIGURING_FAILED_TEXT},
    {"configuringInProgressText", IDS_PRINT_CONFIGURING_IN_PROGRESS_TEXT},
    {"optionPin", IDS_PRINT_PREVIEW_OPTION_PIN},
    {"pinErrorMessage", IDS_PRINT_PREVIEW_PIN_ERROR_MESSAGE},
    {"pinPlaceholder", IDS_PRINT_PREVIEW_PIN_PLACEHOLDER},
    {"printerEulaURL", IDS_PRINT_PREVIEW_EULA_URL},
    {"printerStatusDeviceError", IDS_PRINT_PREVIEW_PRINTER_STATUS_DEVICE_ERROR},
    {"printerStatusDoorOpen", IDS_PRINT_PREVIEW_PRINTER_STATUS_DOOR_OPEN},
    {"printerStatusLowOnInk", IDS_PRINT_PREVIEW_PRINTER_STATUS_LOW_ON_INK},
    {"printerStatusLowOnPaper", IDS_PRINT_PREVIEW_PRINTER_STATUS_LOW_ON_PAPER},
    {"printerStatusOutOfInk", IDS_PRINT_PREVIEW_PRINTER_STATUS_OUT_OF_INK},
    {"printerStatusOutOfPaper", IDS_PRINT_PREVIEW_PRINTER_STATUS_OUT_OF_PAPER},
    {"printerStatusOutputAlmostFull",
     IDS_PRINT_PREVIEW_PRINTER_STATUS_OUPUT_ALMOST_FULL},
    {"printerStatusOutputFull", IDS_PRINT_PREVIEW_PRINTER_STATUS_OUPUT_FULL},
    {"printerStatusPaperJam", IDS_PRINT_PREVIEW_PRINTER_STATUS_PAPER_JAM},
    {"printerStatusPaused", IDS_PRINT_PREVIEW_PRINTER_STATUS_PAUSED},
    {"printerStatusPrinterQueueFull",
     IDS_PRINT_PREVIEW_PRINTER_STATUS_PRINTER_QUEUE_FULL},
    {"printerStatusPrinterUnreachable",
     IDS_PRINT_PREVIEW_PRINTER_STATUS_PRINTER_UNREACHABLE},
    {"printerStatusStopped", IDS_PRINT_PREVIEW_PRINTER_STATUS_STOPPED},
    {"printerStatusTrayMissing", IDS_PRINT_PREVIEW_PRINTER_STATUS_TRAY_MISSING},
#endif
#if BUILDFLAG(IS_MAC)
    {"openPdfInPreviewOption", IDS_PRINT_PREVIEW_OPEN_PDF_IN_PREVIEW_APP},
    {"openingPDFInPreview", IDS_PRINT_PREVIEW_OPENING_PDF_IN_PREVIEW_APP},
#endif
  };
  source->AddLocalizedStrings(kLocalizedStrings);

#if !BUILDFLAG(IS_CHROMEOS)
  const std::u16string shortcut_text(kBasicPrintShortcut);
  source->AddString("systemDialogOption",
                    l10n_util::GetStringFUTF16(
                        IDS_PRINT_PREVIEW_SYSTEM_DIALOG_OPTION, shortcut_text));
#endif

  // Register strings for the PDF viewer, so that $i18n{} replacements work.
  base::Value::Dict pdf_strings;
  pdf_extension_util::AddStrings(
      pdf_extension_util::PdfViewerContext::kPrintPreview, &pdf_strings);
  source->AddLocalizedStrings(pdf_strings);
}

void AddPrintPreviewFlags(content::WebUIDataSource* source, Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS)
  source->AddBoolean("useSystemDefaultPrinter", false);
#else
  bool system_default_printer = profile->GetPrefs()->GetBoolean(
      prefs::kPrintPreviewUseSystemDefaultPrinter);
  source->AddBoolean("useSystemDefaultPrinter", system_default_printer);
#endif

  source->AddBoolean(
      "isEnterpriseManaged",
      policy::ManagementServiceFactory::GetForPlatform()->IsManaged());

  source->AddBoolean("isBorderlessPrintingEnabled", BUILDFLAG(IS_CHROMEOS));
}

void SetupPrintPreviewPlugin(content::WebUIDataSource* source) {
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ChildSrc,
      "child-src 'self' chrome-untrusted://print;");
  source->DisableDenyXFrameOptions();
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ObjectSrc,
      "object-src chrome-untrusted://print;");
}

void CreateAndAddPrintPreviewUISource(Profile* profile) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIPrintHost);
  webui::SetupWebUIDataSource(
      source,
      base::make_span(kPrintPreviewResources, kPrintPreviewResourcesSize),
      IDR_PRINT_PREVIEW_PRINT_PREVIEW_HTML);
  AddPrintPreviewStrings(source);
  source->AddResourcePaths(base::make_span(kPdfResources, kPdfResourcesSize));
  SetupPrintPreviewPlugin(source);
  AddPrintPreviewFlags(source, profile);
}

PrintPreviewHandler* CreatePrintPreviewHandlers(content::WebUI* web_ui) {
  auto handler = std::make_unique<PrintPreviewHandler>();
  PrintPreviewHandler* handler_ptr = handler.get();
#if BUILDFLAG(IS_CHROMEOS)
  web_ui->AddMessageHandler(std::make_unique<PrintPreviewHandlerChromeOS>());
#endif
  web_ui->AddMessageHandler(std::move(handler));
  web_ui->AddMessageHandler(std::make_unique<MetricsHandler>());

  // Add a handler to provide pluralized strings.
  auto plural_string_handler = std::make_unique<PluralStringHandler>();
  plural_string_handler->AddLocalizedString(
      "printPreviewPageSummaryLabel", IDS_PRINT_PREVIEW_PAGE_SUMMARY_LABEL);
  plural_string_handler->AddLocalizedString(
      "printPreviewSheetSummaryLabel", IDS_PRINT_PREVIEW_SHEET_SUMMARY_LABEL);
#if BUILDFLAG(IS_CHROMEOS)
  plural_string_handler->AddLocalizedString(
      "sheetsLimitErrorMessage", IDS_PRINT_PREVIEW_SHEETS_LIMIT_ERROR_MESSAGE);
#endif
  web_ui->AddMessageHandler(std::move(plural_string_handler));

  return handler_ptr;
}

}  // namespace

PrintPreviewUIConfig::PrintPreviewUIConfig()
    : DefaultWebUIConfig(content::kChromeUIScheme, chrome::kChromeUIPrintHost) {
}

bool PrintPreviewUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  bool disabled = profile->GetPrefs()->GetBoolean(prefs::kPrintPreviewDisabled);
  return !disabled;
}

bool PrintPreviewUIConfig::ShouldHandleURL(const GURL& url) {
  return url.path() == "/" || url.path() == "/test_loader.html";
}

PrintPreviewUIConfig::~PrintPreviewUIConfig() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(PrintPreviewUI)

PrintPreviewUI::PrintPreviewUI(content::WebUI* web_ui,
                               std::unique_ptr<PrintPreviewHandler> handler)
    : ConstrainedWebDialogUI(web_ui),
      initial_preview_start_time_(base::TimeTicks::Now()),
      first_print_usage_since_startup_(IsFirstInstanceSinceStartup()),
      handler_(handler.get()) {
  web_ui->AddMessageHandler(std::move(handler));

#if BUILDFLAG(ENABLE_OOP_PRINTING)
  RegisterPrintBackendServiceManagerClient();
#endif
}

PrintPreviewUI::PrintPreviewUI(content::WebUI* web_ui)
    : ConstrainedWebDialogUI(web_ui),
      initial_preview_start_time_(base::TimeTicks::Now()),
      first_print_usage_since_startup_(IsFirstInstanceSinceStartup()),
      handler_(CreatePrintPreviewHandlers(web_ui)) {
  // Allow requests to URLs like chrome-untrusted://print/.
  web_ui->AddRequestableScheme(content::kChromeUIUntrustedScheme);

  // Set up the chrome://print/ data source.
  Profile* profile = Profile::FromWebUI(web_ui);
  CreateAndAddPrintPreviewUISource(profile);

  // Set up the chrome://theme/ source.
  content::URLDataSource::Add(profile, std::make_unique<ThemeSource>(profile));

#if BUILDFLAG(ENABLE_OOP_PRINTING)
  RegisterPrintBackendServiceManagerClient();
#endif
}

PrintPreviewUI::~PrintPreviewUI() {
#if BUILDFLAG(ENABLE_OOP_PRINTING)
  UnregisterPrintBackendServiceManagerClient();
#endif
  ClearPreviewUIId();
}

#if BUILDFLAG(ENABLE_OOP_PRINTING)
void PrintPreviewUI::RegisterPrintBackendServiceManagerClient() {
  if (IsOopPrintingEnabled()) {
    service_manager_client_id_ =
        PrintBackendServiceManager::GetInstance().RegisterQueryClient();
  }
}

void PrintPreviewUI::UnregisterPrintBackendServiceManagerClient() {
  if (IsOopPrintingEnabled()) {
    PrintBackendServiceManager::GetInstance().UnregisterClient(
        service_manager_client_id_);
  }
}
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)

mojo::PendingAssociatedRemote<mojom::PrintPreviewUI>
PrintPreviewUI::BindPrintPreviewUI() {
  return receiver_.BindNewEndpointAndPassRemote();
}

bool PrintPreviewUI::IsBound() const {
  return receiver_.is_bound();
}

void PrintPreviewUI::ClearPreviewUIId() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!id_)
    return;

  receiver_.reset();
  PrintPreviewDataService::GetInstance()->RemoveEntry(*id_);
  GetPrintPreviewRequestIdMap().erase(*id_);
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

void PrintPreviewUI::ClearAllPreviewData() {
  PrintPreviewDataService::GetInstance()->RemoveEntry(*id_);
}

void PrintPreviewUI::NotifyUIPreviewPageReady(
    uint32_t page_index,
    int request_id,
    scoped_refptr<base::RefCountedMemory> data_bytes) {
  if (!data_bytes || !data_bytes->size())
    return;

  // Don't bother notifying the UI if this request has been cancelled already.
  if (ShouldCancelRequest(id_, request_id))
    return;

  DCHECK_NE(page_index, kInvalidPageIndex);
  SetPrintPreviewDataForIndex(base::checked_cast<int>(page_index),
                              std::move(data_bytes));

  if (g_test_delegate)
    g_test_delegate->DidRenderPreviewPage(web_ui()->GetWebContents());
  handler_->SendPagePreviewReady(base::checked_cast<int>(page_index), *id_,
                                 request_id);
}

void PrintPreviewUI::NotifyUIPreviewDocumentReady(
    int request_id,
    scoped_refptr<base::RefCountedMemory> data_bytes) {
  if (!data_bytes || !data_bytes->size())
    return;

  // Don't bother notifying the UI if this request has been cancelled already.
  if (ShouldCancelRequest(id_, request_id))
    return;

  if (!initial_preview_start_time_.is_null()) {
    base::TimeDelta display_time =
        base::TimeTicks::Now() - initial_preview_start_time_;
    base::UmaHistogramTimes("PrintPreview.InitialDisplayTime", display_time);
    base::UmaHistogramCustomTimes("PrintPreview.InitialDisplayTime.LongTimes",
                                  display_time,
                                  /*min=*/base::Seconds(10),
                                  /*max=*/base::Seconds(60), /*buckets=*/50);
    if (first_print_usage_since_startup_) {
      base::UmaHistogramTimes("PrintPreview.InitialDisplayTimeFirstPrint",
                              display_time);
      base::UmaHistogramCustomTimes(
          "PrintPreview.InitialDisplayTimeFirstPrint.LongTimes", display_time,
          /*min=*/base::Seconds(10), /*max=*/base::Seconds(60),
          /*buckets=*/50);
    }
    initial_preview_start_time_ = base::TimeTicks();
  }

  if (g_test_delegate) {
    g_test_delegate->PreviewDocumentReady(web_ui()->GetWebContents(),
                                          *data_bytes);
  }

  SetPrintPreviewDataForIndex(COMPLETE_PREVIEW_DOCUMENT_INDEX,
                              std::move(data_bytes));
  handler_->OnPrintPreviewReady(*id_, request_id);
}

bool PrintPreviewUI::ShouldUseCompositor() const {
  if (!IsOopifEnabled()) {
    return false;
  }

  auto* dialog_controller = PrintPreviewDialogController::GetInstance();
  CHECK(dialog_controller);
  const mojom::RequestPrintPreviewParams* request_params =
      dialog_controller->GetRequestParams(web_ui()->GetWebContents());
  CHECK(request_params);
  return request_params->is_modifiable;
}

void PrintPreviewUI::OnCompositePdfPageDone(
    uint32_t page_index,
    int document_cookie,
    int32_t request_id,
    mojom::PrintCompositor::Status status,
    base::ReadOnlySharedMemoryRegion region) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (ShouldCancelRequest(id_, request_id))
    return;

  if (status != mojom::PrintCompositor::Status::kSuccess) {
    DLOG(ERROR) << "Compositing pdf failed with error " << status;
    OnPrintPreviewFailed(request_id);
    return;
  }

  if (pages_per_sheet_ == 1) {
    NotifyUIPreviewPageReady(
        page_index, request_id,
        base::RefCountedSharedMemoryMapping::CreateFromWholeRegion(region));
  } else {
    AddPdfPageForNupConversion(std::move(region));
    uint32_t current_page_index = GetPageToNupConvertIndex(page_index);
    if (current_page_index == kInvalidPageIndex)
      return;

    if (((current_page_index + 1) % pages_per_sheet_) == 0 ||
        LastPageComposited(page_index)) {
      uint32_t new_page_index =
          base::checked_cast<uint32_t>(current_page_index / pages_per_sheet_);
      DCHECK_NE(new_page_index, kInvalidPageIndex);
      std::vector<base::ReadOnlySharedMemoryRegion> pdf_page_regions =
          TakePagesForNupConvert();

      gfx::Rect printable_rect =
          PageSetup::GetSymmetricalPrintableArea(page_size(), printable_area());
      if (printable_rect.IsEmpty())
        return;

      WebContents* web_contents = GetInitiator(web_ui());
      if (!web_contents)
        return;

      auto* client = PdfNupConverterClient::FromWebContents(web_contents);
      DCHECK(client);
      client->DoNupPdfConvert(
          document_cookie, pages_per_sheet_, page_size(), printable_rect,
          std::move(pdf_page_regions),
          mojo::WrapCallbackWithDefaultInvokeIfNotRun(
              base::BindOnce(&PrintPreviewUI::OnNupPdfConvertDone,
                             weak_ptr_factory_.GetWeakPtr(), new_page_index,
                             request_id),
              mojom::PdfNupConverter::Status::CONVERSION_FAILURE,
              base::ReadOnlySharedMemoryRegion()));
    }
  }
}

void PrintPreviewUI::OnNupPdfConvertDone(
    uint32_t page_index,
    int32_t request_id,
    mojom::PdfNupConverter::Status status,
    base::ReadOnlySharedMemoryRegion region) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (status != mojom::PdfNupConverter::Status::SUCCESS) {
    DLOG(ERROR) << "Nup pdf page conversion failed with error " << status;
    OnPrintPreviewFailed(request_id);
    return;
  }

  NotifyUIPreviewPageReady(
      page_index, request_id,
      base::RefCountedSharedMemoryMapping::CreateFromWholeRegion(region));
}

void PrintPreviewUI::OnCompositeToPdfDone(
    int document_cookie,
    int32_t request_id,
    mojom::PrintCompositor::Status status,
    base::ReadOnlySharedMemoryRegion region) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (ShouldCancelRequest(id_, request_id))
    return;

  if (status != mojom::PrintCompositor::Status::kSuccess) {
    DLOG(ERROR) << "Completion of document to pdf failed with error " << status;
    OnPrintPreviewFailed(request_id);
    return;
  }

  if (pages_per_sheet_ == 1) {
    NotifyUIPreviewDocumentReady(
        request_id,
        base::RefCountedSharedMemoryMapping::CreateFromWholeRegion(region));
  } else {
    WebContents* web_contents = GetInitiator(web_ui());
    if (!web_contents)
      return;

    auto* client = PdfNupConverterClient::FromWebContents(web_contents);
    DCHECK(client);

    gfx::Rect printable_rect =
        PageSetup::GetSymmetricalPrintableArea(page_size_, printable_area_);
    if (printable_rect.IsEmpty())
      return;

    client->DoNupPdfDocumentConvert(
        document_cookie, pages_per_sheet_, page_size_, printable_rect,
        std::move(region),
        mojo::WrapCallbackWithDefaultInvokeIfNotRun(
            base::BindOnce(&PrintPreviewUI::OnNupPdfDocumentConvertDone,
                           weak_ptr_factory_.GetWeakPtr(), request_id),
            mojom::PdfNupConverter::Status::CONVERSION_FAILURE,
            base::ReadOnlySharedMemoryRegion()));
  }
}

void PrintPreviewUI::OnPrepareForDocumentToPdfDone(
    int32_t request_id,
    mojom::PrintCompositor::Status status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (ShouldCancelRequest(id_, request_id))
    return;

  if (status != mojom::PrintCompositor::Status::kSuccess)
    OnPrintPreviewFailed(request_id);
}

void PrintPreviewUI::OnNupPdfDocumentConvertDone(
    int32_t request_id,
    mojom::PdfNupConverter::Status status,
    base::ReadOnlySharedMemoryRegion region) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (status != mojom::PdfNupConverter::Status::SUCCESS) {
    DLOG(ERROR) << "Nup pdf document convert failed with error " << status;
    OnPrintPreviewFailed(request_id);
    return;
  }
  NotifyUIPreviewDocumentReady(
      request_id,
      base::RefCountedSharedMemoryMapping::CreateFromWholeRegion(region));
}

void PrintPreviewUI::SetInitiatorTitle(const std::u16string& job_title) {
  initiator_title_ = job_title;
}

bool PrintPreviewUI::LastPageComposited(uint32_t page_index) const {
  if (pages_to_render_.empty())
    return false;

  return page_index == pages_to_render_.back();
}

uint32_t PrintPreviewUI::GetPageToNupConvertIndex(uint32_t page_index) const {
  for (size_t i = 0; i < pages_to_render_.size(); ++i) {
    if (page_index == pages_to_render_[i]) {
      return i;
    }
  }
  return kInvalidPageIndex;
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
bool PrintPreviewUI::ShouldCancelRequest(
    const std::optional<int32_t>& preview_ui_id,
    int request_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!preview_ui_id)
    return true;

  auto& map = GetPrintPreviewRequestIdMap();
  auto it = map.find(*preview_ui_id);
  return it == map.end() || request_id != it->second;
}

std::optional<int32_t> PrintPreviewUI::GetIDForPrintPreviewUI() const {
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

void PrintPreviewUI::OnPrintPreviewRequest(int request_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!initial_preview_start_time_.is_null()) {
    base::UmaHistogramTimes(
        "PrintPreview.InitializationTime",
        base::TimeTicks::Now() - initial_preview_start_time_);
  }
  GetPrintPreviewRequestIdMap()[*id_] = request_id;
}

void PrintPreviewUI::DidStartPreview(mojom::DidStartPreviewParamsPtr params,
                                     int32_t request_id) {
  if (params->page_count == 0 || params->page_count > kMaxPageCount ||
      params->pages_to_render.empty()) {
    receiver_.ReportBadMessage(kInvalidArgsForDidStartPreview);
    return;
  }

  for (uint32_t page_index : params->pages_to_render) {
    if (!IsValidPageIndex(page_index, params->page_count)) {
      receiver_.ReportBadMessage(kInvalidArgsForDidStartPreview);
      return;
    }
  }

  if (!printing::NupParameters::IsSupported(params->pages_per_sheet)) {
    receiver_.ReportBadMessage(kInvalidArgsForDidStartPreview);
    return;
  }

  if (params->page_size.IsEmpty()) {
    receiver_.ReportBadMessage(kInvalidArgsForDidStartPreview);
    return;
  }

  pages_to_render_ = params->pages_to_render;
  pages_to_render_index_ = 0;
  pages_per_sheet_ = params->pages_per_sheet;
  page_size_ = ToFlooredSize(params->page_size);
  ClearAllPreviewData();

  if (g_test_delegate)
    g_test_delegate->DidGetPreviewPageCount(params->page_count);
  handler_->SendPageCountReady(base::checked_cast<int>(params->page_count),
                               params->fit_to_page_scaling, request_id);
}

void PrintPreviewUI::DidGetDefaultPageLayout(
    mojom::PageSizeMarginsPtr page_layout_in_points,
    const gfx::RectF& printable_area_in_points,
    bool all_pages_have_custom_size,
    bool all_pages_have_custom_orientation,
    int32_t request_id) {
  if (printable_area_in_points.width() <= 0 ||
      printable_area_in_points.height() <= 0) {
    NOTREACHED_IN_MIGRATION();
    return;
  }
  // Save printable_area_in_points information for N-up conversion.
  printable_area_ = ToEnclosedRect(printable_area_in_points);

  if (page_layout_in_points->margin_top < 0 ||
      page_layout_in_points->margin_left < 0 ||
      page_layout_in_points->margin_bottom < 0 ||
      page_layout_in_points->margin_right < 0 ||
      page_layout_in_points->content_width < 0 ||
      page_layout_in_points->content_height < 0) {
    // Even though it early returns here, it doesn't block printing the page.
    return;
  }

  base::Value::Dict layout;
  layout.Set(kSettingMarginTop, page_layout_in_points->margin_top);
  layout.Set(kSettingMarginLeft, page_layout_in_points->margin_left);
  layout.Set(kSettingMarginBottom, page_layout_in_points->margin_bottom);
  layout.Set(kSettingMarginRight, page_layout_in_points->margin_right);
  layout.Set(kSettingContentWidth, page_layout_in_points->content_width);
  layout.Set(kSettingContentHeight, page_layout_in_points->content_height);
  layout.Set(kSettingPrintableAreaX, printable_area_in_points.x());
  layout.Set(kSettingPrintableAreaY, printable_area_in_points.y());
  layout.Set(kSettingPrintableAreaWidth, printable_area_in_points.width());
  layout.Set(kSettingPrintableAreaHeight, printable_area_in_points.height());
  handler_->SendPageLayoutReady(std::move(layout), all_pages_have_custom_size,
                                all_pages_have_custom_orientation, request_id);
}

bool PrintPreviewUI::OnPendingPreviewPage(uint32_t page_index) {
  if (pages_to_render_index_ >= pages_to_render_.size())
    return false;

  bool matched = page_index == pages_to_render_[pages_to_render_index_];
  ++pages_to_render_index_;
  return matched;
}

void PrintPreviewUI::OnCancelPendingPreviewRequest() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (id_) {
    GetPrintPreviewRequestIdMap()[*id_] = -1;
  }
}

void PrintPreviewUI::OnPrintPreviewFailed(int request_id) {
  OnCancelPendingPreviewRequest();
  handler_->OnPrintPreviewFailed(request_id);
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

void PrintPreviewUI::SetOptionsFromDocument(
    const mojom::OptionsFromDocumentParamsPtr params,
    int32_t request_id) {
  if (ShouldCancelRequest(id_, request_id))
    return;
  handler_->SendPrintPresetOptions(params->is_scaling_disabled, params->copies,
                                   params->duplex, request_id);
}

void PrintPreviewUI::DidPrepareDocumentForPreview(int32_t document_cookie,
                                                  int32_t request_id) {
  // Determine if document composition from individual pages with the print
  // compositor is the desired configuration. Issue a preparation call to the
  // PrintCompositeClient if that hasn't been done yet. Otherwise, return early.
  if (!ShouldUseCompositor()) {
    return;
  }

  WebContents* web_contents = GetInitiator(web_ui());
  if (!web_contents)
    return;

  // For case of print preview, page metafile is used to composite into
  // the document PDF at same time.  Need to indicate that this scenario
  // is at play for the compositor.
  auto* client = PrintCompositeClient::FromWebContents(web_contents);
  DCHECK(client);
  if (client->GetIsDocumentConcurrentlyComposited(document_cookie))
    return;

  content::RenderFrameHost* render_frame_host =
      PrintViewManager::FromWebContents(web_contents)->print_preview_rfh();
  // |render_frame_host| could be null when the print preview dialog is closed.
  if (!render_frame_host)
    return;

  PRINTER_LOG(EVENT) << "Compositing for document type "
                     << GetCompositorDocumentType();
  client->PrepareToCompositeDocument(
      document_cookie, render_frame_host, GetCompositorDocumentType(),
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&PrintPreviewUI::OnPrepareForDocumentToPdfDone,
                         weak_ptr_factory_.GetWeakPtr(), request_id),
          mojom::PrintCompositor::Status::kCompositingFailure));
}

void PrintPreviewUI::DidPreviewPage(mojom::DidPreviewPageParamsPtr params,
                                    int32_t request_id) {
  uint32_t page_index = params->page_index;
  const mojom::DidPrintContentParams& content = *params->content;
  if (page_index == kInvalidPageIndex ||
      !content.metafile_data_region.IsValid()) {
    return;
  }

  if (!OnPendingPreviewPage(page_index)) {
    receiver_.ReportBadMessage(kInvalidPageIndexForDidPreviewPage);
    return;
  }

  if (ShouldUseCompositor()) {
    // Don't bother compositing if this request has been cancelled already.
    if (ShouldCancelRequest(id_, request_id))
      return;

    WebContents* web_contents = GetInitiator(web_ui());
    if (!web_contents)
      return;

    auto* client = PrintCompositeClient::FromWebContents(web_contents);
    DCHECK(client);

    content::RenderFrameHost* render_frame_host =
        PrintViewManager::FromWebContents(web_contents)->print_preview_rfh();
    // |render_frame_host| could be null when the print preview dialog is
    // closed.
    if (!render_frame_host)
      return;

    // Use utility process to convert Skia metafile to PDF or XPS.
    client->CompositePage(
        params->document_cookie, render_frame_host, content,
        mojo::WrapCallbackWithDefaultInvokeIfNotRun(
            base::BindOnce(&PrintPreviewUI::OnCompositePdfPageDone,
                           weak_ptr_factory_.GetWeakPtr(), page_index,
                           params->document_cookie, request_id),
            mojom::PrintCompositor::Status::kCompositingFailure,
            base::ReadOnlySharedMemoryRegion()));
  } else {
    NotifyUIPreviewPageReady(
        page_index, request_id,
        base::RefCountedSharedMemoryMapping::CreateFromWholeRegion(
            content.metafile_data_region));
  }
}

void PrintPreviewUI::MetafileReadyForPrinting(
    mojom::DidPreviewDocumentParamsPtr params,
    int32_t request_id) {
  // Always try to stop the worker.
  StopWorker(params->document_cookie);

  const bool composite_document_using_individual_pages = ShouldUseCompositor();
  const base::ReadOnlySharedMemoryRegion& metafile =
      params->content->metafile_data_region;

  // When the Print Compositor is active, the print document is composed from
  // the individual pages, so |metafile| should be invalid.
  // When it is inactive, the print document is composed from |metafile|.
  // So if this comparison succeeds, that means the renderer sent bad data.
  if (composite_document_using_individual_pages == metafile.IsValid())
    return;

  if (params->expected_pages_count == 0) {
    receiver_.ReportBadMessage(kInvalidPageCountForMetafileReadyForPrinting);
    return;
  }

  if (composite_document_using_individual_pages) {
    // Don't bother compositing if this request has been cancelled already.
    if (ShouldCancelRequest(id_, request_id))
      return;

    auto callback = base::BindOnce(&PrintPreviewUI::OnCompositeToPdfDone,
                                   weak_ptr_factory_.GetWeakPtr(),
                                   params->document_cookie, request_id);

    WebContents* web_contents = GetInitiator(web_ui());
    if (!web_contents)
      return;

    // Page metafile is used to composite into the document at same time.
    // Need to provide particulars of how many pages are required before
    // document will be completed.
    auto* client = PrintCompositeClient::FromWebContents(web_contents);
    client->FinishDocumentComposition(
        params->document_cookie, params->expected_pages_count,
        mojo::WrapCallbackWithDefaultInvokeIfNotRun(
            std::move(callback),
            mojom::PrintCompositor::Status::kCompositingFailure,
            base::ReadOnlySharedMemoryRegion()));
  } else {
    NotifyUIPreviewDocumentReady(
        request_id,
        base::RefCountedSharedMemoryMapping::CreateFromWholeRegion(metafile));
  }
}

void PrintPreviewUI::PrintPreviewFailed(int32_t document_cookie,
                                        int32_t request_id) {
  StopWorker(document_cookie);
  if (ShouldCancelRequest(id_, request_id))
    return;
  OnPrintPreviewFailed(request_id);
}

void PrintPreviewUI::PrintPreviewCancelled(int32_t document_cookie,
                                           int32_t request_id) {
  // Always need to stop the worker.
  StopWorker(document_cookie);
  if (ShouldCancelRequest(id_, request_id))
    return;
  handler_->OnPrintPreviewCancelled(request_id);
}

void PrintPreviewUI::PrinterSettingsInvalid(int32_t document_cookie,
                                            int32_t request_id) {
  StopWorker(document_cookie);
  if (ShouldCancelRequest(id_, request_id))
    return;
  handler_->OnInvalidPrinterSettings(request_id);
}

// static
void PrintPreviewUI::SetDelegateForTesting(TestDelegate* delegate) {
  g_test_delegate = delegate;
}

void PrintPreviewUI::SetSelectedFileForTesting(const base::FilePath& path) {
  handler_->FileSelectedForTesting(path, 0);  // IN-TEST
}

void PrintPreviewUI::SetPdfSavedClosureForTesting(base::OnceClosure closure) {
  handler_->SetPdfSavedClosureForTesting(std::move(closure));
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
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!id_);

  id_ = g_print_preview_ui_id_map.Get().Add(this);
  GetPrintPreviewRequestIdMap()[*id_] = -1;
}

}  // namespace printing
