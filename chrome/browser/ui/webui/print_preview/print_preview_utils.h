// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINT_PREVIEW_UTILS_H_
#define CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINT_PREVIEW_UTILS_H_

#include "base/values.h"
#include "chrome/browser/ui/webui/print_preview/printer_handler.h"
#include "printing/backend/print_backend.h"

namespace content {
class WebContents;
}

namespace printing {

// Printer capability setting keys.
extern const char kOptionKey[];
extern const char kResetToDefaultKey[];
extern const char kTypeKey[];
extern const char kSelectCapKey[];
extern const char kSelectString[];
extern const char kTypeKey[];
extern const char kVendorCapabilityKey[];

// Converts `printer_list` to a base::Value::List form, runs `callback` with the
// converted list as the argument if it is not empty, and runs `done_callback`.
void ConvertPrinterListForCallback(
    PrinterHandler::AddedPrintersCallback callback,
    PrinterHandler::GetPrintersDoneCallback done_callback,
    const PrinterList& printer_list);

// Returns a sanitized version of `cdd` to prevent possible JS
// errors in Print Preview. Will remove null items from lists or options lists
// and remove any lists/options that are empty or only contain null values.
// Will also check some CDD entries to make sure the input conforms to the
// requirements for those entries, although not comprehensively.
// On failure, returns an empty dict.
base::Value::Dict ValidateCddForPrintPreview(base::Value::Dict cdd);

// Returns an updated version of `cdd` and ensures it has a valid value for the
// DPI capability. Uses the existing validated value if it exists, or fills in a
// reasonable default if the capability is missing. Having a DPI is not required
// in the CDD, but it is crucial for performing page setup.
//
// Assumes `cdd` is the output from ValidateCddForPrintPreview().
base::Value::Dict UpdateCddWithDpiIfMissing(base::Value::Dict cdd);

// Returns the list of media size options from the `cdd`, or nullptr if it does
// not exist.  Returns a pointer into `cdd`.
const base::Value::List* GetMediaSizeOptionsFromCdd(
    const base::Value::Dict& cdd);

// Updates `cdd` by removing all continuous feed media size options.
void FilterContinuousFeedMediaSizes(base::Value::Dict& cdd);

// Starts a local print of `print_data` with print settings dictionary
// `job_settings`. Runs `callback` on failure or success.
void StartLocalPrint(base::Value::Dict job_settings,
                     scoped_refptr<base::RefCountedMemory> print_data,
                     content::WebContents* preview_web_contents,
                     PrinterHandler::PrintCallback callback);

}  // namespace printing

#endif  // CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINT_PREVIEW_UTILS_H_
