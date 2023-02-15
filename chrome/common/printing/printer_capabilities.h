// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PRINTING_PRINTER_CAPABILITIES_H_
#define CHROME_COMMON_PRINTING_PRINTER_CAPABILITIES_H_

#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "build/build_config.h"
#include "printing/backend/print_backend.h"
#include "printing/buildflags/buildflags.h"

#if !BUILDFLAG(ENABLE_PRINT_PREVIEW)
#error "Only used by Print Preview"
#endif

namespace printing {

struct PrinterBasicInfo;

extern const char kPrinter[];

#if BUILDFLAG(IS_WIN)
std::string GetUserFriendlyName(const std::string& printer_name);
#endif

// Returns a value containing printer capabilities and settings for the device
// registered as `device_name` in the `PrintBackend`.  The returned value is
// suitable for passage to the WebUI in JSON.
// Data from `basic_info`, `has_secure_protocol`, and `caps` are all
// incorporated into the returned value.
base::Value::Dict AssemblePrinterSettings(const std::string& device_name,
                                          const PrinterBasicInfo& basic_info,
                                          bool has_secure_protocol,
                                          PrinterSemanticCapsAndDefaults* caps);

#if !BUILDFLAG(IS_CHROMEOS) || defined(UNIT_TEST)
// Returns the value from `AssemblePrinterSettings()` using the required
// `print_backend` to obtain settings as necessary.  The returned value is
// suitable for passage to the WebUI in JSON.
base::Value::Dict GetSettingsOnBlockingTaskRunner(
    const std::string& device_name,
    const PrinterBasicInfo& basic_info,
    PrinterSemanticCapsAndDefaults::Papers user_defined_papers,
    scoped_refptr<PrintBackend> print_backend);
#endif  // !BUILDFLAG(IS_CHROMEOS) || defined(UNIT_TEST)

}  // namespace printing

#endif  // CHROME_COMMON_PRINTING_PRINTER_CAPABILITIES_H_
