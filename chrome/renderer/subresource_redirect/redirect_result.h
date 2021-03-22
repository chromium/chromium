// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SUBRESOURCE_REDIRECT_REDIRECT_RESULT_H_
#define CHROME_RENDERER_SUBRESOURCE_REDIRECT_REDIRECT_RESULT_H_

namespace subresource_redirect {

// Enumerates the different results possible for subresource redirection, such
// as redirectable or different reasons of ineligibility. This enum should be in
// sync with SubresourceRedirectRedirectResult in enums.xml
enum class RedirectResult {
  kUnknown = 0,

  // The image was determined as public and is eligible to be redirected
  // to a compressed version.
  kRedirectable,

  // Possible reasons for ineligibility:

  // Because of reasons Blink could disallow compression such as non <img>
  // element, CSP/CORS security restrictions, javascript initiated image, etc.
  kIneligibleBlinkDisallowed,

  // Because the resource is fetched for a subframe.
  kIneligibleSubframeResource,

  // Because the compressed subresource fetch failed, and then the original
  // subresource was loaded.
  kIneligibleRedirectFailed,

  // Possible reasons for ineligibility due to public image hints approach:

  // Because the image hint list was not retrieved within certain time limit
  // of navigation start,
  kIneligibleImageHintsUnavailable,

  // Because the image hint list was not retrieved at the time of image fetch,
  // but the image URL was found in the hint list, which finished fetching
  // later.
  kIneligibleImageHintsUnavailableButRedirectable,

  // Because the image hint list was not retrieved at the time of image fetch,
  // and the image URL was not in the hint list as well, which finished
  // fetching later.
  kIneligibleImageHintsUnavailableAndMissingInHints,

  // Because the image URL was not found in the image hints.
  kIneligibleMissingInImageHints,

  // Possible reasons for ineligibility due to login and robots rules
  // based approach:

  // Because the image was disallowed by robots rules of the image origin.
  kIneligibleRobotsDisallowed,

  // Because the robots rules fetch timedout.
  kIneligibleRobotsTimeout,

  // Because the page was detected to be logged-in.
  kIneligibleLoginDetected,

  kMaxValue = RedirectResult::kIneligibleLoginDetected
};

}  // namespace subresource_redirect

#endif  // CHROME_RENDERER_SUBRESOURCE_REDIRECT_REDIRECT_RESULT_H_
