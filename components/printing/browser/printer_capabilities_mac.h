// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRINTING_BROWSER_PRINTER_CAPABILITIES_MAC_H_
#define COMPONENTS_PRINTING_BROWSER_PRINTER_CAPABILITIES_MAC_H_

#include "printing/backend/print_backend.h"

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

#endif  // COMPONENTS_PRINTING_BROWSER_PRINTER_CAPABILITIES_MAC_H_
