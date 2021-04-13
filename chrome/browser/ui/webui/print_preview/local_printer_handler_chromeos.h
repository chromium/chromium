// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_LOCAL_PRINTER_HANDLER_CHROMEOS_H_
#define CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_LOCAL_PRINTER_HANDLER_CHROMEOS_H_

#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/printing/cups_printers_manager.h"
#include "chrome/browser/chromeos/printing/printer_configurer.h"
#include "chrome/browser/ui/webui/print_preview/printer_handler.h"
#include "chromeos/printing/ppd_provider.h"
#include "chromeos/printing/printer_configuration.h"

namespace content {
class WebContents;
}

class Profile;

namespace printing {

// This class must be created and used on the UI thread.
class LocalPrinterHandlerChromeos : public PrinterHandler {
 public:
  static std::unique_ptr<LocalPrinterHandlerChromeos> CreateDefault(
      Profile* profile,
      content::WebContents* preview_web_contents);

  static std::unique_ptr<LocalPrinterHandlerChromeos> CreateForTesting(
      Profile* profile,
      content::WebContents* preview_web_contents,
      chromeos::CupsPrintersManager* printers_manager,
      std::unique_ptr<chromeos::PrinterConfigurer> printer_configurer,
      scoped_refptr<chromeos::PpdProvider> ppd_provider);

  LocalPrinterHandlerChromeos(const LocalPrinterHandlerChromeos&) = delete;
  LocalPrinterHandlerChromeos& operator=(const LocalPrinterHandlerChromeos&) =
      delete;
  ~LocalPrinterHandlerChromeos() override;

  // PrinterHandler implementation
  void Reset() override;
  void GetDefaultPrinter(DefaultPrinterCallback cb) override;
  void StartGetPrinters(AddedPrintersCallback added_printers_callback,
                        GetPrintersDoneCallback done_callback) override;
  void StartGetCapability(const std::string& printer_name,
                          GetCapabilityCallback cb) override;
  void StartPrint(const std::u16string& job_title,
                  base::Value settings,
                  scoped_refptr<base::RefCountedMemory> print_data,
                  PrintCallback callback) override;
  void StartGetEulaUrl(const std::string& destination_id,
                       GetEulaUrlCallback cb) override;
  void StartPrinterStatusRequest(
      const std::string& printer_id,
      PrinterStatusRequestCallback callback) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(LocalPrinterHandlerChromeosTest,
                           GetNativePrinterPolicies);

  LocalPrinterHandlerChromeos(
      Profile* profile,
      content::WebContents* preview_web_contents,
      chromeos::CupsPrintersManager* printers_manager,
      std::unique_ptr<chromeos::PrinterConfigurer> printer_configurer,
      scoped_refptr<chromeos::PpdProvider> ppd_provider);

  // Creates a value dictionary containing the printing policies set by
  // |profile_|.
  // This function must be called from the UI thread.
  base::Value GetNativePrinterPolicies() const;

  Profile* const profile_;
  content::WebContents* const preview_web_contents_;
  chromeos::CupsPrintersManager* printers_manager_;
  std::unique_ptr<chromeos::PrinterConfigurer> printer_configurer_;
  const scoped_refptr<chromeos::PpdProvider> ppd_provider_;
  base::WeakPtrFactory<LocalPrinterHandlerChromeos> weak_factory_{this};
};

}  // namespace printing

#endif  // CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_LOCAL_PRINTER_HANDLER_CHROMEOS_H_
