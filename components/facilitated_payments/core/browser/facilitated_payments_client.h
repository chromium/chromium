// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_FACILITATED_PAYMENTS_CLIENT_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_FACILITATED_PAYMENTS_CLIENT_H_

#include <cstdint>
#include <optional>

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "components/autofill/core/browser/data_model/ewallet.h"
#include "components/autofill/core/browser/payments/risk_data_loader.h"
#include "components/facilitated_payments/core/ui_utils/facilitated_payments_ui_utils.h"
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
  // If the UI was shown, `on_user_decision_callback` is used to inform the
  // result of user's selection: a boolean for acceptance or cancellation and
  // the selected instrument ID in case of acceptance. `pix_account_suggestions`
  // is the list of PIX accounts to be shown to the user for payment.
  virtual void ShowPixPaymentPrompt(
      base::span<const autofill::BankAccount> bank_account_suggestions,
      base::OnceCallback<void(bool, int64_t)> on_user_decision_callback);

  // Shows the user's eWallet accounts from their Google Wallet, and prompts to
  // pay. If the UI was shown, `on_user_decision_callback` is used to inform the
  // result of user's selection: a boolean for acceptance or cancellation and
  // the selected instrument ID in case of acceptance. `ewallet_suggestions` is
  // the list of eWallets to be shown to the user for payment.
  virtual void ShowEwalletPaymentPrompt(
      base::span<const autofill::Ewallet> ewallet_suggestions,
      base::OnceCallback<void(bool, int64_t)> on_user_decision_callback);

  // Shows a progress bar while users wait for server response after selecting a
  // payment account.
  virtual void ShowProgressScreen();

  // Shows an error message if Chrome isn't able to complete transaction after
  // the user has selected a payment account.
  virtual void ShowErrorScreen();

  // Closes the bottom sheet.
  virtual void DismissPrompt();

  // Enables features to pass a callback to listen to UI events.
  virtual void SetUiEventListener(
      base::RepeatingCallback<void(UiEvent)> ui_event_listener);
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_FACILITATED_PAYMENTS_CLIENT_H_
