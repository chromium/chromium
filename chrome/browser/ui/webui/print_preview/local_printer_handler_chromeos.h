// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_LOCAL_PRINTER_HANDLER_CHROMEOS_H_
#define CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_LOCAL_PRINTER_HANDLER_CHROMEOS_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/webui/print_preview/printer_handler.h"
#include "chromeos/crosapi/mojom/local_printer.mojom.h"

namespace base {
class Value;
}

namespace content {
class WebContents;
}

namespace printing {

// This class must be created and used on the UI thread.
class LocalPrinterHandlerChromeos : public PrinterHandler {
 public:
  using AshJobSettingsCallback =
      base::OnceCallback<void(base::Value::Dict settings)>;

  static std::unique_ptr<LocalPrinterHandlerChromeos> Create(
      content::WebContents* preview_web_contents);

  // Creates an instance suitable for testing with the given mojo connection to
  // Ash Chrome and with `preview_web_contents_` set to nullptr. PrinterHandler
  // methods run input callbacks with reasonable defaults when the mojo
  // connection is unavailable.
  static std::unique_ptr<LocalPrinterHandlerChromeos> CreateForTesting(
      crosapi::mojom::LocalPrinter* local_printer);

  // Prefer using Create() above.
  explicit LocalPrinterHandlerChromeos(
      content::WebContents* preview_web_contents);
  LocalPrinterHandlerChromeos(const LocalPrinterHandlerChromeos&) = delete;
  LocalPrinterHandlerChromeos& operator=(const LocalPrinterHandlerChromeos&) =
      delete;
  ~LocalPrinterHandlerChromeos() override;

  // Returns a LocalDestinationInfo object (defined in
  // chrome/browser/resources/print_preview/data/local_parsers.js).
  static base::Value::Dict PrinterToValue(
      const crosapi::mojom::LocalDestinationInfo& printer);

  // Returns a CapabilitiesResponse object (defined in
  // chrome/browser/resources/print_preview/native_layer.js).
  static base::Value::Dict CapabilityToValue(
      crosapi::mojom::CapabilitiesResponsePtr caps);

  // Returns a PrinterStatus object (defined in
  // chrome/browser/resources/print_preview/data/printer_status_cros.js).
  static base::Value::Dict StatusToValue(
      const crosapi::mojom::PrinterStatus& status);

  // PrinterHandler implementation.
  void Reset() override;
  void GetDefaultPrinter(DefaultPrinterCallback callback) override;
  void StartGetPrinters(AddedPrintersCallback added_printers_callback,
                        GetPrintersDoneCallback done_callback) override;
  void StartGetCapability(const std::string& destination_id,
                          GetCapabilityCallback callback) override;
  void StartPrint(const std::u16string& job_title,
                  base::Value::Dict settings,
                  scoped_refptr<base::RefCountedMemory> print_data,
                  PrintCallback callback) override;
  void StartGetEulaUrl(const std::string& destination_id,
                       GetEulaUrlCallback callback) override;
  void StartPrinterStatusRequest(
      const std::string& printer_id,
      PrinterStatusRequestCallback callback) override;

  // Public wrapper for `GetAshJobSettings` to use in tests.
  void GetAshJobSettingsForTesting(std::string printer_id,
                                   AshJobSettingsCallback callback,
                                   base::Value::Dict settings);

 private:
  // Get ash-specific job settings for the specified printer, merge them with
  // `settings`, and run `callback` with the result.
  void GetAshJobSettings(std::string printer_id,
                         AshJobSettingsCallback callback,
                         base::Value::Dict settings);

  // These functions call the corresponding `LocalPrinter` function, convert the
  // result to a job setting, add it to `settings`, and call `callback` with the
  // result.
  void GetUsernamePerPolicy(AshJobSettingsCallback callback,
                            base::Value::Dict settings) const;
  void GetOAuthToken(const std::string& printer_id,
                     AshJobSettingsCallback callback,
                     base::Value::Dict settings) const;
  void GetIppClientInfo(const std::string& printer_id,
                        AshJobSettingsCallback callback,
                        base::Value::Dict settings) const;

  // Wrapper for `printing::StartLocalPrint()` to use as a callback bound to the
  // lifetime of `this`.
  void CallStartLocalPrint(scoped_refptr<base::RefCountedMemory> print_data,
                           PrinterHandler::PrintCallback callback,
                           base::Value::Dict settings);

  const raw_ptr<content::WebContents> preview_web_contents_;
  raw_ptr<crosapi::mojom::LocalPrinter> local_printer_ = nullptr;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  int local_printer_version_ = 0;
#endif
  base::WeakPtrFactory<LocalPrinterHandlerChromeos> weak_ptr_factory_{this};
};

}  // namespace printing

#endif  // CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_LOCAL_PRINTER_HANDLER_CHROMEOS_H_
