// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/print_preview_handler.h"

#include <ctype.h>
#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/i18n/number_formatting.h"
#include "base/json/json_reader.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/metrics/histogram_macros.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/bad_message.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/printing/background_printing_manager.h"
#include "chrome/browser/printing/print_dialog_cloud.h"
#include "chrome/browser/printing/print_error_dialog.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "chrome/browser/printing/print_preview_dialog_controller.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/printing/printer_manager_dialog.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/gaia_cookie_manager_service_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/webui/print_preview/pdf_printer_handler.h"
#include "chrome/browser/ui/webui/print_preview/policy_settings.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_ui.h"
#include "chrome/browser/ui/webui/print_preview/printer_handler.h"
#include "chrome/browser/ui/webui/print_preview/sticky_settings.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/cloud_print/cloud_print_constants.h"
#include "chrome/common/crash_keys.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "components/cloud_devices/common/cloud_device_description.h"
#include "components/cloud_devices/common/cloud_devices_urls.h"
#include "components/cloud_devices/common/printer_description.h"
#include "components/prefs/pref_service.h"
#include "components/printing/common/cloud_print_cdd_conversion.h"
#include "components/printing/common/print_messages.h"
#include "components/printing/common/printer_capabilities.h"
#include "components/signin/core/browser/gaia_cookie_manager_service.h"
#include "components/signin/core/browser/profile_management_switches.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "net/base/url_util.h"
#include "printing/backend/print_backend.h"
#include "printing/backend/print_backend_consts.h"
#include "printing/buildflags/buildflags.h"
#include "printing/print_settings.h"
#include "third_party/icu/source/i18n/unicode/ulocdata.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/settings/device_oauth2_token_service.h"
#include "chrome/browser/chromeos/settings/device_oauth2_token_service_factory.h"
#include "chromeos/printing/printer_configuration.h"
#endif

using content::RenderFrameHost;
using content::WebContents;
using printing::PrintViewManager;
using printing::PrinterType;

namespace {

// Max size for PDFs sent to Cloud Print. Server side limit is currently 80MB
// but PDF will double in size when sent to JS. See crbug.com/793506 and
// crbug.com/372240.
constexpr size_t kMaxCloudPrintPdfDataSizeInBytes = 80 * 1024 * 1024 / 2;

// This enum is used to back an UMA histogram, and should therefore be treated
// as append only.
enum UserActionBuckets {
  PRINT_TO_PRINTER,
  PRINT_TO_PDF,
  CANCEL,
  FALLBACK_TO_ADVANCED_SETTINGS_DIALOG,
  PREVIEW_FAILED,
  PREVIEW_STARTED,
  INITIATOR_CRASHED_UNUSED,
  INITIATOR_CLOSED,
  PRINT_WITH_CLOUD_PRINT,
  PRINT_WITH_PRIVET,
  PRINT_WITH_EXTENSION,
  OPEN_IN_MAC_PREVIEW,
  USERACTION_BUCKET_BOUNDARY
};

// This enum is used to back an UMA histogram, and should therefore be treated
// as append only.
enum PrintSettingsBuckets {
  LANDSCAPE = 0,
  PORTRAIT,
  COLOR,
  BLACK_AND_WHITE,
  COLLATE,
  SIMPLEX,
  DUPLEX,
  TOTAL,
  HEADERS_AND_FOOTERS,
  CSS_BACKGROUND,
  SELECTION_ONLY,
  EXTERNAL_PDF_PREVIEW_UNUSED,
  PAGE_RANGE,
  DEFAULT_MEDIA,
  NON_DEFAULT_MEDIA,
  COPIES,
  NON_DEFAULT_MARGINS,
  DISTILL_PAGE_UNUSED,
  SCALING,
  PRINT_AS_IMAGE,
  PAGES_PER_SHEET,
  FIT_TO_PAGE,
  DEFAULT_DPI,
  NON_DEFAULT_DPI,
  PRINT_SETTINGS_BUCKET_BOUNDARY
};

// This enum is used to back an UMA histogram, and should therefore be treated
// as append only.
enum PrintDocumentTypeBuckets {
  HTML_DOCUMENT = 0,
  PDF_DOCUMENT,
  PRINT_DOCUMENT_TYPE_BUCKET_BOUNDARY
};

void ReportUserActionHistogram(UserActionBuckets event) {
  UMA_HISTOGRAM_ENUMERATION("PrintPreview.UserAction", event,
                            USERACTION_BUCKET_BOUNDARY);
}

void ReportPrintSettingHistogram(PrintSettingsBuckets setting) {
  UMA_HISTOGRAM_ENUMERATION("PrintPreview.PrintSettings", setting,
                            PRINT_SETTINGS_BUCKET_BOUNDARY);
}

void ReportPrintDocumentTypeAndSizeHistograms(PrintDocumentTypeBuckets doctype,
                                              size_t average_page_size_in_kb) {
  UMA_HISTOGRAM_ENUMERATION("PrintPreview.PrintDocumentType", doctype,
                            PRINT_DOCUMENT_TYPE_BUCKET_BOUNDARY);
  switch (doctype) {
    case HTML_DOCUMENT:
      UMA_HISTOGRAM_MEMORY_KB("PrintPreview.PrintDocumentSize.HTML",
                              average_page_size_in_kb);
      break;
    case PDF_DOCUMENT:
      UMA_HISTOGRAM_MEMORY_KB("PrintPreview.PrintDocumentSize.PDF",
                              average_page_size_in_kb);
      break;
    default:
      NOTREACHED();
      break;
  }
}

bool ReportPageCountHistogram(UserActionBuckets user_action, int page_count) {
  switch (user_action) {
    case PRINT_TO_PRINTER:
      UMA_HISTOGRAM_COUNTS_1M("PrintPreview.PageCount.PrintToPrinter",
                              page_count);
      return true;
    case PRINT_TO_PDF:
      UMA_HISTOGRAM_COUNTS_1M("PrintPreview.PageCount.PrintToPDF", page_count);
      return true;
    case FALLBACK_TO_ADVANCED_SETTINGS_DIALOG:
      UMA_HISTOGRAM_COUNTS_1M("PrintPreview.PageCount.SystemDialog",
                              page_count);
      return true;
    case PRINT_WITH_CLOUD_PRINT:
      UMA_HISTOGRAM_COUNTS_1M("PrintPreview.PageCount.PrintToCloudPrint",
                              page_count);
      return true;
    case PRINT_WITH_PRIVET:
      UMA_HISTOGRAM_COUNTS_1M("PrintPreview.PageCount.PrintWithPrivet",
                              page_count);
      return true;
    case PRINT_WITH_EXTENSION:
      UMA_HISTOGRAM_COUNTS_1M("PrintPreview.PageCount.PrintWithExtension",
                              page_count);
      return true;
    case OPEN_IN_MAC_PREVIEW:
      UMA_HISTOGRAM_COUNTS_1M("PrintPreview.PageCount.OpenInMacPreview",
                              page_count);
      return true;
    default:
      return false;
  }
}

PrinterType GetPrinterTypeForUserAction(UserActionBuckets user_action) {
  switch (user_action) {
    case PRINT_WITH_PRIVET:
      return PrinterType::kPrivetPrinter;
    case PRINT_WITH_EXTENSION:
      return PrinterType::kExtensionPrinter;
    case PRINT_TO_PDF:
      return PrinterType::kPdfPrinter;
    case PRINT_TO_PRINTER:
    case FALLBACK_TO_ADVANCED_SETTINGS_DIALOG:
    case OPEN_IN_MAC_PREVIEW:
      return PrinterType::kLocalPrinter;
    default:
      NOTREACHED();
      return PrinterType::kLocalPrinter;
  }
}

base::Value GetErrorValue(UserActionBuckets user_action,
                          base::StringPiece description) {
  return user_action == PRINT_WITH_PRIVET ? base::Value(-1)
                                          : base::Value(description);
}

// Dictionary Fields for Print Preview initial settings. Keep in sync with
// field names for print_preview.NativeInitialSettings in
// chrome/browser/resources/print_preview/native_layer.js
//
// Name of a dictionary field specifying whether to print automatically in
// kiosk mode. See http://crbug.com/31395.
const char kIsInKioskAutoPrintMode[] = "isInKioskAutoPrintMode";
// Dictionary field to indicate whether Chrome is running in forced app (app
// kiosk) mode. It's not the same as desktop Chrome kiosk (the one above).
const char kIsInAppKioskMode[] = "isInAppKioskMode";
// Name of a dictionary field holding the thousands delimeter according to the
// locale.
const char kThousandsDelimeter[] = "thousandsDelimeter";
// Name of a dictionary field holding the decimal delimeter according to the
// locale.
const char kDecimalDelimeter[] = "decimalDelimeter";
// Name of a dictionary field holding the measurement system according to the
// locale.
const char kUnitType[] = "unitType";
// Name of a dictionary field holding the initiator title.
const char kDocumentTitle[] = "documentTitle";
// Name of a dictionary field holding the state of selection for document.
const char kDocumentHasSelection[] = "documentHasSelection";
// Name of a dictionary field holding saved print preview state
const char kAppState[] = "serializedAppStateStr";
// Name of a dictionary field holding the default destination selection rules.
const char kDefaultDestinationSelectionRules[] =
    "serializedDefaultDestinationSelectionRulesStr";
// Name of a dictionary pref holding the default value for the header/footer
// checkbox. If set, takes priority over sticky settings.
const char kHeaderFooter[] = "headerFooter";
// Name of a dictionary field telling us whether the kPrintHeaderFooter pref is
// managed by an enterprise policy.
const char kIsHeaderFooterManaged[] = "isHeaderFooterManaged";

// Get the print job settings dictionary from |json_str|. Returns NULL on
// failure.
std::unique_ptr<base::DictionaryValue> GetSettingsDictionary(
    const std::string& json_str) {
  if (json_str.empty()) {
    NOTREACHED() << "Empty print job settings";
    return NULL;
  }
  std::unique_ptr<base::DictionaryValue> settings =
      base::DictionaryValue::From(base::JSONReader::Read(json_str));
  if (!settings) {
    NOTREACHED() << "Print job settings must be a dictionary.";
    return NULL;
  }

  if (settings->empty()) {
    NOTREACHED() << "Print job settings dictionary is empty";
    return NULL;
  }

  return settings;
}

// Track the popularity of print settings and report the stats.
void ReportPrintSettingsStats(const base::DictionaryValue& print_settings,
                              const base::DictionaryValue& preview_settings,
                              bool is_pdf) {
  ReportPrintSettingHistogram(TOTAL);

  // Print settings can be categorized into 2 groups: settings that are applied
  // via preview generation (page range, selection, headers/footers, background
  // graphics, scaling, layout, page size, pages per sheet, fit to page,
  // margins, rasterize), and settings that are applied at the printer (color,
  // duplex, copies, collate, dpi). The former should be captured from the most
  // recent preview request, as some of them are set to dummy values in the
  // print ticket. Similarly, settings applied at the printer should be pulled
  // from the print ticket, as they may have dummy values in the preview
  // request.
  const base::ListValue* page_range_array = NULL;
  if (preview_settings.GetList(printing::kSettingPageRange,
                               &page_range_array) &&
      !page_range_array->empty()) {
    ReportPrintSettingHistogram(PAGE_RANGE);
  }

  const base::DictionaryValue* media_size_value = NULL;
  if (preview_settings.GetDictionary(printing::kSettingMediaSize,
                                     &media_size_value) &&
      !media_size_value->empty()) {
    bool is_default = false;
    if (media_size_value->GetBoolean(printing::kSettingMediaSizeIsDefault,
                                     &is_default) &&
        is_default) {
      ReportPrintSettingHistogram(DEFAULT_MEDIA);
    } else {
      ReportPrintSettingHistogram(NON_DEFAULT_MEDIA);
    }
  }

  bool landscape = false;
  if (preview_settings.GetBoolean(printing::kSettingLandscape, &landscape))
    ReportPrintSettingHistogram(landscape ? LANDSCAPE : PORTRAIT);

  int copies = 1;
  if (print_settings.GetInteger(printing::kSettingCopies, &copies) &&
      copies > 1) {
    ReportPrintSettingHistogram(COPIES);
  }

  int scaling = 100;
  if (preview_settings.GetInteger(printing::kSettingScaleFactor, &scaling) &&
      scaling != 100) {
    ReportPrintSettingHistogram(SCALING);
  }

  int pages_per_sheet = 1;
  if (preview_settings.GetInteger(printing::kSettingPagesPerSheet,
                                  &pages_per_sheet) &&
      pages_per_sheet != 1) {
    ReportPrintSettingHistogram(PAGES_PER_SHEET);
  }

  bool collate = false;
  if (print_settings.GetBoolean(printing::kSettingCollate, &collate) && collate)
    ReportPrintSettingHistogram(COLLATE);

  int duplex_mode = 0;
  if (print_settings.GetInteger(printing::kSettingDuplexMode, &duplex_mode))
    ReportPrintSettingHistogram(duplex_mode ? DUPLEX : SIMPLEX);

  int color_mode = 0;
  if (print_settings.GetInteger(printing::kSettingColor, &color_mode)) {
    ReportPrintSettingHistogram(
        printing::IsColorModelSelected(color_mode) ? COLOR : BLACK_AND_WHITE);
  }

  int margins_type = 0;
  if (preview_settings.GetInteger(printing::kSettingMarginsType,
                                  &margins_type) &&
      margins_type != 0) {
    ReportPrintSettingHistogram(NON_DEFAULT_MARGINS);
  }

  bool headers = false;
  if (preview_settings.GetBoolean(printing::kSettingHeaderFooterEnabled,
                                  &headers) &&
      headers) {
    ReportPrintSettingHistogram(HEADERS_AND_FOOTERS);
  }

  bool css_background = false;
  if (preview_settings.GetBoolean(printing::kSettingShouldPrintBackgrounds,
                                  &css_background) &&
      css_background) {
    ReportPrintSettingHistogram(CSS_BACKGROUND);
  }

  bool selection_only = false;
  if (preview_settings.GetBoolean(printing::kSettingShouldPrintSelectionOnly,
                                  &selection_only) &&
      selection_only) {
    ReportPrintSettingHistogram(SELECTION_ONLY);
  }

  bool rasterize = false;
  if (preview_settings.GetBoolean(printing::kSettingRasterizePdf, &rasterize) &&
      rasterize) {
    ReportPrintSettingHistogram(PRINT_AS_IMAGE);
  }

  bool fit_to_page = false;
  if (is_pdf &&
      preview_settings.GetBoolean(printing::kSettingFitToPageEnabled,
                                  &fit_to_page) &&
      fit_to_page) {
    ReportPrintSettingHistogram(FIT_TO_PAGE);
  }

  int dpi_horizontal = 0;
  int dpi_vertical = 0;
  if (print_settings.GetInteger(printing::kSettingDpiHorizontal,
                                &dpi_horizontal) &&
      print_settings.GetInteger(printing::kSettingDpiVertical, &dpi_vertical) &&
      dpi_horizontal > 0 && dpi_vertical > 0) {
    bool is_default = false;
    if (print_settings.GetBoolean(printing::kSettingDpiDefault, &is_default))
      ReportPrintSettingHistogram(is_default ? DEFAULT_DPI : NON_DEFAULT_DPI);
  }
}

UserActionBuckets DetermineUserAction(const base::DictionaryValue& settings) {
  bool value = false;
#if defined(OS_MACOSX)
  value = settings.HasKey(printing::kSettingOpenPDFInPreview);
#endif
  if (value)
    return OPEN_IN_MAC_PREVIEW;
  if (settings.HasKey(printing::kSettingCloudPrintId))
    return PRINT_WITH_CLOUD_PRINT;
  settings.GetBoolean(printing::kSettingPrintWithPrivet, &value);
  if (value)
    return PRINT_WITH_PRIVET;
  settings.GetBoolean(printing::kSettingPrintWithExtension, &value);
  if (value)
    return PRINT_WITH_EXTENSION;
  settings.GetBoolean(printing::kSettingPrintToPDF, &value);
  if (value)
    return PRINT_TO_PDF;
  settings.GetBoolean(printing::kSettingShowSystemDialog, &value);
  if (value)
    return FALLBACK_TO_ADVANCED_SETTINGS_DIALOG;
  return PRINT_TO_PRINTER;
}

base::LazyInstance<printing::StickySettings>::DestructorAtExit
    g_sticky_settings = LAZY_INSTANCE_INITIALIZER;

printing::StickySettings* GetStickySettings() {
  return g_sticky_settings.Pointer();
}

}  // namespace

class PrintPreviewHandler::AccessTokenService
    : public OAuth2TokenService::Consumer {
 public:
  explicit AccessTokenService(PrintPreviewHandler* handler)
      : OAuth2TokenService::Consumer("print_preview"),
        handler_(handler) {
  }

  void RequestToken(const std::string& type, const std::string& callback_id) {
    if (requests_.find(type) != requests_.end()) {
      NOTREACHED();  // Should never happen, see cloud_print_interface.js
      return;
    }

    OAuth2TokenService* service = nullptr;
    std::string account_id;
    if (type == "profile") {
      Profile* profile = Profile::FromWebUI(handler_->web_ui());
      if (profile) {
        ProfileOAuth2TokenService* token_service =
            ProfileOAuth2TokenServiceFactory::GetForProfile(profile);
        SigninManagerBase* signin_manager =
            SigninManagerFactory::GetInstance()->GetForProfile(profile);
        account_id = signin_manager->GetAuthenticatedAccountId();
        service = token_service;
      }
    } else if (type == "device") {
#if defined(OS_CHROMEOS)
      chromeos::DeviceOAuth2TokenService* token_service =
          chromeos::DeviceOAuth2TokenServiceFactory::Get();
      account_id = token_service->GetRobotAccountId();
      service = token_service;
#endif
    }

    if (!service) {
      // Unknown type.
      handler_->SendAccessToken(callback_id, std::string());
      return;
    }

    OAuth2TokenService::ScopeSet oauth_scopes;
    oauth_scopes.insert(cloud_devices::kCloudPrintAuthScope);
    requests_[type].request =
        service->StartRequest(account_id, oauth_scopes, this);
    requests_[type].callback_id = callback_id;
  }

  void OnGetTokenSuccess(
      const OAuth2TokenService::Request* request,
      const OAuth2AccessTokenConsumer::TokenResponse& token_response) override {
    OnServiceResponse(request, token_response.access_token);
  }

  void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                         const GoogleServiceAuthError& error) override {
    OnServiceResponse(request, std::string());
  }

 private:
  void OnServiceResponse(const OAuth2TokenService::Request* request,
                         const std::string& access_token) {
    for (auto it = requests_.begin(); it != requests_.end(); ++it) {
      auto& entry = it->second;
      if (entry.request.get() == request) {
        handler_->SendAccessToken(entry.callback_id, access_token);
        requests_.erase(it);
        return;
      }
    }
    NOTREACHED();
  }

  struct Request {
    std::unique_ptr<OAuth2TokenService::Request> request;
    std::string callback_id;
  };
  // Maps types to Requests.
  base::flat_map<std::string, Request> requests_;

  PrintPreviewHandler* const handler_;

  DISALLOW_COPY_AND_ASSIGN(AccessTokenService);
};

PrintPreviewHandler::PrintPreviewHandler()
    : regenerate_preview_request_count_(0),
      manage_printers_dialog_request_count_(0),
      reported_failed_preview_(false),
      has_logged_printers_count_(false),
      gaia_cookie_manager_service_(nullptr),
      weak_factory_(this) {
  ReportUserActionHistogram(PREVIEW_STARTED);
}

PrintPreviewHandler::~PrintPreviewHandler() {
  UMA_HISTOGRAM_COUNTS_1M("PrintPreview.ManagePrinters",
                          manage_printers_dialog_request_count_);
  UnregisterForGaiaCookieChanges();
}

void PrintPreviewHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getPrinters",
      base::BindRepeating(&PrintPreviewHandler::HandleGetPrinters,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getPreview", base::BindRepeating(&PrintPreviewHandler::HandleGetPreview,
                                        base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "print", base::BindRepeating(&PrintPreviewHandler::HandlePrint,
                                   base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getPrinterCapabilities",
      base::BindRepeating(&PrintPreviewHandler::HandleGetPrinterCapabilities,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setupPrinter",
      base::BindRepeating(&PrintPreviewHandler::HandlePrinterSetup,
                          base::Unretained(this)));
#if BUILDFLAG(ENABLE_BASIC_PRINT_DIALOG)
  web_ui()->RegisterMessageCallback(
      "showSystemDialog",
      base::BindRepeating(&PrintPreviewHandler::HandleShowSystemDialog,
                          base::Unretained(this)));
#endif
  web_ui()->RegisterMessageCallback(
      "signIn", base::BindRepeating(&PrintPreviewHandler::HandleSignin,
                                    base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getAccessToken",
      base::BindRepeating(&PrintPreviewHandler::HandleGetAccessToken,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "managePrinters",
      base::BindRepeating(&PrintPreviewHandler::HandleManagePrinters,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "closePrintPreviewDialog",
      base::BindRepeating(&PrintPreviewHandler::HandleClosePreviewDialog,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "hidePreview",
      base::BindRepeating(&PrintPreviewHandler::HandleHidePreview,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "cancelPendingPrintRequest",
      base::BindRepeating(&PrintPreviewHandler::HandleCancelPendingPrintRequest,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "saveAppState",
      base::BindRepeating(&PrintPreviewHandler::HandleSaveAppState,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getInitialSettings",
      base::BindRepeating(&PrintPreviewHandler::HandleGetInitialSettings,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "forceOpenNewTab",
      base::BindRepeating(&PrintPreviewHandler::HandleForceOpenNewTab,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "grantExtensionPrinterAccess",
      base::BindRepeating(
          &PrintPreviewHandler::HandleGrantExtensionPrinterAccess,
          base::Unretained(this)));
}

void PrintPreviewHandler::OnJavascriptAllowed() {
  // Now that the UI is initialized, any future account changes will require
  // a printer list refresh.
  RegisterForGaiaCookieChanges();
}

void PrintPreviewHandler::OnJavascriptDisallowed() {
  // Normally the handler and print preview will be destroyed together, but
  // this is necessary for refresh or navigation from the chrome://print page.
  weak_factory_.InvalidateWeakPtrs();
  preview_callbacks_.clear();
  preview_failures_.clear();
  UnregisterForGaiaCookieChanges();
}

WebContents* PrintPreviewHandler::preview_web_contents() const {
  return web_ui()->GetWebContents();
}

PrefService* PrintPreviewHandler::GetPrefs() const {
  auto* prefs = Profile::FromWebUI(web_ui())->GetPrefs();
  DCHECK(prefs);
  return prefs;
}

PrintPreviewUI* PrintPreviewHandler::print_preview_ui() const {
  return static_cast<PrintPreviewUI*>(web_ui()->GetController());
}

bool PrintPreviewHandler::ShouldReceiveRendererMessage(int request_id) {
  if (!IsJavascriptAllowed()) {
    BadMessageReceived();
    return false;
  }

  if (!base::ContainsKey(preview_callbacks_, request_id)) {
    BadMessageReceived();
    return false;
  }

  return true;
}

std::string PrintPreviewHandler::GetCallbackId(int request_id) {
  std::string result;
  if (!IsJavascriptAllowed()) {
    BadMessageReceived();
    return result;
  }

  auto it = preview_callbacks_.find(request_id);
  if (it == preview_callbacks_.end()) {
    BadMessageReceived();
    return result;
  }
  result = it->second;
  preview_callbacks_.erase(it);
  return result;
}

void PrintPreviewHandler::HandleGetPrinters(const base::ListValue* args) {
  std::string callback_id;
  CHECK(args->GetString(0, &callback_id));
  CHECK(!callback_id.empty());
  int type;
  CHECK(args->GetInteger(1, &type));
  PrinterType printer_type = static_cast<PrinterType>(type);

  PrinterHandler* handler = GetPrinterHandler(printer_type);
  if (!handler) {
    RejectJavascriptCallback(base::Value(callback_id), base::Value());
    return;
  }
  // Make sure all in progress requests are canceled before new printer search
  // starts.
  handler->Reset();
  handler->StartGetPrinters(
      base::Bind(&PrintPreviewHandler::OnAddedPrinters,
                 weak_factory_.GetWeakPtr(), printer_type),
      base::Bind(&PrintPreviewHandler::OnGetPrintersDone,
                 weak_factory_.GetWeakPtr(), callback_id));
}

void PrintPreviewHandler::HandleGrantExtensionPrinterAccess(
    const base::ListValue* args) {
  std::string callback_id;
  std::string printer_id;
  bool ok = args->GetString(0, &callback_id) &&
            args->GetString(1, &printer_id) && !callback_id.empty();
  DCHECK(ok);

  GetPrinterHandler(PrinterType::kExtensionPrinter)
      ->StartGrantPrinterAccess(
          printer_id,
          base::Bind(&PrintPreviewHandler::OnGotExtensionPrinterInfo,
                     weak_factory_.GetWeakPtr(), callback_id));
}

void PrintPreviewHandler::HandleGetPrinterCapabilities(
    const base::ListValue* args) {
  std::string callback_id;
  std::string printer_name;
  int type;
  if (!args->GetString(0, &callback_id) || !args->GetString(1, &printer_name) ||
      !args->GetInteger(2, &type) || callback_id.empty() ||
      printer_name.empty()) {
    RejectJavascriptCallback(base::Value(callback_id), base::Value());
    return;
  }
  PrinterType printer_type = static_cast<PrinterType>(type);

  PrinterHandler* handler = GetPrinterHandler(printer_type);
  if (!handler) {
    RejectJavascriptCallback(base::Value(callback_id), base::Value());
    return;
  }

  handler->StartGetCapability(
      printer_name,
      base::BindOnce(&PrintPreviewHandler::SendPrinterCapabilities,
                     weak_factory_.GetWeakPtr(), callback_id));
}

void PrintPreviewHandler::HandleGetPreview(const base::ListValue* args) {
  DCHECK_EQ(2U, args->GetSize());
  std::string callback_id;
  std::string json_str;

  // All of the conditions below should be guaranteed by the print preview
  // javascript.
  args->GetString(0, &callback_id);
  CHECK(!callback_id.empty());
  args->GetString(1, &json_str);
  std::unique_ptr<base::DictionaryValue> settings =
      GetSettingsDictionary(json_str);
  CHECK(settings);
  int request_id = -1;
  settings->GetInteger(printing::kPreviewRequestID, &request_id);
  CHECK_GT(request_id, -1);

  CHECK(!base::ContainsKey(preview_callbacks_, request_id));
  preview_callbacks_[request_id] = callback_id;
  print_preview_ui()->OnPrintPreviewRequest(request_id);
  // Add an additional key in order to identify |print_preview_ui| later on
  // when calling PrintPreviewUI::ShouldCancelRequest() on the IO thread.
  settings->SetInteger(printing::kPreviewUIID,
                       print_preview_ui()->GetIDForPrintPreviewUI());

  // Increment request count.
  ++regenerate_preview_request_count_;

  WebContents* initiator = GetInitiator();
  RenderFrameHost* rfh =
      initiator
          ? PrintViewManager::FromWebContents(initiator)->print_preview_rfh()
          : nullptr;
  if (!rfh) {
    ReportUserActionHistogram(INITIATOR_CLOSED);
    print_preview_ui()->OnClosePrintPreviewDialog();
    return;
  }

  // Retrieve the page title and url and send it to the renderer process if
  // headers and footers are to be displayed.
  bool display_header_footer = false;
  bool success = settings->GetBoolean(printing::kSettingHeaderFooterEnabled,
                                      &display_header_footer);
  DCHECK(success);
  if (display_header_footer) {
    settings->SetString(printing::kSettingHeaderFooterTitle,
                        initiator->GetTitle());

    url::Replacements<char> url_sanitizer;
    url_sanitizer.ClearUsername();
    url_sanitizer.ClearPassword();
    const GURL& initiator_url = initiator->GetLastCommittedURL();
    settings->SetString(printing::kSettingHeaderFooterURL,
                        initiator_url.ReplaceComponents(url_sanitizer).spec());
  }

  VLOG(1) << "Print preview request start";

  rfh->Send(new PrintMsg_PrintPreview(rfh->GetRoutingID(), *settings));
  last_preview_settings_ = std::move(settings);
}

void PrintPreviewHandler::HandlePrint(const base::ListValue* args) {
  // Record the number of times the user requests to regenerate preview data
  // before printing.
  UMA_HISTOGRAM_COUNTS_1M("PrintPreview.RegeneratePreviewRequest.BeforePrint",
                          regenerate_preview_request_count_);
  std::string callback_id;
  CHECK(args->GetString(0, &callback_id));
  CHECK(!callback_id.empty());
  std::string json_str;
  CHECK(args->GetString(1, &json_str));

  std::unique_ptr<base::DictionaryValue> settings =
      GetSettingsDictionary(json_str);
  if (!settings) {
    RejectJavascriptCallback(base::Value(callback_id), base::Value(-1));
    return;
  }

  const UserActionBuckets user_action = DetermineUserAction(*settings);

  int page_count = 0;
  if (!settings->GetInteger(printing::kSettingPreviewPageCount, &page_count) ||
      page_count <= 0) {
    RejectJavascriptCallback(base::Value(callback_id),
                             GetErrorValue(user_action, "NO_PAGE_COUNT"));
    return;
  }

  scoped_refptr<base::RefCountedMemory> data;
  print_preview_ui()->GetPrintPreviewDataForIndex(
      printing::COMPLETE_PREVIEW_DOCUMENT_INDEX, &data);
  if (!data) {
    // Nothing to print, no preview available.
    RejectJavascriptCallback(base::Value(callback_id),
                             GetErrorValue(user_action, "NO_DATA"));
    return;
  }
  DCHECK(data->size());
  DCHECK(data->front());

  std::string destination_id;
  std::string print_ticket;
  std::string capabilities;
  int width = 0;
  int height = 0;
  if (user_action == PRINT_WITH_PRIVET || user_action == PRINT_WITH_EXTENSION) {
    if (!settings->GetString(printing::kSettingDeviceName, &destination_id) ||
        !settings->GetString(printing::kSettingTicket, &print_ticket) ||
        !settings->GetString(printing::kSettingCapabilities, &capabilities) ||
        !settings->GetInteger(printing::kSettingPageWidth, &width) ||
        !settings->GetInteger(printing::kSettingPageHeight, &height) ||
        width <= 0 || height <= 0) {
      NOTREACHED();
      RejectJavascriptCallback(base::Value(callback_id),
                               GetErrorValue(user_action, "FAILED"));
      return;
    }
  }

  // After validating |settings|, record metrics.
  bool is_pdf = !print_preview_ui()->source_is_modifiable();
  if (last_preview_settings_)
    ReportPrintSettingsStats(*settings, *last_preview_settings_, is_pdf);
  {
    PrintDocumentTypeBuckets doc_type = is_pdf ? PDF_DOCUMENT : HTML_DOCUMENT;
    size_t average_page_size_in_kb = data->size() / page_count;
    average_page_size_in_kb /= 1024;
    ReportPrintDocumentTypeAndSizeHistograms(doc_type, average_page_size_in_kb);
  }
  ReportUserActionHistogram(user_action);
  if (!ReportPageCountHistogram(user_action, page_count)) {
    NOTREACHED();
    return;
  }

  if (user_action == PRINT_WITH_CLOUD_PRINT) {
    // Does not send the title like the other printer handler types below,
    // because JS already has the document title from the initial settings.
    SendCloudPrintJob(callback_id, data.get());
    return;
  }

  PrinterType type = GetPrinterTypeForUserAction(user_action);
  PrinterHandler* handler = GetPrinterHandler(type);
  handler->StartPrint(
      destination_id, capabilities, print_preview_ui()->initiator_title(),
      type == PrinterType::kLocalPrinter ? json_str : print_ticket,
      gfx::Size(width, height), data,
      base::BindOnce(&PrintPreviewHandler::OnPrintResult,
                     weak_factory_.GetWeakPtr(), callback_id));
}

void PrintPreviewHandler::HandleHidePreview(const base::ListValue* /*args*/) {
  print_preview_ui()->OnHidePreviewDialog();
}

void PrintPreviewHandler::HandleCancelPendingPrintRequest(
    const base::ListValue* /*args*/) {
  WebContents* initiator = GetInitiator();
  if (initiator)
    ClearInitiatorDetails();
  ShowPrintErrorDialog();
}

void PrintPreviewHandler::HandleSaveAppState(const base::ListValue* args) {
  std::string data_to_save;
  printing::StickySettings* sticky_settings = GetStickySettings();
  if (args->GetString(0, &data_to_save) && !data_to_save.empty())
    sticky_settings->StoreAppState(data_to_save);
  sticky_settings->SaveInPrefs(GetPrefs());
}

// |args| is expected to contain a string with representing the callback id
// followed by a list of arguments the first of which should be the printer id.
void PrintPreviewHandler::HandlePrinterSetup(const base::ListValue* args) {
  std::string callback_id;
  std::string printer_name;
  if (!args->GetString(0, &callback_id) || !args->GetString(1, &printer_name) ||
      callback_id.empty() || printer_name.empty()) {
    RejectJavascriptCallback(base::Value(callback_id),
                             base::Value(printer_name));
    return;
  }

  GetPrinterHandler(PrinterType::kLocalPrinter)
      ->StartGetCapability(
          printer_name, base::BindOnce(&PrintPreviewHandler::SendPrinterSetup,
                                       weak_factory_.GetWeakPtr(), callback_id,
                                       printer_name));
}

void PrintPreviewHandler::OnSigninComplete(const std::string& callback_id) {
  ResolveJavascriptCallback(base::Value(callback_id), base::Value());
}

void PrintPreviewHandler::HandleSignin(const base::ListValue* args) {
  std::string callback_id;
  bool add_account = false;
  CHECK(args->GetString(0, &callback_id));
  CHECK(!callback_id.empty());
  CHECK(args->GetBoolean(1, &add_account));

  chrome::ScopedTabbedBrowserDisplayer displayer(Profile::FromWebUI(web_ui()));
  print_dialog_cloud::CreateCloudPrintSigninTab(
      displayer.browser(), add_account,
      base::Bind(&PrintPreviewHandler::OnSigninComplete,
                 weak_factory_.GetWeakPtr(), callback_id));
}

void PrintPreviewHandler::HandleGetAccessToken(const base::ListValue* args) {
  std::string callback_id;
  std::string type;

  bool ok = args->GetString(0, &callback_id) && args->GetString(1, &type) &&
            !callback_id.empty();
  DCHECK(ok);

  if (!token_service_)
    token_service_ = std::make_unique<AccessTokenService>(this);
  token_service_->RequestToken(type, callback_id);
}

// TODO (rbpotter): Remove this when the old Print Preview page is deleted.
void PrintPreviewHandler::HandleManagePrinters(const base::ListValue* args) {
  GURL local_printers_manage_url(
      chrome::GetSettingsUrl(chrome::kPrintingSettingsSubPage));
  preview_web_contents()->OpenURL(
      content::OpenURLParams(local_printers_manage_url, content::Referrer(),
                             WindowOpenDisposition::NEW_FOREGROUND_TAB,
                             ui::PAGE_TRANSITION_LINK, false));
}

#if BUILDFLAG(ENABLE_BASIC_PRINT_DIALOG)
void PrintPreviewHandler::HandleShowSystemDialog(
    const base::ListValue* /*args*/) {
  manage_printers_dialog_request_count_++;
  ReportUserActionHistogram(FALLBACK_TO_ADVANCED_SETTINGS_DIALOG);

  WebContents* initiator = GetInitiator();
  if (!initiator)
    return;

  auto* print_view_manager = PrintViewManager::FromWebContents(initiator);
  print_view_manager->PrintForSystemDialogNow(
      base::Bind(&PrintPreviewHandler::ClosePreviewDialog,
                 weak_factory_.GetWeakPtr()));

  // Cancel the pending preview request if exists.
  print_preview_ui()->OnCancelPendingPreviewRequest();
}
#endif

void PrintPreviewHandler::HandleClosePreviewDialog(
    const base::ListValue* /*args*/) {
  ReportUserActionHistogram(CANCEL);

  // Record the number of times the user requests to regenerate preview data
  // before cancelling.
  UMA_HISTOGRAM_COUNTS_1M("PrintPreview.RegeneratePreviewRequest.BeforeCancel",
                          regenerate_preview_request_count_);
}

void PrintPreviewHandler::GetNumberFormatAndMeasurementSystem(
    base::DictionaryValue* settings) {

  // Getting the measurement system based on the locale.
  UErrorCode errorCode = U_ZERO_ERROR;
  const char* locale = g_browser_process->GetApplicationLocale().c_str();
  UMeasurementSystem system = ulocdata_getMeasurementSystem(locale, &errorCode);
  // On error, assume the units are SI.
  // Since the only measurement units print preview's WebUI cares about are
  // those for measuring distance, assume anything non-US is SI.
  if (errorCode > U_ZERO_ERROR || system != UMS_US)
    system = UMS_SI;

  // Getting the number formatting based on the locale and writing to
  // dictionary.
  base::string16 number_format = base::FormatDouble(123456.78, 2);
  settings->SetString(kDecimalDelimeter, number_format.substr(7, 1));
  settings->SetString(kThousandsDelimeter, number_format.substr(3, 1));
  settings->SetInteger(kUnitType, system);
}

void PrintPreviewHandler::HandleGetInitialSettings(
    const base::ListValue* args) {
  std::string callback_id;
  CHECK(args->GetString(0, &callback_id));
  CHECK(!callback_id.empty());

  AllowJavascript();

  // Send before SendInitialSettings() to allow cloud printer auto select.
  SendCloudPrintEnabled();
  GetPrinterHandler(PrinterType::kLocalPrinter)
      ->GetDefaultPrinter(base::Bind(&PrintPreviewHandler::SendInitialSettings,
                                     weak_factory_.GetWeakPtr(), callback_id));
}

// TODO(rbpotter): Remove this when the old Print Preview page is deleted.
void PrintPreviewHandler::HandleForceOpenNewTab(const base::ListValue* args) {
  std::string url;
  if (!args->GetString(0, &url))
    return;
  Browser* browser = chrome::FindBrowserWithWebContents(GetInitiator());
  if (!browser)
    return;
  chrome::AddSelectedTabWithURL(browser,
                                GURL(url),
                                ui::PAGE_TRANSITION_LINK);
}

void PrintPreviewHandler::SendInitialSettings(
    const std::string& callback_id,
    const std::string& default_printer) {
  base::DictionaryValue initial_settings;
  initial_settings.SetString(kDocumentTitle,
                             print_preview_ui()->initiator_title());
  initial_settings.SetBoolean(printing::kSettingPreviewModifiable,
                              print_preview_ui()->source_is_modifiable());
  initial_settings.SetString(printing::kSettingPrinterName, default_printer);
  initial_settings.SetBoolean(kDocumentHasSelection,
                              print_preview_ui()->source_has_selection());
  initial_settings.SetBoolean(printing::kSettingShouldPrintSelectionOnly,
                              print_preview_ui()->print_selection_only());
  PrefService* prefs = GetPrefs();
  printing::StickySettings* sticky_settings = GetStickySettings();
  sticky_settings->RestoreFromPrefs(prefs);
  if (sticky_settings->printer_app_state()) {
    initial_settings.SetString(kAppState,
                               *sticky_settings->printer_app_state());
  } else {
    initial_settings.SetKey(kAppState, base::Value());
  }

  if (prefs->HasPrefPath(prefs::kPrintHeaderFooter)) {
    // Don't override sticky settings, unless kPrintHeaderFooter is actually
    // customized.
    initial_settings.SetBoolean(kHeaderFooter,
                                prefs->GetBoolean(prefs::kPrintHeaderFooter));
  }
  initial_settings.SetBoolean(
      kIsHeaderFooterManaged,
      prefs->IsManagedPreference(prefs::kPrintHeaderFooter));

  base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
  initial_settings.SetBoolean(kIsInKioskAutoPrintMode,
                              cmdline->HasSwitch(switches::kKioskModePrinting));
  initial_settings.SetBoolean(kIsInAppKioskMode,
                              chrome::IsRunningInForcedAppMode());
  const std::string rules_str =
      prefs->GetString(prefs::kPrintPreviewDefaultDestinationSelectionRules);
  if (rules_str.empty()) {
    initial_settings.SetKey(kDefaultDestinationSelectionRules, base::Value());
  } else {
    initial_settings.SetString(kDefaultDestinationSelectionRules, rules_str);
  }

  GetNumberFormatAndMeasurementSystem(&initial_settings);
  ResolveJavascriptCallback(base::Value(callback_id), initial_settings);
}

void PrintPreviewHandler::ClosePreviewDialog() {
  print_preview_ui()->OnClosePrintPreviewDialog();
}

void PrintPreviewHandler::SendAccessToken(const std::string& callback_id,
                                          const std::string& access_token) {
  VLOG(1) << "Get getAccessToken finished";
  ResolveJavascriptCallback(base::Value(callback_id),
                            base::Value(access_token));
}

void PrintPreviewHandler::SendPrinterCapabilities(
    const std::string& callback_id,
    std::unique_ptr<base::DictionaryValue> settings_info) {
  // Check that |settings_info| is valid.
  if (settings_info &&
      settings_info->FindKeyOfType(printing::kSettingCapabilities,
                                   base::Value::Type::DICTIONARY)) {
    VLOG(1) << "Get printer capabilities finished";
    ResolveJavascriptCallback(base::Value(callback_id), *settings_info);
    return;
  }

  VLOG(1) << "Get printer capabilities failed";
  RejectJavascriptCallback(base::Value(callback_id), base::Value());
}

void PrintPreviewHandler::SendPrinterSetup(
    const std::string& callback_id,
    const std::string& printer_name,
    std::unique_ptr<base::DictionaryValue> destination_info) {
  base::DictionaryValue response;
  base::Value* caps_value =
      destination_info
          ? destination_info->FindKeyOfType(printing::kSettingCapabilities,
                                            base::Value::Type::DICTIONARY)
          : nullptr;
  response.SetString("printerId", printer_name);
  response.SetBoolean("success", !!caps_value);
  response.SetKey("capabilities", caps_value ? std::move(*caps_value)
                                             : base::DictionaryValue());
  if (caps_value) {
    base::Value* printer = destination_info->FindKeyOfType(
        printing::kPrinter, base::Value::Type::DICTIONARY);
    if (printer) {
      base::Value* policies_value = printer->FindKeyOfType(
          printing::kSettingPolicies, base::Value::Type::DICTIONARY);
      if (policies_value)
        response.SetKey("policies", std::move(*policies_value));
    }
  } else {
    LOG(WARNING) << "Printer setup failed";
  }
  ResolveJavascriptCallback(base::Value(callback_id), response);
}

void PrintPreviewHandler::SendCloudPrintEnabled() {
  PrefService* prefs = GetPrefs();
  if (prefs->GetBoolean(prefs::kCloudPrintSubmitEnabled) &&
      !base::FeatureList::IsEnabled(features::kCloudPrinterHandler)) {
    FireWebUIListener(
        "use-cloud-print",
        base::Value(GURL(cloud_devices::GetCloudPrintURL()).spec()),
        base::Value(chrome::IsRunningInForcedAppMode()));
  }
}

void PrintPreviewHandler::SendCloudPrintJob(
    const std::string& callback_id,
    const base::RefCountedMemory* data) {
  // BASE64 encode the job data.
  const base::StringPiece raw_data(data->front_as<char>(), data->size());
  std::string base64_data;
  base::Base64Encode(raw_data, &base64_data);

  if (base64_data.size() >= kMaxCloudPrintPdfDataSizeInBytes) {
    RejectJavascriptCallback(base::Value(callback_id),
                             base::Value("OVERSIZED_PDF"));
    return;
  }
  ResolveJavascriptCallback(base::Value(callback_id), base::Value(base64_data));
}

WebContents* PrintPreviewHandler::GetInitiator() const {
  printing::PrintPreviewDialogController* dialog_controller =
      printing::PrintPreviewDialogController::GetInstance();
  if (!dialog_controller)
    return NULL;
  return dialog_controller->GetInitiator(preview_web_contents());
}

void PrintPreviewHandler::OnAddAccountToCookieCompleted(
    const std::string& account_id,
    const GoogleServiceAuthError& error) {
  FireWebUIListener("reload-printer-list");
}

void PrintPreviewHandler::OnPrintPreviewReady(int preview_uid, int request_id) {
  std::string callback_id = GetCallbackId(request_id);
  if (callback_id.empty())
    return;

  ResolveJavascriptCallback(base::Value(callback_id), base::Value(preview_uid));
}

void PrintPreviewHandler::OnPrintPreviewFailed(int request_id) {
  std::string callback_id = GetCallbackId(request_id);
  if (callback_id.empty())
    return;

  if (!reported_failed_preview_) {
    reported_failed_preview_ = true;
    ReportUserActionHistogram(PREVIEW_FAILED);
  }

  // Keep track of failures.
  bool inserted = preview_failures_.insert(request_id).second;
  DCHECK(inserted);
  RejectJavascriptCallback(base::Value(callback_id),
                           base::Value("PREVIEW_FAILED"));
}

void PrintPreviewHandler::OnInvalidPrinterSettings(int request_id) {
  std::string callback_id = GetCallbackId(request_id);
  if (callback_id.empty())
    return;

  RejectJavascriptCallback(base::Value(callback_id),
                           base::Value("SETTINGS_INVALID"));
}

void PrintPreviewHandler::SendPrintPresetOptions(bool disable_scaling,
                                                 int copies,
                                                 int duplex,
                                                 int request_id) {
  if (!ShouldReceiveRendererMessage(request_id))
    return;

  FireWebUIListener("print-preset-options", base::Value(disable_scaling),
                    base::Value(copies), base::Value(duplex));
}

void PrintPreviewHandler::SendPageCountReady(int page_count,
                                             int fit_to_page_scaling,
                                             int request_id) {
  if (!ShouldReceiveRendererMessage(request_id))
    return;

  FireWebUIListener("page-count-ready", base::Value(page_count),
                    base::Value(request_id), base::Value(fit_to_page_scaling));
}

void PrintPreviewHandler::SendPageLayoutReady(
    const base::DictionaryValue& layout,
    bool has_custom_page_size_style,
    int request_id) {
  if (!ShouldReceiveRendererMessage(request_id))
    return;

  FireWebUIListener("page-layout-ready", layout,
                    base::Value(has_custom_page_size_style));
}

void PrintPreviewHandler::SendPagePreviewReady(int page_index,
                                               int preview_uid,
                                               int preview_request_id) {
  // With print compositing, by the time compositing finishes and this method
  // gets called, the print preview may have failed. Since the failure message
  // may have arrived first, check for this case and bail out instead of
  // thinking this may be a bad IPC message.
  if (base::ContainsKey(preview_failures_, preview_request_id))
    return;

  if (!ShouldReceiveRendererMessage(preview_request_id))
    return;

  FireWebUIListener("page-preview-ready", base::Value(page_index),
                    base::Value(preview_uid), base::Value(preview_request_id));
}

void PrintPreviewHandler::OnPrintPreviewCancelled(int request_id) {
  std::string callback_id = GetCallbackId(request_id);
  if (callback_id.empty())
    return;

  RejectJavascriptCallback(base::Value(callback_id), base::Value("CANCELLED"));
}

void PrintPreviewHandler::OnPrintRequestCancelled() {
  HandleCancelPendingPrintRequest(nullptr);
}

void PrintPreviewHandler::ClearInitiatorDetails() {
  WebContents* initiator = GetInitiator();
  if (!initiator)
    return;

  // We no longer require the initiator details. Remove those details associated
  // with the preview dialog to allow the initiator to create another preview
  // dialog.
  printing::PrintPreviewDialogController* dialog_controller =
      printing::PrintPreviewDialogController::GetInstance();
  if (dialog_controller)
    dialog_controller->EraseInitiatorInfo(preview_web_contents());
}

PrinterHandler* PrintPreviewHandler::GetPrinterHandler(
    PrinterType printer_type) {
  if (printer_type == PrinterType::kExtensionPrinter) {
    if (!extension_printer_handler_) {
      extension_printer_handler_ = PrinterHandler::CreateForExtensionPrinters(
          Profile::FromWebUI(web_ui()));
    }
    return extension_printer_handler_.get();
  }
#if BUILDFLAG(ENABLE_SERVICE_DISCOVERY)
  if (printer_type == PrinterType::kPrivetPrinter) {
    if (!privet_printer_handler_) {
      privet_printer_handler_ =
          PrinterHandler::CreateForPrivetPrinters(Profile::FromWebUI(web_ui()));
    }
    return privet_printer_handler_.get();
  }
#endif
  if (printer_type == PrinterType::kPdfPrinter) {
    if (!pdf_printer_handler_) {
      pdf_printer_handler_ = PrinterHandler::CreateForPdfPrinter(
          Profile::FromWebUI(web_ui()), preview_web_contents(),
          GetStickySettings());
    }
    return pdf_printer_handler_.get();
  }
  if (printer_type == PrinterType::kLocalPrinter) {
    if (!local_printer_handler_) {
      local_printer_handler_ = PrinterHandler::CreateForLocalPrinters(
          preview_web_contents(), Profile::FromWebUI(web_ui()));
    }
    return local_printer_handler_.get();
  }
  if (printer_type == PrinterType::kCloudPrinter) {
    // This printer handler is currently experimental. Ensure it is never
    // created unless the flag is enabled.
    CHECK(base::FeatureList::IsEnabled(features::kCloudPrinterHandler));
    if (!cloud_printer_handler_)
      cloud_printer_handler_ = PrinterHandler::CreateForCloudPrinters();
    return cloud_printer_handler_.get();
  }
  NOTREACHED();
  return nullptr;
}

PdfPrinterHandler* PrintPreviewHandler::GetPdfPrinterHandler() {
  return static_cast<PdfPrinterHandler*>(
      GetPrinterHandler(PrinterType::kPdfPrinter));
}

void PrintPreviewHandler::OnAddedPrinters(printing::PrinterType printer_type,
                                          const base::ListValue& printers) {
  DCHECK(printer_type == PrinterType::kExtensionPrinter ||
         printer_type == PrinterType::kPrivetPrinter ||
         printer_type == PrinterType::kLocalPrinter);
  DCHECK(!printers.empty());
  FireWebUIListener("printers-added", base::Value(printer_type), printers);

  if (printer_type == PrinterType::kLocalPrinter &&
      !has_logged_printers_count_) {
    UMA_HISTOGRAM_COUNTS_1M("PrintPreview.NumberOfPrinters",
                            printers.GetSize());
    has_logged_printers_count_ = true;
  }
}

void PrintPreviewHandler::OnGetPrintersDone(const std::string& callback_id) {
  ResolveJavascriptCallback(base::Value(callback_id), base::Value());
}

void PrintPreviewHandler::OnGotExtensionPrinterInfo(
    const std::string& callback_id,
    const base::DictionaryValue& printer_info) {
  if (printer_info.empty()) {
    RejectJavascriptCallback(base::Value(callback_id), base::Value());
    return;
  }
  ResolveJavascriptCallback(base::Value(callback_id), printer_info);
}

void PrintPreviewHandler::OnPrintResult(const std::string& callback_id,
                                        const base::Value& error) {
  if (error.is_none())
    ResolveJavascriptCallback(base::Value(callback_id), error);
  else
    RejectJavascriptCallback(base::Value(callback_id), error);
  // Remove the preview dialog from the background printing manager if it is
  // being stored there. Since the PDF has been sent and the callback is
  // resolved or rejected, it is no longer needed and can be destroyed.
  printing::BackgroundPrintingManager* background_printing_manager =
      g_browser_process->background_printing_manager();
  if (background_printing_manager->HasPrintPreviewDialog(
          preview_web_contents())) {
    background_printing_manager->OnPrintRequestCancelled(
        preview_web_contents());
  }
}

void PrintPreviewHandler::RegisterForGaiaCookieChanges() {
  DCHECK(!gaia_cookie_manager_service_);
  Profile* profile = Profile::FromWebUI(web_ui());
  if (AccountConsistencyModeManager::IsMirrorEnabledForProfile(profile)) {
    gaia_cookie_manager_service_ =
        GaiaCookieManagerServiceFactory::GetForProfile(profile);
    if (gaia_cookie_manager_service_)
      gaia_cookie_manager_service_->AddObserver(this);
  }
}

void PrintPreviewHandler::UnregisterForGaiaCookieChanges() {
  if (gaia_cookie_manager_service_)
    gaia_cookie_manager_service_->RemoveObserver(this);
}

void PrintPreviewHandler::BadMessageReceived() {
  bad_message::ReceivedBadMessage(
      GetInitiator()->GetMainFrame()->GetProcess(),
      bad_message::BadMessageReason::PPH_EXTRA_PREVIEW_MESSAGE);
}

void PrintPreviewHandler::FileSelectedForTesting(const base::FilePath& path,
                                                 int index,
                                                 void* params) {
  GetPdfPrinterHandler()->FileSelected(path, index, params);
}

void PrintPreviewHandler::SetPdfSavedClosureForTesting(
    const base::Closure& closure) {
  GetPdfPrinterHandler()->SetPdfSavedClosureForTesting(closure);
}

void PrintPreviewHandler::SendEnableManipulateSettingsForTest() {
  FireWebUIListener("enable-manipulate-settings-for-test", base::Value());
}

void PrintPreviewHandler::SendManipulateSettingsForTest(
    const base::DictionaryValue& settings) {
  FireWebUIListener("manipulate-settings-for-test", settings);
}
