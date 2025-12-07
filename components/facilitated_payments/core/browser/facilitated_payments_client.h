// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_FACILITATED_PAYMENTS_CLIENT_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_FACILITATED_PAYMENTS_CLIENT_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "components/autofill/core/browser/data_model/payments/ewallet.h"
#include "components/autofill/core/browser/payments/risk_data_loader.h"
#include "components/facilitated_payments/core/browser/device_delegate.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_app_info_list.h"
#include "components/facilitated_payments/core/browser/payment_link_manager.h"
#include "components/facilitated_payments/core/utils/facilitated_payments_ui_utils.h"
#include "components/signin/public/identity_manager/account_info.h"

namespace url {
class Origin;
}  // namespace url

namespace optimization_guide {
class OptimizationGuideDecider;
}  // namespace optimization_guide

namespace autofill {
class BankAccount;
class PaymentsDataManager;
}  // namespace autofill

namespace strike_database {
class StrikeDatabase;
}  // namespace strike_database

namespace payments::facilitated {

class PixAccountLinkingManager;
class FacilitatedPaymentsNetworkInterface;

// A cross-platform client interface for showing UI for non-form based FOPs.
class FacilitatedPaymentsClient : public autofill::RiskDataLoader {
 public:
  FacilitatedPaymentsClient();
  ~FacilitatedPaymentsClient() override;

  // Gets the URL of the last committed page.
  virtual const url::Origin& GetLastCommittedOrigin() const = 0;

  // Gets the `PaymentsDataManager` instance associated with the Chrome profile.
  // It is used to get user's account info.
  virtual autofill::PaymentsDataManager* GetPaymentsDataManager() = 0;

  // Gets the `FacilitatedPaymentsNetworkInterface` instance owned by the client
  // used for making payment requests. It can be null if the browser context
  // associated with the WebContents is null. Support multiple active
  // requests at a time.
  virtual FacilitatedPaymentsNetworkInterface*
  GetFacilitatedPaymentsNetworkInterface() = 0;

  // Provides access to the core information of the user's primary account.
  virtual std::optional<CoreAccountInfo> GetCoreAccountInfo() = 0;

  // Returns true if the device is being used in the landscape mode.
  virtual bool IsInLandscapeMode() = 0;

  // Returns true if the device is a foldable device.
  virtual bool IsFoldable() = 0;

  // Returns true if the current tab is opened as a CCT in another app instead
  // of the Chrome browser app.
  virtual bool IsInChromeCustomTabMode() = 0;

  // Returns an instance of the OptimizationGuideDecider associated with the
  // Chrome profile. It is used to determine whether a render frame host URL is
  // part of a feature allowlist.
  virtual optimization_guide::OptimizationGuideDecider*
  GetOptimizationGuideDecider() = 0;

  // Returns the `DeviceDelegate` instance owned by the implementation class.
  virtual DeviceDelegate* GetDeviceDelegate() = 0;

  // Returns true if the WebContents associated with this instance is either
  // visible or occluded, but not hidden. When a tab is occluded, it is still
  // open, but not visible either because it is covered by other windows or
  // because it's outside the screen bounds.
  virtual bool IsWebContentsVisibleOrOccluded() = 0;

  // Shows the user's Pix accounts from their Google Wallet, and prompts to pay.
  // `bank_account_suggestions` is the list of Pix accounts to be shown to the
  // user for payment. `on_payment_account_selected` is the callback called with
  // the instrument id of the bank account selected by the user for payment.
  virtual void ShowPixPaymentPrompt(
      base::span<const autofill::BankAccount> bank_account_suggestions,
      base::OnceCallback<void(int64_t)> on_payment_account_selected) = 0;

  // Shows the user's payment options and prompts to pay. `ewallet_suggestions`
  // is the list of eWallets to be shown to the user for payment.
  // `app_suggestions` is the list of packages of payment apps to be shown to
  // the user for payment. `on_payment_account_selected` is the callback called
  // with the instrument id of the eWallet account selected by the user for
  // payment.
  virtual void ShowPaymentLinkPrompt(
      base::span<const autofill::Ewallet> ewallet_suggestions,
      std::unique_ptr<FacilitatedPaymentsAppInfoList> app_suggestions,
      base::OnceCallback<void(SelectedFopData)> on_fop_selected) = 0;

  // Shows a progress bar while users wait for server response after selecting a
  // payment account.
  virtual void ShowProgressScreen() = 0;

  // Shows an error message if Chrome isn't able to complete transaction after
  // the user has selected a payment account.
  virtual void ShowErrorScreen() = 0;

  // Closes the bottom sheet.
  virtual void DismissPrompt() = 0;

  // Enables features to pass a callback to listen to UI events.
  virtual void SetUiEventListener(
      base::RepeatingCallback<void(UiEvent)> ui_event_listener) = 0;

  // Gets the StrikeDatabase associated with the client. Note: Nullptr may be
  // returned so check before use.
  virtual strike_database::StrikeDatabase* GetStrikeDatabase() = 0;

  virtual void InitPixAccountLinkingFlow(
      const url::Origin& pix_payment_page_origin) = 0;

  // Shows the Pix account linking prompt.
  virtual void ShowPixAccountLinkingPrompt(
      base::OnceCallback<void()> on_accepted,
      base::OnceCallback<void()> on_declined) = 0;

  // Check whether the device has the screenlock or biometric set up which is
  // required for Pix account linking in Wallet.
  virtual bool HasScreenlockOrBiometricSetup() = 0;

  void SetPixAccountLinkingManagerForTesting(
      std::unique_ptr<PixAccountLinkingManager> pix_account_linking_manager);

 protected:
  std::unique_ptr<PixAccountLinkingManager> pix_account_linking_manager_;
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_FACILITATED_PAYMENTS_CLIENT_H_
