// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PRINTING_PRINTER_CAPABILITIES_H_
#define CHROME_COMMON_PRINTING_PRINTER_CAPABILITIES_H_

#include <string>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "build/build_config.h"
#include "printing/backend/print_backend.h"

namespace printing {

struct PrinterBasicInfo;

extern const char kPrinter[];

#if defined(OS_WIN)
std::string GetUserFriendlyName(const std::string& printer_name);
#endif

// Extracts the printer display name and description from the
// appropriate fields in `printer` for the platform.
std::pair<std::string, std::string> GetPrinterNameAndDescription(
    const PrinterBasicInfo& printer);

// Returns a value containing printer capabilities and settings for the device
// registered as `device_name` in the `PrintBackend`.  The returned value is
// suitable for passage to the WebUI in JSON.
// Data from `basic_info`, `user_defined_papers`, `has_secure_protocol`,
// and `caps` are all incorporated into the returned value.
base::Value AssemblePrinterSettings(
    const std::string& device_name,
    const PrinterBasicInfo& basic_info,
    const PrinterSemanticCapsAndDefaults::Papers& user_defined_papers,
    bool has_secure_protocol,
    PrinterSemanticCapsAndDefaults* caps);

// Returns the value from `AssemblePrinterSettings()` using the required
// `print_backend` to obtain settings as necessary.  The returned value is
// suitable for passage to the WebUI in JSON.
base::Value GetSettingsOnBlockingTaskRunner(
    const std::string& device_name,
    const PrinterBasicInfo& basic_info,
    PrinterSemanticCapsAndDefaults::Papers user_defined_papers,
    bool has_secure_protocol,
    scoped_refptr<PrintBackend> print_backend);

}  // namespace printing

#endif  // CHROME_COMMON_PRINTING_PRINTER_CAPABILITIES_H_
