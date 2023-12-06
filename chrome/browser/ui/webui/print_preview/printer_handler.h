// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINTER_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINTER_HANDLER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/buildflags.h"

namespace content {
class WebContents;
}

class Profile;

namespace printing {

class PrintPreviewStickySettings;

// Wrapper around PrinterProviderAPI to be used by print preview.
// It makes request lifetime management easier, and hides details of more
// complex operations like printing from the print preview handler. This class
// expects to be created and all functions called on the UI thread.
class PrinterHandler {
 public:
  using DefaultPrinterCallback =
      base::OnceCallback<void(const std::string& printer_name)>;
  using AddedPrintersCallback =
      base::RepeatingCallback<void(base::Value::List printers)>;
  using GetPrintersDoneCallback = base::OnceClosure;
  // `capability` should contain a CDD with key `kSettingCapabilities`.
  // It may also contain other information about the printer in a dictionary
  // with key `kPrinter`.
  // If `capability` does not contain a `kSettingCapabilities` key, this
  // indicates a failure to retrieve capabilities. If the dictionary with key
  // `kSettingCapabilities` is empty, this indicates capabilities were retrieved
  // but the printer does not support any of the capability fields in a CDD.
  using GetCapabilityCallback =
      base::OnceCallback<void(base::Value::Dict capability)>;
  using PrintCallback = base::OnceCallback<void(const base::Value& error)>;
  using GetPrinterInfoCallback =
      base::OnceCallback<void(const base::Value::Dict& printer_info)>;
#if BUILDFLAG(IS_CHROMEOS)
  using GetEulaUrlCallback =
      base::OnceCallback<void(const std::string& license)>;
  using PrinterStatusRequestCallback = base::OnceCallback<void(
      std::optional<base::Value::Dict> cups_printer_status)>;
#endif

  // Creates an instance of a PrinterHandler for extension printers.
  static std::unique_ptr<PrinterHandler> CreateForExtensionPrinters(
      Profile* profile);

  // Creates an instance of a PrinterHandler for PDF printer.
  static std::unique_ptr<PrinterHandler> CreateForPdfPrinter(
      Profile* profile,
      content::WebContents* preview_web_contents,
      PrintPreviewStickySettings* sticky_settings);

  static std::unique_ptr<PrinterHandler> CreateForLocalPrinters(
      content::WebContents* preview_web_contents,
      Profile* profile);

  virtual ~PrinterHandler() = default;

  // Cancels all pending requests.
  virtual void Reset() = 0;

  // Returns the name of the default printer through |cb|. Must be overridden
  // by implementations that expect this method to be called.
  virtual void GetDefaultPrinter(DefaultPrinterCallback cb);

  // Starts getting available printers.
  // |added_printers_callback| should be called in the response when printers
  // are found. May be called multiple times, or never if there are no printers
  // to add.
  // |done_callback| must be called exactly once when the search is complete.
  virtual void StartGetPrinters(AddedPrintersCallback added_printers_callback,
                                GetPrintersDoneCallback done_callback) = 0;

  // Starts getting printing capability of the printer with the provided
  // destination ID.
  // |callback| should be called in response to the request.
  virtual void StartGetCapability(const std::string& destination_id,
                                  GetCapabilityCallback callback) = 0;

  // Starts granting access to the given provisional printer. The print handler
  // will respond with more information about the printer including its non-
  // provisional printer id. Must be overridden by implementations that expect
  // this method to be called.
  // |callback| should be called in response to the request.
  virtual void StartGrantPrinterAccess(const std::string& printer_id,
                                       GetPrinterInfoCallback callback);

  // Starts a print request.
  // |job_title|: The title used for print job.
  // |settings|: The print job settings.
  // |print_data|: The document bytes to print.
  // |callback| should be called in the response to the request.
  virtual void StartPrint(const std::u16string& job_title,
                          base::Value::Dict settings,
                          scoped_refptr<base::RefCountedMemory> print_data,
                          PrintCallback callback) = 0;

#if BUILDFLAG(IS_CHROMEOS)
  // Starts getting the printer's PPD EULA URL with the provided destination ID.
  // |destination_id|: The ID of the printer.
  // |callback| should be called in response to the request.
  virtual void StartGetEulaUrl(const std::string& destination_id,
                               GetEulaUrlCallback callback);

  // Initiates a status request for specified printer.
  // |printer_id|: Printer id.
  // |callback|: should be called in response to the request.
  virtual void StartPrinterStatusRequest(const std::string& printer_id,
                                         PrinterStatusRequestCallback callback);
#endif
};

}  // namespace printing

#endif  // CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINTER_HANDLER_H_
