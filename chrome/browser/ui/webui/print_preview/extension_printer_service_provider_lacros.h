// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_EXTENSION_PRINTER_SERVICE_PROVIDER_LACROS_H_
#define CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_EXTENSION_PRINTER_SERVICE_PROVIDER_LACROS_H_

#include <memory>
#include <string>

#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ui/webui/print_preview/printer_handler.h"
#include "chromeos/crosapi/mojom/extension_printer.mojom.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace content {
class BrowserContext;
}

namespace printing {

// Implements crosapi::mojom::ExtensionPrinterServiceProvider to enable
// ash-chrome to request printing operations (e.g., querying printers,
// submitting jobs) from Lacros extensions. One instance per BrowserContext.
class ExtensionPrinterServiceProviderLacros
    : public extensions::BrowserContextKeyedAPI,
      public crosapi::mojom::ExtensionPrinterServiceProvider {
 public:
  explicit ExtensionPrinterServiceProviderLacros(
      content::BrowserContext* context);
  ExtensionPrinterServiceProviderLacros(
      const ExtensionPrinterServiceProviderLacros&) = delete;
  ExtensionPrinterServiceProviderLacros& operator=(
      const ExtensionPrinterServiceProviderLacros&) = delete;
  ~ExtensionPrinterServiceProviderLacros() override;

  content::BrowserContext* browser_context() { return browser_context_; }

  static ExtensionPrinterServiceProviderLacros* GetForBrowserContext(
      content::BrowserContext* context);

  // crosapi::mojom::ExtensionPrinterServiceProvider:
  void DispatchGetPrintersRequest(
      const ::base::UnguessableToken& request_id) override;
  void DispatchResetRequest() override;
  void DispatchStartGetCapability(
      const std::string& destination_id,
      DispatchStartGetCapabilityCallback callback) override;
  void DispatchStartPrint(const std::u16string& job_title,
                          base::Value::Dict settings,
                          scoped_refptr<::base::RefCountedMemory> print_data,
                          DispatchStartPrintCallback callback) override;
  void DispatchStartGrantPrinterAccess(
      const std::string& printer_id,
      DispatchStartGrantPrinterAccessCallback callback) override;

  void SetPrinterHandlerForTesting(std::unique_ptr<PrinterHandler> handler) {
    extension_printer_handler_ = std::move(handler);
  }

 private:
  void OnAddedPrinters(const base::UnguessableToken request_id,
                       base::Value::List printers);
  void OnGetPrintersDone(base::UnguessableToken request_id);

  friend class extensions::BrowserContextKeyedAPIFactory<
      ExtensionPrinterServiceProviderLacros>;

  raw_ptr<content::BrowserContext> browser_context_;  // not owned.
  mojo::Receiver<crosapi::mojom::ExtensionPrinterServiceProvider> receiver_{
      this};
  // Handles requests for extension printers.
  std::unique_ptr<PrinterHandler> extension_printer_handler_;

  base::WeakPtrFactory<ExtensionPrinterServiceProviderLacros> weak_ptr_factory_{
      this};
};

}  // namespace printing

#endif  // CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_EXTENSION_PRINTER_SERVICE_PROVIDER_LACROS_H_
