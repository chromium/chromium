// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/print_preview_handler_chromeos.h"

#include <ctype.h>
#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/lazy_instance.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/device_identity/device_oauth2_token_service.h"
#include "chrome/browser/device_identity/device_oauth2_token_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_handler.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_ui.h"
#include "chrome/browser/ui/webui/print_preview/printer_handler.h"
#include "chrome/common/printing/printer_capabilities.h"
#include "chromeos/crosapi/mojom/local_printer.mojom.h"
#include "chromeos/printing/printer_configuration.h"
#include "components/cloud_devices/common/cloud_devices_urls.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "content/public/browser/web_ui.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "printing/mojom/print.mojom.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/local_printer_ash.h"
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_service.h"
#endif

namespace printing {

namespace {

base::Value PrintServersConfigMojomToValue(
    crosapi::mojom::PrintServersConfigPtr config) {
  base::Value ui_print_servers(base::Value::Type::LIST);
  for (const auto& print_server : config->print_servers) {
    base::Value ui_print_server(base::Value::Type::DICTIONARY);
    ui_print_server.SetStringKey("id", print_server->id);
    ui_print_server.SetStringKey("name", print_server->name);
    ui_print_servers.Append(std::move(ui_print_server));
  }
  base::Value ui_print_servers_config(base::Value::Type::DICTIONARY);
  ui_print_servers_config.SetKey("printServers", std::move(ui_print_servers));
  ui_print_servers_config.SetBoolKey(
      "isSingleServerFetchingMode",
      config->fetching_mode ==
          ash::ServerPrintersFetchingMode::kSingleServerOnly);
  return ui_print_servers_config;
}

}  // namespace

class PrintPreviewHandlerChromeOS::AccessTokenService
    : public OAuth2AccessTokenManager::Consumer {
 public:
  AccessTokenService() : OAuth2AccessTokenManager::Consumer("print_preview") {}
  AccessTokenService(const AccessTokenService&) = delete;
  AccessTokenService& operator=(const AccessTokenService&) = delete;
  ~AccessTokenService() override = default;

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
};

PrintPreviewHandlerChromeOS::PrintPreviewHandlerChromeOS() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  DCHECK(crosapi::CrosapiManager::IsInitialized());
  local_printer_ =
      crosapi::CrosapiManager::Get()->crosapi_ash()->local_printer_ash();
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  if (!service->IsAvailable<crosapi::mojom::LocalPrinter>()) {
    LOG(ERROR) << "Local printer not available";
    return;
  }
  local_printer_ = service->GetRemote<crosapi::mojom::LocalPrinter>().get();
#endif
}

PrintPreviewHandlerChromeOS::~PrintPreviewHandlerChromeOS() = default;

void PrintPreviewHandlerChromeOS::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "setupPrinter",
      base::BindRepeating(&PrintPreviewHandlerChromeOS::HandlePrinterSetup,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getAccessToken",
      base::BindRepeating(&PrintPreviewHandlerChromeOS::HandleGetAccessToken,
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
}

void PrintPreviewHandlerChromeOS::OnJavascriptAllowed() {
  receiver_.reset();  // Just in case this method is called multiple times.
  if (!local_printer_) {
    LOG(ERROR) << "Local printer not available";
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

void PrintPreviewHandlerChromeOS::HandleGetAccessToken(
    const base::Value::List& args) {
  DCHECK(args[0].is_string());

  std::string callback_id = args[0].GetString();
  DCHECK(!callback_id.empty());
  MaybeAllowJavascript();

  if (!token_service_)
    token_service_ = std::make_unique<AccessTokenService>();
  token_service_->RequestToken(
      base::BindOnce(&PrintPreviewHandlerChromeOS::SendAccessToken,
                     weak_factory_.GetWeakPtr(), callback_id));
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

void PrintPreviewHandlerChromeOS::SendAccessToken(
    const std::string& callback_id,
    const std::string& access_token) {
  VLOG(1) << "Get getAccessToken finished";
  ResolveJavascriptCallback(base::Value(callback_id),
                            base::Value(access_token));
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
    base::Value destination_info) {
  base::Value* caps_value =
      destination_info.is_dict()
          ? destination_info.FindKeyOfType(kSettingCapabilities,
                                           base::Value::Type::DICTIONARY)
          : nullptr;
  if (!caps_value) {
    VLOG(1) << "Printer setup failed";
    RejectJavascriptCallback(base::Value(callback_id), base::Value());
    return;
  }

  base::Value response(base::Value::Type::DICTIONARY);
  response.SetKey("printerId", base::Value(printer_name));
  response.SetKey("capabilities", std::move(*caps_value));
  base::Value* printer =
      destination_info.FindKeyOfType(kPrinter, base::Value::Type::DICTIONARY);
  if (printer) {
    base::Value* policies_value =
        printer->FindKeyOfType(kSettingPolicies, base::Value::Type::DICTIONARY);
    if (policies_value)
      response.SetKey("policies", std::move(*policies_value));
  }
  ResolveJavascriptCallback(base::Value(callback_id), response);
}

PrintPreviewHandler* PrintPreviewHandlerChromeOS::GetPrintPreviewHandler() {
  PrintPreviewUI* ui = static_cast<PrintPreviewUI*>(web_ui()->GetController());
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
    const base::DictionaryValue& printer_info) {
  if (printer_info.DictEmpty()) {
    RejectJavascriptCallback(base::Value(callback_id), base::Value());
    return;
  }
  ResolveJavascriptCallback(base::Value(callback_id), printer_info);
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
      base::BindOnce(&PrintPreviewHandlerChromeOS::ResolveJavascriptCallback,
                     weak_factory_.GetWeakPtr(), base::Value(callback_id)));
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
    LOG(ERROR) << "Local printer not available";
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
    LOG(ERROR) << "Local printer not available";
    ResolveJavascriptCallback(base::Value(callback_id), base::Value());
    return;
  }
  local_printer_->GetPrintServersConfig(
      base::BindOnce(PrintServersConfigMojomToValue)
          .Then(base::BindOnce(
              &PrintPreviewHandlerChromeOS::ResolveJavascriptCallback,
              weak_factory_.GetWeakPtr(), base::Value(callback_id))));
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

}  // namespace printing
