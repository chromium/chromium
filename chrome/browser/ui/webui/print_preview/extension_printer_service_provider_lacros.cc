// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/extension_printer_service_provider_lacros.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/unguessable_token.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/print_preview/extension_printer_service_provider_factory_lacros.h"
#include "chrome/browser/ui/webui/print_preview/printer_handler.h"
#include "chromeos/crosapi/mojom/extension_printer.mojom.h"
#include "chromeos/lacros/lacros_service.h"

namespace printing {

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
      ->PrintersAdded(request_id, std::move(printers), /* is_done=*/false);
}

void ExtensionPrinterServiceProviderLacros::OnGetPrintersDone(
    base::UnguessableToken request_id) {
  VLOG(1) << "ExtensionPrinterServiceProviderLacros::OnGetPrintersDone():"
          << " request_id=" << request_id;

  base::Value::List printers;  // return an empty list of printers.
  chromeos::LacrosService::Get()
      ->GetRemote<crosapi::mojom::ExtensionPrinterService>()
      ->PrintersAdded(request_id, std::move(printers), /* is_done=*/true);
}

}  // namespace printing
