// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_SIGN_IN_PROMO_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_SIGN_IN_PROMO_BUBBLE_CONTROLLER_H_

#include "base/memory/weak_ptr.h"

struct AccountInfo;
class PasswordsModelDelegate;
class Profile;

// This controller provides data and actions for the PasswordSignInPromoView.
class SignInPromoBubbleController {
 public:
  explicit SignInPromoBubbleController(
      base::WeakPtr<PasswordsModelDelegate> delegate);
  ~SignInPromoBubbleController();

  Profile* GetProfile() const;

  // Called by the view when the "Sign in" button or the "Sync to" button in the
  // promo bubble is clicked.
  void OnSignInToChromeClicked(const AccountInfo& account);

 private:
  // A bridge to ManagePasswordsUIController instance.
  base::WeakPtr<PasswordsModelDelegate> delegate_;
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_SIGN_IN_PROMO_BUBBLE_CONTROLLER_H_
