// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/common/pdf_util.h"

#include "base/metrics/histogram_macros.h"
#include "content/public/common/url_utils.h"
#include "extensions/buildflags/buildflags.h"
#include "pdf/buildflags.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_PDF)
#include "base/feature_list.h"
#include "pdf/pdf_features.h"
#endif  // BUILDFLAG(ENABLE_PDF)

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/constants.h"  // nogncheck
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

namespace {

// LINT.IfChange(PdfBackgroundColor)
constexpr SkColor kPdfExtensionBackgroundColor = SkColorSetRGB(82, 86, 89);
#if BUILDFLAG(ENABLE_PDF)
constexpr SkColor kPdfExtensionBackgroundColorCr23 = SkColorSetRGB(40, 40, 40);
#endif  // BUILDFLAG(ENABLE_PDF)
// LINT.ThenChange(//chrome/browser/resources/pdf/pdf_viewer.ts:PdfBackgroundColor)

}  // namespace

void ReportPDFLoadStatus(PDFLoadStatus status) {
  UMA_HISTOGRAM_ENUMERATION("PDF.LoadStatus2", status,
                            PDFLoadStatus::kPdfLoadStatusCount);
}

bool IsPdfExtensionOrigin(const url::Origin& origin) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return origin.scheme() == extensions::kExtensionScheme &&
         origin.host() == extension_misc::kPdfExtensionId;
#else
  return false;
#endif
}

bool IsPdfInternalPluginAllowedOrigin(const url::Origin& origin) {
  // Only allow the PDF plugin in the known, trustworthy origins that are
  // allowlisted. See also https://crbug.com/520422 and
  // https://crbug.com/1027173.
  return IsPdfExtensionOrigin(origin) ||
         content::IsPdfInternalPluginAllowedOrigin(origin);
}

SkColor GetPdfBackgroundColor() {
#if BUILDFLAG(ENABLE_PDF)
  if (base::FeatureList::IsEnabled(chrome_pdf::features::kPdfCr23)) {
    return kPdfExtensionBackgroundColorCr23;
  }
#endif  // BUILDFLAG(ENABLE_PDF)
  return kPdfExtensionBackgroundColor;
}
