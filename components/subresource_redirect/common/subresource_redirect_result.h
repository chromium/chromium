// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_REDIRECT_COMMON_SUBRESOURCE_REDIRECT_RESULT_H_
#define COMPONENTS_SUBRESOURCE_REDIRECT_COMMON_SUBRESOURCE_REDIRECT_RESULT_H_

namespace subresource_redirect {

// Enumerates the different results possible for subresource redirection, such
// as redirectable or different reasons of ineligibility. This enum should be in
// sync with SubresourceRedirectRedirectResult in enums.xml
enum class SubresourceRedirectResult {
  kUnknown = 0,

  // The image was determined as public and is eligible to be redirected
  // to a compressed version.
  kRedirectable = 1,

  // Possible reasons for ineligibility:

  // Because of reasons Blink could disallow compression such as non <img>
  // element, CSP/CORS security restrictions, javascript initiated image, etc.
  kIneligibleBlinkDisallowed = 2,

  // Because the resource is fetched for a subframe.
  kIneligibleSubframeResource = 3,

  // Because the compressed subresource fetch failed, and then the original
  // subresource was loaded.
  kIneligibleRedirectFailed = 4,

  // Possible reasons for ineligibility due to public image hints approach:

  // Because the image hint list was not retrieved within certain time limit
  // of navigation start,
  kIneligibleImageHintsUnavailable = 5,

  // Because the image hint list was not retrieved at the time of image fetch,
  // but the image URL was found in the hint list, which finished fetching
  // later.
  kIneligibleImageHintsUnavailableButRedirectable = 6,

  // Because the image hint list was not retrieved at the time of image fetch,
  // and the image URL was not in the hint list as well, which finished
  // fetching later.
  kIneligibleImageHintsUnavailableAndMissingInHints = 7,

  // Because the image URL was not found in the image hints.
  kIneligibleMissingInImageHints = 8,

  // Possible reasons for ineligibility due to login and robots rules
  // based approach:

  // Because the image was disallowed by robots rules of the image origin.
  kIneligibleRobotsDisallowed = 9,

  // Because the robots rules fetch timedout.
  kIneligibleRobotsTimeout = 10,

  // Because the page was detected to be logged-in.
  kIneligibleLoginDetected = 11,

  // Because the subresource was within the first k subresources on the page and
  // got disabled.
  kIneligibleFirstKDisableSubresourceRedirect = 12,

  // Because the subresource redirection was disabled, where only metrics are
  // recorded and the actual subresource redirection does not happen.
  kIneligibleCompressionDisabled = 13,

  kMaxValue = SubresourceRedirectResult::kIneligibleCompressionDisabled
};

}  // namespace subresource_redirect

#endif  // COMPONENTS_SUBRESOURCE_REDIRECT_COMMON_SUBRESOURCE_REDIRECT_RESULT_H_
