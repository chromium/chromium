// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/print_preview_handler.h"

#include <ctype.h>
#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/i18n/number_formatting.h"
#include "base/json/json_reader.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/bad_message.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/printing/background_printing_manager.h"
#include "chrome/browser/printing/print_error_dialog.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "chrome/browser/printing/print_preview_dialog_controller.h"
#include "chrome/browser/printing/print_preview_sticky_settings.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/printing/printer_manager_dialog.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/webui/print_preview/cloud_print_signin.h"
#include "chrome/browser/ui/webui/print_preview/pdf_printer_handler.h"
#include "chrome/browser/ui/webui/print_preview/policy_settings.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_metrics.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_ui.h"
#include "chrome/browser/ui/webui/print_preview/printer_handler.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/cloud_print/cloud_print_constants.h"
#include "chrome/common/crash_keys.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "components/cloud_devices/common/cloud_device_description.h"
#include "components/cloud_devices/common/cloud_devices_urls.h"
#include "components/cloud_devices/common/printer_description.h"
#include "components/prefs/pref_service.h"
#include "components/printing/browser/printer_capabilities.h"
#include "components/printing/common/cloud_print_cdd_conversion.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "net/base/url_util.h"
#include "printing/backend/print_backend.h"
#include "printing/backend/print_backend_consts.h"
#include "printing/backend/printing_restrictions.h"
#include "printing/buildflags/buildflags.h"
#include "printing/printing_utils.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/icu/source/i18n/unicode/ulocdata.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/account_manager/account_manager_util.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/device_identity/device_oauth2_token_service.h"
#include "chrome/browser/device_identity/device_oauth2_token_service_factory.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom.h"
#include "chrome/browser/ui/webui/signin/inline_login_dialog_chromeos.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/printing/printer_configuration.h"
#include "components/signin/public/identity_manager/scope_set.h"
#endif

using content::RenderFrameHost;
using content::WebContents;

namespace printing {

namespace {

// Max size for PDFs sent to Cloud Print. Server side limit is currently 80MB
// but PDF will double in size when sent to JS. See crbug.com/793506 and
// crbug.com/372240.
constexpr size_t kMaxCloudPrintPdfDataSizeInBytes = 80 * 1024 * 1024 / 2;

PrinterType GetPrinterTypeForUserAction(UserActionBuckets user_action) {
  switch (user_action) {
    case UserActionBuckets::kPrintWithPrivet:
      return PrinterType::kPrivet;
    case UserActionBuckets::kPrintWithExtension:
      return PrinterType::kExtension;
    // On Chrome OS, printing to Google Drive needs to open the local file
    // picker so |kPrintToGoogleDriveCros| action should be handled by the
    // PDFPrinterHandler.
    case UserActionBuckets::kPrintToGoogleDriveCros:
    case UserActionBuckets::kPrintToPdf:
      return PrinterType::kPdf;
    case UserActionBuckets::kPrintToPrinter:
    case UserActionBuckets::kFallbackToAdvancedSettingsDialog:
    case UserActionBuckets::kOpenInMacPreview:
      return PrinterType::kLocal;
    default:
      NOTREACHED();
      return PrinterType::kLocal;
  }
}

base::Value GetErrorValue(UserActionBuckets user_action,
                          base::StringPiece description) {
  return user_action == UserActionBuckets::kPrintWithPrivet
             ? base::Value(-1)
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
// Name of a dictionary field holding the UI locale.
const char kUiLocale[] = "uiLocale";
// Name of a dictionary field holding the thousands delimiter according to the
// locale.
const char kThousandsDelimiter[] = "thousandsDelimiter";
// Name of a dictionary field holding the decimal delimiter according to the
// locale.
const char kDecimalDelimiter[] = "decimalDelimiter";
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
// Name of a dictionary field holding policy values for printing settings.
const char kPolicies[] = "policies";
// Name of a dictionary field holding policy allowed mode value for the setting.
const char kAllowedMode[] = "allowedMode";
// Name of a dictionary field holding policy default mode value for the setting.
const char kDefaultMode[] = "defaultMode";
// Name of a dictionary pref holding the policy value for the header/footer
// checkbox.
const char kHeaderFooter[] = "headerFooter";
// Name of a dictionary pref holding the policy value for the background
// graphics checkbox.
const char kCssBackground[] = "cssBackground";
// Name of a dictionary pref holding the policy value for the paper size
// setting.
const char kMediaSize[] = "mediaSize";
#if defined(OS_CHROMEOS)
// Name of a dictionary field holding policy value for the setting.
const char kValue[] = "value";
// Name of a dictionary pref holding the policy value for the sheets number.
const char kSheets[] = "sheets";
#endif  // defined(OS_CHROMEOS)
// Name of a dictionary field indicating whether the 'Save to PDF' destination
// is disabled.
const char kPdfPrinterDisabled[] = "pdfPrinterDisabled";
// Name of a dictionary field indicating whether the destinations are managed by
// the PrinterTypeDenyList enterprise policy.
const char kDestinationsManaged[] = "destinationsManaged";
// Name of a dictionary field holding the cloud print URL.
const char kCloudPrintURL[] = "cloudPrintURL";
// Name of a dictionary field holding the signed in user accounts.
const char kUserAccounts[] = "userAccounts";
// Name of a dictionary field indicating whether sync is available. If false,
// Print Preview will always send a request to the Google Cloud Print server on
// load, to check the user's sign in state.
const char kSyncAvailable[] = "syncAvailable";
#if defined(OS_CHROMEOS)
// Name of a dictionary field indicating whether the user's Drive directory is
// mounted.
const char kIsDriveMounted[] = "isDriveMounted";
#endif  // defined(OS_CHROMEOS)

// Get the print job settings dictionary from |json_str|.
// Returns |base::Value()| on failure.
base::Value GetSettingsDictionary(const std::string& json_str) {
  base::Optional<base::Value> settings = base::JSONReader::Read(json_str);
  if (!settings || !settings->is_dict()) {
    NOTREACHED() << "Print job settings must be a dictionary.";
    return base::Value();
  }

  if (settings->DictEmpty()) {
    NOTREACHED() << "Print job settings dictionary is empty";
    return base::Value();
  }

  return std::move(*settings);
}

UserActionBuckets DetermineUserAction(const base::Value& settings) {
#if defined(OS_MAC)
  if (settings.FindKey(kSettingOpenPDFInPreview))
    return UserActionBuckets::kOpenInMacPreview;
#endif

#if defined(OS_CHROMEOS)
  if (base::FeatureList::IsEnabled(chromeos::features::kPrintSaveToDrive) &&
      settings.FindBoolKey(kSettingPrintToGoogleDrive).value_or(false)) {
    return UserActionBuckets::kPrintToGoogleDriveCros;
  }
#endif

  // This needs to be checked before checking for a cloud print ID, since a
  // print ticket for printing to Drive will also contain a cloud print ID.
  if (settings.FindBoolKey(kSettingPrintToGoogleDrive).value_or(false))
    return UserActionBuckets::kPrintToGoogleDrive;
  if (settings.FindKey(kSettingCloudPrintId))
    return UserActionBuckets::kPrintWithCloudPrint;

  PrinterType type = static_cast<PrinterType>(
      settings.FindIntKey(kSettingPrinterType).value());
  switch (type) {
    case PrinterType::kPrivet:
      return UserActionBuckets::kPrintWithPrivet;
    case PrinterType::kExtension:
      return UserActionBuckets::kPrintWithExtension;
    case PrinterType::kPdf:
      return UserActionBuckets::kPrintToPdf;
    case PrinterType::kLocal:
      break;
    default:
      NOTREACHED();
      break;
  }

  if (settings.FindBoolKey(kSettingShowSystemDialog).value_or(false))
    return UserActionBuckets::kFallbackToAdvancedSettingsDialog;
  return UserActionBuckets::kPrintToPrinter;
}

base::Optional<gfx::Size> ParsePaperSize(const base::Value* paper_size_value) {
  if (!paper_size_value || paper_size_value->DictEmpty())
    return base::nullopt;

  const base::Value* custom_size =
      paper_size_value->FindKey(kPaperSizeCustomSize);
  if (custom_size) {
    return gfx::Size(*custom_size->FindIntKey(kPaperSizeWidth),
                     *custom_size->FindIntKey(kPaperSizeHeight));
  }

  const std::string* name = paper_size_value->FindStringKey(kPaperSizeName);
  DCHECK(name);
  return ParsePaper(*name).size_um;
}

base::Value GetPolicies(const PrefService& prefs) {
  base::Value policies(base::Value::Type::DICTIONARY);

  base::Value header_footer_policy(base::Value::Type::DICTIONARY);
  if (prefs.HasPrefPath(prefs::kPrintHeaderFooter)) {
    if (prefs.IsManagedPreference(prefs::kPrintHeaderFooter)) {
      header_footer_policy.SetBoolKey(
          kAllowedMode, prefs.GetBoolean(prefs::kPrintHeaderFooter));
    } else {
      header_footer_policy.SetBoolKey(
          kDefaultMode, prefs.GetBoolean(prefs::kPrintHeaderFooter));
    }
  }
  if (!header_footer_policy.DictEmpty())
    policies.SetKey(kHeaderFooter, std::move(header_footer_policy));

  base::Value background_graphics_policy(base::Value::Type::DICTIONARY);
  if (prefs.HasPrefPath(prefs::kPrintingAllowedBackgroundGraphicsModes)) {
    background_graphics_policy.SetIntKey(
        kAllowedMode,
        prefs.GetInteger(prefs::kPrintingAllowedBackgroundGraphicsModes));
  }
  if (prefs.HasPrefPath(prefs::kPrintingBackgroundGraphicsDefault)) {
    background_graphics_policy.SetIntKey(
        kDefaultMode,
        prefs.GetInteger(prefs::kPrintingBackgroundGraphicsDefault));
  }
  if (!background_graphics_policy.DictEmpty())
    policies.SetKey(kCssBackground, std::move(background_graphics_policy));

  base::Value paper_size_policy(base::Value::Type::DICTIONARY);
  if (prefs.HasPrefPath(prefs::kPrintingPaperSizeDefault)) {
    base::Optional<gfx::Size> default_paper_size =
        ParsePaperSize(prefs.Get(prefs::kPrintingPaperSizeDefault));
    if (default_paper_size.has_value()) {
      base::Value default_paper_size_value(base::Value::Type::DICTIONARY);
      default_paper_size_value.SetIntKey(kPaperSizeWidth,
                                         default_paper_size.value().width());
      default_paper_size_value.SetIntKey(kPaperSizeHeight,
                                         default_paper_size.value().height());
      paper_size_policy.SetKey(kDefaultMode,
                               std::move(default_paper_size_value));
    }
  }
  if (!paper_size_policy.DictEmpty())
    policies.SetKey(kMediaSize, std::move(paper_size_policy));

#if defined(OS_CHROMEOS)
  if (prefs.HasPrefPath(prefs::kPrintingMaxSheetsAllowed)) {
    base::Value sheets_policy(base::Value::Type::DICTIONARY);
    sheets_policy.SetIntKey(kValue,
                            prefs.GetInteger(prefs::kPrintingMaxSheetsAllowed));
    policies.SetKey(kSheets, std::move(sheets_policy));
  }
#endif  // defined(OS_CHROMEOS)

  return policies;
}

}  // namespace

#if defined(OS_CHROMEOS)
class PrintPreviewHandler::AccessTokenService
    : public OAuth2AccessTokenManager::Consumer {
 public:
  AccessTokenService() : OAuth2AccessTokenManager::Consumer("print_preview") {}

  void RequestToken(base::OnceCallback<void(const std::string&)> callback) {
    // There can only be one pending request at a time. See
    // cloud_print_interface_js.js.
    const signin::ScopeSet scopes{cloud_devices::kCloudPrintAuthScope};
    DCHECK(!device_request_callback_);

    DeviceOAuth2TokenService* token_service =
        DeviceOAuth2TokenServiceFactory::Get();
    device_request_ = token_service->StartAccessTokenRequest(scopes, this);
    device_request_callback_ = std::move(callback);
  }

  void OnGetTokenSuccess(
      const OAuth2AccessTokenManager::Request* request,
      const OAuth2AccessTokenConsumer::TokenResponse& token_response) override {
    OnServiceResponse(request, token_response.access_token);
  }

  void OnGetTokenFailure(const OAuth2AccessTokenManager::Request* request,
                         const GoogleServiceAuthError& error) override {
    OnServiceResponse(request, std::string());
  }

 private:
  void OnServiceResponse(const OAuth2AccessTokenManager::Request* request,
                         const std::string& access_token) {
    DCHECK_EQ(request, device_request_.get());
    std::move(device_request_callback_).Run(access_token);
    device_request_.reset();
  }

  std::unique_ptr<OAuth2AccessTokenManager::Request> device_request_;
  base::OnceCallback<void(const std::string&)> device_request_callback_;

  DISALLOW_COPY_AND_ASSIGN(AccessTokenService);
};
#endif  // defined(OS_CHROMEOS)

PrintPreviewHandler::PrintPreviewHandler() {
  ReportUserActionHistogram(UserActionBuckets::kPreviewStarted);
}

PrintPreviewHandler::~PrintPreviewHandler() {
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
#if defined(OS_CHROMEOS)
  web_ui()->RegisterMessageCallback(
      "getAccessToken",
      base::BindRepeating(&PrintPreviewHandler::HandleGetAccessToken,
                          base::Unretained(this)));
#endif
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
      "grantExtensionPrinterAccess",
      base::BindRepeating(
          &PrintPreviewHandler::HandleGrantExtensionPrinterAccess,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "openPrinterSettings",
      base::BindRepeating(&PrintPreviewHandler::HandleOpenPrinterSettings,
                          base::Unretained(this)));
#if defined(OS_CHROMEOS)
  web_ui()->RegisterMessageCallback(
      "getEulaUrl", base::BindRepeating(&PrintPreviewHandler::HandleGetEulaUrl,
                                        base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "requestPrinterStatus",
      base::BindRepeating(
          &PrintPreviewHandler::HandleRequestPrinterStatusUpdate,
          base::Unretained(this)));
#endif
}

void PrintPreviewHandler::OnJavascriptAllowed() {
  print_preview_ui()->SetPreviewUIId();
  // Now that the UI is initialized, any future account changes will require
  // a printer list refresh.
  ReadPrinterTypeDenyListFromPrefs();
  RegisterForGaiaCookieChanges();
}

void PrintPreviewHandler::OnJavascriptDisallowed() {
  // Normally the handler and print preview will be destroyed together, but
  // this is necessary for refresh or navigation from the chrome://print page.
  weak_factory_.InvalidateWeakPtrs();
  print_preview_ui()->ClearPreviewUIId();
  preview_callbacks_.clear();
  preview_failures_.clear();
  printer_type_deny_list_.clear();
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

void PrintPreviewHandler::ReadPrinterTypeDenyListFromPrefs() {
  PrefService* prefs = GetPrefs();
  if (!prefs->HasPrefPath(prefs::kPrinterTypeDenyList))
    return;

  const base::Value* deny_list_types = prefs->Get(prefs::kPrinterTypeDenyList);
  if (!deny_list_types || !deny_list_types->is_list())
    return;

  for (const base::Value& deny_list_type : deny_list_types->GetList()) {
    if (!deny_list_type.is_string())
      continue;

    // The expected printer type strings are enumerated in
    // components/policy/resources/policy_templates.json
    const std::string& deny_list_str = deny_list_type.GetString();
    if (deny_list_str == "privet")
      printer_type_deny_list_.insert(PrinterType::kPrivet);
    else if (deny_list_str == "extension")
      printer_type_deny_list_.insert(PrinterType::kExtension);
    else if (deny_list_str == "pdf")
      printer_type_deny_list_.insert(PrinterType::kPdf);
    else if (deny_list_str == "local")
      printer_type_deny_list_.insert(PrinterType::kLocal);
    else if (deny_list_str == "cloud")
      printer_type_deny_list_.insert(PrinterType::kCloud);
  }
}

PrintPreviewUI* PrintPreviewHandler::print_preview_ui() const {
  return static_cast<PrintPreviewUI*>(web_ui()->GetController());
}

bool PrintPreviewHandler::ShouldReceiveRendererMessage(int request_id) {
  if (!IsJavascriptAllowed()) {
    BadMessageReceived();
    return false;
  }

  if (!base::Contains(preview_callbacks_, request_id)) {
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

  // Immediately resolve the callback without fetching printers if the printer
  // type is on the deny list.
  if (base::Contains(printer_type_deny_list_, printer_type)) {
    ResolveJavascriptCallback(base::Value(callback_id), base::Value());
    return;
  }

  PrinterHandler* handler = GetPrinterHandler(printer_type);
  if (!handler) {
    RejectJavascriptCallback(base::Value(callback_id), base::Value());
    return;
  }
  // Make sure all in progress requests are canceled before new printer search
  // starts.
  handler->Reset();
  handler->StartGetPrinters(
      base::BindRepeating(&PrintPreviewHandler::OnAddedPrinters,
                          weak_factory_.GetWeakPtr(), printer_type),
      base::BindOnce(&PrintPreviewHandler::OnGetPrintersDone,
                     weak_factory_.GetWeakPtr(), callback_id));
}

void PrintPreviewHandler::HandleGrantExtensionPrinterAccess(
    const base::ListValue* args) {
  std::string callback_id;
  std::string printer_id;
  bool ok = args->GetString(0, &callback_id) &&
            args->GetString(1, &printer_id) && !callback_id.empty();
  DCHECK(ok);

  PrinterHandler* handler = GetPrinterHandler(PrinterType::kExtension);
  handler->StartGrantPrinterAccess(
      printer_id,
      base::BindOnce(&PrintPreviewHandler::OnGotExtensionPrinterInfo,
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

  // Reject the callback if the printer type is on the deny list.
  if (base::Contains(printer_type_deny_list_, printer_type)) {
    RejectJavascriptCallback(base::Value(callback_id), base::Value());
    return;
  }

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
  base::Value settings = GetSettingsDictionary(json_str);
  CHECK(settings.is_dict());
  int request_id = settings.FindIntKey(kPreviewRequestID).value();
  CHECK_GT(request_id, -1);

  CHECK(!base::Contains(preview_callbacks_, request_id));
  preview_callbacks_[request_id] = callback_id;
  print_preview_ui()->OnPrintPreviewRequest(request_id);
  // Add an additional key in order to identify |print_preview_ui| later on
  // when calling PrintPreviewUI::ShouldCancelRequest() on the IO thread.
  settings.SetKey(
      kPreviewUIID,
      base::Value(print_preview_ui()->GetIDForPrintPreviewUI().value()));

  // Increment request count.
  ++regenerate_preview_request_count_;

  WebContents* initiator = GetInitiator();
  RenderFrameHost* rfh =
      initiator
          ? PrintViewManager::FromWebContents(initiator)->print_preview_rfh()
          : nullptr;
  if (!rfh) {
    ReportUserActionHistogram(UserActionBuckets::kInitiatorClosed);
    print_preview_ui()->OnClosePrintPreviewDialog();
    return;
  }

  // Retrieve the page title and url and send it to the renderer process if
  // headers and footers are to be displayed.
  base::Optional<bool> display_header_footer_opt =
      settings.FindBoolKey(kSettingHeaderFooterEnabled);
  DCHECK(display_header_footer_opt);
  if (display_header_footer_opt.value_or(false)) {
    settings.SetKey(kSettingHeaderFooterTitle,
                    base::Value(initiator->GetTitle()));

    url::Replacements<char> url_sanitizer;
    url_sanitizer.ClearUsername();
    url_sanitizer.ClearPassword();
    const GURL& initiator_url = initiator->GetLastCommittedURL();
    settings.SetKey(kSettingHeaderFooterURL,
                    base::Value(url_formatter::FormatUrl(
                        initiator_url.ReplaceComponents(url_sanitizer))));
  }

  VLOG(1) << "Print preview request start";

  if (!print_render_frame_.is_bound())
    rfh->GetRemoteAssociatedInterfaces()->GetInterface(&print_render_frame_);

  if (!print_preview_ui()->IsBound()) {
    print_render_frame_->SetPrintPreviewUI(
        print_preview_ui()->BindPrintPreviewUI());
  }
  print_render_frame_->PrintPreview(settings.Clone());
  last_preview_settings_ = std::move(settings);
}

void PrintPreviewHandler::HandlePrint(const base::ListValue* args) {
  ReportRegeneratePreviewRequestCountBeforePrint(
      regenerate_preview_request_count_);
  std::string callback_id;
  CHECK(args->GetString(0, &callback_id));
  CHECK(!callback_id.empty());
  std::string json_str;
  CHECK(args->GetString(1, &json_str));

  base::Value settings = GetSettingsDictionary(json_str);
  if (!settings.is_dict()) {
    RejectJavascriptCallback(base::Value(callback_id), base::Value(-1));
    return;
  }

  const UserActionBuckets user_action = DetermineUserAction(settings);

  int page_count = settings.FindIntKey(kSettingPreviewPageCount).value_or(-1);
  if (page_count <= 0) {
    RejectJavascriptCallback(base::Value(callback_id),
                             GetErrorValue(user_action, "NO_PAGE_COUNT"));
    return;
  }

  scoped_refptr<base::RefCountedMemory> data;
  print_preview_ui()->GetPrintPreviewDataForIndex(
      COMPLETE_PREVIEW_DOCUMENT_INDEX, &data);
  if (!data) {
    // Nothing to print, no preview available.
    RejectJavascriptCallback(base::Value(callback_id),
                             GetErrorValue(user_action, "NO_DATA"));
    return;
  }
  DCHECK(data->size());
  DCHECK(data->front());

  // After validating |settings|, record metrics.
  bool is_pdf = !print_preview_ui()->source_is_modifiable();
  if (last_preview_settings_.is_dict())
    ReportPrintSettingsStats(settings, last_preview_settings_, is_pdf);
  {
    PrintDocumentTypeBuckets doc_type =
        is_pdf ? PrintDocumentTypeBuckets::kPdfDocument
               : PrintDocumentTypeBuckets::kHtmlDocument;
    size_t average_page_size_in_kb = data->size() / page_count;
    average_page_size_in_kb /= 1024;
    ReportPrintDocumentTypeAndSizeHistograms(doc_type, average_page_size_in_kb);
  }
  ReportUserActionHistogram(user_action);

  if (user_action == UserActionBuckets::kPrintWithCloudPrint ||
      user_action == UserActionBuckets::kPrintToGoogleDrive) {
    // Does not send the title like the other printer handler types below,
    // because JS already has the document title from the initial settings.
    SendCloudPrintJob(callback_id, data.get());
    return;
  }

  PrinterHandler* handler =
      GetPrinterHandler(GetPrinterTypeForUserAction(user_action));
  handler->StartPrint(print_preview_ui()->initiator_title(),
                      std::move(settings), data,
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
  PrintPreviewStickySettings* sticky_settings =
      PrintPreviewStickySettings::GetInstance();
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

  PrinterHandler* handler = GetPrinterHandler(PrinterType::kLocal);
  handler->StartGetCapability(
      printer_name,
      base::BindOnce(&PrintPreviewHandler::SendPrinterSetup,
                     weak_factory_.GetWeakPtr(), callback_id, printer_name));
}

void PrintPreviewHandler::HandleSignin(const base::ListValue* /*args*/) {
  Profile* profile = Profile::FromWebUI(web_ui());
  DCHECK(profile);

#if defined(OS_CHROMEOS)
  if (chromeos::IsAccountManagerAvailable(profile)) {
    // Chrome OS Account Manager is enabled on this Profile and hence, all
    // account management flows will go through native UIs and not through a
    // tabbed browser window.
    chromeos::InlineLoginDialogChromeOS::Show(
        chromeos::InlineLoginDialogChromeOS::Source::kPrintPreviewDialog);
    return;
  }
#endif

  chrome::ScopedTabbedBrowserDisplayer displayer(profile);
  CreateCloudPrintSigninTab(
      displayer.browser(),
      base::BindOnce(&PrintPreviewHandler::OnSignInTabClosed,
                     weak_factory_.GetWeakPtr()));
}

void PrintPreviewHandler::OnSignInTabClosed() {
  if (identity_manager_) {
    // Sign in state will be reported in OnAccountsInCookieJarUpdated, so no
    // need to do anything here.
    return;
  }
  FireWebUIListener("check-for-account-update");
}

#if defined(OS_CHROMEOS)
void PrintPreviewHandler::HandleGetAccessToken(const base::ListValue* args) {
  std::string callback_id;

  bool ok = args->GetString(0, &callback_id) && !callback_id.empty();
  DCHECK(ok);

  if (!token_service_)
    token_service_ = std::make_unique<AccessTokenService>();
  token_service_->RequestToken(
      base::BindOnce(&PrintPreviewHandler::SendAccessToken,
                     weak_factory_.GetWeakPtr(), callback_id));
}
#endif

#if BUILDFLAG(ENABLE_BASIC_PRINT_DIALOG)
void PrintPreviewHandler::HandleShowSystemDialog(
    const base::ListValue* /*args*/) {
  ReportUserActionHistogram(
      UserActionBuckets::kFallbackToAdvancedSettingsDialog);

  WebContents* initiator = GetInitiator();
  if (!initiator)
    return;

  auto* print_view_manager = PrintViewManager::FromWebContents(initiator);
  print_view_manager->PrintForSystemDialogNow(base::BindOnce(
      &PrintPreviewHandler::ClosePreviewDialog, weak_factory_.GetWeakPtr()));

  // Cancel the pending preview request if exists.
  print_preview_ui()->OnCancelPendingPreviewRequest();
}
#endif

void PrintPreviewHandler::HandleClosePreviewDialog(
    const base::ListValue* /*args*/) {
  ReportUserActionHistogram(UserActionBuckets::kCancel);

  ReportRegeneratePreviewRequestCountBeforeCancel(
      regenerate_preview_request_count_);
}

void PrintPreviewHandler::HandleOpenPrinterSettings(
    const base::ListValue* args) {
#if defined(OS_CHROMEOS)
  chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
      Profile::FromWebUI(web_ui()),
      chromeos::settings::mojom::kPrintingDetailsSubpagePath);
#else
  GURL url(chrome::GetSettingsUrl(chrome::kPrintingSettingsSubPage));
  content::OpenURLParams params(url, content::Referrer(),
                                WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                ui::PAGE_TRANSITION_LINK, false);
  preview_web_contents()->OpenURL(params);
#endif
}

#if defined(OS_CHROMEOS)
void PrintPreviewHandler::HandleGetEulaUrl(const base::ListValue* args) {
  CHECK_EQ(2U, args->GetSize());

  const std::string& callback_id = args->GetList()[0].GetString();
  const std::string& destination_id = args->GetList()[1].GetString();

  PrinterHandler* handler = GetPrinterHandler(PrinterType::kLocal);
  handler->StartGetEulaUrl(
      destination_id, base::BindOnce(&PrintPreviewHandler::SendEulaUrl,
                                     weak_factory_.GetWeakPtr(), callback_id));
}
#endif

void PrintPreviewHandler::GetLocaleInformation(base::Value* settings) {
  // Getting the measurement system based on the locale.
  UErrorCode errorCode = U_ZERO_ERROR;
  const char* locale = g_browser_process->GetApplicationLocale().c_str();
  settings->SetStringKey(kUiLocale, std::string(locale));
  UMeasurementSystem system = ulocdata_getMeasurementSystem(locale, &errorCode);
  // On error, assume the units are SI.
  // Since the only measurement units print preview's WebUI cares about are
  // those for measuring distance, assume anything non-US is SI.
  if (errorCode > U_ZERO_ERROR || system != UMS_US)
    system = UMS_SI;

  // Getting the number formatting based on the locale and writing to
  // dictionary.
  base::string16 number_format = base::FormatDouble(123456.78, 2);
  size_t thousands_pos = number_format.find('3') + 1;
  base::string16 thousands_delimiter = number_format.substr(thousands_pos, 1);
  if (number_format[thousands_pos] == '4')
    thousands_delimiter.clear();
  size_t decimal_pos = number_format.find('6') + 1;
  DCHECK_NE(number_format[decimal_pos], '7');
  base::string16 decimal_delimiter = number_format.substr(decimal_pos, 1);
  settings->SetStringKey(kDecimalDelimiter, decimal_delimiter);
  settings->SetStringKey(kThousandsDelimiter, thousands_delimiter);
  settings->SetIntKey(kUnitType, system);
}

void PrintPreviewHandler::HandleGetInitialSettings(
    const base::ListValue* args) {
  std::string callback_id;
  CHECK(args->GetString(0, &callback_id));
  CHECK(!callback_id.empty());

  AllowJavascript();

  PrinterHandler* handler = GetPrinterHandler(PrinterType::kLocal);
  handler->GetDefaultPrinter(
      base::BindOnce(&PrintPreviewHandler::SendInitialSettings,
                     weak_factory_.GetWeakPtr(), callback_id));
}

void PrintPreviewHandler::GetUserAccountList(base::Value* settings) {
  base::Value account_list(base::Value::Type::LIST);
  if (identity_manager_) {
    const std::vector<gaia::ListedAccount>& accounts =
        identity_manager_->GetAccountsInCookieJar().signed_in_accounts;
    for (const gaia::ListedAccount& account : accounts) {
      account_list.Append(account.email);
    }
    settings->SetKey(kSyncAvailable, base::Value(true));
  } else {
    settings->SetKey(kSyncAvailable, base::Value(false));
  }
  settings->SetKey(kUserAccounts, std::move(account_list));
}

void PrintPreviewHandler::SendInitialSettings(
    const std::string& callback_id,
    const std::string& default_printer) {
  base::Value initial_settings(base::Value::Type::DICTIONARY);
  initial_settings.SetStringKey(kDocumentTitle,
                                print_preview_ui()->initiator_title());
  initial_settings.SetBoolKey(kSettingPreviewModifiable,
                              print_preview_ui()->source_is_modifiable());
  initial_settings.SetBoolKey(kSettingPreviewIsFromArc,
                              print_preview_ui()->source_is_arc());
  initial_settings.SetBoolKey(kSettingPreviewIsPdf,
                              print_preview_ui()->source_is_pdf());
  initial_settings.SetStringKey(kSettingPrinterName, default_printer);
  initial_settings.SetBoolKey(kDocumentHasSelection,
                              print_preview_ui()->source_has_selection());
  initial_settings.SetBoolKey(kSettingShouldPrintSelectionOnly,
                              print_preview_ui()->print_selection_only());
  PrefService* prefs = GetPrefs();
  PrintPreviewStickySettings* sticky_settings =
      PrintPreviewStickySettings::GetInstance();
  sticky_settings->RestoreFromPrefs(prefs);
  if (sticky_settings->printer_app_state()) {
    initial_settings.SetStringKey(kAppState,
                                  *sticky_settings->printer_app_state());
  } else {
    initial_settings.SetKey(kAppState, base::Value());
  }

  base::Value policies = GetPolicies(*prefs);
  if (!policies.DictEmpty())
    initial_settings.SetKey(kPolicies, std::move(policies));

  if (IsCloudPrintEnabled()) {
    initial_settings.SetStringKey(
        kCloudPrintURL, GURL(cloud_devices::GetCloudPrintURL()).spec());
  }

  initial_settings.SetBoolKey(
      kPdfPrinterDisabled,
      base::Contains(printer_type_deny_list_, PrinterType::kPdf));

  const bool destinations_managed =
      !printer_type_deny_list_.empty() &&
      prefs->IsManagedPreference(prefs::kPrinterTypeDenyList);
  initial_settings.SetBoolKey(kDestinationsManaged, destinations_managed);

  base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
  initial_settings.SetBoolKey(kIsInKioskAutoPrintMode,
                              cmdline->HasSwitch(switches::kKioskModePrinting));
  initial_settings.SetBoolKey(kIsInAppKioskMode,
                              chrome::IsRunningInForcedAppMode());
  const std::string rules_str =
      prefs->GetString(prefs::kPrintPreviewDefaultDestinationSelectionRules);
  if (rules_str.empty()) {
    initial_settings.SetKey(kDefaultDestinationSelectionRules, base::Value());
  } else {
    initial_settings.SetStringKey(kDefaultDestinationSelectionRules, rules_str);
  }

  GetLocaleInformation(&initial_settings);
  if (IsCloudPrintEnabled()) {
    GetUserAccountList(&initial_settings);
  }

#if defined(OS_CHROMEOS)
  if (base::FeatureList::IsEnabled(chromeos::features::kPrintSaveToDrive)) {
    drive::DriveIntegrationService* drive_service =
        drive::DriveIntegrationServiceFactory::GetForProfile(
            Profile::FromWebUI(web_ui()));
    initial_settings.SetBoolKey(kIsDriveMounted,
                                drive_service && drive_service->IsMounted());
  }
#endif  // defined(OS_CHROMEOS)

  ResolveJavascriptCallback(base::Value(callback_id), initial_settings);
}

void PrintPreviewHandler::ClosePreviewDialog() {
  print_preview_ui()->OnClosePrintPreviewDialog();
}

#if defined(OS_CHROMEOS)
void PrintPreviewHandler::SendAccessToken(const std::string& callback_id,
                                          const std::string& access_token) {
  VLOG(1) << "Get getAccessToken finished";
  ResolveJavascriptCallback(base::Value(callback_id),
                            base::Value(access_token));
}

void PrintPreviewHandler::SendEulaUrl(const std::string& callback_id,
                                      const std::string& eula_url) {
  VLOG(1) << "Get PPD license finished";
  ResolveJavascriptCallback(base::Value(callback_id), base::Value(eula_url));
}
#endif

void PrintPreviewHandler::SendPrinterCapabilities(
    const std::string& callback_id,
    base::Value settings_info) {
  // Check that |settings_info| is valid.
  if (settings_info.is_dict() &&
      settings_info.FindKeyOfType(kSettingCapabilities,
                                  base::Value::Type::DICTIONARY)) {
    VLOG(1) << "Get printer capabilities finished";
    ResolveJavascriptCallback(base::Value(callback_id), settings_info);
    return;
  }

  VLOG(1) << "Get printer capabilities failed";
  RejectJavascriptCallback(base::Value(callback_id), base::Value());
}

void PrintPreviewHandler::SendPrinterSetup(const std::string& callback_id,
                                           const std::string& printer_name,
                                           base::Value destination_info) {
  base::Value response(base::Value::Type::DICTIONARY);
  base::Value* caps_value =
      destination_info.is_dict()
          ? destination_info.FindKeyOfType(kSettingCapabilities,
                                           base::Value::Type::DICTIONARY)
          : nullptr;
  response.SetKey("printerId", base::Value(printer_name));
  response.SetKey("success", base::Value(!!caps_value));
  response.SetKey("capabilities",
                  caps_value ? std::move(*caps_value)
                             : base::Value(base::Value::Type::DICTIONARY));
  if (caps_value) {
    base::Value* printer =
        destination_info.FindKeyOfType(kPrinter, base::Value::Type::DICTIONARY);
    if (printer) {
      base::Value* policies_value = printer->FindKeyOfType(
          kSettingPolicies, base::Value::Type::DICTIONARY);
      if (policies_value)
        response.SetKey("policies", std::move(*policies_value));
    }
  } else {
    LOG(WARNING) << "Printer setup failed";
  }
  ResolveJavascriptCallback(base::Value(callback_id), response);
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
  PrintPreviewDialogController* dialog_controller =
      PrintPreviewDialogController::GetInstance();
  if (!dialog_controller)
    return NULL;
  return dialog_controller->GetInitiator(preview_web_contents());
}

void PrintPreviewHandler::OnAccountsInCookieUpdated(
    const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
    const GoogleServiceAuthError& error) {
  base::Value account_list(base::Value::Type::LIST);
  const std::vector<gaia::ListedAccount>& accounts =
      accounts_in_cookie_jar_info.signed_in_accounts;
  for (const auto& account : accounts) {
    account_list.Append(account.email);
  }
  FireWebUIListener("user-accounts-updated", std::move(account_list));
}

void PrintPreviewHandler::OnPrintPreviewReady(int preview_uid, int request_id) {
  std::string callback_id = GetCallbackId(request_id);
  if (callback_id.empty())
    return;

  ResolveJavascriptCallback(base::Value(callback_id), base::Value(preview_uid));
}

void PrintPreviewHandler::OnPrintPreviewFailed(int request_id) {
  WebContents* initiator = GetInitiator();
  if (!initiator || initiator->IsBeingDestroyed())
    return;  // Drop notification if fired during destruction sequence.

  std::string callback_id = GetCallbackId(request_id);
  if (callback_id.empty())
    return;

  if (!reported_failed_preview_) {
    reported_failed_preview_ = true;
    ReportUserActionHistogram(UserActionBuckets::kPreviewFailed);
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
                                                 mojom::DuplexMode duplex,
                                                 int request_id) {
  if (!ShouldReceiveRendererMessage(request_id))
    return;

  FireWebUIListener("print-preset-options", base::Value(disable_scaling),
                    base::Value(copies), base::Value(static_cast<int>(duplex)));
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
  if (base::Contains(preview_failures_, preview_request_id))
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
  PrintPreviewDialogController* dialog_controller =
      PrintPreviewDialogController::GetInstance();
  if (dialog_controller)
    dialog_controller->EraseInitiatorInfo(preview_web_contents());
}

PrinterHandler* PrintPreviewHandler::GetPrinterHandler(
    PrinterType printer_type) {
  if (printer_type == PrinterType::kExtension) {
    if (!extension_printer_handler_) {
      extension_printer_handler_ = PrinterHandler::CreateForExtensionPrinters(
          Profile::FromWebUI(web_ui()));
    }
    return extension_printer_handler_.get();
  }
#if BUILDFLAG(ENABLE_SERVICE_DISCOVERY)
  if (printer_type == PrinterType::kPrivet) {
    if (!privet_printer_handler_) {
      privet_printer_handler_ =
          PrinterHandler::CreateForPrivetPrinters(Profile::FromWebUI(web_ui()));
    }
    return privet_printer_handler_.get();
  }
#endif
  if (printer_type == PrinterType::kPdf) {
    if (!pdf_printer_handler_) {
      pdf_printer_handler_ = PrinterHandler::CreateForPdfPrinter(
          Profile::FromWebUI(web_ui()), preview_web_contents(),
          PrintPreviewStickySettings::GetInstance());
    }
    return pdf_printer_handler_.get();
  }
  if (printer_type == PrinterType::kLocal) {
    if (!local_printer_handler_) {
      local_printer_handler_ = PrinterHandler::CreateForLocalPrinters(
          preview_web_contents(), Profile::FromWebUI(web_ui()));
    }
    return local_printer_handler_.get();
  }
  NOTREACHED();
  return nullptr;
}

PdfPrinterHandler* PrintPreviewHandler::GetPdfPrinterHandler() {
  return static_cast<PdfPrinterHandler*>(GetPrinterHandler(PrinterType::kPdf));
}

void PrintPreviewHandler::OnAddedPrinters(PrinterType printer_type,
                                          const base::ListValue& printers) {
  DCHECK(printer_type == PrinterType::kExtension ||
         printer_type == PrinterType::kPrivet ||
         printer_type == PrinterType::kLocal);
  DCHECK(!printers.empty());
  FireWebUIListener("printers-added",
                    base::Value(static_cast<int>(printer_type)), printers);

  if (printer_type == PrinterType::kLocal && !has_logged_printers_count_) {
    ReportNumberOfPrinters(printers.GetSize());
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
  BackgroundPrintingManager* background_printing_manager =
      g_browser_process->background_printing_manager();
  if (background_printing_manager->HasPrintPreviewDialog(
          preview_web_contents())) {
    background_printing_manager->OnPrintRequestCancelled(
        preview_web_contents());
  }
}

void PrintPreviewHandler::RegisterForGaiaCookieChanges() {
  DCHECK(!identity_manager_);
  cloud_print_enabled_ =
      !base::Contains(printer_type_deny_list_, PrinterType::kCloud) &&
      GetPrefs()->GetBoolean(prefs::kCloudPrintSubmitEnabled);

  if (!cloud_print_enabled_)
    return;

  Profile* profile = Profile::FromWebUI(web_ui());
  if (!AccountConsistencyModeManager::IsMirrorEnabledForProfile(profile) &&
      !AccountConsistencyModeManager::IsDiceEnabledForProfile(profile)) {
    return;
  }

  identity_manager_ = IdentityManagerFactory::GetForProfile(profile);
  identity_manager_->AddObserver(this);
}

void PrintPreviewHandler::UnregisterForGaiaCookieChanges() {
  if (!identity_manager_)
    return;

  identity_manager_->RemoveObserver(this);
  identity_manager_ = nullptr;
  cloud_print_enabled_ = false;
}

bool PrintPreviewHandler::IsCloudPrintEnabled() {
  return cloud_print_enabled_;
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
    base::OnceClosure closure) {
  GetPdfPrinterHandler()->SetPdfSavedClosureForTesting(std::move(closure));
}

void PrintPreviewHandler::SendEnableManipulateSettingsForTest() {
  FireWebUIListener("enable-manipulate-settings-for-test", base::Value());
}

void PrintPreviewHandler::SendManipulateSettingsForTest(
    const base::DictionaryValue& settings) {
  FireWebUIListener("manipulate-settings-for-test", settings);
}

#if defined(OS_CHROMEOS)
void PrintPreviewHandler::HandleRequestPrinterStatusUpdate(
    const base::ListValue* args) {
  CHECK_EQ(2U, args->GetSize());

  const std::string& callback_id = args->GetList()[0].GetString();
  const std::string& printer_id = args->GetList()[1].GetString();

  PrinterHandler* handler = GetPrinterHandler(PrinterType::kLocal);
  handler->StartPrinterStatusRequest(
      printer_id, base::BindOnce(&PrintPreviewHandler::OnPrinterStatusUpdated,
                                 weak_factory_.GetWeakPtr(), callback_id));
}

void PrintPreviewHandler::OnPrinterStatusUpdated(
    const std::string& callback_id,
    const base::Value& cups_printer_status) {
  ResolveJavascriptCallback(base::Value(callback_id), cups_printer_status);
}
#endif

}  // namespace printing
