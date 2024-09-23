// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/extension_printer_service_provider_lacros.h"

#include <memory>
#include <string>
#include <unordered_map>

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/print_preview/extension_printer_service_provider_factory_lacros.h"
#include "chrome/browser/ui/webui/print_preview/printer_handler.h"
#include "chromeos/crosapi/mojom/extension_printer.mojom.h"
#include "chromeos/lacros/lacros_service.h"

namespace printing {

using crosapi::mojom::StartPrintStatus;

StartPrintStatus ToStartPrintStatus(const base::Value& status) {
  static base::NoDestructor<std::unordered_map<std::string, StartPrintStatus>>
      string_to_status_map(
          {{"UNKNOWN", StartPrintStatus::kUnknown},
           {"OK", StartPrintStatus::KOk},
           {"FAILED", StartPrintStatus::kFailed},
           {"INVALID_TICKET", StartPrintStatus::kInvalidTicket},
           {"INVALID_DATA", StartPrintStatus::kInvalidData}});

  // Extension printer handler returns a none value when print status is "OK".
  if (status.is_none()) {
    return StartPrintStatus::KOk;
  }
  const auto it = string_to_status_map->find(status.GetString());
  return it != string_to_status_map->end() ? it->second
                                           : StartPrintStatus::kUnknown;
}

ExtensionPrinterServiceProviderLacros::ExtensionPrinterServiceProviderLacros(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  // Printing extensions from primary profile only is supported for now.
  if (profile != ProfileManager::GetPrimaryUserProfile()) {
    VLOG(1) << "ExtensionPrinterServiceProviderLacros():"
               "not the main profile";
    return;
  }

  auto* service = chromeos::LacrosService::Get();
  if (!service->IsAvailable<crosapi::mojom::ExtensionPrinterService>()) {
    VLOG(1) << "ExtensionPrinterServiceProviderLacros():"
            << " crosapi::mojom::ExtensionPrinterService is not available";
    return;
  }
  service->GetRemote<crosapi::mojom::ExtensionPrinterService>()
      ->RegisterServiceProvider(
          receiver_.BindNewPipeAndPassRemoteWithVersion());
  extension_printer_handler_ =
      PrinterHandler::CreateForExtensionPrinters(profile);
}

ExtensionPrinterServiceProviderLacros::
    ~ExtensionPrinterServiceProviderLacros() = default;

ExtensionPrinterServiceProviderLacros*
ExtensionPrinterServiceProviderLacros::GetForBrowserContext(
    content::BrowserContext* context) {
  return ExtensionPrinterServiceProviderFactoryLacros::GetForBrowserContext(
      context);
}

void ExtensionPrinterServiceProviderLacros::DispatchGetPrintersRequest(
    const ::base::UnguessableToken& request_id) {
  VLOG(1)
      << "ExtensionPrinterServiceProviderLacros::DispatchGetPrintersRequest():"
      << " request_id=" << request_id;
  extension_printer_handler_->StartGetPrinters(
      base::BindRepeating(
          &ExtensionPrinterServiceProviderLacros::OnAddedPrinters,
          weak_ptr_factory_.GetWeakPtr(), request_id),
      base::BindOnce(&ExtensionPrinterServiceProviderLacros::OnGetPrintersDone,
                     weak_ptr_factory_.GetWeakPtr(), request_id));
}

void ExtensionPrinterServiceProviderLacros::OnAddedPrinters(
    const base::UnguessableToken request_id,
    base::Value::List printers) {
  VLOG(1) << "ExtensionPrinterServiceProviderLacros::OnAddedPrinters():"
          << " request_id=" << request_id
          << ", # of printers=" << printers.size();

  chromeos::LacrosService::Get()
      ->GetRemote<crosapi::mojom::ExtensionPrinterService>()
      ->PrintersAdded(request_id, std::move(printers), /*is_done=*/false);
}

void ExtensionPrinterServiceProviderLacros::OnGetPrintersDone(
    base::UnguessableToken request_id) {
  VLOG(1) << "ExtensionPrinterServiceProviderLacros::OnGetPrintersDone():"
          << " request_id=" << request_id;

  // return an empty list of printers.
  chromeos::LacrosService::Get()
      ->GetRemote<crosapi::mojom::ExtensionPrinterService>()
      ->PrintersAdded(request_id, base::Value::List(), /*is_done=*/true);
}

void ExtensionPrinterServiceProviderLacros::DispatchResetRequest() {
  VLOG(1) << "ExtensionPrinterServiceProviderLacros::ClearOngoingRequests()";
  extension_printer_handler_->Reset();
}

void ExtensionPrinterServiceProviderLacros::DispatchStartGetCapability(
    const std::string& destination_id,
    DispatchStartGetCapabilityCallback callback) {
  VLOG(1)
      << "ExtensionPrinterServiceProviderLacros::DispatchStartGetCapability():"
      << " destination_id=" << destination_id;
  extension_printer_handler_->StartGetCapability(destination_id,
                                                 std::move(callback));
}

void ExtensionPrinterServiceProviderLacros::DispatchStartPrint(
    const std::u16string& job_title,
    base::Value::Dict settings,
    scoped_refptr<::base::RefCountedMemory> print_data,
    DispatchStartPrintCallback callback) {
  VLOG(1) << "ExtensionPrinterServiceProviderLacros::DispatchStartPrint():"
          << " job_title=" << job_title;
  extension_printer_handler_->StartPrint(
      job_title, std::move(settings), print_data,
      base::BindOnce(
          [](DispatchStartPrintCallback callback, const base::Value& status) {
            std::move(callback).Run(ToStartPrintStatus(status));
          },
          std::move(callback)));
}

void ExtensionPrinterServiceProviderLacros::DispatchStartGrantPrinterAccess(
    const std::string& printer_id,
    DispatchStartGrantPrinterAccessCallback callback) {
  VLOG(1) << "ExtensionPrinterServiceProviderLacros::"
             "DispatchStartGrantPrinterAccess():"
          << " printer_id=" << printer_id;
  extension_printer_handler_->StartGrantPrinterAccess(
      printer_id, base::BindOnce(
                      [](DispatchStartGrantPrinterAccessCallback callback,
                         const base::Value::Dict& printer_info) {
                        std::move(callback).Run(printer_info.Clone());
                      },
                      std::move(callback)));
}

}  // namespace printing
