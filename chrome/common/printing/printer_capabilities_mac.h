// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PRINTING_PRINTER_CAPABILITIES_MAC_H_
#define CHROME_COMMON_PRINTING_PRINTER_CAPABILITIES_MAC_H_

#include "printing/backend/print_backend.h"
#include "printing/buildflags/buildflags.h"

#if !BUILDFLAG(ENABLE_PRINT_PREVIEW)
#error "Only used by Print Preview"
#endif

namespace base {
class FilePath;
}

namespace printing {

// Must be called on a task runner that allows blocking.
PrinterSemanticCapsAndDefaults::Papers GetMacCustomPaperSizes();

// Override the values returned by GetMacCustomPaperSizes() for unit tests. To
// accurately emulate custom paper sizes from a macOS device, the papers should
// have an empty |vendor_id| field.
void SetMacCustomPaperSizesForTesting(
    const PrinterSemanticCapsAndDefaults::Papers& papers);

namespace internal {

PrinterSemanticCapsAndDefaults::Papers GetMacCustomPaperSizesFromFile(
    const base::FilePath& path);

}  // namespace internal

}  // namespace printing

#endif  // CHROME_COMMON_PRINTING_PRINTER_CAPABILITIES_MAC_H_
