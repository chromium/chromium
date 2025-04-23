// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/page_content_type_conversions.h"

namespace lens {

lens::MimeType StringMimeTypeToDocumentType(const std::string& mime_type) {
  if (mime_type == "application/pdf") {
    return lens::MimeType::kPdf;
  } else if (mime_type == "text/html") {
    return lens::MimeType::kHtml;
  } else if (mime_type == "text/plain") {
    return lens::MimeType::kPlainText;
  } else if (mime_type.starts_with("image/")) {
    return lens::MimeType::kImage;
  } else if (mime_type.starts_with("video/")) {
    return lens::MimeType::kVideo;
  } else if (mime_type.starts_with("audio/")) {
    return lens::MimeType::kAudio;
  } else if (mime_type == "application/json") {
    return lens::MimeType::kJson;
  }
  return lens::MimeType::kUnknown;
}

lens::mojom::PageContentType StringMimeTypeToMojoPageContentType(
    const std::string& mime_type) {
  if (mime_type == "application/pdf") {
    return lens::mojom::PageContentType::kPdf;
  } else if (mime_type == "text/html") {
    return lens::mojom::PageContentType::kHtml;
  }
  return lens::mojom::PageContentType::kUnknown;
}
}  // namespace lens
