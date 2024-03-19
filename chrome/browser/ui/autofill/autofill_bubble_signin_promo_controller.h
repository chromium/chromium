// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_BUBBLE_SIGNIN_PROMO_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_BUBBLE_SIGNIN_PROMO_CONTROLLER_H_

#include "base/memory/weak_ptr.h"

struct AccountInfo;
class PasswordsModelDelegate;

namespace autofill {

// This controller provides data and actions for the
// AutofillSignInPromoBubbleView.
class AutofillBubbleSignInPromoController {
 public:
  explicit AutofillBubbleSignInPromoController(
      base::WeakPtr<PasswordsModelDelegate> delegate);
  ~AutofillBubbleSignInPromoController();

  // Called by the view when the "Sign in" button in the promo bubble is
  // clicked.
  void OnSignInToChromeClicked(const AccountInfo& account);

 private:
  // A bridge to ManagePasswordsUIController instance.
  // TODO(crbug.com/319411728): Should be something across all autofill types
  // instead.
  base::WeakPtr<PasswordsModelDelegate> delegate_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_BUBBLE_SIGNIN_PROMO_CONTROLLER_H_
