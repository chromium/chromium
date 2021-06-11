// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PDF_BROWSER_PDF_NAVIGATION_THROTTLE_H_
#define COMPONENTS_PDF_BROWSER_PDF_NAVIGATION_THROTTLE_H_

#include <memory>

#include "content/public/browser/navigation_throttle.h"

namespace content {
class NavigationHandle;
}  // namespace content

namespace pdf {

class PdfNavigationThrottle final : public content::NavigationThrottle {
 public:
  static std::unique_ptr<content::NavigationThrottle> MaybeCreateThrottleFor(
      content::NavigationHandle* navigation_handle);

  explicit PdfNavigationThrottle(content::NavigationHandle* navigation_handle);
  PdfNavigationThrottle(const PdfNavigationThrottle&) = delete;
  PdfNavigationThrottle& operator=(const PdfNavigationThrottle&) = delete;
  ~PdfNavigationThrottle() override;

  // `content::NavigationThrottle`:
  const char* GetNameForLogging() override;
};

}  // namespace pdf

#endif  // COMPONENTS_PDF_BROWSER_PDF_NAVIGATION_THROTTLE_H_
