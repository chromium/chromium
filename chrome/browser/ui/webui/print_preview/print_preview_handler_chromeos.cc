// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/print_preview_handler_chromeos.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/to_value_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/printing/print_preview_dialog_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/print_preview/local_printer_handler_chromeos.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_handler.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_ui.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_utils.h"
#include "chrome/browser/ui/webui/print_preview/printer_handler.h"
#include "chrome/common/printing/printer_capabilities.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/crosapi/mojom/local_printer.mojom.h"
#include "chromeos/printing/printer_configuration.h"
#include "chromeos/printing/printing_constants.h"
#include "components/device_event_log/device_event_log.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "printing/mojom/print.mojom.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/local_printer_ash.h"
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_service.h"
#endif

namespace printing {

namespace {

base::Value::Dict PrintServersConfigMojomToValue(
    crosapi::mojom::PrintServersConfigPtr config) {
  base::Value::List ui_print_servers;
  for (const auto& print_server : config->print_servers) {
    base::Value::Dict ui_print_server;
    ui_print_server.Set("id", print_server->id);
    ui_print_server.Set("name", print_server->name);
    ui_print_servers.Append(std::move(ui_print_server));
  }
  base::Value::Dict ui_print_servers_config;
  ui_print_servers_config.Set("printServers", std::move(ui_print_servers));
  ui_print_servers_config.Set(
      "isSingleServerFetchingMode",
      config->fetching_mode ==
          ash::ServerPrintersFetchingMode::kSingleServerOnly);
  return ui_print_servers_config;
}

base::Value::List ConvertPrintersToValues(
    const std::vector<crosapi::mojom::LocalDestinationInfoPtr>& printers) {
  return base::ToValueList(printers, [](const auto& printer) {
    return LocalPrinterHandlerChromeos::PrinterToValue(*printer);
  });
}

}  // namespace

PrintPreviewHandlerChromeOS::PrintPreviewHandlerChromeOS() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  DCHECK(crosapi::CrosapiManager::IsInitialized());
  local_printer_ =
      crosapi::CrosapiManager::Get()->crosapi_ash()->local_printer_ash();
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  if (!service->IsAvailable<crosapi::mojom::LocalPrinter>()) {
    PRINTER_LOG(DEBUG) << "Local printer not available";
    return;
  }
  local_printer_ = service->GetRemote<crosapi::mojom::LocalPrinter>().get();
  local_printer_version_ =
      service->GetInterfaceVersion<crosapi::mojom::LocalPrinter>();
#endif
}

PrintPreviewHandlerChromeOS::~PrintPreviewHandlerChromeOS() = default;

void PrintPreviewHandlerChromeOS::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "setupPrinter",
      base::BindRepeating(&PrintPreviewHandlerChromeOS::HandlePrinterSetup,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "grantExtensionPrinterAccess",
      base::BindRepeating(
          &PrintPreviewHandlerChromeOS::HandleGrantExtensionPrinterAccess,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getEulaUrl",
      base::BindRepeating(&PrintPreviewHandlerChromeOS::HandleGetEulaUrl,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "requestPrinterStatus",
      base::BindRepeating(
          &PrintPreviewHandlerChromeOS::HandleRequestPrinterStatusUpdate,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "choosePrintServers",
      base::BindRepeating(
          &PrintPreviewHandlerChromeOS::HandleChoosePrintServers,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getPrintServersConfig",
      base::BindRepeating(
          &PrintPreviewHandlerChromeOS::HandleGetPrintServersConfig,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "recordPrintAttemptOutcome",
      base::BindRepeating(
          &PrintPreviewHandlerChromeOS::HandleRecordPrintAttemptOutcome,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getShowManagePrinters",
      base::BindRepeating(
          &PrintPreviewHandlerChromeOS::HandleGetShowManagePrinters,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "observeLocalPrinters",
      base::BindRepeating(
          &PrintPreviewHandlerChromeOS::HandleObserveLocalPrinters,
          base::Unretained(this)));
}

void PrintPreviewHandlerChromeOS::OnJavascriptAllowed() {
  receiver_.reset();  // Just in case this method is called multiple times.
  if (!local_printer_) {
    PRINTER_LOG(DEBUG) << "Local printer not available";
    return;
  }
  local_printer_->AddPrintServerObserver(
      receiver_.BindNewPipeAndPassRemoteWithVersion(), base::DoNothing());
}

void PrintPreviewHandlerChromeOS::OnJavascriptDisallowed() {
  // Normally the handler and print preview will be destroyed together, but
  // this is necessary for refresh or navigation from the chrome://print page.
  weak_factory_.InvalidateWeakPtrs();
  receiver_.reset();
}

void PrintPreviewHandlerChromeOS::HandleGrantExtensionPrinterAccess(
    const base::Value::List& args) {
  DCHECK(args[0].is_string());
  DCHECK(args[1].is_string());
  std::string callback_id = args[0].GetString();
  std::string printer_id = args[1].GetString();
  DCHECK(!callback_id.empty());
  MaybeAllowJavascript();

  PrinterHandler* handler = GetPrinterHandler(mojom::PrinterType::kExtension);
  handler->StartGrantPrinterAccess(
      printer_id,
      base::BindOnce(&PrintPreviewHandlerChromeOS::OnGotExtensionPrinterInfo,
                     weak_factory_.GetWeakPtr(), callback_id));
}

// |args| is expected to contain a string with representing the callback id
// followed by a list of arguments the first of which should be the printer id.
void PrintPreviewHandlerChromeOS::HandlePrinterSetup(
    const base::Value::List& args) {
  std::string callback_id;
  std::string printer_name;
  MaybeAllowJavascript();
  if (args[0].is_string() && args[1].is_string()) {
    callback_id = args[0].GetString();
    printer_name = args[1].GetString();
  }

  if (callback_id.empty() || printer_name.empty()) {
    RejectJavascriptCallback(base::Value(callback_id),
                             base::Value(printer_name));
    return;
  }

  PrinterHandler* handler = GetPrinterHandler(mojom::PrinterType::kLocal);
  handler->StartGetCapability(
      printer_name,
      base::BindOnce(&PrintPreviewHandlerChromeOS::SendPrinterSetup,
                     weak_factory_.GetWeakPtr(), callback_id, printer_name));
}

void PrintPreviewHandlerChromeOS::HandleGetEulaUrl(
    const base::Value::List& args) {
  CHECK_EQ(2U, args.size());
  MaybeAllowJavascript();

  const std::string& callback_id = args[0].GetString();
  const std::string& destination_id = args[1].GetString();

  PrinterHandler* handler = GetPrinterHandler(mojom::PrinterType::kLocal);
  handler->StartGetEulaUrl(
      destination_id, base::BindOnce(&PrintPreviewHandlerChromeOS::SendEulaUrl,
                                     weak_factory_.GetWeakPtr(), callback_id));
}

void PrintPreviewHandlerChromeOS::SendEulaUrl(const std::string& callback_id,
                                              const std::string& eula_url) {
  VLOG(1) << "Get PPD license finished";
  ResolveJavascriptCallback(base::Value(callback_id), base::Value(eula_url));
}

// Resolves the callback with a PrinterSetupResponse object (defined in
// chrome/browser/resources/print_preview/native_layer_cros.js) or rejects it
// if `destination_info` does not contain a capabilities dictionary.
// `destination_info` is a CapabilitiesResponse object (defined in
// chrome/browser/resources/print_preview/native_layer.js).
void PrintPreviewHandlerChromeOS::SendPrinterSetup(
    const std::string& callback_id,
    const std::string& printer_name,
    base::Value::Dict destination_info) {
  base::Value::Dict* caps_value =
      destination_info.FindDict(kSettingCapabilities);
  if (!caps_value) {
    VLOG(1) << "Printer setup failed";
    RejectJavascriptCallback(base::Value(callback_id), base::Value());
    return;
  }

  FilterContinuousFeedMediaSizes(*caps_value);
  base::Value::Dict response;
  response.Set("printerId", printer_name);
  response.Set("capabilities", std::move(*caps_value));
  base::Value::Dict* printer = destination_info.FindDict(kPrinter);
  if (printer) {
    base::Value::Dict* policies_value = printer->FindDict(kSettingPolicies);
    if (policies_value)
      response.Set("policies", std::move(*policies_value));
  }
  ResolveJavascriptCallback(base::Value(callback_id), response);
}

PrintPreviewHandler* PrintPreviewHandlerChromeOS::GetPrintPreviewHandler() {
  PrintPreviewUI* ui = web_ui()->GetController()->GetAs<PrintPreviewUI>();
  CHECK(ui);
  return ui->handler();
}

PrinterHandler* PrintPreviewHandlerChromeOS::GetPrinterHandler(
    mojom::PrinterType printer_type) {
  return GetPrintPreviewHandler()->GetPrinterHandler(printer_type);
}

void PrintPreviewHandlerChromeOS::MaybeAllowJavascript() {
  if (!IsJavascriptAllowed() &&
      GetPrintPreviewHandler()->IsJavascriptAllowed()) {
    AllowJavascript();
  }
}

void PrintPreviewHandlerChromeOS::OnGotExtensionPrinterInfo(
    const std::string& callback_id,
    const base::Value::Dict& printer_info) {
  if (printer_info.empty()) {
    RejectJavascriptCallback(base::Value(callback_id), base::Value());
  } else {
    ResolveJavascriptCallback(base::Value(callback_id), printer_info);
  }
}

void PrintPreviewHandlerChromeOS::HandleRequestPrinterStatusUpdate(
    const base::Value::List& args) {
  CHECK_EQ(2U, args.size());

  const std::string& callback_id = args[0].GetString();
  const std::string& printer_id = args[1].GetString();

  MaybeAllowJavascript();
  PrinterHandler* handler = GetPrinterHandler(mojom::PrinterType::kLocal);
  handler->StartPrinterStatusRequest(
      printer_id,
      base::BindOnce(&PrintPreviewHandlerChromeOS::
                         HandleRequestPrinterStatusUpdateCompletion,
                     weak_factory_.GetWeakPtr(), base::Value(callback_id)));
}

void PrintPreviewHandlerChromeOS::HandleRequestPrinterStatusUpdateCompletion(
    base::Value callback_id,
    std::optional<base::Value::Dict> result) {
  if (result)
    ResolveJavascriptCallback(callback_id, *result);
  else
    ResolveJavascriptCallback(callback_id, base::Value());
}

void PrintPreviewHandlerChromeOS::HandleChoosePrintServers(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());

  const base::Value& val = args[0];
  std::vector<std::string> print_server_ids;
  for (const auto& id : val.GetList()) {
    print_server_ids.push_back(id.GetString());
  }
  MaybeAllowJavascript();
  FireWebUIListener("server-printers-loading", base::Value(true));
  if (!local_printer_) {
    PRINTER_LOG(DEBUG) << "Local printer not available";
    return;
  }
  local_printer_->ChoosePrintServers(print_server_ids, base::DoNothing());
}

void PrintPreviewHandlerChromeOS::HandleGetPrintServersConfig(
    const base::Value::List& args) {
  CHECK(args[0].is_string());
  std::string callback_id = args[0].GetString();
  CHECK(!callback_id.empty());
  MaybeAllowJavascript();
  if (!local_printer_) {
    PRINTER_LOG(DEBUG) << "Local printer not available";
    ResolveJavascriptCallback(base::Value(callback_id), base::Value());
    return;
  }
  local_printer_->GetPrintServersConfig(
      base::BindOnce(PrintServersConfigMojomToValue)
          .Then(base::BindOnce(
              &PrintPreviewHandlerChromeOS::ResolveJavascriptCallback,
              weak_factory_.GetWeakPtr(), base::Value(callback_id))));
}

void PrintPreviewHandlerChromeOS::HandleRecordPrintAttemptOutcome(
    const base::Value::List& args) {
  CHECK(args[0].is_int());
  chromeos::PrintAttemptOutcome result =
      static_cast<chromeos::PrintAttemptOutcome>(args[0].GetInt());
  base::UmaHistogramEnumeration("PrintPreview.PrintAttemptOutcome", result);
}

void PrintPreviewHandlerChromeOS::OnPrintServersChanged(
    crosapi::mojom::PrintServersConfigPtr ptr) {
  MaybeAllowJavascript();
  FireWebUIListener("print-servers-config-changed",
                    PrintServersConfigMojomToValue(std::move(ptr)));
}

void PrintPreviewHandlerChromeOS::OnServerPrintersChanged() {
  MaybeAllowJavascript();
  FireWebUIListener("server-printers-loading", base::Value(false));
}

content::WebContents* PrintPreviewHandlerChromeOS::GetInitiator() {
  if (this->test_initiator_) {
    return this->test_initiator_;
  }

  auto* dialog_controller = PrintPreviewDialogController::GetInstance();
  CHECK(dialog_controller);
  return dialog_controller->GetInitiator(web_ui()->GetWebContents());
}

void PrintPreviewHandlerChromeOS::HandleGetShowManagePrinters(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  CHECK(args[0].is_string());

  // AllowJavascript needs to be called here instead of relying on
  // `HandleGetInitialSettings` due to timing of calls.
  if (!IsJavascriptAllowed()) {
    AllowJavascript();
  }

  auto* initiator = this->GetInitiator();
  if (initiator == nullptr) {
    ResolveJavascriptCallback(args[0], base::Value(false));
    return;
  }

  const bool domain_is_os_settings = initiator->GetLastCommittedURL().DomainIs(
      chrome::kChromeUIOSSettingsHost);
  ResolveJavascriptCallback(args[0], base::Value(!domain_is_os_settings));
}

void PrintPreviewHandlerChromeOS::HandleObserveLocalPrinters(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  CHECK(args[0].is_string());
  const std::string& callback_id = args[0].GetString();

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (int{crosapi::mojom::LocalPrinter::MethodMinVersions::
              kAddLocalPrintersObserverMinVersion} > local_printer_version_) {
    PRINTER_LOG(DEBUG) << "Local printer version incompatible";
    ResolveJavascriptCallback(callback_id, base::Value::List());
    return;
  }
#endif

  if (!local_printer_) {
    PRINTER_LOG(DEBUG) << "Local printer not available";
    ResolveJavascriptCallback(callback_id, base::Value::List());
    return;
  }

  // Each instance of Print Preview only needs to subscribe once.
  if (local_printers_receiver_.is_bound()) {
    ResolveJavascriptCallback(callback_id, base::Value::List());
    return;
  }

  local_printer_->AddLocalPrintersObserver(
      local_printers_receiver_.BindNewPipeAndPassRemoteWithVersion(),
      base::BindOnce(&PrintPreviewHandlerChromeOS::OnHandleObserveLocalPrinters,
                     weak_factory_.GetWeakPtr(), callback_id));
}

void PrintPreviewHandlerChromeOS::OnHandleObserveLocalPrinters(
    const std::string& callback_id,
    std::vector<crosapi::mojom::LocalDestinationInfoPtr> printers) {
  ResolveJavascriptCallback(callback_id, ConvertPrintersToValues(printers));
}

void PrintPreviewHandlerChromeOS::OnLocalPrintersUpdated(
    std::vector<crosapi::mojom::LocalDestinationInfoPtr> printers) {
  FireWebUIListener("local-printers-updated",
                    ConvertPrintersToValues(printers));
}

void PrintPreviewHandlerChromeOS::SetInitiatorForTesting(
    content::WebContents* test_initiator) {
  this->test_initiator_ = test_initiator;
}

}  // namespace printing
