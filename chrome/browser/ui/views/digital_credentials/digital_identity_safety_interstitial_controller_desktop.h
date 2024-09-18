// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DIGITAL_CREDENTIALS_DIGITAL_IDENTITY_SAFETY_INTERSTITIAL_CONTROLLER_DESKTOP_H_
#define CHROME_BROWSER_UI_VIEWS_DIGITAL_CREDENTIALS_DIGITAL_IDENTITY_SAFETY_INTERSTITIAL_CONTROLLER_DESKTOP_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/digital_credentials/digital_identity_interstitial_closed_reason.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/digital_identity_interstitial_type.h"
#include "content/public/browser/digital_identity_provider.h"
#include "content/public/browser/web_contents.h"
#include "url/origin.h"

namespace views {
class Widget;
}  // namespace views

class DigitalIdentitySafetyInterstitialControllerDesktop {
 public:
  DigitalIdentitySafetyInterstitialControllerDesktop();
  ~DigitalIdentitySafetyInterstitialControllerDesktop();

  content::DigitalIdentityProvider::DigitalIdentityInterstitialAbortCallback
  ShowInterstitial(
      content::WebContents& web_contents,
      const url::Origin& rp_origin,
      content::DigitalIdentityInterstitialType,
      content::DigitalIdentityProvider::DigitalIdentityInterstitialCallback
          callback);

 private:
  class CloseOnNavigationObserver
      : public web_modal::WebContentsModalDialogManager::
            CloseOnNavigationObserver {
   public:
    CloseOnNavigationObserver();
    ~CloseOnNavigationObserver() override;

    // Starts observing web-modal dialogs being closed as a result of page
    // navigation.
    void Observe(content::WebContents& web_contents);

    // Returns whether any web-modal dialogs are about-to-close or have closed
    // as a result of a page navigation.
    bool WillCloseOnNavigation() const { return will_close_due_to_navigation_; }

    // CloseOnNavigationObserver:
    void OnWillClose() override;

   private:
    bool will_close_due_to_navigation_ = false;

    base::WeakPtr<content::WebContents> web_contents_;
  };

  void Abort();

  void ShowInterstitialImpl(content::WebContents& web_contents,
                            bool was_request_aborted);

  void OnDialogClosed(DigitalIdentityInterstitialClosedReason reason);

  url::Origin rp_origin_;
  content::DigitalIdentityInterstitialType interstitial_type_;
  content::DigitalIdentityProvider::DigitalIdentityInterstitialCallback
      callback_;

  base::WeakPtr<content::WebContents> web_contents_;
  raw_ptr<views::Widget> dialog_widget_;

  std::unique_ptr<CloseOnNavigationObserver> close_on_navigation_observer_;

  base::WeakPtrFactory<DigitalIdentitySafetyInterstitialControllerDesktop>
      weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_DIGITAL_CREDENTIALS_DIGITAL_IDENTITY_SAFETY_INTERSTITIAL_CONTROLLER_DESKTOP_H_
