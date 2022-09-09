// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_LOCAL_PRINTER_HANDLER_DEFAULT_H_
#define CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_LOCAL_PRINTER_HANDLER_DEFAULT_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/print_preview/printer_handler.h"
#include "printing/backend/print_backend.h"

namespace base {
class TaskRunner;
}

namespace content {
class WebContents;
}

namespace printing {

class LocalPrinterHandlerDefault : public PrinterHandler {
 public:
  explicit LocalPrinterHandlerDefault(
      content::WebContents* preview_web_contents);

  LocalPrinterHandlerDefault(const LocalPrinterHandlerDefault&) = delete;
  LocalPrinterHandlerDefault& operator=(const LocalPrinterHandlerDefault&) =
      delete;

  ~LocalPrinterHandlerDefault() override;

  // PrinterHandler implementation.
  void Reset() override;
  void GetDefaultPrinter(DefaultPrinterCallback cb) override;
  void StartGetPrinters(AddedPrintersCallback added_printers_callback,
                        GetPrintersDoneCallback done_callback) override;
  void StartGetCapability(const std::string& destination_id,
                          GetCapabilityCallback callback) override;
  void StartPrint(const std::u16string& job_title,
                  base::Value::Dict settings,
                  scoped_refptr<base::RefCountedMemory> print_data,
                  PrintCallback callback) override;

 private:
  static PrinterList EnumeratePrintersAsync(const std::string& locale);
  static base::Value::Dict FetchCapabilitiesAsync(
      const std::string& device_name,
      const std::string& locale);
  static std::string GetDefaultPrinterAsync(const std::string& locale);

  const raw_ptr<content::WebContents> preview_web_contents_;

  // TaskRunner for blocking tasks. Threading behavior is platform-specific.
  scoped_refptr<base::TaskRunner> const task_runner_;
};

}  // namespace printing

#endif  // CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_LOCAL_PRINTER_HANDLER_DEFAULT_H_
