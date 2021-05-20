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
#include "chromeos/printing/printer_configuration.h"
#include "components/cloud_devices/common/cloud_devices_urls.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "content/public/browser/web_ui.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/account_manager/account_manager_util.h"
#include "chrome/browser/chromeos/printing/cups_printers_manager.h"
#include "chrome/browser/chromeos/printing/cups_printers_manager_factory.h"
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/local_printer.mojom.h"
#include "chromeos/lacros/lacros_chrome_service_impl.h"
#include "mojo/public/cpp/bindings/receiver.h"
#endif

namespace printing {

namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)
base::Value ConvertPrintServersConfig(
    const chromeos::PrintServersConfig& config) {
  base::Value ui_print_servers(base::Value::Type::LIST);
  for (const auto& print_server : config.print_servers) {
    base::Value ui_print_server(base::Value::Type::DICTIONARY);
    ui_print_server.SetStringKey("id", print_server.GetId());
    ui_print_server.SetStringKey("name", print_server.GetName());
    ui_print_servers.Append(std::move(ui_print_server));
  }
  base::Value ui_print_servers_config(base::Value::Type::DICTIONARY);
  ui_print_servers_config.SetKey("printServers", std::move(ui_print_servers));
  ui_print_servers_config.SetBoolKey(
      "isSingleServerFetchingMode",
      config.fetching_mode ==
          chromeos::ServerPrintersFetchingMode::kSingleServerOnly);
  return ui_print_servers_config;
}

#elif BUILDFLAG(IS_CHROMEOS_LACROS)
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
          chromeos::ServerPrintersFetchingMode::kSingleServerOnly);
  return ui_print_servers_config;
}
#endif

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
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  chromeos::LacrosChromeServiceImpl* service =
      chromeos::LacrosChromeServiceImpl::Get();
  if (!service->IsAvailable<crosapi::mojom::LocalPrinter>()) {
    LOG(ERROR) << "Local printer not available";
    return;
  }
  local_printer_ = service->GetRemote<crosapi::mojom::LocalPrinter>().get();
#endif
}

PrintPreviewHandlerChromeOS::~PrintPreviewHandlerChromeOS() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!base::FeatureList::IsEnabled(chromeos::features::kPrintServerScaling))
    return;
  Profile* profile = Profile::FromWebUI(web_ui());
  auto* cups_manager =
      chromeos::CupsPrintersManagerFactory::GetForBrowserContext(profile);
  if (cups_manager) {
    auto* print_servers_manager = cups_manager->GetPrintServersManager();
    print_servers_manager->RemoveObserver(this);
  }
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  receiver_.reset();
#endif
}

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
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!base::FeatureList::IsEnabled(chromeos::features::kPrintServerScaling))
    return;
#endif
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
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!base::FeatureList::IsEnabled(chromeos::features::kPrintServerScaling))
    return;
  Profile* profile = Profile::FromWebUI(web_ui());
  print_servers_manager_ =
      chromeos::CupsPrintersManagerFactory::GetForBrowserContext(profile)
          ->GetPrintServersManager();
  print_servers_manager_->AddObserver(this);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  receiver_.reset();  // Just in case this method is called multiple times.
  if (!local_printer_) {
    LOG(ERROR) << "Local printer not available";
    return;
  }
  local_printer_->AddObserver(receiver_.BindNewPipeAndPassRemoteWithVersion(),
                              base::DoNothing());
#endif
}

void PrintPreviewHandlerChromeOS::OnJavascriptDisallowed() {
  // Normally the handler and print preview will be destroyed together, but
  // this is necessary for refresh or navigation from the chrome://print page.
  weak_factory_.InvalidateWeakPtrs();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!base::FeatureList::IsEnabled(chromeos::features::kPrintServerScaling))
    return;
  print_servers_manager_->RemoveObserver(this);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  receiver_.reset();
#endif
}

void PrintPreviewHandlerChromeOS::HandleGrantExtensionPrinterAccess(
    const base::ListValue* args) {
  std::string callback_id;
  std::string printer_id;
  bool ok = args->GetString(0, &callback_id) &&
            args->GetString(1, &printer_id) && !callback_id.empty();
  DCHECK(ok);
  MaybeAllowJavascript();

  PrinterHandler* handler = GetPrinterHandler(PrinterType::kExtension);
  handler->StartGrantPrinterAccess(
      printer_id,
      base::BindOnce(&PrintPreviewHandlerChromeOS::OnGotExtensionPrinterInfo,
                     weak_factory_.GetWeakPtr(), callback_id));
}

// |args| is expected to contain a string with representing the callback id
// followed by a list of arguments the first of which should be the printer id.
void PrintPreviewHandlerChromeOS::HandlePrinterSetup(
    const base::ListValue* args) {
  std::string callback_id;
  std::string printer_name;
  MaybeAllowJavascript();
  if (!args->GetString(0, &callback_id) || !args->GetString(1, &printer_name) ||
      callback_id.empty() || printer_name.empty()) {
    RejectJavascriptCallback(base::Value(callback_id),
                             base::Value(printer_name));
    return;
  }

  PrinterHandler* handler = GetPrinterHandler(PrinterType::kLocal);
  handler->StartGetCapability(
      printer_name,
      base::BindOnce(&PrintPreviewHandlerChromeOS::SendPrinterSetup,
                     weak_factory_.GetWeakPtr(), callback_id, printer_name));
}

void PrintPreviewHandlerChromeOS::HandleGetAccessToken(
    const base::ListValue* args) {
  std::string callback_id;

  bool ok = args->GetString(0, &callback_id) && !callback_id.empty();
  DCHECK(ok);
  MaybeAllowJavascript();

  if (!token_service_)
    token_service_ = std::make_unique<AccessTokenService>();
  token_service_->RequestToken(
      base::BindOnce(&PrintPreviewHandlerChromeOS::SendAccessToken,
                     weak_factory_.GetWeakPtr(), callback_id));
}

void PrintPreviewHandlerChromeOS::HandleGetEulaUrl(
    const base::ListValue* args) {
  CHECK_EQ(2U, args->GetSize());
  MaybeAllowJavascript();

  const std::string& callback_id = args->GetList()[0].GetString();
  const std::string& destination_id = args->GetList()[1].GetString();

  PrinterHandler* handler = GetPrinterHandler(PrinterType::kLocal);
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

void PrintPreviewHandlerChromeOS::SendPrinterSetup(
    const std::string& callback_id,
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

PrintPreviewHandler* PrintPreviewHandlerChromeOS::GetPrintPreviewHandler() {
  PrintPreviewUI* ui = static_cast<PrintPreviewUI*>(web_ui()->GetController());
  return ui->handler();
}

PrinterHandler* PrintPreviewHandlerChromeOS::GetPrinterHandler(
    PrinterType printer_type) {
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
    const base::ListValue* args) {
  CHECK_EQ(2U, args->GetSize());

  const std::string& callback_id = args->GetList()[0].GetString();
  const std::string& printer_id = args->GetList()[1].GetString();

  MaybeAllowJavascript();
  PrinterHandler* handler = GetPrinterHandler(PrinterType::kLocal);
  handler->StartPrinterStatusRequest(
      printer_id,
      base::BindOnce(&PrintPreviewHandlerChromeOS::ResolveJavascriptCallback,
                     weak_factory_.GetWeakPtr(), base::Value(callback_id)));
}

void PrintPreviewHandlerChromeOS::HandleChoosePrintServers(
    const base::ListValue* args) {
  CHECK_EQ(1U, args->GetSize());

  const base::Value& val = args->GetList()[0];
  std::vector<std::string> print_server_ids;
  for (const auto& id : val.GetList()) {
    print_server_ids.push_back(id.GetString());
  }
  MaybeAllowJavascript();
  FireWebUIListener("server-printers-loading", base::Value(true));
#if BUILDFLAG(IS_CHROMEOS_ASH)
  print_servers_manager_->ChoosePrintServer(print_server_ids);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!local_printer_) {
    LOG(ERROR) << "Local printer not available";
    return;
  }
  local_printer_->ChoosePrintServers(print_server_ids, base::DoNothing());
#endif
}

void PrintPreviewHandlerChromeOS::HandleGetPrintServersConfig(
    const base::ListValue* args) {
  std::string callback_id;
  CHECK(args->GetString(0, &callback_id));
  CHECK(!callback_id.empty());
  MaybeAllowJavascript();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  const chromeos::PrintServersConfig print_servers_config =
      print_servers_manager_->GetPrintServersConfig();
  base::Value ui_print_servers_config =
      ConvertPrintServersConfig(print_servers_config);
  ResolveJavascriptCallback(base::Value(callback_id), ui_print_servers_config);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
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
#endif
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void PrintPreviewHandlerChromeOS::OnPrintServersChanged(
    const chromeos::PrintServersConfig& config) {
  MaybeAllowJavascript();
  FireWebUIListener("print-servers-config-changed",
                    ConvertPrintServersConfig(config));
}

void PrintPreviewHandlerChromeOS::OnServerPrintersChanged(
    const std::vector<chromeos::PrinterDetector::DetectedPrinter>&) {
  MaybeAllowJavascript();
  FireWebUIListener("server-printers-loading", base::Value(false));
}

#elif BUILDFLAG(IS_CHROMEOS_LACROS)
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
#endif

}  // namespace printing
