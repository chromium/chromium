// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIGNIN_BUBBLE_SIGNIN_PROMO_DELEGATE_H_
#define CHROME_BROWSER_UI_SIGNIN_BUBBLE_SIGNIN_PROMO_DELEGATE_H_

struct AccountInfo;

// Delegate for the bubble sign in promo view.
class BubbleSignInPromoDelegate {
 public:
  virtual ~BubbleSignInPromoDelegate() {}

  // Informs the delegate to sign in |account| or to present
  // the browser sign-in page if |account| is empty.
  virtual void OnSignIn(const AccountInfo& account) = 0;
};

#endif  // CHROME_BROWSER_UI_SIGNIN_BUBBLE_SIGNIN_PROMO_DELEGATE_H_
