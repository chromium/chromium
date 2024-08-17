// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/print_preview_handler.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/dcheck_is_on.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/number_formatting.h"
#include "base/json/json_reader.h"
#include "base/lazy_instance.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/bad_message.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/printing/background_printing_manager.h"
#include "chrome/browser/printing/prefs_util.h"
#include "chrome/browser/printing/print_error_dialog.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "chrome/browser/printing/print_preview_dialog_controller.h"
#include "chrome/browser/printing/print_preview_sticky_settings.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/printing/printer_manager_dialog.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/print_preview/pdf_printer_handler.h"
#include "chrome/browser/ui/webui/print_preview/policy_settings.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_metrics.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_ui.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_utils.h"
#include "chrome/browser/ui/webui/print_preview/printer_handler.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/crash_keys.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/printing/printer_capabilities.h"
#include "chrome/common/webui_url_constants.h"
#include "components/cloud_devices/common/cloud_device_description.h"
#include "components/cloud_devices/common/printer_description.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/prefs/pref_service.h"
#include "components/printing/common/cloud_print_cdd_conversion.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "net/base/url_util.h"
#include "printing/backend/print_backend.h"
#include "printing/backend/print_backend_consts.h"
#include "printing/backend/print_backend_utils.h"
#include "printing/backend/printing_restrictions.h"
#include "printing/buildflags/buildflags.h"
#include "printing/mojom/print.mojom.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/icu/source/i18n/unicode/ulocdata.h"
#include "ui/shell_dialogs/selected_file_info.h"

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
#include "chrome/browser/enterprise/data_protection/print_utils.h"
#if BUILDFLAG(IS_MAC)
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#endif  // BUILDFLAG(IS_MAC)
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/crosapi/mojom/local_printer.mojom.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/local_printer_ash.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ui/webui/print_preview/extension_printer_handler_adapter_ash.h"
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/common/chrome_paths_lacros.h"
#include "chromeos/lacros/lacros_service.h"
#endif

#if DCHECK_IS_ON()
#include "base/debug/stack_trace.h"
#endif

using content::RenderFrameHost;
using content::WebContents;

namespace printing {

namespace {

mojom::PrinterType GetPrinterTypeForUserAction(UserActionBuckets user_action) {
  switch (user_action) {
    case UserActionBuckets::kPrintWithExtension:
      return mojom::PrinterType::kExtension;
    // On Chrome OS, printing to Google Drive needs to open the local file
    // picker so |kPrintToGoogleDriveCros| action should be handled by the
    // PDFPrinterHandler.
    case UserActionBuckets::kPrintToGoogleDriveCros:
    case UserActionBuckets::kPrintToPdf:
      return mojom::PrinterType::kPdf;
    case UserActionBuckets::kPrintToPrinter:
    case UserActionBuckets::kFallbackToAdvancedSettingsDialog:
    case UserActionBuckets::kOpenInMacPreview:
      return mojom::PrinterType::kLocal;
    default:
      NOTREACHED();
  }
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
#if BUILDFLAG(IS_CHROMEOS)
// Name of a dictionary field holding policy value for the setting.
const char kValue[] = "value";
// Name of a dictionary pref holding the policy value for the sheets number.
const char kSheets[] = "sheets";
// Name of a dictionary pref holding the policy value for the color setting.
const char kColor[] = "color";
// Name of a dictionary pref holding the policy value for the duplex setting.
const char kDuplex[] = "duplex";
// Name of a dictionary pref holding the policy value for the pin setting.
const char kPin[] = "pin";
#endif  // BUILDFLAG(IS_CHROMEOS)
// Name of a dictionary field indicating whether the 'Save to PDF' destination
// is disabled.
const char kPdfPrinterDisabled[] = "pdfPrinterDisabled";
// Name of a dictionary field indicating whether the destinations are managed by
// the PrinterTypeDenyList enterprise policy.
const char kDestinationsManaged[] = "destinationsManaged";
#if BUILDFLAG(IS_CHROMEOS)
// Name of a dictionary field indicating whether the user's Drive directory is
// mounted.
const char kIsDriveMounted[] = "isDriveMounted";
#endif  // BUILDFLAG(IS_CHROMEOS)
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
// Name of a dictionary pref holding the policy value for whether the
// "Print as image" option should be available to the user in the Print Preview
// for a PDF job.
const char kPrintPdfAsImageAvailability[] = "printPdfAsImageAvailability";
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
// Name of dictionary pref holding policy value for whether the
// "Print as image" option should default to set in Print Preview for
// a PDF job.
const char kPrintPdfAsImage[] = "printPdfAsImage";

// Gets the print job settings dictionary from |json_str|. Assumes the Print
// Preview WebUI does not send over invalid data.
base::Value::Dict GetSettingsDictionary(const std::string& json_str) {
  std::optional<base::Value> settings = base::JSONReader::Read(json_str);
  base::Value::Dict dict = std::move(*settings).TakeDict();
  CHECK(!dict.empty());
  return dict;
}

UserActionBuckets DetermineUserAction(const base::Value::Dict& settings) {
#if BUILDFLAG(IS_MAC)
  if (settings.contains(kSettingOpenPDFInPreview))
    return UserActionBuckets::kOpenInMacPreview;
#endif

#if BUILDFLAG(IS_CHROMEOS)
  if (settings.FindBool(kSettingPrintToGoogleDrive).value_or(false)) {
    return UserActionBuckets::kPrintToGoogleDriveCros;
  }
#endif

  mojom::PrinterType type = static_cast<mojom::PrinterType>(
      settings.FindInt(kSettingPrinterType).value());
  switch (type) {
    case mojom::PrinterType::kExtension:
      return UserActionBuckets::kPrintWithExtension;
    case mojom::PrinterType::kPdf:
      return UserActionBuckets::kPrintToPdf;
    case mojom::PrinterType::kLocal:
      break;
    default:
      NOTREACHED();
  }

  if (settings.FindBool(kSettingShowSystemDialog).value_or(false))
    return UserActionBuckets::kFallbackToAdvancedSettingsDialog;
  return UserActionBuckets::kPrintToPrinter;
}

#if BUILDFLAG(IS_CHROMEOS)
base::Value::Dict PoliciesToValue(crosapi::mojom::PoliciesPtr ptr) {
  base::Value::Dict policies;

  base::Value::Dict header_footer_policy;
  if (ptr->print_header_footer_allowed !=
      crosapi::mojom::Policies::OptionalBool::kUnset) {
    header_footer_policy.Set(kAllowedMode,
                             ptr->print_header_footer_allowed ==
                                 crosapi::mojom::Policies::OptionalBool::kTrue);
  }
  if (ptr->print_header_footer_default !=
      crosapi::mojom::Policies::OptionalBool::kUnset) {
    header_footer_policy.Set(kDefaultMode,
                             ptr->print_header_footer_default ==
                                 crosapi::mojom::Policies::OptionalBool::kTrue);
  }
  if (!header_footer_policy.empty())
    policies.Set(kHeaderFooter, std::move(header_footer_policy));

  base::Value::Dict background_graphics_policy;
  int value = static_cast<int>(ptr->allowed_background_graphics_modes);
  if (value)
    background_graphics_policy.Set(kAllowedMode, value);
  value = static_cast<int>(ptr->background_graphics_default);
  if (value)
    background_graphics_policy.Set(kDefaultMode, value);
  if (!background_graphics_policy.empty())
    policies.Set(kCssBackground, std::move(background_graphics_policy));

  base::Value::Dict paper_size_policy;
  const std::optional<gfx::Size>& default_paper_size = ptr->paper_size_default;
  if (default_paper_size.has_value()) {
    base::Value::Dict default_paper_size_value;
    default_paper_size_value.Set(kPaperSizeWidth,
                                 default_paper_size.value().width());
    default_paper_size_value.Set(kPaperSizeHeight,
                                 default_paper_size.value().height());
    paper_size_policy.Set(kDefaultMode, std::move(default_paper_size_value));
  }
  if (!paper_size_policy.empty())
    policies.Set(kMediaSize, std::move(paper_size_policy));

  if (ptr->max_sheets_allowed_has_value) {
    base::Value::Dict sheets_policy;
    sheets_policy.Set(kValue, static_cast<int>(ptr->max_sheets_allowed));
    policies.Set(kSheets, std::move(sheets_policy));
  }

  base::Value::Dict color_policy;
  if (ptr->allowed_color_modes)
    color_policy.Set(kAllowedMode, static_cast<int>(ptr->allowed_color_modes));
  if (ptr->default_color_mode != printing::mojom::ColorModeRestriction::kUnset)
    color_policy.Set(kDefaultMode, static_cast<int>(ptr->default_color_mode));
  if (!color_policy.empty())
    policies.Set(kColor, std::move(color_policy));

  base::Value::Dict duplex_policy;
  if (ptr->allowed_duplex_modes)
    duplex_policy.Set(kAllowedMode,
                      static_cast<int>(ptr->allowed_duplex_modes));
  if (ptr->default_duplex_mode !=
      printing::mojom::DuplexModeRestriction::kUnset)
    duplex_policy.Set(kDefaultMode, static_cast<int>(ptr->default_duplex_mode));
  if (!duplex_policy.empty())
    policies.Set(kDuplex, std::move(duplex_policy));

  base::Value::Dict pin_policy;
  if (ptr->allowed_pin_modes != printing::mojom::PinModeRestriction::kUnset)
    pin_policy.Set(kAllowedMode, static_cast<int>(ptr->allowed_pin_modes));
  if (ptr->default_pin_mode != printing::mojom::PinModeRestriction::kUnset)
    pin_policy.Set(kDefaultMode, static_cast<int>(ptr->default_pin_mode));
  if (!pin_policy.empty())
    policies.Set(kPin, std::move(pin_policy));

  base::Value::Dict print_as_image_for_pdf_default_policy;
  if (ptr->default_print_pdf_as_image !=
      crosapi::mojom::Policies::OptionalBool::kUnset) {
    print_as_image_for_pdf_default_policy.Set(
        kDefaultMode, ptr->default_print_pdf_as_image ==
                          crosapi::mojom::Policies::OptionalBool::kTrue);
  }
  if (!print_as_image_for_pdf_default_policy.empty()) {
    policies.Set(kPrintPdfAsImage,
                 std::move(print_as_image_for_pdf_default_policy));
  }

  return policies;
}

#else
base::Value::Dict GetPolicies(const PrefService& prefs) {
  base::Value::Dict policies;

  base::Value::Dict header_footer_policy;
  if (prefs.HasPrefPath(prefs::kPrintHeaderFooter)) {
    if (prefs.IsManagedPreference(prefs::kPrintHeaderFooter)) {
      header_footer_policy.Set(kAllowedMode,
                               prefs.GetBoolean(prefs::kPrintHeaderFooter));
    } else {
      header_footer_policy.Set(kDefaultMode,
                               prefs.GetBoolean(prefs::kPrintHeaderFooter));
    }
  }
  if (!header_footer_policy.empty())
    policies.Set(kHeaderFooter, std::move(header_footer_policy));

  base::Value::Dict background_graphics_policy;
  if (prefs.HasPrefPath(prefs::kPrintingAllowedBackgroundGraphicsModes)) {
    background_graphics_policy.Set(
        kAllowedMode,
        prefs.GetInteger(prefs::kPrintingAllowedBackgroundGraphicsModes));
  }
  if (prefs.HasPrefPath(prefs::kPrintingBackgroundGraphicsDefault)) {
    background_graphics_policy.Set(
        kDefaultMode,
        prefs.GetInteger(prefs::kPrintingBackgroundGraphicsDefault));
  }
  if (!background_graphics_policy.empty())
    policies.Set(kCssBackground, std::move(background_graphics_policy));

  base::Value::Dict paper_size_policy;
  std::optional<gfx::Size> default_paper_size = ParsePaperSizeDefault(prefs);
  if (default_paper_size.has_value()) {
    base::Value::Dict default_paper_size_value;
    default_paper_size_value.Set(kPaperSizeWidth,
                                 default_paper_size.value().width());
    default_paper_size_value.Set(kPaperSizeHeight,
                                 default_paper_size.value().height());
    paper_size_policy.Set(kDefaultMode, std::move(default_paper_size_value));
  }
  if (!paper_size_policy.empty())
    policies.Set(kMediaSize, std::move(paper_size_policy));

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  base::Value::Dict print_as_image_available_for_pdf_policy;
  if (prefs.HasPrefPath(prefs::kPrintPdfAsImageAvailability)) {
    print_as_image_available_for_pdf_policy.Set(
        kAllowedMode, prefs.GetBoolean(prefs::kPrintPdfAsImageAvailability));
  }
  if (!print_as_image_available_for_pdf_policy.empty()) {
    policies.Set(kPrintPdfAsImageAvailability,
                 std::move(print_as_image_available_for_pdf_policy));
  }
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

  base::Value::Dict print_as_image_for_pdf_default_policy;
  if (prefs.HasPrefPath(prefs::kPrintPdfAsImageDefault)) {
    print_as_image_for_pdf_default_policy.Set(
        kDefaultMode, prefs.GetBoolean(prefs::kPrintPdfAsImageDefault));
  }
  if (!print_as_image_for_pdf_default_policy.empty()) {
    policies.Set(kPrintPdfAsImage,
                 std::move(print_as_image_for_pdf_default_policy));
  }

  return policies;
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace

PrintPreviewHandler::PrintPreviewHandler() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  DCHECK(crosapi::CrosapiManager::IsInitialized());
  local_printer_ =
      crosapi::CrosapiManager::Get()->crosapi_ash()->local_printer_ash();
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  if (service->IsAvailable<crosapi::mojom::LocalPrinter>()) {
    local_printer_ = service->GetRemote<crosapi::mojom::LocalPrinter>().get();
    local_printer_version_ =
        service->GetInterfaceVersion<crosapi::mojom::LocalPrinter>();
  } else {
    LOG(ERROR) << "Local printer not available";
  }
#endif
  ReportUserActionHistogram(UserActionBuckets::kPreviewStarted);
}

PrintPreviewHandler::~PrintPreviewHandler() = default;

void PrintPreviewHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getPrinters",
      base::BindRepeating(&PrintPreviewHandler::HandleGetPrinters,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getPreview", base::BindRepeating(&PrintPreviewHandler::HandleGetPreview,
                                        base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "doPrint", base::BindRepeating(&PrintPreviewHandler::HandleDoPrint,
                                     base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getPrinterCapabilities",
      base::BindRepeating(&PrintPreviewHandler::HandleGetPrinterCapabilities,
                          base::Unretained(this)));
#if BUILDFLAG(ENABLE_BASIC_PRINT_DIALOG)
  web_ui()->RegisterMessageCallback(
      "showSystemDialog",
      base::BindRepeating(&PrintPreviewHandler::HandleShowSystemDialog,
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
      "managePrinters",
      base::BindRepeating(&PrintPreviewHandler::HandleManagePrinters,
                          base::Unretained(this)));
}

void PrintPreviewHandler::OnJavascriptAllowed() {
  print_preview_ui()->SetPreviewUIId();
  ReadPrinterTypeDenyListFromPrefs();
}

void PrintPreviewHandler::OnJavascriptDisallowed() {
  // Normally the handler and print preview will be destroyed together, but
  // this is necessary for refresh or navigation from the chrome://print page.
  weak_factory_.InvalidateWeakPtrs();
  print_preview_ui()->ClearPreviewUIId();
  preview_callbacks_.clear();
  preview_failures_.clear();
  printer_type_deny_list_.clear();
}

WebContents* PrintPreviewHandler::preview_web_contents() {
  return web_ui()->GetWebContents();
}

PrefService* PrintPreviewHandler::GetPrefs() {
  auto* prefs = Profile::FromWebUI(web_ui())->GetPrefs();
  DCHECK(prefs);
  return prefs;
}

void PrintPreviewHandler::ReadPrinterTypeDenyListFromPrefs() {
#if BUILDFLAG(IS_CHROMEOS)
  if (!local_printer_)
    return;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (local_printer_version_ <
      int{crosapi::mojom::LocalPrinter::MethodMinVersions::
              kGetPrinterTypeDenyListMinVersion}) {
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  local_printer_->GetPrinterTypeDenyList(
      base::BindOnce(&PrintPreviewHandler::OnPrinterTypeDenyListReady,
                     weak_factory_.GetWeakPtr()));
  return;
#else
  PrefService* prefs = GetPrefs();
  if (!prefs->HasPrefPath(prefs::kPrinterTypeDenyList))
    return;

  const base::Value::List& deny_list_from_prefs =
      prefs->GetList(prefs::kPrinterTypeDenyList);

  std::vector<mojom::PrinterType> deny_list;
  deny_list.reserve(deny_list_from_prefs.size());
  for (const base::Value& deny_list_value : deny_list_from_prefs) {
    const std::string& deny_list_str = deny_list_value.GetString();
    mojom::PrinterType printer_type;
    if (deny_list_str == "extension")
      printer_type = mojom::PrinterType::kExtension;
    else if (deny_list_str == "pdf")
      printer_type = mojom::PrinterType::kPdf;
    else if (deny_list_str == "local")
      printer_type = mojom::PrinterType::kLocal;
    else
      continue;

    deny_list.push_back(printer_type);
  }
  OnPrinterTypeDenyListReady(deny_list);
#endif  // BUILDFLAG(IS_CHROMEOS)
}

void PrintPreviewHandler::OnPrinterTypeDenyListReady(
    const std::vector<mojom::PrinterType>& deny_list_types) {
  printer_type_deny_list_ = deny_list_types;
}

PrintPreviewUI* PrintPreviewHandler::print_preview_ui() {
  return web_ui()->GetController()->GetAs<PrintPreviewUI>();
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

void PrintPreviewHandler::HandleGetPrinters(const base::Value::List& args) {
  CHECK_GE(args.size(), 2u);
  const std::string& callback_id = args[0].GetString();
  CHECK(!callback_id.empty());
  int type = args[1].GetInt();
  mojom::PrinterType printer_type = static_cast<mojom::PrinterType>(type);

  // Immediately resolve the callback without fetching printers if the printer
  // type is on the deny list.
  if (base::Contains(printer_type_deny_list_, printer_type)) {
    ResolveJavascriptCallback(base::Value(callback_id), base::Value());
    return;
  }

  // Make sure all in progress requests are canceled before new printer search
  // starts.
  PrinterHandler* handler = GetPrinterHandler(printer_type);
  handler->Reset();
  handler->StartGetPrinters(
      base::BindRepeating(&PrintPreviewHandler::OnAddedPrinters,
                          weak_factory_.GetWeakPtr(), printer_type),
      base::BindOnce(&PrintPreviewHandler::OnGetPrintersDone,
                     weak_factory_.GetWeakPtr(), callback_id, printer_type,
                     base::TimeTicks::Now()));
}

void PrintPreviewHandler::HandleGetPrinterCapabilities(
    const base::Value::List& args) {
  // Validate that we have a valid callback_id
  if (args.size() < 1 || !args[0].is_string() || args[0].GetString().empty()) {
    RejectJavascriptCallback(base::Value(""), base::Value());
    return;
  }
  // If we got here, we know that we have at least one string element.
  const std::string& callback_id = args[0].GetString();
  if (args.size() < 3) {
    RejectJavascriptCallback(base::Value(callback_id), base::Value());
    return;
  }
  const std::string* printer_name = args[1].GetIfString();
  std::optional<int> type = args[2].GetIfInt();
  if (!printer_name || printer_name->empty() || !type.has_value()) {
    RejectJavascriptCallback(base::Value(callback_id), base::Value());
    return;
  }
  mojom::PrinterType printer_type = static_cast<mojom::PrinterType>(*type);

  // Reject the callback if the printer type is on the deny list.
  if (base::Contains(printer_type_deny_list_, printer_type)) {
    RejectJavascriptCallback(base::Value(callback_id), base::Value());
    return;
  }

  PrinterHandler* handler = GetPrinterHandler(printer_type);
  handler->StartGetCapability(
      *printer_name,
      base::BindOnce(&PrintPreviewHandler::SendPrinterCapabilities,
                     weak_factory_.GetWeakPtr(), callback_id));
}

void PrintPreviewHandler::HandleGetPreview(const base::Value::List& args) {
  DCHECK_EQ(2U, args.size());

  // All of the conditions below should be guaranteed by the print preview
  // javascript.
  const std::string& callback_id = args[0].GetString();
  CHECK(!callback_id.empty());
  const std::string& json_str = args[1].GetString();
  base::Value::Dict settings = GetSettingsDictionary(json_str);
  int request_id = settings.FindInt(kPreviewRequestID).value();
  CHECK_GT(request_id, -1);
  mojom::PrinterType printer_type = static_cast<mojom::PrinterType>(
      settings.FindInt(kSettingPrinterType).value());
  CHECK_NE(printer_type, mojom::PrinterType::kCloudDeprecated);

  CHECK(!base::Contains(preview_callbacks_, request_id));
  preview_callbacks_[request_id] = callback_id;
  print_preview_ui()->OnPrintPreviewRequest(request_id);
  // Add an additional key in order to identify |print_preview_ui| later on
  // when calling PrintPreviewUI::ShouldCancelRequest() on the IO thread.
  settings.Set(kPreviewUIID,
               print_preview_ui()->GetIDForPrintPreviewUI().value());

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
  std::optional<bool> display_header_footer_opt =
      settings.FindBool(kSettingHeaderFooterEnabled);
  DCHECK(display_header_footer_opt);
  if (display_header_footer_opt.value_or(false)) {
    settings.Set(kSettingHeaderFooterTitle, initiator->GetTitle());

    GURL::Replacements url_sanitizer;
    url_sanitizer.ClearUsername();
    url_sanitizer.ClearPassword();
    const GURL& initiator_url = initiator->GetLastCommittedURL();
    settings.Set(kSettingHeaderFooterURL,
                 url_formatter::FormatUrl(
                     initiator_url.ReplaceComponents(url_sanitizer)));
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

void PrintPreviewHandler::HandleDoPrint(const base::Value::List& args) {
  CHECK(args[0].is_string());
  const std::string& callback_id = args[0].GetString();
  CHECK(!callback_id.empty());
  CHECK(args[1].is_string());
  const std::string& json_str = args[1].GetString();

  base::Value::Dict settings = GetSettingsDictionary(json_str);
  const UserActionBuckets user_action = DetermineUserAction(settings);

  int page_count = settings.FindInt(kSettingPreviewPageCount).value_or(-1);
  if (page_count <= 0) {
    RejectJavascriptCallback(base::Value(callback_id),
                             base::Value("NO_PAGE_COUNT"));
    return;
  }

  scoped_refptr<base::RefCountedMemory> data;
  print_preview_ui()->GetPrintPreviewDataForIndex(
      COMPLETE_PREVIEW_DOCUMENT_INDEX, &data);
  if (!data) {
    // Nothing to print, no preview available.
    RejectJavascriptCallback(base::Value(callback_id), base::Value("NO_DATA"));
    return;
  }
  DCHECK(data->size());

  // After validating |settings|, record metrics.
  const mojom::RequestPrintPreviewParams* request_params = GetRequestParams();
  CHECK(request_params);
  bool is_pdf = !request_params->is_modifiable;
  if (last_preview_settings_.has_value())
    ReportPrintSettingsStats(settings, last_preview_settings_.value(), is_pdf);
  {
    PrintDocumentTypeBuckets doc_type =
        is_pdf ? PrintDocumentTypeBuckets::kPdfDocument
               : PrintDocumentTypeBuckets::kHtmlDocument;
    ReportPrintDocumentTypeHistograms(doc_type);
  }
  ReportUserActionHistogram(user_action);

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
  std::string device_name = *settings.FindString(kSettingDeviceName);

  using enterprise_data_protection::PrintScanningContext;
  auto scan_context =
      settings.FindBool(kSettingShowSystemDialog).value_or(false)
          ? PrintScanningContext::kSystemPrintAfterPreview
          : PrintScanningContext::kNormalPrintAfterPreview;

#if BUILDFLAG(IS_MAC)
  if (settings.FindBool(kSettingOpenPDFInPreview).value_or(false)) {
    // This override only affects reporting of content analysis violations, and
    // the rest of the printing stack is expected to use the same device name
    // present in `settings` if content analysis allows printing.
    device_name =
        l10n_util::GetStringUTF8(IDS_PRINT_PREVIEW_OPEN_PDF_IN_PREVIEW_APP);
    scan_context = PrintScanningContext::kOpenPdfInPreview;
  }
#endif  // BUILDFLAG(IS_MAC)

  auto on_verdict =
      base::BindOnce(&PrintPreviewHandler::OnVerdictByEnterprisePolicy,
                     weak_factory_.GetWeakPtr(), user_action,
                     std::move(settings), data, callback_id);

  auto hide_preview = base::BindOnce(&PrintPreviewHandler::OnHidePreviewDialog,
                                     weak_factory_.GetWeakPtr());

  enterprise_data_protection::PrintIfAllowedByPolicy(
      data, GetInitiator(), std::move(device_name), scan_context,
      std::move(on_verdict), std::move(hide_preview));

#else
  FinishHandleDoPrint(user_action, std::move(settings), data, callback_id);
#endif  // BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
}

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
void PrintPreviewHandler::OnVerdictByEnterprisePolicy(
    UserActionBuckets user_action,
    base::Value::Dict settings,
    scoped_refptr<base::RefCountedMemory> data,
    const std::string& callback_id,
    bool allowed) {
  if (allowed) {
    FinishHandleDoPrint(user_action, std::move(settings), data, callback_id);
  } else {
    OnPrintResult(callback_id, base::Value("NOT_ALLOWED"));
  }
}

void PrintPreviewHandler::OnHidePreviewDialog() {
  print_preview_ui()->OnHidePreviewDialog();
}
#endif  // BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)

void PrintPreviewHandler::FinishHandleDoPrint(
    UserActionBuckets user_action,
    base::Value::Dict settings,
    scoped_refptr<base::RefCountedMemory> data,
    const std::string& callback_id) {
  PrinterHandler* handler =
      GetPrinterHandler(GetPrinterTypeForUserAction(user_action));
  handler->StartPrint(print_preview_ui()->initiator_title(),
                      std::move(settings), data,
                      base::BindOnce(&PrintPreviewHandler::OnPrintResult,
                                     weak_factory_.GetWeakPtr(), callback_id));
}

void PrintPreviewHandler::HandleHidePreview(const base::Value::List& /*args*/) {
  print_preview_ui()->OnHidePreviewDialog();
}

void PrintPreviewHandler::HandleCancelPendingPrintRequest(
    const base::Value::List& /*args*/) {
  WebContents* initiator = GetInitiator();
  if (initiator)
    ClearInitiatorDetails();
  ShowPrintErrorDialogForGenericError();
}

void PrintPreviewHandler::HandleSaveAppState(const base::Value::List& args) {
  std::string data_to_save;
  PrintPreviewStickySettings* sticky_settings =
      PrintPreviewStickySettings::GetInstance();
  if (args[0].is_string())
    data_to_save = args[0].GetString();
  if (!data_to_save.empty())
    sticky_settings->StoreAppState(data_to_save);
  sticky_settings->SaveInPrefs(GetPrefs());
}

#if BUILDFLAG(ENABLE_BASIC_PRINT_DIALOG)
void PrintPreviewHandler::HandleShowSystemDialog(
    const base::Value::List& /*args*/) {
  ReportUserActionHistogram(
      UserActionBuckets::kFallbackToAdvancedSettingsDialog);

  WebContents* initiator = GetInitiator();
  if (!initiator)
    return;

  auto weak_this = weak_factory_.GetWeakPtr();
  auto* print_view_manager = PrintViewManager::FromWebContents(initiator);
  print_view_manager->PrintForSystemDialogNow(base::BindOnce(
      &PrintPreviewHandler::ClosePreviewDialog, weak_factory_.GetWeakPtr()));
  if (!weak_this)
    return;

  // Cancel the pending preview request if exists.
  print_preview_ui()->OnCancelPendingPreviewRequest();
}
#endif

void PrintPreviewHandler::HandleClosePreviewDialog(
    const base::Value::List& /*args*/) {
  ReportUserActionHistogram(UserActionBuckets::kCancel);
}

void PrintPreviewHandler::GetLocaleInformation(base::Value::Dict* settings) {
  // Getting the measurement system based on the locale.
  UErrorCode errorCode = U_ZERO_ERROR;
  const char* locale = g_browser_process->GetApplicationLocale().c_str();
  settings->Set(kUiLocale, std::string(locale));
  UMeasurementSystem system = ulocdata_getMeasurementSystem(locale, &errorCode);
  // On error, assume the units are SI.
  // Since the only measurement units print preview's WebUI cares about are
  // those for measuring distance, assume anything non-US is SI.
  if (errorCode > U_ZERO_ERROR || system != UMS_US)
    system = UMS_SI;

  // Getting the number formatting based on the locale and writing to
  // dictionary.
  std::u16string number_format = base::FormatDouble(123456.78, 2);
  size_t thousands_pos = number_format.find('3') + 1;
  std::u16string thousands_delimiter = number_format.substr(thousands_pos, 1);
  if (number_format[thousands_pos] == '4')
    thousands_delimiter.clear();
  size_t decimal_pos = number_format.find('6') + 1;
  DCHECK_NE(number_format[decimal_pos], '7');
  std::u16string decimal_delimiter = number_format.substr(decimal_pos, 1);
  settings->Set(kDecimalDelimiter, decimal_delimiter);
  settings->Set(kThousandsDelimiter, thousands_delimiter);
  settings->Set(kUnitType, system);
}

void PrintPreviewHandler::HandleGetInitialSettings(
    const base::Value::List& args) {
  CHECK(args[0].is_string());
  const std::string& callback_id = args[0].GetString();
  CHECK(!callback_id.empty());

  AllowJavascript();

  PrinterHandler* handler = GetPrinterHandler(mojom::PrinterType::kLocal);
  base::OnceCallback<void(base::Value::Dict, const std::string&)> cb =
      base::BindOnce(&PrintPreviewHandler::SendInitialSettings,
                     weak_factory_.GetWeakPtr(), callback_id);
#if BUILDFLAG(IS_CHROMEOS)
  if (!local_printer_) {
    LOG(ERROR) << "Local printer not available";
    handler->GetDefaultPrinter(
        base::BindOnce(std::move(cb), base::Value::Dict()));
    return;
  }
  local_printer_->GetPolicies(
      base::BindOnce(PoliciesToValue)
          .Then(base::BindOnce(
              [](base::OnceCallback<void(base::Value::Dict, const std::string&)>
                     cb,
                 PrinterHandler* handler, base::Value::Dict policies) {
                handler->GetDefaultPrinter(
                    base::BindOnce(std::move(cb), std::move(policies)));
              },
              std::move(cb), handler)));
#else
  handler->GetDefaultPrinter(
      base::BindOnce(std::move(cb), GetPolicies(*GetPrefs())));
#endif
}

void PrintPreviewHandler::SendInitialSettings(
    const std::string& callback_id,
    base::Value::Dict policies,
    const std::string& default_printer) {
  const mojom::RequestPrintPreviewParams* request_params = GetRequestParams();
  mojom::RequestPrintPreviewParams dummy_params;
  if (!request_params) {
    // This only happens with a direct navigation to chrome://print, which can
    // happen in some tests. Just use `dummy_params` to set up the test with
    // some sane values, so it does not crash.
    dummy_params.is_modifiable = true;
    request_params = &dummy_params;
  }

  base::Value::Dict initial_settings;
  initial_settings.Set(kDocumentTitle, print_preview_ui()->initiator_title());
  initial_settings.Set(kSettingPreviewModifiable,
                       request_params->is_modifiable);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  bool source_is_arc = request_params->is_from_arc;
#else
  bool source_is_arc = false;
#endif
  initial_settings.Set(kSettingPreviewIsFromArc, source_is_arc);
  initial_settings.Set(kSettingPrinterName, default_printer);
  initial_settings.Set(kDocumentHasSelection, request_params->has_selection);
  initial_settings.Set(kSettingShouldPrintSelectionOnly,
                       request_params->selection_only);
  PrefService* prefs = GetPrefs();
  PrintPreviewStickySettings* sticky_settings =
      PrintPreviewStickySettings::GetInstance();
  sticky_settings->RestoreFromPrefs(prefs);
  if (sticky_settings->printer_app_state()) {
    initial_settings.Set(kAppState, *sticky_settings->printer_app_state());
  } else {
    initial_settings.Set(kAppState, base::Value());
  }

  if (!policies.empty())
    initial_settings.Set(kPolicies, std::move(policies));

  initial_settings.Set(
      kPdfPrinterDisabled,
      base::Contains(printer_type_deny_list_, mojom::PrinterType::kPdf));

  const bool destinations_managed =
      !printer_type_deny_list_.empty() &&
      prefs->IsManagedPreference(prefs::kPrinterTypeDenyList);
  initial_settings.Set(kDestinationsManaged, destinations_managed);

  base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
  initial_settings.Set(kIsInKioskAutoPrintMode,
                       cmdline->HasSwitch(switches::kKioskModePrinting));
  initial_settings.Set(kIsInAppKioskMode, IsRunningInForcedAppMode());
  const std::string rules_str =
      prefs->GetString(prefs::kPrintPreviewDefaultDestinationSelectionRules);
  if (rules_str.empty()) {
    initial_settings.Set(kDefaultDestinationSelectionRules, base::Value());
  } else {
    initial_settings.Set(kDefaultDestinationSelectionRules, rules_str);
  }

  GetLocaleInformation(&initial_settings);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  drive::DriveIntegrationService* drive_service =
      drive::DriveIntegrationServiceFactory::GetForProfile(
          Profile::FromWebUI(web_ui()));
  initial_settings.Set(kIsDriveMounted,
                       drive_service && drive_service->IsMounted());
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  // The "Save to Google Drive" option is only allowed for the primary profile
  // in the Lacros browser.
  if (Profile::FromWebUI(web_ui())->IsMainProfile()) {
    base::FilePath drive_path;
    initial_settings.Set(
        kIsDriveMounted,
        chrome::GetDriveFsMountPointPath(&drive_path) && !drive_path.empty());
  }
#endif

  ResolveJavascriptCallback(base::Value(callback_id), initial_settings);
}

void PrintPreviewHandler::ClosePreviewDialog() {
  print_preview_ui()->OnClosePrintPreviewDialog();
}

void PrintPreviewHandler::SendPrinterCapabilities(
    const std::string& callback_id,
    base::Value::Dict settings_info) {
  // Check that |settings_info| is valid.
  base::Value::Dict* settings = settings_info.FindDict(kSettingCapabilities);
  if (settings) {
    FilterContinuousFeedMediaSizes(*settings);
    VLOG(1) << "Get printer capabilities finished";
    ResolveJavascriptCallback(base::Value(callback_id), settings_info);
    return;
  }

  VLOG(1) << "Get printer capabilities failed";
  RejectJavascriptCallback(base::Value(callback_id), base::Value());
}

WebContents* PrintPreviewHandler::GetInitiator() {
  auto* dialog_controller = PrintPreviewDialogController::GetInstance();
  CHECK(dialog_controller);
  return dialog_controller->GetInitiator(preview_web_contents());
}

const mojom::RequestPrintPreviewParams*
PrintPreviewHandler::GetRequestParams() {
  auto* dialog_controller = PrintPreviewDialogController::GetInstance();
  CHECK(dialog_controller);
  return dialog_controller->GetRequestParams(preview_web_contents());
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
    base::Value::Dict layout,
    bool all_pages_have_custom_size,
    bool all_pages_have_custom_orientation,
    int request_id) {
  if (!ShouldReceiveRendererMessage(request_id))
    return;

  FireWebUIListener("page-layout-ready", std::move(layout),
                    base::Value(all_pages_have_custom_size),
                    base::Value(all_pages_have_custom_orientation));
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
  base::Value::List empty;
  HandleCancelPendingPrintRequest(empty);
}

void PrintPreviewHandler::ClearInitiatorDetails() {
  WebContents* initiator = GetInitiator();
  if (!initiator)
    return;

  // We no longer require the initiator details. Remove those details associated
  // with the preview dialog to allow the initiator to create another preview
  // dialog.
  auto* dialog_controller = PrintPreviewDialogController::GetInstance();
  CHECK(dialog_controller);
  dialog_controller->EraseInitiatorInfo(preview_web_contents());
}

PrinterHandler* PrintPreviewHandler::GetPrinterHandler(
    mojom::PrinterType printer_type) {
  if (printer_type == mojom::PrinterType::kExtension) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // When Lacros is enabled, uses the ExtensionPrinterHandlerAdapterAsh to
    // talk to Lacros's extension printers.
    if (ash::features::IsLacrosExtensionPrintingEnabled() &&
        crosapi::browser_util::IsLacrosEnabled()) {
      if (!extension_printer_handler_adapter_) {
        extension_printer_handler_adapter_ =
            std::make_unique<ExtensionPrinterHandlerAdapterAsh>();
      }
      return extension_printer_handler_adapter_.get();
    }
#endif
    if (!extension_printer_handler_) {
      extension_printer_handler_ = PrinterHandler::CreateForExtensionPrinters(
          Profile::FromWebUI(web_ui()));
    }
    return extension_printer_handler_.get();
  }
  if (printer_type == mojom::PrinterType::kPdf) {
    if (!pdf_printer_handler_) {
      pdf_printer_handler_ = PrinterHandler::CreateForPdfPrinter(
          Profile::FromWebUI(web_ui()), preview_web_contents(),
          PrintPreviewStickySettings::GetInstance());
    }
    return pdf_printer_handler_.get();
  }
  if (printer_type == mojom::PrinterType::kLocal) {
    if (!local_printer_handler_) {
      local_printer_handler_ = PrinterHandler::CreateForLocalPrinters(
          preview_web_contents(), Profile::FromWebUI(web_ui()));
    }
    return local_printer_handler_.get();
  }
  NOTREACHED();
}

PdfPrinterHandler* PrintPreviewHandler::GetPdfPrinterHandler() {
  return static_cast<PdfPrinterHandler*>(
      GetPrinterHandler(mojom::PrinterType::kPdf));
}

void PrintPreviewHandler::OnAddedPrinters(mojom::PrinterType printer_type,
                                          base::Value::List printers) {
  DCHECK(printer_type == mojom::PrinterType::kExtension ||
         printer_type == mojom::PrinterType::kLocal);
  // Save the count here, as `printers` gets moved below.
  const size_t printer_count = printers.size();
  DCHECK(printer_count);
  FireWebUIListener("printers-added",
                    base::Value(static_cast<int>(printer_type)), printers);

  if (printer_type == mojom::PrinterType::kLocal &&
      !has_logged_printers_count_) {
    ReportNumberOfPrinters(printer_count);
    has_logged_printers_count_ = true;
  }
}

void PrintPreviewHandler::OnGetPrintersDone(const std::string& callback_id,
                                            mojom::PrinterType printer_type,
                                            const base::TimeTicks& start_time) {
  RecordGetPrintersTimeHistogram(printer_type, start_time);
  ResolveJavascriptCallback(base::Value(callback_id), base::Value());
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

void PrintPreviewHandler::BadMessageReceived() {
  bad_message::ReceivedBadMessage(
      GetInitiator()->GetPrimaryMainFrame()->GetProcess(),
      bad_message::BadMessageReason::PPH_EXTRA_PREVIEW_MESSAGE);
#if DCHECK_IS_ON()
  // TODO(crbug.com/40870686): Remove this once the bug is fixed.
  base::debug::StackTrace().Print();
#endif
}

void PrintPreviewHandler::FileSelectedForTesting(const base::FilePath& path,
                                                 int index) {
  GetPdfPrinterHandler()->FileSelected(ui::SelectedFileInfo(path), index);
}

void PrintPreviewHandler::SetPdfSavedClosureForTesting(
    base::OnceClosure closure) {
  GetPdfPrinterHandler()->SetPdfSavedClosureForTesting(std::move(closure));
}

void PrintPreviewHandler::HandleManagePrinters(const base::Value::List& args) {
#if BUILDFLAG(IS_CHROMEOS)
  if (!local_printer_) {
    LOG(ERROR) << "Local printer not available";
    return;
  }
  local_printer_->ShowSystemPrintSettings(base::DoNothing());
#else
  printing::PrinterManagerDialog::ShowPrinterManagerDialog();
#endif
}

}  // namespace printing
