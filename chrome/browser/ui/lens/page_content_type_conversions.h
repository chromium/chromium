// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_PAGE_CONTENT_TYPE_CONVERSIONS_H_
#define CHROME_BROWSER_UI_LENS_PAGE_CONTENT_TYPE_CONVERSIONS_H_

#include <string>

#include "chrome/browser/lens/core/mojom/page_content_type.mojom.h"
#include "components/lens/lens_overlay_mime_type.h"

namespace lens {

// Converts a string mime type to a lens::MimeType.
lens::MimeType StringMimeTypeToDocumentType(const std::string& mime_type);

// Converts a string mime type to a lens::mojom::PageContentType.
lens::mojom::PageContentType StringMimeTypeToMojoPageContentType(
    const std::string& mime_type);

}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_PAGE_CONTENT_TYPE_CONVERSIONS_H_
