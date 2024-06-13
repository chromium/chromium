// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBID_DIGITAL_IDENTITY_SAFETY_INTERSTITIAL_CONTROLLER_H_
#define CHROME_BROWSER_UI_WEBID_DIGITAL_IDENTITY_SAFETY_INTERSTITIAL_CONTROLLER_H_

#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/digital_identity_interstitial_type.h"

namespace content {
class WebContents;
enum class DigitalIdentityInterstitialType;
}  // namespace content

namespace url {
class Origin;
}

// Pure virtual class for showing modal dialog asking user whether they want to
// share their identity with website.
class DigitalIdentitySafetyInterstitialController {
 public:
  virtual content::ContentBrowserClient::
      DigitalIdentityInterstitialAbortCallback
      ShowInterstitial(
          content::WebContents& web_contents,
          const url::Origin& rp_origin,
          content::DigitalIdentityInterstitialType,
          content::ContentBrowserClient::DigitalIdentityInterstitialCallback
              callback) = 0;

 protected:
  virtual ~DigitalIdentitySafetyInterstitialController() = default;
};

#endif  // CHROME_BROWSER_UI_WEBID_DIGITAL_IDENTITY_SAFETY_INTERSTITIAL_CONTROLLER_H_
