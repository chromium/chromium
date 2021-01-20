// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_EXTENSION_PRINTER_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_EXTENSION_PRINTER_HANDLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/webui/print_preview/printer_handler.h"
#include "extensions/browser/api/printer_provider/printer_provider_api.h"

namespace base {
class DictionaryValue;
class ListValue;
class RefCountedMemory;
}

namespace cloud_devices {
class CloudDeviceDescription;
}

namespace gfx {
class Size;
}

class Profile;

namespace printing {

class PwgRasterConverter;

// Implementation of PrinterHandler interface backed by printerProvider
// extension API.
class ExtensionPrinterHandler : public PrinterHandler {
 public:
  using PrintJobCallback = base::OnceCallback<void(
      std::unique_ptr<extensions::PrinterProviderPrintJob>)>;

  explicit ExtensionPrinterHandler(Profile* profile);

  ~ExtensionPrinterHandler() override;

  // PrinterHandler implementation:
  void Reset() override;
  void StartGetPrinters(AddedPrintersCallback added_printers_callback,
                        GetPrintersDoneCallback done_callback) override;
  void StartGetCapability(const std::string& destination_id,
                          GetCapabilityCallback callback) override;
  void StartPrint(const base::string16& job_title,
                  base::Value settings,
                  scoped_refptr<base::RefCountedMemory> print_data,
                  PrintCallback callback) override;
  void StartGrantPrinterAccess(const std::string& printer_id,
                               GetPrinterInfoCallback callback) override;

 private:
  friend class ExtensionPrinterHandlerTest;

  void SetPwgRasterConverterForTesting(
      std::unique_ptr<PwgRasterConverter> pwg_raster_converter);

  // Converts |data| to PWG raster format (from PDF) for a printer described
  // by |printer_description|.
  // |callback| is called with the converted data.
  void ConvertToPWGRaster(
      scoped_refptr<base::RefCountedMemory> data,
      const cloud_devices::CloudDeviceDescription& printer_description,
      const gfx::Size& page_size,
      std::unique_ptr<extensions::PrinterProviderPrintJob> job,
      PrintJobCallback callback);

  // Sets print job document data and dispatches it using printerProvider API.
  void DispatchPrintJob(
      PrintCallback callback,
      std::unique_ptr<extensions::PrinterProviderPrintJob> print_job);

  // Methods used as wrappers to callbacks for extensions::PrinterProviderAPI
  // methods, primarily so the callbacks can be bound to this class' weak ptr.
  // They just propagate results to callbacks passed to them.
  void WrapGetPrintersCallback(AddedPrintersCallback callback,
                               const base::ListValue& printers,
                               bool done);
  void WrapGetCapabilityCallback(GetCapabilityCallback callback,
                                 const base::DictionaryValue& capability);
  void WrapPrintCallback(PrintCallback callback, const base::Value& status);
  void WrapGetPrinterInfoCallback(GetPrinterInfoCallback callback,
                                  const base::DictionaryValue& printer_info);

  void OnUsbDevicesEnumerated(
      AddedPrintersCallback callback,
      std::vector<device::mojom::UsbDeviceInfoPtr> devices);

  Profile* const profile_;
  GetPrintersDoneCallback done_callback_;
  PrintJobCallback print_job_callback_;
  std::unique_ptr<PwgRasterConverter> pwg_raster_converter_;
  int pending_enumeration_count_ = 0;

  base::WeakPtrFactory<ExtensionPrinterHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ExtensionPrinterHandler);
};

}  // namespace printing

#endif  // CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_EXTENSION_PRINTER_HANDLER_H_
