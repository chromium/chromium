// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PDF_UTIL_H_
#define CHROME_COMMON_PDF_UTIL_H_

#include <string>

class GURL;

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

// Returns the HTML contents of the placeholder.
std::string GetPDFPlaceholderHTML(const GURL& pdf_url);

constexpr char kPDFMimeType[] = "application/pdf";

#endif  // CHROME_COMMON_PDF_UTIL_H_
