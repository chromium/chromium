// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRINTING_COMMON_CLOUD_PRINT_CDD_CONVERSION_H_
#define COMPONENTS_PRINTING_COMMON_CLOUD_PRINT_CDD_CONVERSION_H_

#include "base/values.h"

namespace printing {
struct PrinterSemanticCapsAndDefaults;
}

namespace cloud_print {

base::Value PrinterSemanticCapsAndDefaultsToCdd(
    const printing::PrinterSemanticCapsAndDefaults& semantic_info);

}  // namespace cloud_print

#endif  // COMPONENTS_PRINTING_COMMON_CLOUD_PRINT_CDD_CONVERSION_H_
