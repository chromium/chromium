// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_EXTENSION_PRINTER_HANDLER_ADAPTER_ASH_H_
#define CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_EXTENSION_PRINTER_HANDLER_ADAPTER_ASH_H_

#include "chrome/browser/ui/webui/print_preview/printer_handler.h"

namespace crosapi {
class ExtensionPrinterServiceAsh;
}

namespace printing {

// Ash-chrome implementation of the PrinterHandler interface specifically for
// lacros extensions.
//
// - Delegates print requests to ExtensionPrinterServiceAsh for forwarding to
//   lacros and receiving responses via crosapi.
//
// - Actively replaces the default ExtensionPrinterHandler when lacros is in
//   use, as the default handler is not compatible with lacros extensions.
class ExtensionPrinterHandlerAdapterAsh : public PrinterHandler {
 public:
  ExtensionPrinterHandlerAdapterAsh();
  ExtensionPrinterHandlerAdapterAsh(const ExtensionPrinterHandlerAdapterAsh&) =
      delete;
  ExtensionPrinterHandlerAdapterAsh& operator=(
      const ExtensionPrinterHandlerAdapterAsh&) = delete;
  ~ExtensionPrinterHandlerAdapterAsh() override;

  // PrinterHandler implementation:
  void Reset() override;
  void StartGetPrinters(AddedPrintersCallback added_printers_callback,
                        GetPrintersDoneCallback done_callback) override;
  void StartGetCapability(const std::string& destination_id,
                          GetCapabilityCallback callback) override;
  void StartPrint(const std::u16string& job_title,
                  base::Value::Dict settings,
                  scoped_refptr<base::RefCountedMemory> print_data,
                  PrintCallback callback) override;
  void StartGrantPrinterAccess(const std::string& printer_id,
                               GetPrinterInfoCallback callback) override;

 private:
  crosapi::ExtensionPrinterServiceAsh* GetExtensionPrinterService();
};

}  // namespace printing

#endif  // CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_EXTENSION_PRINTER_HANDLER_ADAPTER_ASH_H_
