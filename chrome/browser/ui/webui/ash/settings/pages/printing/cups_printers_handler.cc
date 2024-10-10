// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/printing/cups_printers_handler.h"

#include <optional>
#include <set>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chrome/browser/ash/printing/cups_printers_manager.h"
#include "chrome/browser/ash/printing/ppd_provider_factory.h"
#include "chrome/browser/ash/printing/printer_event_tracker.h"
#include "chrome/browser/ash/printing/printer_event_tracker_factory.h"
#include "chrome/browser/ash/printing/printer_info.h"
#include "chrome/browser/ash/printing/server_printers_fetcher.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/local_discovery/endpoint_resolver.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/webui/ash/settings/pages/printing/server_printer_url_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/printing/cups_printer_status.h"
#include "chromeos/printing/ppd_line_reader.h"
#include "chromeos/printing/printer_configuration.h"
#include "chromeos/printing/printer_translator.h"
#include "chromeos/printing/printing_constants.h"
#include "chromeos/printing/uri.h"
#include "components/device_event_log/device_event_log.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "google_apis/google_api_keys.h"
#include "net/base/filename_util.h"
#include "net/base/ip_endpoint.h"
#include "printing/backend/print_backend.h"
#include "printing/printer_status.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/shell_dialogs/selected_file_info.h"
#include "url/gurl.h"

namespace ash::settings {

namespace {

using ::chromeos::PpdProvider;
using ::chromeos::Printer;
using ::chromeos::PrinterClass;
using ::chromeos::Uri;
using ::printing::PrinterQueryResult;

constexpr int kPpdMaxLineLength = 255;

constexpr char kNearbyAutomaticPrintersHistogramName[] =
    "Printing.CUPS.NearbyNetworkAutomaticPrintersCount";
constexpr char kNearbyDiscoveredPrintersHistogramName[] =
    "Printing.CUPS.NearbyNetworkDiscoveredPrintersCount";
constexpr char kSavedPrintersCountHistogramName[] =
    "Printing.CUPS.SavedPrintersCount";

// Log if the IPP attributes request was succesful.
void RecordIppQueryResult(const PrinterQueryResult& result) {
  bool reachable = result != PrinterQueryResult::kHostnameResolution &&
                   result != PrinterQueryResult::kUnreachable;
  UMA_HISTOGRAM_BOOLEAN("Printing.CUPS.IppDeviceReachable", reachable);

  if (reachable) {
    // Only record whether the query was successful if we reach the printer.
    bool query_success = (result == PrinterQueryResult::kSuccess);
    UMA_HISTOGRAM_BOOLEAN("Printing.CUPS.IppAttributesSuccess", query_success);
  }
}

// Query an IPP printer to check for autoconf support where the printer is
// located at |printer_uri|.  Results are reported through |callback|.  The
// scheme of |printer_uri| must equal "ipp" or "ipps".
void QueryAutoconf(const Uri& uri, PrinterInfoCallback callback) {
  QueryIppPrinter(
      uri.GetHostEncoded(), uri.GetPort(), uri.GetPathEncodedAsString(),
      uri.GetScheme() == chromeos::kIppsScheme, std::move(callback));
}

// Returns the list of |printers| formatted as a CupsPrintersList.
base::Value::Dict BuildCupsPrintersList(const std::vector<Printer>& printers) {
  base::Value::List printers_list;
  for (const Printer& printer : printers) {
    // Some of these printers could be invalid but we want to allow the user
    // to edit them. crbug.com/778383
    printers_list.Append(GetCupsPrinterInfo(printer));
  }

  base::Value::Dict response;
  response.Set("printerList", std::move(printers_list));
  return response;
}

// Generates a Printer from |printer_dict| where |printer_dict| is a
// CupsPrinterInfo representation.  If any of the required fields are missing,
// returns nullptr.
std::unique_ptr<chromeos::Printer> DictToPrinter(
    const base::Value::Dict& printer_dict) {
  const std::string* printer_id = printer_dict.FindString("printerId");
  const std::string* printer_name = printer_dict.FindString("printerName");
  const std::string* printer_description =
      printer_dict.FindString("printerDescription");
  const std::string* printer_make_and_model =
      printer_dict.FindString("printerMakeAndModel");
  const std::string* printer_address =
      printer_dict.FindString("printerAddress");
  const std::string* printer_protocol =
      printer_dict.FindString("printerProtocol");
  const std::string* print_server_uri =
      printer_dict.FindString("printServerUri");
  if (!printer_id || !printer_name || !printer_description ||
      !printer_make_and_model || !printer_address || !printer_protocol ||
      !print_server_uri) {
    return nullptr;
  }

  std::string printer_queue;
  // The protocol "socket" does not allow path.
  if (*printer_protocol != "socket") {
    if (const std::string* ptr = printer_dict.FindString("printerQueue")) {
      printer_queue = *ptr;
      // Path must start from '/' character.
      if (!printer_queue.empty() && printer_queue.front() != '/') {
        printer_queue.insert(0, "/");
      }
    }
  }

  auto printer = std::make_unique<chromeos::Printer>(*printer_id);
  printer->set_display_name(*printer_name);
  printer->set_description(*printer_description);
  printer->set_make_and_model(*printer_make_and_model);
  printer->set_print_server_uri(*print_server_uri);

  Uri uri(*printer_protocol + url::kStandardSchemeSeparator + *printer_address +
          printer_queue);
  if (uri.GetLastParsingError().status != Uri::ParserStatus::kNoErrors) {
    PRINTER_LOG(ERROR) << "Uri parse error: "
                       << static_cast<int>(uri.GetLastParsingError().status);
    return nullptr;
  }

  std::string message;
  if (!printer->SetUri(uri, &message)) {
    PRINTER_LOG(ERROR) << "Incorrect uri: " << message;
    return nullptr;
  }

  return printer;
}

std::string ReadFileToStringWithMaxSize(const base::FilePath& path,
                                        int max_size) {
  std::string contents;
  // This call can fail, but it doesn't matter for our purposes. If it fails,
  // we simply return an empty string for the contents, and it will be rejected
  // as an invalid PPD.
  base::ReadFileToStringWithMaxSize(path, &contents, max_size);
  return contents;
}

// Determines whether changing the URI in |existing_printer| to the URI in
// |new_printer| would be valid. Network printers are not allowed to change
// their protocol to a non-network protocol, but can change anything else.
// Non-network printers are not allowed to change anything in their URI.
bool IsValidUriChange(const Printer& existing_printer,
                      const Printer& new_printer) {
  if (new_printer.GetProtocol() == Printer::PrinterProtocol::kUnknown) {
    return false;
  }
  if (existing_printer.HasNetworkProtocol()) {
    return new_printer.HasNetworkProtocol();
  }
  return existing_printer.uri() == new_printer.uri();
}

// Assumes |info| is a dictionary.
void SetPpdReference(const Printer::PpdReference& ppd_ref,
                     base::Value::Dict* info) {
  if (!ppd_ref.user_supplied_ppd_url.empty()) {
    info->Set("ppdRefUserSuppliedPpdUrl", ppd_ref.user_supplied_ppd_url);
  } else if (!ppd_ref.effective_make_and_model.empty()) {
    info->Set("ppdRefEffectiveMakeAndModel", ppd_ref.effective_make_and_model);
  } else {  // Must be autoconf, shouldn't be possible
    NOTREACHED_IN_MIGRATION() << "Succeeded in PPD matching without emm";
  }
}

Printer::PpdReference GetPpdReference(const base::Value::Dict* info) {
  auto* user_supplied_ppd_url =
      info->FindByDottedPath("printerPpdReference.userSuppliedPPDUrl");
  auto* effective_make_and_model =
      info->FindByDottedPath("printerPpdReference.effectiveMakeAndModel");
  auto* autoconf = info->FindByDottedPath("printerPpdReference.autoconf");

  Printer::PpdReference ret;

  if (user_supplied_ppd_url) {
    ret.user_supplied_ppd_url = user_supplied_ppd_url->GetString();
  }

  if (effective_make_and_model) {
    ret.effective_make_and_model = effective_make_and_model->GetString();
  }

  if (autoconf) {
    ret.autoconf = autoconf->GetBool();
  }

  return ret;
}

GURL GenerateHttpCupsServerUrl(const GURL& server_url) {
  GURL::Replacements replacement;
  replacement.SetSchemeStr("http");
  replacement.SetPortStr("631");
  return server_url.ReplaceComponents(replacement);
}

}  // namespace

CupsPrintersHandler::CupsPrintersHandler(Profile* profile,
                                         CupsPrintersManager* printers_manager)
    : CupsPrintersHandler(profile,
                          CreatePpdProvider(profile),
                          printers_manager) {}

CupsPrintersHandler::CupsPrintersHandler(
    Profile* profile,
    scoped_refptr<PpdProvider> ppd_provider,
    CupsPrintersManager* printers_manager)
    : profile_(profile),
      ppd_provider_(ppd_provider),
      printers_manager_(printers_manager),
      endpoint_resolver_(
          std::make_unique<local_discovery::EndpointResolver>()) {}

// static
std::unique_ptr<CupsPrintersHandler> CupsPrintersHandler::CreateForTesting(
    Profile* profile,
    scoped_refptr<PpdProvider> ppd_provider,
    CupsPrintersManager* printers_manager) {
  // Using 'new' to access non-public constructor.
  return base::WrapUnique(
      new CupsPrintersHandler(profile, ppd_provider, printers_manager));
}

CupsPrintersHandler::~CupsPrintersHandler() {
  if (select_file_dialog_) {
    select_file_dialog_->ListenerDestroyed();
  }
}

void CupsPrintersHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getCupsSavedPrintersList",
      base::BindRepeating(&CupsPrintersHandler::HandleGetCupsSavedPrintersList,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getCupsEnterprisePrintersList",
      base::BindRepeating(
          &CupsPrintersHandler::HandleGetCupsEnterprisePrintersList,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "updateCupsPrinter",
      base::BindRepeating(&CupsPrintersHandler::HandleUpdateCupsPrinter,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "removeCupsPrinter",
      base::BindRepeating(&CupsPrintersHandler::HandleRemoveCupsPrinter,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "addCupsPrinter",
      base::BindRepeating(&CupsPrintersHandler::HandleAddCupsPrinter,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "retrieveCupsPrinterPpd",
      base::BindRepeating(&CupsPrintersHandler::HandleRetrieveCupsPrinterPpd,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "reconfigureCupsPrinter",
      base::BindRepeating(&CupsPrintersHandler::HandleReconfigureCupsPrinter,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getPrinterInfo",
      base::BindRepeating(&CupsPrintersHandler::HandleGetPrinterInfo,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getCupsPrinterManufacturersList",
      base::BindRepeating(
          &CupsPrintersHandler::HandleGetCupsPrinterManufacturers,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getCupsPrinterModelsList",
      base::BindRepeating(&CupsPrintersHandler::HandleGetCupsPrinterModels,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "selectPPDFile",
      base::BindRepeating(&CupsPrintersHandler::HandleSelectPPDFile,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "startDiscoveringPrinters",
      base::BindRepeating(&CupsPrintersHandler::HandleStartDiscovery,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "stopDiscoveringPrinters",
      base::BindRepeating(&CupsPrintersHandler::HandleStopDiscovery,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getPrinterPpdManufacturerAndModel",
      base::BindRepeating(
          &CupsPrintersHandler::HandleGetPrinterPpdManufacturerAndModel,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "addDiscoveredPrinter",
      base::BindRepeating(&CupsPrintersHandler::HandleAddDiscoveredPrinter,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "cancelPrinterSetUp",
      base::BindRepeating(&CupsPrintersHandler::HandleSetUpCancel,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getEulaUrl", base::BindRepeating(&CupsPrintersHandler::HandleGetEulaUrl,
                                        base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "queryPrintServer",
      base::BindRepeating(&CupsPrintersHandler::HandleQueryPrintServer,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "openPrintManagementApp",
      base::BindRepeating(&CupsPrintersHandler::HandleOpenPrintManagementApp,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "openScanningApp",
      base::BindRepeating(&CupsPrintersHandler::HandleOpenScanningApp,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "requestPrinterStatus",
      base::BindRepeating(&CupsPrintersHandler::HandleRequestPrinterStatus,
                          base::Unretained(this)));
}

void CupsPrintersHandler::OnJavascriptAllowed() {
  DCHECK(!printers_manager_observation_.IsObserving());
  printers_manager_observation_.Observe(printers_manager_.get());
  DCHECK(!local_printers_observation_.IsObserving());
  local_printers_observation_.Observe(printers_manager_.get());
}

void CupsPrintersHandler::OnJavascriptDisallowed() {
  printers_manager_observation_.Reset();
  local_printers_observation_.Reset();
}

void CupsPrintersHandler::SetWebUIForTest(content::WebUI* web_ui) {
  set_web_ui(web_ui);
}

void CupsPrintersHandler::HandleGetCupsSavedPrintersList(
    const base::Value::List& args) {
  AllowJavascript();

  CHECK_EQ(1U, args.size());
  const std::string& callback_id = args[0].GetString();

  std::vector<Printer> printers =
      printers_manager_->GetPrinters(PrinterClass::kSaved);
  base::UmaHistogramCounts100(kSavedPrintersCountHistogramName,
                              printers.size());

  ResolveJavascriptCallback(base::Value(callback_id),
                            BuildCupsPrintersList(printers));
}

void CupsPrintersHandler::HandleGetCupsEnterprisePrintersList(
    const base::Value::List& args) {
  AllowJavascript();

  CHECK_EQ(1U, args.size());
  std::string callback_id = args[0].GetString();

  std::vector<Printer> printers =
      printers_manager_->GetPrinters(PrinterClass::kEnterprise);

  ResolveJavascriptCallback(base::Value(callback_id),
                            BuildCupsPrintersList(printers));
}

void CupsPrintersHandler::HandleUpdateCupsPrinter(
    const base::Value::List& args) {
  CHECK_EQ(3U, args.size());
  const std::string& callback_id = args[0].GetString();
  const std::string& printer_id = args[1].GetString();
  const std::string& printer_name = args[2].GetString();

  Printer printer(printer_id);
  printer.set_display_name(printer_name);

  if (!profile_->GetPrefs()->GetBoolean(prefs::kUserPrintersAllowed)) {
    PRINTER_LOG(DEBUG) << "HandleUpdateCupsPrinter() called when "
                          "kUserPrintersAllowed is set to false";
    OnAddedOrEditedPrinterCommon(printer,
                                 PrinterSetupResult::kNativePrintersNotAllowed);
    // Logs the error and runs the callback.
    OnAddOrEditPrinterError(callback_id,
                            PrinterSetupResult::kNativePrintersNotAllowed);
    return;
  }

  OnAddedOrEditedSpecifiedPrinter(callback_id, printer,
                                  true /* is_printer_edit */,
                                  PrinterSetupResult::kEditSuccess);
}

void CupsPrintersHandler::HandleRetrieveCupsPrinterPpd(
    const base::Value::List& args) {
  CHECK_EQ(3U, args.size());

  const std::string& printer_id = args[0].GetString();
  const std::string& printer_name = args[1].GetString();
  const std::string& eula = args[2].GetString();

  PRINTER_LOG(DEBUG) << printer_id << ": Retrieving printer PPD for "
                     << printer_name;

  // We first make sure the printer is setup in CUPS backend (when the user logs
  // out, CUPS will clear a bunch of cached state).

  std::optional<chromeos::Printer> printer =
      printers_manager_->GetPrinter(printer_id);
  if (!printer) {
    PRINTER_LOG(ERROR) << printer_id << ": GetPrinter failed";
    OnRetrievePpdError(printer_name);
    return;
  }

  printers_manager_->SetUpPrinter(
      *printer, /*is_automatic_installation=*/true,
      base::BindOnce(&CupsPrintersHandler::OnSetUpPrinter,
                     weak_factory_.GetWeakPtr(), printer_id, printer_name,
                     eula));
}

void CupsPrintersHandler::OnSetUpPrinter(const std::string& printer_id,
                                         const std::string& printer_name,
                                         const std::string& eula,
                                         PrinterSetupResult result) {
  if (result != PrinterSetupResult::kSuccess) {
    PRINTER_LOG(ERROR) << printer_id << ": Cannot set up printer "
                       << printer_name << ": " << ResultCodeToMessage(result);
    OnRetrievePpdError(printer_name);
    return;
  }

  // Once the printer has been setup we can request the PPD.
  printscanmgr::CupsRetrievePpdResponse empty_response;

  printscanmgr::CupsRetrievePpdRequest request;
  request.set_name(printer_id);
  PrintscanmgrClient::Get()->CupsRetrievePrinterPpd(
      request,
      base::BindOnce(&CupsPrintersHandler::OnRetrieveCupsPrinterPpd,
                     weak_factory_.GetWeakPtr(), printer_id, printer_name,
                     eula),
      base::BindOnce(&CupsPrintersHandler::OnRetrieveCupsPrinterPpd,
                     weak_factory_.GetWeakPtr(), printer_id, printer_name, eula,
                     empty_response));
}

void CupsPrintersHandler::OnRetrieveCupsPrinterPpd(
    const std::string& printer_id,
    const std::string& printer_name,
    const std::string& eula,
    std::optional<printscanmgr::CupsRetrievePpdResponse> response) {
  if (!response) {
    PRINTER_LOG(ERROR) << printer_id
                       << ": No response to retrieve PPD request for "
                       << printer_name;
    OnRetrievePpdError(printer_name);
    return;
  }

  if (response->ppd() == "") {
    PRINTER_LOG(ERROR) << printer_id << ": Retrieved an empty PPD for "
                       << printer_name;
    OnRetrievePpdError(printer_name);
    return;
  }

  std::string ppd = response->ppd();

  // If we have a eula link, insert that into our PPD as a comment.
  if (!eula.empty()) {
    const std::string ppd_start(R"(*PPD-Adobe: "4.3")");
    std::string::size_type index = ppd.find(ppd_start);
    if (index == std::string::npos) {
      PRINTER_LOG(ERROR)
          << printer_id
          << ": Unable to find start of PPD while inserting license for "
          << printer_name;
      OnRetrievePpdError(printer_name);
      return;
    }
    index += ppd_start.length();
    const std::string eulaText =
        l10n_util::GetStringFUTF8(IDS_SETTINGS_PRINTING_CUPS_EULA_NOTICE_HEADER,
                                  std::u16string(eula.begin(), eula.end()));
    ppd.insert(index, base::StringPrintf(R"(
*%%
*%%  %s
*%%)",
                                         eulaText.data()));
  }

  WriteAndDisplayPpdFile(printer_name, ppd);
}

void CupsPrintersHandler::OnRetrievePpdError(const std::string& printer_name) {
  // When there is an error retrieving the PPD, instead of saving the PPD file
  // to the Downloads dir, we write a file containing an error message and
  // display that.
  const std::string message = l10n_util::GetStringFUTF8(
      IDS_SETTINGS_PRINTING_CUPS_VIEW_PPD_ERROR_MESSAGE,
      std::u16string(printer_name.begin(), printer_name.end()));
  WriteAndDisplayPpdFile(printer_name, message);
}

base::FilePath DownloadPpdFile(const base::FilePath& ppd_file_path_base,
                               const std::string& ppd) {
  // Make sure we don't overwrite any of the user's current files.
  const base::FilePath ppd_file_path = base::GetUniquePath(ppd_file_path_base);
  if (ppd_file_path.empty()) {
    PRINTER_LOG(ERROR) << "Unable to save PPD file ("
                       << ppd_file_path_base.value() << ") - file exists";
    return base::FilePath();
  }

  if (!base::WriteFile(ppd_file_path, ppd)) {
    PRINTER_LOG(ERROR) << "Unable to save PPD file to specified path: "
                       << ppd_file_path.value();
    return base::FilePath();
  }

  return ppd_file_path;
}

void CupsPrintersHandler::WriteAndDisplayPpdFile(
    const std::string& printer_name,
    const std::string& ppd) {
  const base::FilePath downloads_path =
      DownloadPrefs::FromDownloadManager(profile_->GetDownloadManager())
          ->DownloadPath();
  // To make sure an appropriate filename is created, remove any dir separators.
  std::string sanitized_name = printer_name;
  base::ReplaceChars(sanitized_name, "/", "_", &sanitized_name);
  const base::FilePath ppd_file_path_base =
      downloads_path.Append(sanitized_name).AddExtension("ppd");

  // Use USER_BLOCKING here since the user is expecting a new web page to load
  // after clicking the View PPD link.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(DownloadPpdFile, ppd_file_path_base, ppd),
      base::BindOnce(&CupsPrintersHandler::DisplayPpdFile,
                     weak_factory_.GetWeakPtr()));
}

void CupsPrintersHandler::DisplayPpdFile(const base::FilePath& ppd_file_path) {
  if (ppd_file_path.empty()) {
    return;
  }

  PRINTER_LOG(DEBUG) << "PPD saved to " << ppd_file_path;
  ash::NewWindowDelegate::GetPrimary()->OpenUrl(
      GURL(base::StringPrintf("file://%s", ppd_file_path.value().c_str())),
      ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      ash::NewWindowDelegate::Disposition::kSwitchToTab);
}

void CupsPrintersHandler::HandleRemoveCupsPrinter(
    const base::Value::List& args) {
  // Printer name also expected in 2nd parameter.
  const std::string& printer_id = args[0].GetString();
  PRINTER_LOG(USER) << printer_id << ": Printer removal requested";
  auto printer = printers_manager_->GetPrinter(printer_id);
  if (!printer) {
    return;
  }

  // Record removal before the printer is deleted.
  PrinterEventTrackerFactory::GetForBrowserContext(profile_)
      ->RecordPrinterRemoved(*printer);

  // Printer is deleted here.  Do not access after this line.
  printers_manager_->RemoveSavedPrinter(printer_id);
}

void CupsPrintersHandler::HandleGetPrinterInfo(const base::Value::List& args) {
  if (args.empty() || !args[0].is_string()) {
    NOTREACHED_IN_MIGRATION() << "Expected request for a promise";
    return;
  }
  const std::string& callback_id = args[0].GetString();

  if (args.size() < 2u) {
    NOTREACHED_IN_MIGRATION() << "Dictionary missing";
    return;
  }

  const base::Value& printer_value = args[1];
  if (!printer_value.is_dict()) {
    NOTREACHED_IN_MIGRATION() << "Dictionary missing";
    return;
  }
  const base::Value::Dict& printer_dict = printer_value.GetDict();

  AllowJavascript();

  const std::string* printer_address =
      printer_dict.FindString("printerAddress");
  if (!printer_address) {
    NOTREACHED_IN_MIGRATION() << "Address missing";
    return;
  }

  std::string printer_queue;
  if (const std::string* ptr = printer_dict.FindString("printerQueue")) {
    printer_queue = *ptr;
    // Path must start from '/' character.
    if (!printer_queue.empty() && printer_queue.front() != '/') {
      printer_queue = "/" + printer_queue;
    }
  }

  const std::string* printer_protocol =
      printer_dict.FindString("printerProtocol");
  if (!printer_protocol) {
    NOTREACHED_IN_MIGRATION() << "Protocol missing";
    return;
  }

  DCHECK(*printer_protocol == chromeos::kIppScheme ||
         *printer_protocol == chromeos::kIppsScheme)
      << "Printer info requests only supported for IPP and IPPS printers";

  Uri uri(*printer_protocol + url::kStandardSchemeSeparator + *printer_address +
          printer_queue);
  if (uri.GetLastParsingError().status != Uri::ParserStatus::kNoErrors ||
      !IsValidPrinterUri(uri)) {
    // Run the failure callback.
    OnAutoconfQueried(callback_id, PrinterQueryResult::kUnknownFailure,
                      ::printing::PrinterStatus(), /*make_and_model=*/"",
                      /*document_formats=*/{}, /*ipp_everywhere=*/false,
                      chromeos::PrinterAuthenticationInfo{});
    return;
  }

  PRINTER_LOG(DEBUG) << "Querying printer info";
  QueryAutoconf(uri, base::BindOnce(&CupsPrintersHandler::OnAutoconfQueried,
                                    weak_factory_.GetWeakPtr(), callback_id));
}

void CupsPrintersHandler::OnAutoconfQueriedDiscovered(
    const std::string& callback_id,
    Printer printer,
    PrinterQueryResult result,
    const ::printing::PrinterStatus& printer_status,
    const std::string& make_and_model,
    const std::vector<std::string>& document_formats,
    bool ipp_everywhere,
    const chromeos::PrinterAuthenticationInfo& /*auth_info*/) {
  RecordIppQueryResult(result);

  const bool success = result == PrinterQueryResult::kSuccess;
  if (success) {
    PRINTER_LOG(DEBUG) << printer.id()
                       << ": Received IPP attributes: "
                          " make_and_model: "
                       << make_and_model
                       << ", ipp_everywhere: " << ipp_everywhere
                       << ", status message: " << printer_status.message
                       << ", status reasons: "
                       << printer_status.AllReasonsAsString()
                       << ", document formats: "
                       << base::JoinString(document_formats, ";");
    // If we queried a valid make and model, use it.  The mDNS record isn't
    // guaranteed to have it.  However, don't overwrite it if the printer
    // advertises an empty value through printer-make-and-model.
    if (!make_and_model.empty()) {
      printer.set_make_and_model(make_and_model);
    }

    // Autoconfig available, use it.
    if (ipp_everywhere) {
      PRINTER_LOG(DEBUG) << printer.id() << ": Performing autoconf setup";
      printer.mutable_ppd_reference()->autoconf = true;
      printers_manager_->SetUpPrinter(
          printer, /*is_automatic_installation=*/true,
          base::BindOnce(&CupsPrintersHandler::OnAddedDiscoveredPrinter,
                         weak_factory_.GetWeakPtr(), callback_id, printer));
      return;
    }
  } else {
    PRINTER_LOG(DEBUG) << printer.id() << ": IPP attribute query failed: "
                       << static_cast<int>(result);
  }

  // We don't have enough from discovery to configure the printer.  Fill in as
  // much information as we can about the printer, and ask the user to supply
  // the rest.
  PRINTER_LOG(EVENT) << printer.id() << ": Automatic setup not supported."
                     << "  Fallback to asking the user.";
  RejectJavascriptCallback(base::Value(callback_id),
                           GetCupsPrinterInfo(printer));
}

void CupsPrintersHandler::OnAutoconfQueried(
    const std::string& callback_id,
    PrinterQueryResult result,
    const ::printing::PrinterStatus& printer_status,
    const std::string& make_and_model,
    const std::vector<std::string>& document_formats,
    bool ipp_everywhere,
    const chromeos::PrinterAuthenticationInfo& /*auth_info*/) {
  RecordIppQueryResult(result);
  const bool success = result == PrinterQueryResult::kSuccess;

  if (result == PrinterQueryResult::kHostnameResolution ||
      result == PrinterQueryResult::kUnreachable) {
    PRINTER_LOG(DEBUG) << "Could not reach printer: "
                       << (result == PrinterQueryResult::kHostnameResolution
                               ? "hostname resolution failed"
                               : "printer unreachable");
    RejectJavascriptCallback(
        base::Value(callback_id),
        base::Value(static_cast<int>(PrinterSetupResult::kPrinterUnreachable)));
    return;
  }

  if (!success) {
    PRINTER_LOG(DEBUG) << "Could not query printer: "
                       << static_cast<int>(result);
    base::Value::Dict reject;
    reject.Set("message", "Querying printer failed");
    RejectJavascriptCallback(
        base::Value(callback_id),
        base::Value(static_cast<int>(PrinterSetupResult::kFatalError)));
    return;
  }

  PRINTER_LOG(DEBUG) << "Resolved printer information: "
                        " make_and_model: "
                     << make_and_model << ", ipp_everywhere: " << ipp_everywhere
                     << ", status message: " << printer_status.message
                     << ", status reasons: "
                     << printer_status.AllReasonsAsString()
                     << ", document formats: "
                     << base::JoinString(document_formats, ";");

  // Bundle printer metadata
  base::Value::Dict info;
  info.Set("makeAndModel", make_and_model);
  info.Set("autoconf", ipp_everywhere);

  if (ipp_everywhere) {
    info.Set("ppdReferenceResolved", true);
    ResolveJavascriptCallback(base::Value(callback_id), info);
    return;
  }

  chromeos::PrinterSearchData ppd_search_data;
  ppd_search_data.discovery_type =
      chromeos::PrinterSearchData::PrinterDiscoveryType::kManual;
  ppd_search_data.make_and_model.push_back(make_and_model);
  ppd_search_data.supported_document_formats = document_formats;

  // Try to resolve the PPD matching.
  ppd_provider_->ResolvePpdReference(
      ppd_search_data,
      base::BindOnce(&CupsPrintersHandler::OnPpdResolved,
                     weak_factory_.GetWeakPtr(), callback_id, std::move(info)));
}

void CupsPrintersHandler::OnPpdResolved(const std::string& callback_id,
                                        base::Value::Dict info,
                                        PpdProvider::CallbackResultCode res,
                                        const Printer::PpdReference& ppd_ref,
                                        const std::string& usb_manufacturer) {
  if (res != PpdProvider::CallbackResultCode::SUCCESS) {
    info.Set("ppdReferenceResolved", false);
    ResolveJavascriptCallback(base::Value(callback_id), info);
    return;
  }

  SetPpdReference(ppd_ref, &info);
  info.Set("ppdReferenceResolved", true);
  ResolveJavascriptCallback(base::Value(callback_id), info);
}

void CupsPrintersHandler::HandleAddCupsPrinter(const base::Value::List& args) {
  AllowJavascript();
  AddOrReconfigurePrinter(args, false /* is_printer_edit */);
}

void CupsPrintersHandler::HandleReconfigureCupsPrinter(
    const base::Value::List& args) {
  AllowJavascript();
  AddOrReconfigurePrinter(args, true /* is_printer_edit */);
}

void CupsPrintersHandler::AddOrReconfigurePrinter(const base::Value::List& args,
                                                  bool is_printer_edit) {
  CHECK_EQ(2U, args.size());
  std::string callback_id = args[0].GetString();
  const base::Value& printer_value = args[1];
  CHECK(printer_value.is_dict());
  const base::Value::Dict& printer_dict = printer_value.GetDict();

  std::unique_ptr<Printer> printer = DictToPrinter(printer_dict);
  if (!printer) {
    PRINTER_LOG(ERROR) << "Failed to parse printer URI";
    OnAddOrEditPrinterError(callback_id, PrinterSetupResult::kFatalError);
    return;
  }

  if (!profile_->GetPrefs()->GetBoolean(prefs::kUserPrintersAllowed)) {
    PRINTER_LOG(DEBUG) << "AddOrReconfigurePrinter() called when "
                          "kUserPrintersAllowed is set to false";
    OnAddedOrEditedPrinterCommon(*printer,
                                 PrinterSetupResult::kNativePrintersNotAllowed);
    // Used to fire the web UI listener.
    OnAddOrEditPrinterError(callback_id,
                            PrinterSetupResult::kNativePrintersNotAllowed);
    return;
  }

  // Grab the existing printer object and check that we are not making any
  // changes that will make |existing_printer_object| unusable.
  if (printer->id().empty()) {
    // If the printer object has not already been created, error out since this
    // is not a valid case.
    PRINTER_LOG(ERROR) << "Failed to parse printer ID";
    OnAddOrEditPrinterError(callback_id, PrinterSetupResult::kFatalError);
    return;
  }

  std::optional<Printer> existing_printer_object =
      printers_manager_->GetPrinter(printer->id());
  if (existing_printer_object) {
    if (!IsValidUriChange(*existing_printer_object, *printer)) {
      OnAddOrEditPrinterError(callback_id,
                              PrinterSetupResult::kInvalidPrinterUpdate);
      return;
    }
  }

  // Read PPD selection if it was used.
  const std::string* ppd_manufacturer =
      printer_dict.FindString("ppdManufacturer");
  const std::string* ppd_model = printer_dict.FindString("ppdModel");

  // Read user provided PPD if it was used.
  const std::string* printer_ppd_path =
      printer_dict.FindString("printerPPDPath");

  // Check if the printer already has a valid ppd_reference.
  Printer::PpdReference ppd_ref = GetPpdReference(&printer_dict);
  if (ppd_ref.IsFilled()) {
    *printer->mutable_ppd_reference() = ppd_ref;
  } else if (printer_ppd_path && !printer_ppd_path->empty()) {
    GURL tmp = net::FilePathToFileURL(base::FilePath(*printer_ppd_path));
    if (!tmp.is_valid()) {
      LOG(ERROR) << "Invalid ppd path: " << *printer_ppd_path;
      OnAddOrEditPrinterError(callback_id, PrinterSetupResult::kInvalidPpd);
      return;
    }
    printer->mutable_ppd_reference()->user_supplied_ppd_url = tmp.spec();
  } else if (ppd_manufacturer && !ppd_manufacturer->empty() && ppd_model &&
             !ppd_model->empty()) {
    // Pull out the ppd reference associated with the selected manufacturer and
    // model.
    bool found = false;
    for (const auto& resolved_printer : resolved_printers_[*ppd_manufacturer]) {
      if (resolved_printer.name == *ppd_model) {
        *printer->mutable_ppd_reference() = resolved_printer.ppd_ref;
        found = true;
        break;
      }
    }
    if (!found) {
      LOG(ERROR) << "Failed to get ppd reference";
      OnAddOrEditPrinterError(callback_id, PrinterSetupResult::kPpdNotFound);
      return;
    }

    if (printer->make_and_model().empty()) {
      // PPD Model names are actually make and model.
      printer->set_make_and_model(*ppd_model);
    }
  } else {
    // TODO(https://crbug.com/738514): Support PPD guessing for non-autoconf
    // printers. i.e. !autoconf && !manufacturer.empty() && !model.empty()
    NOTREACHED_IN_MIGRATION()
        << "A configuration option must have been selected to add a printer";
  }

  printers_manager_->SetUpPrinter(
      *printer,
      /*is_automatic_installation=*/false,
      base::BindOnce(&CupsPrintersHandler::OnAddedOrEditedSpecifiedPrinter,
                     weak_factory_.GetWeakPtr(), callback_id, *printer,
                     is_printer_edit));
}

void CupsPrintersHandler::OnAddedOrEditedPrinterCommon(
    const Printer& printer,
    PrinterSetupResult result_code) {
  if (printer.IsZeroconf()) {
    UMA_HISTOGRAM_ENUMERATION("Printing.CUPS.ZeroconfPrinterSetupResult",
                              result_code, PrinterSetupResult::kMaxValue);
  } else {
    UMA_HISTOGRAM_ENUMERATION("Printing.CUPS.PrinterSetupResult", result_code,
                              PrinterSetupResult::kMaxValue);
  }

  switch (result_code) {
    case PrinterSetupResult::kSuccess:
      UMA_HISTOGRAM_ENUMERATION("Printing.CUPS.PrinterAdded",
                                printer.GetProtocol(), Printer::kProtocolMax);
      PRINTER_LOG(USER) << printer.id() << ": Setup succeeded.  Saving printer "
                        << printer.make_and_model();
      printers_manager_->SavePrinter(printer);
      if (printer.IsUsbProtocol()) {
        // Record UMA for USB printer setup source.
        PrinterConfigurer::RecordUsbPrinterSetupSource(
            UsbPrinterSetupSource::kSettings);
      }
      return;
    case PrinterSetupResult::kEditSuccess:
      PRINTER_LOG(USER) << printer.id() << ": Edited printer successfully: "
                        << ResultCodeToMessage(result_code);
      printers_manager_->SavePrinter(printer);
      return;
    case PrinterSetupResult::kNativePrintersNotAllowed:
    case PrinterSetupResult::kBadUri:
    case PrinterSetupResult::kInvalidPrinterUpdate:
    case PrinterSetupResult::kPrinterUnreachable:
    case PrinterSetupResult::kPrinterSentWrongResponse:
    case PrinterSetupResult::kPrinterIsNotAutoconfigurable:
    case PrinterSetupResult::kPpdTooLarge:
    case PrinterSetupResult::kInvalidPpd:
    case PrinterSetupResult::kPpdNotFound:
    case PrinterSetupResult::kPpdUnretrievable:
    case PrinterSetupResult::kDbusError:
    case PrinterSetupResult::kDbusNoReply:
    case PrinterSetupResult::kDbusTimeout:
    case PrinterSetupResult::kIoError:
    case PrinterSetupResult::kMemoryAllocationError:
    case PrinterSetupResult::kFatalError:
    case PrinterSetupResult::kManualSetupRequired:
    case PrinterSetupResult::kPrinterRemoved:
    case PrinterSetupResult::kPrintscanmgrDbusNoReply:
    case PrinterSetupResult::kDebugdDbusNoReply:
    case PrinterSetupResult::kComponentUnavailable:
      PRINTER_LOG(ERROR) << printer.id() << ": Error during setup for "
                         << printer.make_and_model() << ": "
                         << ResultCodeToMessage(result_code);
      break;
  }
  // Log an event that tells us this printer setup failed, so we can get
  // statistics about which printers are giving users difficulty.
  printers_manager_->RecordSetupAbandoned(printer);
}

void CupsPrintersHandler::OnAddedDiscoveredPrinter(
    const std::string& callback_id,
    const Printer& printer,
    PrinterSetupResult result_code) {
  OnAddedOrEditedPrinterCommon(printer, result_code);
  if (result_code == PrinterSetupResult::kSuccess) {
    ResolveJavascriptCallback(base::Value(callback_id),
                              base::Value(static_cast<int>(result_code)));
  } else {
    PRINTER_LOG(EVENT) << printer.id()
                       << ": Automatic setup failed for discovered printer "
                       << printer.make_and_model() << ": "
                       << ResultCodeToMessage(result_code)
                       << ".  Fall back to manual.";
    // Could not set up printer.  Asking user for manufacturer data.
    RejectJavascriptCallback(base::Value(callback_id),
                             GetCupsPrinterInfo(printer));
  }
}

void CupsPrintersHandler::OnAddedOrEditedSpecifiedPrinter(
    const std::string& callback_id,
    const Printer& printer,
    bool is_printer_edit,
    PrinterSetupResult result_code) {
  if (is_printer_edit && result_code == PrinterSetupResult::kSuccess) {
    result_code = PrinterSetupResult::kEditSuccess;
  }
  const int result_code_int = static_cast<int>(result_code);
  PRINTER_LOG(EVENT) << "Add/Update manual printer: "
                     << ResultCodeToMessage(result_code);
  OnAddedOrEditedPrinterCommon(printer, result_code);

  if (result_code != PrinterSetupResult::kSuccess &&
      result_code != PrinterSetupResult::kEditSuccess) {
    RejectJavascriptCallback(base::Value(callback_id),
                             base::Value(result_code_int));
    return;
  }

  ResolveJavascriptCallback(base::Value(callback_id),
                            base::Value(result_code_int));
}

void CupsPrintersHandler::OnAddOrEditPrinterError(
    const std::string& callback_id,
    PrinterSetupResult result_code) {
  const int result_code_int = static_cast<int>(result_code);
  PRINTER_LOG(EVENT) << "Add printer error: "
                     << ResultCodeToMessage(result_code);
  RejectJavascriptCallback(base::Value(callback_id),
                           base::Value(result_code_int));
}

void CupsPrintersHandler::HandleGetCupsPrinterManufacturers(
    const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(1U, args.size());
  const std::string& callback_id = args[0].GetString();
  ppd_provider_->ResolveManufacturers(
      base::BindOnce(&CupsPrintersHandler::ResolveManufacturersDone,
                     weak_factory_.GetWeakPtr(), callback_id));
}

void CupsPrintersHandler::HandleGetCupsPrinterModels(
    const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(2U, args.size());
  const std::string& callback_id = args[0].GetString();
  const std::string& manufacturer = args[1].GetString();

  // Empty manufacturer queries may be triggered as a part of the ui
  // initialization, and should just return empty results.
  if (manufacturer.empty()) {
    base::Value::Dict response;
    response.Set("success", true);
    response.Set("models", base::Value::List());
    ResolveJavascriptCallback(base::Value(callback_id), response);
    return;
  }

  ppd_provider_->ResolvePrinters(
      manufacturer,
      base::BindOnce(&CupsPrintersHandler::ResolvePrintersDone,
                     weak_factory_.GetWeakPtr(), manufacturer, callback_id));
}

void CupsPrintersHandler::HandleSelectPPDFile(const base::Value::List& args) {
  // Early return if the select file dialog is already active.
  if (select_file_dialog_) {
    return;
  }

  CHECK_EQ(1U, args.size());
  webui_callback_id_ = args[0].GetString();

  base::FilePath downloads_path =
      DownloadPrefs::FromDownloadManager(profile_->GetDownloadManager())
          ->DownloadPath();

  content::WebContents* web_contents = web_ui()->GetWebContents();
  select_file_dialog_ = ui::SelectFileDialog::Create(
      this, std::make_unique<ChromeSelectFilePolicy>(web_contents));

  gfx::NativeWindow owning_window =
      web_contents ? chrome::FindBrowserWithTab(web_contents)
                         ->window()
                         ->GetNativeWindow()
                   : gfx::NativeWindow();

  ui::SelectFileDialog::FileTypeInfo file_type_info;
  file_type_info.extensions.push_back({"ppd"});
  file_type_info.extensions.push_back({"ppd.gz"});
  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_OPEN_FILE, std::u16string(), downloads_path,
      &file_type_info, 0, FILE_PATH_LITERAL(""), owning_window);
}

void CupsPrintersHandler::ResolveManufacturersDone(
    const std::string& callback_id,
    PpdProvider::CallbackResultCode result_code,
    const std::vector<std::string>& manufacturers) {
  base::Value::List manufacturers_value;
  if (result_code == PpdProvider::SUCCESS) {
    for (const std::string& manufacturer : manufacturers) {
      manufacturers_value.Append(manufacturer);
    }
  }
  base::Value::Dict response;
  response.Set("success", result_code == PpdProvider::SUCCESS);
  response.Set("manufacturers", std::move(manufacturers_value));
  ResolveJavascriptCallback(base::Value(callback_id), response);
}

void CupsPrintersHandler::ResolvePrintersDone(
    const std::string& manufacturer,
    const std::string& callback_id,
    PpdProvider::CallbackResultCode result_code,
    const PpdProvider::ResolvedPrintersList& printers) {
  base::Value::List printers_value;
  if (result_code == PpdProvider::SUCCESS) {
    resolved_printers_[manufacturer] = printers;
    for (const auto& printer : printers) {
      printers_value.Append(printer.name);
    }
  }
  base::Value::Dict response;
  response.Set("success", result_code == PpdProvider::SUCCESS);
  response.Set("models", std::move(printers_value));
  ResolveJavascriptCallback(base::Value(callback_id), response);
}

void CupsPrintersHandler::FileSelected(const ui::SelectedFileInfo& file,
                                       int index) {
  DCHECK(!webui_callback_id_.empty());

  select_file_dialog_ = nullptr;

  // Load the beginning contents of |file| and callback into VerifyPpdContents()
  // in order to determine whether the file appears to be a PPD file. The task's
  // priority is USER_BLOCKING because the this task updates the UI as a result
  // of a direct user action.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&ReadFileToStringWithMaxSize, file.path(),
                     kPpdMaxLineLength),
      base::BindOnce(&CupsPrintersHandler::VerifyPpdContents,
                     weak_factory_.GetWeakPtr(), file.path()));
}

void CupsPrintersHandler::FileSelectionCanceled() {
  select_file_dialog_ = nullptr;
}

void CupsPrintersHandler::VerifyPpdContents(const base::FilePath& path,
                                            const std::string& contents) {
  std::string result;
  if (chromeos::PpdLineReader::ContainsMagicNumber(contents,
                                                   kPpdMaxLineLength)) {
    result = path.value();
  }

  ResolveJavascriptCallback(base::Value(webui_callback_id_),
                            base::Value(result));
  webui_callback_id_.clear();
}

void CupsPrintersHandler::HandleStartDiscovery(const base::Value::List& args) {
  PRINTER_LOG(DEBUG) << "Start printer discovery";
  AllowJavascript();
  discovery_active_ = true;
  OnPrintersChanged(PrinterClass::kAutomatic,
                    printers_manager_->GetPrinters(PrinterClass::kAutomatic));
  OnPrintersChanged(PrinterClass::kDiscovered,
                    printers_manager_->GetPrinters(PrinterClass::kDiscovered));

  base::UmaHistogramCounts100(kNearbyAutomaticPrintersHistogramName,
                              automatic_printers_.size());
  base::UmaHistogramCounts100(kNearbyDiscoveredPrintersHistogramName,
                              discovered_printers_.size());
  UMA_HISTOGRAM_COUNTS_100(
      "Printing.CUPS.PrintersDiscovered",
      discovered_printers_.size() + automatic_printers_.size());
  printers_manager_->RecordNearbyNetworkPrinterCounts();
  // Scan completes immediately right now.  Emit done.
  FireWebUIListener("on-printer-discovery-done");
}

void CupsPrintersHandler::HandleStopDiscovery(const base::Value::List& args) {
  PRINTER_LOG(DEBUG) << "Stop printer discovery";
  discovered_printers_.clear();
  automatic_printers_.clear();

  // Free up memory while we're not discovering.
  discovered_printers_.shrink_to_fit();
  automatic_printers_.shrink_to_fit();
  discovery_active_ = false;
}

void CupsPrintersHandler::HandleSetUpCancel(const base::Value::List& args) {
  PRINTER_LOG(DEBUG) << "Printer setup cancelled";
  const base::Value& printer_value = args[0];
  CHECK(printer_value.is_dict());

  std::unique_ptr<Printer> printer = DictToPrinter(printer_value.GetDict());
  if (printer) {
    printers_manager_->RecordSetupAbandoned(*printer);
  }
}

void CupsPrintersHandler::OnPrintersChanged(
    PrinterClass printer_class,
    const std::vector<Printer>& printers) {
  switch (printer_class) {
    case PrinterClass::kAutomatic:
      automatic_printers_ = printers;
      UpdateDiscoveredPrinters();
      break;
    case PrinterClass::kDiscovered:
      discovered_printers_ = printers;
      UpdateDiscoveredPrinters();
      break;
    case PrinterClass::kSaved: {
      FireWebUIListener("on-saved-printers-changed",
                        BuildCupsPrintersList(printers));
      break;
    }
    case PrinterClass::kEnterprise:
      FireWebUIListener("on-enterprise-printers-changed",
                        BuildCupsPrintersList(printers));
      break;
  }
}

void CupsPrintersHandler::OnLocalPrintersUpdated() {
  const std::vector<chromeos::Printer> printers =
      printers_manager_->GetPrinters(PrinterClass::kSaved);
  base::Value::List printers_as_values =
      base::Value::List::with_capacity(printers.size());
  for (const auto& printer : printers) {
    printers_as_values.Append(GetCupsPrinterInfo(printer));
  }
  FireWebUIListener("local-printers-updated", printers_as_values);
}

void CupsPrintersHandler::UpdateDiscoveredPrinters() {
  if (!discovery_active_) {
    PRINTER_LOG(DEBUG) << "Discovered printers update skipped";
    return;
  }

  base::Value::List automatic_printers_list;
  for (const Printer& printer : automatic_printers_) {
    automatic_printers_list.Append(GetCupsPrinterInfo(printer));
  }

  base::Value::List discovered_printers_list;
  for (const Printer& printer : discovered_printers_) {
    discovered_printers_list.Append(GetCupsPrinterInfo(printer));
  }

  PRINTER_LOG(DEBUG) << "Discovered printers updating. Automatic: "
                     << automatic_printers_list.size()
                     << " Discovered: " << discovered_printers_list.size();
  FireWebUIListener("on-nearby-printers-changed", automatic_printers_list,
                    discovered_printers_list);
}

void CupsPrintersHandler::HandleAddDiscoveredPrinter(
    const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(2U, args.size());
  const std::string& callback_id = args[0].GetString();
  const std::string& printer_id = args[1].GetString();

  PRINTER_LOG(USER) << printer_id << ": Adding discovered printer";
  std::optional<Printer> printer = printers_manager_->GetPrinter(printer_id);
  if (!printer) {
    PRINTER_LOG(ERROR) << printer_id << ": Discovered printer disappeared";
    // Printer disappeared, so we don't have information about it anymore and
    // can't really do much. Fail the add.
    ResolveJavascriptCallback(
        base::Value(callback_id),
        base::Value(static_cast<int>(PrinterSetupResult::kPrinterUnreachable)));
    return;
  }

  if (!printer->HasUri()) {
    PRINTER_LOG(ERROR) << printer_id << ": URI missing";
    // The printer uri was not parsed successfully. Fail the add.
    ResolveJavascriptCallback(
        base::Value(callback_id),
        base::Value(static_cast<int>(PrinterSetupResult::kPrinterUnreachable)));
    return;
  }
  PRINTER_LOG(DEBUG) << printer_id
                     << ": make and model: " << printer->make_and_model();

  if (printer->ppd_reference().autoconf ||
      !printer->ppd_reference().effective_make_and_model.empty() ||
      !printer->ppd_reference().user_supplied_ppd_url.empty()) {
    PRINTER_LOG(EVENT) << printer_id
                       << ": Start automatic setup of discovered printer";
    // If we have something that looks like a ppd reference for this printer,
    // try to configure it.
    printers_manager_->SetUpPrinter(
        *printer, /*is_automatic_installation=*/true,
        base::BindOnce(&CupsPrintersHandler::OnAddedDiscoveredPrinter,
                       weak_factory_.GetWeakPtr(), callback_id, *printer));
    return;
  }

  // We need a special case for USB printers here. We cannot query them
  // directly, so we have to fall back to manual configuration here.
  if (printer->IsUsbProtocol()) {
    RejectJavascriptCallback(base::Value(callback_id),
                             GetCupsPrinterInfo(*printer));
    return;
  }

  // The mDNS record doesn't guarantee we can setup the printer.  Query it to
  // see if we want to try IPP.
  auto address = printer->GetHostAndPort();
  if (address.IsEmpty()) {
    PRINTER_LOG(ERROR) << printer_id << ": Hostname is invalid: "
                       << printer->uri().GetNormalized(true);
    OnAddedDiscoveredPrinter(callback_id, *printer,
                             PrinterSetupResult::kPrinterUnreachable);
    return;
  }
  PRINTER_LOG(DEBUG) << printer_id << ": Resolving IP of "
                     << address.ToString();
  endpoint_resolver_->Start(
      address, base::BindOnce(&CupsPrintersHandler::OnIpResolved,
                              weak_factory_.GetWeakPtr(), callback_id,
                              std::move(*printer)));
}

void CupsPrintersHandler::HandleGetPrinterPpdManufacturerAndModel(
    const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(2U, args.size());
  const std::string& callback_id = args[0].GetString();
  const std::string& printer_id = args[1].GetString();

  auto printer = printers_manager_->GetPrinter(printer_id);
  if (!printer) {
    RejectJavascriptCallback(base::Value(callback_id), base::Value());
    return;
  }

  ppd_provider_->ReverseLookup(
      printer->ppd_reference().effective_make_and_model,
      base::BindOnce(&CupsPrintersHandler::OnGetPrinterPpdManufacturerAndModel,
                     weak_factory_.GetWeakPtr(), callback_id));
}

void CupsPrintersHandler::OnGetPrinterPpdManufacturerAndModel(
    const std::string& callback_id,
    PpdProvider::CallbackResultCode result_code,
    const std::string& manufacturer,
    const std::string& model) {
  if (result_code != PpdProvider::SUCCESS) {
    RejectJavascriptCallback(base::Value(callback_id), base::Value());
    return;
  }
  base::Value::Dict info;
  info.Set("ppdManufacturer", manufacturer);
  info.Set("ppdModel", model);
  ResolveJavascriptCallback(base::Value(callback_id),
                            base::Value(std::move(info)));
}

void CupsPrintersHandler::HandleGetEulaUrl(const base::Value::List& args) {
  CHECK_EQ(3U, args.size());
  const std::string callback_id = args[0].GetString();
  const std::string ppd_manufacturer = args[1].GetString();
  const std::string ppd_model = args[2].GetString();

  auto resolved_printers_it = resolved_printers_.find(ppd_manufacturer);
  if (resolved_printers_it == resolved_printers_.end()) {
    // Exit early if lookup for printers fails for |ppd_manufacturer|.
    OnGetEulaUrl(callback_id, PpdProvider::CallbackResultCode::NOT_FOUND,
                 /*license=*/std::string());
    return;
  }

  const PpdProvider::ResolvedPrintersList& printers_for_manufacturer =
      resolved_printers_it->second;

  auto printer_it =
      base::ranges::find(printers_for_manufacturer, ppd_model,
                         &PpdProvider::ResolvedPpdReference::name);

  if (printer_it == printers_for_manufacturer.end()) {
    // Unable to find the PpdReference, resolve promise with empty string.
    OnGetEulaUrl(callback_id, PpdProvider::CallbackResultCode::NOT_FOUND,
                 /*license=*/std::string());
    return;
  }

  ppd_provider_->ResolvePpdLicense(
      printer_it->ppd_ref.effective_make_and_model,
      base::BindOnce(&CupsPrintersHandler::OnGetEulaUrl,
                     weak_factory_.GetWeakPtr(), callback_id));
}

void CupsPrintersHandler::OnGetEulaUrl(const std::string& callback_id,
                                       PpdProvider::CallbackResultCode result,
                                       const std::string& license) {
  if (result != PpdProvider::SUCCESS || license.empty()) {
    ResolveJavascriptCallback(base::Value(callback_id), base::Value());
    return;
  }

  GURL eula_url = PrinterConfigurer::GeneratePrinterEulaUrl(license);
  ResolveJavascriptCallback(
      base::Value(callback_id),
      eula_url.is_valid() ? base::Value(eula_url.spec()) : base::Value());
}

void CupsPrintersHandler::OnIpResolved(const std::string& callback_id,
                                       const Printer& printer,
                                       const net::IPEndPoint& endpoint) {
  bool address_resolved = endpoint.address().IsValid();
  UMA_HISTOGRAM_BOOLEAN("Printing.CUPS.AddressResolutionResult",
                        address_resolved);
  if (!address_resolved) {
    PRINTER_LOG(ERROR) << printer.id() << ": IP Resolution failed";
    OnAddedDiscoveredPrinter(callback_id, printer,
                             PrinterSetupResult::kPrinterUnreachable);
    return;
  }

  PRINTER_LOG(EVENT) << printer.id() << ": IP Resolution succeeded";
  const Uri uri = printer.ReplaceHostAndPort(endpoint);

  if (IsIppUri(uri)) {
    PRINTER_LOG(EVENT) << printer.id() << ": Sending IPP attributes query to "
                       << printer.uri().GetNormalized(
                              /*always_show_port=*/true);
    QueryAutoconf(
        uri, base::BindOnce(&CupsPrintersHandler::OnAutoconfQueriedDiscovered,
                            weak_factory_.GetWeakPtr(), callback_id, printer));
    return;
  }

  PRINTER_LOG(EVENT) << printer.id() << ": Non-IPP printer URI "
                     << printer.uri().GetNormalized(/*always_show_port=*/true)
                     << ". Will request make and model from user";
  // If it's not an IPP printer, the user must choose a PPD.
  RejectJavascriptCallback(base::Value(callback_id),
                           GetCupsPrinterInfo(printer));
}

void CupsPrintersHandler::HandleQueryPrintServer(
    const base::Value::List& args) {
  CHECK_EQ(2U, args.size());
  const std::string& callback_id = args[0].GetString();
  const std::string& server_url = args[1].GetString();

  std::optional<GURL> converted_server_url =
      GenerateServerPrinterUrlWithValidScheme(server_url);
  if (!converted_server_url) {
    RejectJavascriptCallback(
        base::Value(callback_id),
        base::Value(PrintServerQueryResult::kIncorrectUrl));
    return;
  }

  // Use fallback only if HasValidServerPrinterScheme is false.
  QueryPrintServer(callback_id, converted_server_url.value(),
                   !HasValidServerPrinterScheme(GURL(server_url)));
}

void CupsPrintersHandler::QueryPrintServer(const std::string& callback_id,
                                           const GURL& server_url,
                                           bool should_fallback) {
  server_printers_fetcher_ = std::make_unique<ServerPrintersFetcher>(
      profile_, server_url, "(from user)",
      base::BindRepeating(&CupsPrintersHandler::OnQueryPrintServerCompleted,
                          weak_factory_.GetWeakPtr(), callback_id,
                          should_fallback));
}

void CupsPrintersHandler::OnQueryPrintServerCompleted(
    const std::string& callback_id,
    bool should_fallback,
    const ServerPrintersFetcher* sender,
    const GURL& server_url,
    std::vector<PrinterDetector::DetectedPrinter>&& returned_printers) {
  const PrintServerQueryResult result = sender->GetLastError();
  if (result != PrintServerQueryResult::kNoErrors) {
    if (should_fallback) {
      // Apply the fallback query.
      QueryPrintServer(callback_id, GenerateHttpCupsServerUrl(server_url),
                       /*should_fallback=*/false);
      return;
    }

    RejectJavascriptCallback(base::Value(callback_id), base::Value(result));
    return;
  }

  // Get all "saved" printers and organize them according to their URL.
  const std::vector<Printer> saved_printers =
      printers_manager_->GetPrinters(PrinterClass::kSaved);
  std::set<GURL> known_printers;
  for (const Printer& printer : saved_printers) {
    std::optional<GURL> gurl =
        GenerateServerPrinterUrlWithValidScheme(printer.uri().GetNormalized());
    if (gurl) {
      known_printers.insert(gurl.value());
    }
  }

  // Built final list of printers and a list of current names. If "current name"
  // is a null value, then a corresponding printer is not saved in the profile
  // (it can be added).
  std::vector<Printer> printers;
  printers.reserve(returned_printers.size());
  for (PrinterDetector::DetectedPrinter& printer : returned_printers) {
    printers.push_back(std::move(printer.printer));
    std::optional<GURL> printer_gurl = GenerateServerPrinterUrlWithValidScheme(
        printers.back().uri().GetNormalized());
    if (printer_gurl && known_printers.count(printer_gurl.value())) {
      printers.pop_back();
    }
  }

  // Delete fetcher object.
  server_printers_fetcher_.reset();

  // Create result value and finish the callback.
  ResolveJavascriptCallback(base::Value(callback_id),
                            BuildCupsPrintersList(printers));
}

void CupsPrintersHandler::HandleOpenPrintManagementApp(
    const base::Value::List& args) {
  DCHECK(args.empty());
  chrome::ShowPrintManagementApp(profile_);
}

void CupsPrintersHandler::HandleOpenScanningApp(const base::Value::List& args) {
  DCHECK(args.empty());
  chrome::ShowScanningApp(profile_);
}

void CupsPrintersHandler::HandleRequestPrinterStatus(
    const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(2U, args.size());
  const std::string& callback_id = args[0].GetString();
  const std::string& printer_id = args[1].GetString();

  printers_manager_->FetchPrinterStatus(
      printer_id, base::BindOnce(&CupsPrintersHandler::OnPrinterStatusReceived,
                                 weak_factory_.GetWeakPtr(), callback_id));
}

void CupsPrintersHandler::OnPrinterStatusReceived(
    const std::string& callback_id,
    const chromeos::CupsPrinterStatus& printer_status) {
  if (!IsJavascriptAllowed()) {
    return;
  }

  ResolveJavascriptCallback(base::Value(callback_id),
                            printer_status.ConvertToValue());
}

}  // namespace ash::settings
