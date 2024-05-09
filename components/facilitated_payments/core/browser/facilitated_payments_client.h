// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_FACILITATED_PAYMENTS_CLIENT_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_FACILITATED_PAYMENTS_CLIENT_H_

#include <cstdint>

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "components/autofill/core/browser/payments/risk_data_loader.h"

namespace autofill {
class BankAccount;
class PersonalDataManager;
}  // namespace autofill

namespace payments::facilitated {

class FacilitatedPaymentsNetworkInterface;

// A cross-platform client interface for showing UI for non-form based FOPs.
class FacilitatedPaymentsClient : public autofill::RiskDataLoader {
 public:
  ~FacilitatedPaymentsClient() override;

  // Gets the `PersonalDataManager` instance associated with the Chrome profile.
  // It is used to get user's account info.
  virtual autofill::PersonalDataManager* GetPersonalDataManager() = 0;

  // Gets the `FacilitatedPaymentsNetworkInterface` instance owned by the client
  // used for making payment requests. It can be null if the browser context
  // associated with the WebContents is null.
  virtual FacilitatedPaymentsNetworkInterface*
  GetFacilitatedPaymentsNetworkInterface() = 0;

  // Shows the user's PIX accounts from their Google Wallet, and prompts to pay.
  // If the UI was shown, then returns true and later invokes the
  // `on_user_decision_callback` with the result of user's selection: a boolean
  // for acceptance or cancellation and the selected instrument ID in case of
  // acceptance. `pix_account_suggestions` is the list of PIX accounts to be
  // shown to the user for payment. If the UI was not shown, then returns false
  // and does not invoke the callback.
  virtual bool ShowPixPaymentPrompt(
      base::span<autofill::BankAccount> bank_account_suggestions,
      base::OnceCallback<void(bool, int64_t)> on_user_decision_callback);
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_FACILITATED_PAYMENTS_CLIENT_H_
