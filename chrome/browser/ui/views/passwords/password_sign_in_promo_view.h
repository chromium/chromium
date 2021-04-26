// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_SIGN_IN_PROMO_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_SIGN_IN_PROMO_VIEW_H_

#include "chrome/browser/ui/passwords/bubble_controllers/sign_in_promo_bubble_controller.h"
#include "chrome/browser/ui/sync/bubble_sync_promo_delegate.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace content {
class WebContents;
}
class SignInPromoBubbleController;

// A view that can show up after saving a password without being signed in to
// offer signing users in so they can access their credentials across devices.
class PasswordSignInPromoView : public views::View {
 public:
  METADATA_HEADER(PasswordSignInPromoView);
  explicit PasswordSignInPromoView(content::WebContents* web_contents);
  PasswordSignInPromoView(const PasswordSignInPromoView&) = delete;
  PasswordSignInPromoView& operator=(const PasswordSignInPromoView&) = delete;
  ~PasswordSignInPromoView() override;

 private:
  // Delegate for the personalized sync promo view used when desktop identity
  // consistency is enabled.
  class DiceSyncPromoDelegate : public BubbleSyncPromoDelegate {
   public:
    explicit DiceSyncPromoDelegate(SignInPromoBubbleController* controller);
    DiceSyncPromoDelegate(const DiceSyncPromoDelegate&) = delete;
    DiceSyncPromoDelegate& operator=(const DiceSyncPromoDelegate&) = delete;
    ~DiceSyncPromoDelegate() override;

    // BubbleSyncPromoDelegate:
    void OnEnableSync(const AccountInfo& account) override;

   private:
    SignInPromoBubbleController* controller_;
  };

  SignInPromoBubbleController controller_;
  std::unique_ptr<DiceSyncPromoDelegate> dice_sync_promo_delegate_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_SIGN_IN_PROMO_VIEW_H_
