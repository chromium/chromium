// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_LOCAL_PRINTER_HANDLER_DEFAULT_H_
#define CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_LOCAL_PRINTER_HANDLER_DEFAULT_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string16.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/print_preview/printer_handler.h"

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
  ~LocalPrinterHandlerDefault() override;

  // PrinterHandler implementation.
  void Reset() override;
  void GetDefaultPrinter(DefaultPrinterCallback cb) override;
  void StartGetPrinters(AddedPrintersCallback added_printers_callback,
                        GetPrintersDoneCallback done_callback) override;
  void StartGetCapability(const std::string& destination_id,
                          GetCapabilityCallback callback) override;
  void StartPrint(const base::string16& job_title,
                  base::Value settings,
                  scoped_refptr<base::RefCountedMemory> print_data,
                  PrintCallback callback) override;

 private:
  content::WebContents* const preview_web_contents_;

  // TaskRunner for blocking tasks. Threading behavior is platform-specific.
  scoped_refptr<base::TaskRunner> const task_runner_;

  DISALLOW_COPY_AND_ASSIGN(LocalPrinterHandlerDefault);
};

}  // namespace printing

#endif  // CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_LOCAL_PRINTER_HANDLER_DEFAULT_H_
