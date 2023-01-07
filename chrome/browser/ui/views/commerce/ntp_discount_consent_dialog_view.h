// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_COMMERCE_NTP_DISCOUNT_CONSENT_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_COMMERCE_NTP_DISCOUNT_CONSENT_DIALOG_VIEW_H_

#include "base/functional/callback_helpers.h"
#include "chrome/browser/cart/chrome_cart.mojom.h"
#include "ui/views/window/dialog_delegate.h"

class NtpDiscountConsentDialogView : public views::DialogDelegateView {
 public:
  using ActionCallback =
      base::OnceCallback<void(chrome_cart::mojom::ConsentStatus)>;
  explicit NtpDiscountConsentDialogView(ActionCallback callback);
  ~NtpDiscountConsentDialogView() override;

 private:
  ActionCallback callback_;

  SkColor GetBackgroundColor();
  // Called when the accept button is clicked.
  void OnAccept();
  // Called when the reject button is clicked.
  void OnReject();
  // Called when the Esc key is used.
  void OnDismiss();
};

#endif  // CHROME_BROWSER_UI_VIEWS_COMMERCE_NTP_DISCOUNT_CONSENT_DIALOG_VIEW_H_
