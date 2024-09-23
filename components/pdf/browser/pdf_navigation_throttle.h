// Copyright 2021 The Chromium Authors
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

class PdfStreamDelegate;

class PdfNavigationThrottle final : public content::NavigationThrottle {
 public:
  PdfNavigationThrottle(content::NavigationHandle* navigation_handle,
                        std::unique_ptr<PdfStreamDelegate> stream_delegate);
  PdfNavigationThrottle(const PdfNavigationThrottle&) = delete;
  PdfNavigationThrottle& operator=(const PdfNavigationThrottle&) = delete;
  ~PdfNavigationThrottle() override;

  // `content::NavigationThrottle`:
  const char* GetNameForLogging() override;
  ThrottleCheckResult WillProcessResponse() override;
  ThrottleCheckResult WillStartRequest() override;

 private:
  std::unique_ptr<PdfStreamDelegate> stream_delegate_;
};

}  // namespace pdf

#endif  // COMPONENTS_PDF_BROWSER_PDF_NAVIGATION_THROTTLE_H_
