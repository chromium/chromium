// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_LOCAL_PRINTER_HANDLER_LACROS_H_
#define CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_LOCAL_PRINTER_HANDLER_LACROS_H_

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/print_preview/printer_handler.h"
#include "chromeos/lacros/lacros_chrome_service_impl.h"

namespace content {
class WebContents;
}

namespace printing {

class LocalPrinterHandlerLacros : public PrinterHandler {
 public:
  static std::unique_ptr<LocalPrinterHandlerLacros> CreateDefault(
      content::WebContents* preview_web_contents);
  LocalPrinterHandlerLacros(const LocalPrinterHandlerLacros&) = delete;
  LocalPrinterHandlerLacros& operator=(const LocalPrinterHandlerLacros&) =
      delete;
  ~LocalPrinterHandlerLacros() override;

  // PrinterHandler implementation.
  void Reset() override;
  void GetDefaultPrinter(DefaultPrinterCallback callback) override;
  void StartGetPrinters(AddedPrintersCallback added_printers_callback,
                        GetPrintersDoneCallback done_callback) override;
  void StartGetCapability(const std::string& destination_id,
                          GetCapabilityCallback callback) override;
  void StartPrint(const std::u16string& job_title,
                  base::Value settings,
                  scoped_refptr<base::RefCountedMemory> print_data,
                  PrintCallback callback) override;
  void StartGetEulaUrl(const std::string& destination_id,
                       GetEulaUrlCallback callback) override;
  void StartPrinterStatusRequest(
      const std::string& printer_id,
      PrinterStatusRequestCallback callback) override;

 private:
  explicit LocalPrinterHandlerLacros(
      content::WebContents* preview_web_contents);
  content::WebContents* const preview_web_contents_;
  chromeos::LacrosChromeServiceImpl* const service_;
};

}  // namespace printing

#endif  // CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_LOCAL_PRINTER_HANDLER_LACROS_H_
