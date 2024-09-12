// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_BUBBLE_SIGNIN_PROMO_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_BUBBLE_SIGNIN_PROMO_CONTROLLER_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"

struct AccountInfo;
namespace signin {
enum class SignInAutofillBubblePromoType;
}  // namespace signin

namespace signin_metrics {
enum class AccessPoint;
}  // namespace signin_metrics

namespace content {
class WebContents;
}  // namespace content

namespace autofill {

// This controller provides data and actions for the
// AutofillSignInPromoBubbleView.
class AutofillBubbleSignInPromoController {
 public:
  explicit AutofillBubbleSignInPromoController(
      content::WebContents& web_contents,
      signin_metrics::AccessPoint access_point,
      base::OnceCallback<void(content::WebContents*)> move_callback);
  ~AutofillBubbleSignInPromoController();

  // Called by the view when the "Sign in" button in the promo bubble is
  // clicked.
  void OnSignInToChromeClicked(const AccountInfo& account);

 private:
  // Used to move the local data item to the account storage once the sign in
  // has been completed.
  base::OnceCallback<void(content::WebContents*)> move_callback_;
  base::WeakPtr<content::WebContents> web_contents_;
  signin_metrics::AccessPoint access_point_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_BUBBLE_SIGNIN_PROMO_CONTROLLER_H_
