// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/browser/pdf_navigation_throttle.h"

#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"

namespace pdf {

PdfNavigationThrottle::PdfNavigationThrottle(
    content::NavigationHandle* navigation_handle)
    : content::NavigationThrottle(navigation_handle) {}

PdfNavigationThrottle::~PdfNavigationThrottle() = default;

const char* PdfNavigationThrottle::GetNameForLogging() {
  return "PdfNavigationThrottle";
}

}  // namespace pdf
