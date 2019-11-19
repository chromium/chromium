// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/printer_handler.h"

#include "build/buildflag.h"
#include "chrome/browser/ui/webui/print_preview/cloud_printer_handler.h"
#include "chrome/browser/ui/webui/print_preview/extension_printer_handler.h"
#include "chrome/browser/ui/webui/print_preview/pdf_printer_handler.h"
#include "chrome/common/buildflags.h"

#if BUILDFLAG(ENABLE_SERVICE_DISCOVERY)
#include "chrome/browser/ui/webui/print_preview/privet_printer_handler.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/webui/print_preview/local_printer_handler_chromeos.h"
#else
#include "chrome/browser/ui/webui/print_preview/local_printer_handler_default.h"
#endif

namespace printing {

// static
std::unique_ptr<PrinterHandler> PrinterHandler::CreateForCloudPrinters() {
  return std::make_unique<CloudPrinterHandler>();
}

// static
std::unique_ptr<PrinterHandler> PrinterHandler::CreateForExtensionPrinters(
    Profile* profile) {
  return std::make_unique<ExtensionPrinterHandler>(profile);
}

// static
std::unique_ptr<PrinterHandler> PrinterHandler::CreateForLocalPrinters(
    content::WebContents* preview_web_contents,
    Profile* profile) {
#if defined(OS_CHROMEOS)
  return LocalPrinterHandlerChromeos::CreateDefault(profile,
                                                    preview_web_contents);
#else
  return std::make_unique<LocalPrinterHandlerDefault>(preview_web_contents);
#endif
}

// static
std::unique_ptr<PrinterHandler> PrinterHandler::CreateForPdfPrinter(
    Profile* profile,
    content::WebContents* preview_web_contents,
    StickySettings* sticky_settings) {
  return std::make_unique<PdfPrinterHandler>(profile, preview_web_contents,
                                             sticky_settings);
}

#if BUILDFLAG(ENABLE_SERVICE_DISCOVERY)
// static
std::unique_ptr<PrinterHandler> PrinterHandler::CreateForPrivetPrinters(
    Profile* profile) {
  return std::make_unique<PrivetPrinterHandler>(profile);
}
#endif

void PrinterHandler::GetDefaultPrinter(DefaultPrinterCallback cb) {
  NOTREACHED();
}

void PrinterHandler::StartGrantPrinterAccess(const std::string& printer_id,
                                             GetPrinterInfoCallback callback) {
  NOTREACHED();
}

}  // namespace printing
