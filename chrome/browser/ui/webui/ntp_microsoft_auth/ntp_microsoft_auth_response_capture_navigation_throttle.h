// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NTP_MICROSOFT_AUTH_NTP_MICROSOFT_AUTH_RESPONSE_CAPTURE_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_UI_WEBUI_NTP_MICROSOFT_AUTH_NTP_MICROSOFT_AUTH_RESPONSE_CAPTURE_NAVIGATION_THROTTLE_H_

#include "base/memory/weak_ptr.h"
#include "components/url_matcher/url_matcher.h"
#include "content/public/browser/navigation_throttle.h"

// This throttle looks for redirections from a microsoft popup (for login
// explicitly started by the user) or iframe (for silent reauthentication in
// the background) opened on the NTP to a fake url in order to instead redirect
// to about:blank at the same origin as the opener or parent frame.
class NtpMicrosoftAuthResponseCaptureNavigationThrottle
    : public content::NavigationThrottle {
 public:
  static void MaybeCreateAndAdd(content::NavigationThrottleRegistry& registry);

  explicit NtpMicrosoftAuthResponseCaptureNavigationThrottle(
      content::NavigationThrottleRegistry& registry);

  NtpMicrosoftAuthResponseCaptureNavigationThrottle(
      const NtpMicrosoftAuthResponseCaptureNavigationThrottle&) = delete;
  NtpMicrosoftAuthResponseCaptureNavigationThrottle& operator=(
      const NtpMicrosoftAuthResponseCaptureNavigationThrottle&) = delete;
  ~NtpMicrosoftAuthResponseCaptureNavigationThrottle() override;

  // content::NavigationThrottle:
  ThrottleCheckResult WillRedirectRequest() override;
  ThrottleCheckResult WillProcessResponse() override;

  const char* GetNameForLogging() override;

 private:
  ThrottleCheckResult AttemptToTriggerInterception();
  void RedirectNavigationHandleToAboutBlank();

  base::WeakPtrFactory<NtpMicrosoftAuthResponseCaptureNavigationThrottle>
      weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_NTP_MICROSOFT_AUTH_NTP_MICROSOFT_AUTH_RESPONSE_CAPTURE_NAVIGATION_THROTTLE_H_
