// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_PAYMENTS_CHURNED_USERS_UI_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_PAYMENTS_CHURNED_USERS_UI_DELEGATE_H_

#include "base/functional/callback_forward.h"
#include "components/autofill/core/browser/ui/payments/payments_ui_closed_reasons.h"
#include "components/signin/public/identity_manager/account_info.h"

namespace autofill::payments {

// The cross-platform C++ UI delegate interface for displaying the Payments
// Churned Users resurrection UI. This UI is shown to resurrect users who
// previously disabled Autofill payment methods, prompting them with a value
// proposition to turn Autofill back on when they interact with a credit card
// form.
// Owned by PaymentsAutofillClient (e.g., ChromePaymentsAutofillClient) and
// lazily created upon first access. Its lifecycle matches the remaining
// lifetime of its owning PaymentsAutofillClient.
class PaymentsChurnedUsersUiDelegate {
 public:
  virtual ~PaymentsChurnedUsersUiDelegate() = default;

  // Requests the platform UI layer to show the Payments Churned Users
  // resurrection UI, prompting churned users who turned off Autofill payment
  // methods to resurrect and re-enable Autofill.
  virtual void ShowPaymentsChurnedUsersUI(
      AccountInfo account_info,
      base::OnceCallback<void(PaymentsUiClosedReason)> closed_callback) = 0;
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_PAYMENTS_CHURNED_USERS_UI_DELEGATE_H_
