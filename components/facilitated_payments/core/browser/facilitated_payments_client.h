// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_FACILITATED_PAYMENTS_CLIENT_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_FACILITATED_PAYMENTS_CLIENT_H_

#include <cstdint>
#include <optional>

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "components/autofill/core/browser/payments/risk_data_loader.h"
#include "components/signin/public/identity_manager/account_info.h"

namespace autofill {
class BankAccount;
class PaymentsDataManager;
}  // namespace autofill

namespace payments::facilitated {

class FacilitatedPaymentsNetworkInterface;

// TODO: b/350661525 - Make all methods pure virtual.
// A cross-platform client interface for showing UI for non-form based FOPs.
class FacilitatedPaymentsClient : public autofill::RiskDataLoader {
 public:
  ~FacilitatedPaymentsClient() override;

  // Gets the `PaymentsDataManager` instance associated with the Chrome profile.
  // It is used to get user's account info.
  virtual autofill::PaymentsDataManager* GetPaymentsDataManager() = 0;

  // Gets the `FacilitatedPaymentsNetworkInterface` instance owned by the client
  // used for making payment requests. It can be null if the browser context
  // associated with the WebContents is null.
  virtual FacilitatedPaymentsNetworkInterface*
  GetFacilitatedPaymentsNetworkInterface() = 0;

  // Provides access to the core information of the user's primary account.
  virtual std::optional<CoreAccountInfo> GetCoreAccountInfo() = 0;

  // Returns true if the device is being used in the landscape mode.
  virtual bool IsInLandscapeMode() = 0;

  // Shows the user's PIX accounts from their Google Wallet, and prompts to pay.
  // If the UI was shown, then returns true and later invokes the
  // `on_user_decision_callback` with the result of user's selection: a boolean
  // for acceptance or cancellation and the selected instrument ID in case of
  // acceptance. `pix_account_suggestions` is the list of PIX accounts to be
  // shown to the user for payment. If the UI was not shown, then returns false
  // and does not invoke the callback.
  virtual bool ShowPixPaymentPrompt(
      base::span<const autofill::BankAccount> bank_account_suggestions,
      base::OnceCallback<void(bool, int64_t)> on_user_decision_callback);

  // Shows a progress bar while users wait for server response after selecting a
  // payment account.
  virtual void ShowProgressScreen();

  // Shows an error message if Chrome isn't able to complete transaction after
  // the user has selected a payment account.
  virtual void ShowErrorScreen();

  // Closes the bottom sheet.
  virtual void DismissPrompt();
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_FACILITATED_PAYMENTS_CLIENT_H_
