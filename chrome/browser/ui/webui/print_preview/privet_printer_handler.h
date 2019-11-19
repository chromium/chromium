// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRIVET_PRINTER_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRIVET_PRINTER_HANDLER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "chrome/browser/local_discovery/service_discovery_shared_client.h"
#include "chrome/browser/printing/cloud_print/privet_local_printer_lister.h"
#include "chrome/browser/ui/webui/print_preview/printer_handler.h"

namespace base {
class DictionaryValue;
class OneShotTimer;
class RefCountedMemory;
}  // namespace base

namespace gfx {
class Size;
}

namespace printing {

// Implementation of PrinterHandler interface
class PrivetPrinterHandler
    : public PrinterHandler,
      public cloud_print::PrivetLocalPrinterLister::Delegate,
      public cloud_print::PrivetLocalPrintOperation::Delegate {
 public:
  explicit PrivetPrinterHandler(Profile* profile);

  ~PrivetPrinterHandler() override;

  // PrinterHandler implementation:
  void Reset() override;
  void StartGetPrinters(AddedPrintersCallback added_printers_callback,
                        GetPrintersDoneCallback done_callback) override;
  void StartGetCapability(const std::string& destination_id,
                          GetCapabilityCallback calback) override;
  void StartPrint(const base::string16& job_title,
                  base::Value ticket,
                  scoped_refptr<base::RefCountedMemory> print_data,
                  PrintCallback callback) override;

  // PrivetLocalPrinterLister::Delegate implementation.
  void LocalPrinterChanged(
      const std::string& name,
      bool has_local_printing,
      const cloud_print::DeviceDescription& description) override;
  void LocalPrinterRemoved(const std::string& name) override;
  void LocalPrinterCacheFlushed() override;

  // PrivetLocalPrintOperation::Delegate implementation.
  void OnPrivetPrintingDone(
      const cloud_print::PrivetLocalPrintOperation* print_operation) override;
  void OnPrivetPrintingError(
      const cloud_print::PrivetLocalPrintOperation* print_operation,
      int http_code) override;

 private:
  void StartLister(
      scoped_refptr<local_discovery::ServiceDiscoverySharedClient> client);
  void StopLister();
  void CapabilitiesUpdateClient(
      std::unique_ptr<cloud_print::PrivetHTTPClient> http_client);
  void OnGotCapabilities(const base::DictionaryValue* capabilities);
  void PrintUpdateClient(
      const base::string16& job_title,
      scoped_refptr<base::RefCountedMemory> print_data,
      base::Value print_ticket,
      const std::string& capabilities,
      const gfx::Size& page_size,
      std::unique_ptr<cloud_print::PrivetHTTPClient> http_client);
  bool UpdateClient(std::unique_ptr<cloud_print::PrivetHTTPClient> http_client);
  void StartPrint(const base::string16& job_title,
                  scoped_refptr<base::RefCountedMemory> print_data,
                  base::Value print_ticket,
                  const std::string& capabilities,
                  const gfx::Size& page_size);
  void CreateHTTP(
      const std::string& name,
      cloud_print::PrivetHTTPAsynchronousFactory::ResultCallback callback);

  Profile* profile_;
  scoped_refptr<local_discovery::ServiceDiscoverySharedClient>
      service_discovery_client_;
  std::unique_ptr<cloud_print::PrivetLocalPrinterLister> printer_lister_;
  std::unique_ptr<base::OneShotTimer> privet_lister_timer_;
  std::unique_ptr<cloud_print::PrivetHTTPAsynchronousFactory>
      privet_http_factory_;
  std::unique_ptr<cloud_print::PrivetHTTPResolution> privet_http_resolution_;
  std::unique_ptr<cloud_print::PrivetV1HTTPClient> privet_http_client_;
  std::unique_ptr<cloud_print::PrivetJSONOperation>
      privet_capabilities_operation_;
  std::unique_ptr<cloud_print::PrivetLocalPrintOperation>
      privet_local_print_operation_;
  AddedPrintersCallback added_printers_callback_;
  GetPrintersDoneCallback done_callback_;
  PrintCallback print_callback_;
  GetCapabilityCallback capabilities_callback_;

  base::WeakPtrFactory<PrivetPrinterHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PrivetPrinterHandler);
};

}  // namespace printing

#endif  // CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRIVET_PRINTER_HANDLER_H_
