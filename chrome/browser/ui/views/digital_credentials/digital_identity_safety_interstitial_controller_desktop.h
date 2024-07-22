// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DIGITAL_CREDENTIALS_DIGITAL_IDENTITY_SAFETY_INTERSTITIAL_CONTROLLER_DESKTOP_H_
#define CHROME_BROWSER_UI_VIEWS_DIGITAL_CREDENTIALS_DIGITAL_IDENTITY_SAFETY_INTERSTITIAL_CONTROLLER_DESKTOP_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/digital_credentials/digital_identity_safety_interstitial_controller.h"
#include "content/public/browser/digital_identity_interstitial_type.h"
#include "content/public/browser/digital_identity_provider.h"
#include "content/public/browser/web_contents.h"
#include "url/origin.h"

namespace views {
class Widget;
}  // namespace views

class DigitalIdentitySafetyInterstitialControllerDesktop
    : public DigitalIdentitySafetyInterstitialController {
 public:
  DigitalIdentitySafetyInterstitialControllerDesktop();
  ~DigitalIdentitySafetyInterstitialControllerDesktop() override;

  content::DigitalIdentityProvider::DigitalIdentityInterstitialAbortCallback
  ShowInterstitial(
      content::WebContents& web_contents,
      const url::Origin& rp_origin,
      content::DigitalIdentityInterstitialType,
      content::DigitalIdentityProvider::DigitalIdentityInterstitialCallback
          callback) override;

 private:
  void Abort();

  void ShowInterstitialImpl(content::WebContents& web_contents,
                            bool was_request_aborted);

  // Called when the user denies permission.
  void OnUserDeniedPermission();

  // Called when the user grants permission.
  void OnUserGrantedPermission();

  url::Origin rp_origin_;
  content::DigitalIdentityInterstitialType interstitial_type_;
  content::DigitalIdentityProvider::DigitalIdentityInterstitialCallback
      callback_;

  base::WeakPtr<content::WebContents> web_contents_;
  raw_ptr<views::Widget> dialog_widget_;

  base::WeakPtrFactory<DigitalIdentitySafetyInterstitialControllerDesktop>
      weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_DIGITAL_CREDENTIALS_DIGITAL_IDENTITY_SAFETY_INTERSTITIAL_CONTROLLER_DESKTOP_H_
