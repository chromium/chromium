// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_DATA_REQUEST_FILTER_H_
#define CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_DATA_REQUEST_FILTER_H_

#include <string>

namespace content {
class WebUIDataSource;
}  // namespace content

namespace printing {

// Adds a request filter for serving preview PDF data.
void AddDataRequestFilter(content::WebUIDataSource& source);

// Parses a preview PDF data path (i.e., what comes after chrome://print/ or
// chrome-untrusted://print/), and returns true if the path seems to be valid.
// `ui_id` and `page_index` are set to the parsed values if the provided
// pointers aren't `nullptr`.
//
// The format for requesting preview PDF data is as follows:
//   chrome://print/<PrintPreviewUIID>/<PageIndex>/print.pdf
//   chrome-untrusted://print/<PrintPreviewUIID>/<PageIndex>/print.pdf
//
// Required parameters:
//   <PrintPreviewUIID> = PrintPreview UI ID
//   <PageIndex> = Page index is zero-based or `COMPLETE_PREVIEW_DOCUMENT_INDEX`
//                 to represent a print ready PDF.
//
// Example:
//   chrome://print/123/10/print.pdf
//   chrome-untrusted://print/123/10/print.pdf
bool ParseDataPath(const std::string& path, int* ui_id, int* page_index);

}  // namespace printing

#endif  // CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_DATA_REQUEST_FILTER_H_
