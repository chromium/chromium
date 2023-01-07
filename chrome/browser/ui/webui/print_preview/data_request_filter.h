// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_DATA_REQUEST_FILTER_H_
#define CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_DATA_REQUEST_FILTER_H_

#include <string>

#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
class WebUIDataSource;
}  // namespace content

namespace printing {

// Result of `ParseDataPath()`.
struct PrintPreviewIdAndPageIndex {
  // Print Preview UI ID.
  int ui_id;

  // Zero-based page index, or `COMPLETE_PREVIEW_DOCUMENT_INDEX` for a
  // print-ready PDF.
  int page_index;
};

// Adds a request filter for serving preview PDF data.
void AddDataRequestFilter(content::WebUIDataSource& source);

// Parses a preview PDF data path (i.e., what comes after
// chrome-untrusted://print/).
//
// The format for requesting preview PDF data is as follows:
//   chrome-untrusted://print/<ui_id>/<page_index>/print.pdf
//
// Example:
//   chrome-untrusted://print/123/10/print.pdf
absl::optional<PrintPreviewIdAndPageIndex> ParseDataPath(
    const std::string& path);

}  // namespace printing

#endif  // CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_DATA_REQUEST_FILTER_H_
