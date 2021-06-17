// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/browser/pdf_navigation_throttle.h"

#include <memory>

#include "base/feature_list.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "extensions/common/constants.h"
#include "pdf/pdf_features.h"
#include "url/gurl.h"

namespace pdf {

// static
std::unique_ptr<content::NavigationThrottle>
PdfNavigationThrottle::MaybeCreateThrottleFor(
    content::NavigationHandle* navigation_handle) {
  if (!base::FeatureList::IsEnabled(chrome_pdf::features::kPdfUnseasoned))
    return nullptr;

  if (navigation_handle->IsInMainFrame())
    return nullptr;

  return std::make_unique<PdfNavigationThrottle>(navigation_handle);
}

PdfNavigationThrottle::PdfNavigationThrottle(
    content::NavigationHandle* navigation_handle)
    : content::NavigationThrottle(navigation_handle) {}

PdfNavigationThrottle::~PdfNavigationThrottle() = default;

const char* PdfNavigationThrottle::GetNameForLogging() {
  return "PdfNavigationThrottle";
}

content::NavigationThrottle::ThrottleCheckResult
PdfNavigationThrottle::WillStartRequest() {
  // Quickly ignore URLs that do not look like stream URLs. The path check does
  // not need to be precise, it just needs to not match any legitimate PDF
  // extension URL. We'll assume such URLs contain multiple path components, or
  // a file extension.
  const GURL& url = navigation_handle()->GetURL();
  if (!url.SchemeIs(extensions::kExtensionScheme) ||
      url.host_piece() != extension_misc::kPdfExtensionId ||
      url.path_piece().find_last_of("/.") != 0) {
    return PROCEED;
  }

  // TODO(crbug.com/1123621): Enqueue navigation to stream's original URL.
  return CANCEL_AND_IGNORE;
}

}  // namespace pdf
