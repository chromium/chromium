// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRINTING_BROWSER_PRINT_MANAGER_UTILS_H_
#define COMPONENTS_PRINTING_BROWSER_PRINT_MANAGER_UTILS_H_

#include <string>

#include "components/printing/common/print.mojom-forward.h"

namespace content {
class WebContents;
}

namespace printing {

class PrintSettings;

bool IsOopifEnabled();

// Check on the current feature settings to decide whether we need to
// create a PDF compositor client for this |web_contents|.
void CreateCompositeClientIfNeeded(content::WebContents* web_contents,
                                   const std::string& user_agent);

// Converts given settings to PrintParams and stores them in the output
// parameter |params|.
void RenderParamsFromPrintSettings(const PrintSettings& settings,
                                   mojom::PrintParams* params);

}  // namespace printing

#endif  // COMPONENTS_PRINTING_BROWSER_PRINT_MANAGER_UTILS_H_
