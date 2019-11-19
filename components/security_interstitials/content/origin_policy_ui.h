// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_ORIGIN_POLICY_UI_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_ORIGIN_POLICY_UI_H_

#include <memory>

#include <string>
#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/string16.h"

class GURL;

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace network {
enum class OriginPolicyState;
}

namespace security_interstitials {
class SecurityInterstitialPage;

// A helper class to build the error page for Origin Policy errors.
class OriginPolicyUI {
 public:
  // Create the error page for the given NavigationHandle.
  // This is intended to implement the ContentBrowserClient interface.
  static base::Optional<std::string> GetErrorPageAsHTML(
      network::OriginPolicyState error_reason,
      content::NavigationHandle* handle);

  // Create the error page instance for the given WebContents + URL.
  // This is intended for use by debug functions (like chrome:://interstitials).
  static SecurityInterstitialPage* GetBlockingPage(
      network::OriginPolicyState error_reason,
      content::WebContents* web_contents,
      const GURL& url);
};

}  // namespace security_interstitials

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_ORIGIN_POLICY_UI_H_
