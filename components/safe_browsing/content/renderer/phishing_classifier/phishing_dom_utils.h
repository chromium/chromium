// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_PHISHING_CLASSIFIER_PHISHING_DOM_UTILS_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_PHISHING_CLASSIFIER_PHISHING_DOM_UTILS_H_

namespace blink {
class WebLocalFrame;
}

namespace safe_browsing {

enum class PhishingProcessStatus {
  kValid = 0,
  kInvalidUrlFormat = 1,
  kInvalidDomLoader = 2,
  kMaxValue = kInvalidDomLoader,
};

// Checks whether the frame document is suitable for phishing detection.
// Currently, we only classify http/https URLs that are GET requests.
PhishingProcessStatus CanPerformPhishingDetection(blink::WebLocalFrame* frame);

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_PHISHING_CLASSIFIER_PHISHING_DOM_UTILS_H_
