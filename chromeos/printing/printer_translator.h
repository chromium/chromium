// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PRINTING_PRINTER_TRANSLATOR_H_
#define CHROMEOS_PRINTING_PRINTER_TRANSLATOR_H_

#include <memory>

#include "base/component_export.h"
#include "base/values.h"
#include "chromeos/printing/printer_configuration.h"

namespace chromeos {

class CupsPrinterStatus;

COMPONENT_EXPORT(CHROMEOS_PRINTING) extern const char kPrinterId[];

// Returns a new printer populated with the fields from |pref|.  Processes
// dictionaries from policy.
COMPONENT_EXPORT(CHROMEOS_PRINTING)
std::unique_ptr<Printer> RecommendedPrinterToPrinter(
    const base::Value::Dict& pref);

// Returns a JSON representation of |printer| as a CupsPrinterInfo. If the
// printer uri cannot be parsed, the relevant fields are populated with default
// values. CupsPrinterInfo is defined in cups_printers_browser_proxy.js.
COMPONENT_EXPORT(CHROMEOS_PRINTING)
base::Value::Dict GetCupsPrinterInfo(const Printer& printer);

// Returns a JSON representation of a CupsPrinterStatus
COMPONENT_EXPORT(CHROMEOS_PRINTING)
base::Value::Dict CreateCupsPrinterStatusDictionary(
    const CupsPrinterStatus& cups_printer_status);

}  // namespace chromeos

#endif  // CHROMEOS_PRINTING_PRINTER_TRANSLATOR_H_
