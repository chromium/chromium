// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PDF_COMMON_PDF_UTIL_H_
#define COMPONENTS_PDF_COMMON_PDF_UTIL_H_

#include "third_party/skia/include/core/SkColor.h"

namespace url {
class Origin;
}  // namespace url

// Must be kept in sync with PDFLoadStatus enum in histograms.xml.
// This enum should be treated as append-only.
enum class PDFLoadStatus {
  kLoadedFullPagePdfWithPdfium = 0,
  kLoadedEmbeddedPdfWithPdfium = 1,
  kShowedDisabledPluginPlaceholderForEmbeddedPdf = 2,
  kTriggeredNoGestureDriveByDownload = 3,
  kLoadedIframePdfWithNoPdfViewer = 4,
  kViewPdfClickedInPdfPluginPlaceholder = 5,
  kPdfLoadStatusCount
};

void ReportPDFLoadStatus(PDFLoadStatus status);

// Returns whether `origin` is for the built-in PDF extension.
bool IsPdfExtensionOrigin(const url::Origin& origin);

// Returns `true` if the origin is allowed to create the internal PDF plugin.
// Note that for the Pepper-free plugin, this applies to the origin of the
// parent of the frame that contains the in-process plugin.
bool IsPdfInternalPluginAllowedOrigin(const url::Origin& origin);

// Returns the background color of the PDF extension.
SkColor GetPdfBackgroundColor();

#endif  // COMPONENTS_PDF_COMMON_PDF_UTIL_H_
