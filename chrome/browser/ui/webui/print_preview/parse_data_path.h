// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PARSE_DATA_PATH_H_
#define CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PARSE_DATA_PATH_H_

#include <optional>
#include <string>

#include "base/unguessable_token.h"

namespace printing {

// Result of `ParseDataPath()`.
struct PrintPreviewIdAndPageIndex {
  // Print Preview UI ID.
  base::UnguessableToken ui_id;

  // Zero-based page index, or `COMPLETE_PREVIEW_DOCUMENT_INDEX` for a
  // print-ready PDF.
  int page_index;
};

// Parses a preview PDF data path (i.e., what comes after
// chrome-untrusted://print/).
//
// The format for requesting preview PDF data is as follows:
//   chrome-untrusted://print/<ui_id>/<page_index>/print.pdf
//
// Example:
//   chrome-untrusted://print/0123456789abcdef0123456789abcdef/10/print.pdf
std::optional<PrintPreviewIdAndPageIndex> ParseDataPath(
    const std::string& path);

}  // namespace printing

#endif  // CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PARSE_DATA_PATH_H_
