// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_PAYMENTS_CHURNED_USERS_UI_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_PAYMENTS_CHURNED_USERS_UI_DELEGATE_ANDROID_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/ui/payments/payments_churned_users_ui_delegate.h"

namespace autofill {

class AutofillPaymentsChurnedUsersBottomSheetBridge;
class ContentAutofillClient;

namespace payments {

// Android implementation of PaymentsChurnedUsersUiDelegate.
// This class handles the UI for resurrecting churned payments users on Android.
// Owned by ChromePaymentsAutofillClient and lazily created upon first access to
// GetPaymentsChurnedUsersUiDelegate(). Its lifecycle matches the remaining
// lifetime of ChromePaymentsAutofillClient.
class PaymentsChurnedUsersUiDelegateAndroid
    : public PaymentsChurnedUsersUiDelegate {
 public:
  explicit PaymentsChurnedUsersUiDelegateAndroid(ContentAutofillClient* client);
  ~PaymentsChurnedUsersUiDelegateAndroid() override;

  PaymentsChurnedUsersUiDelegateAndroid(
      const PaymentsChurnedUsersUiDelegateAndroid&) = delete;
  PaymentsChurnedUsersUiDelegateAndroid& operator=(
      const PaymentsChurnedUsersUiDelegateAndroid&) = delete;

  // PaymentsChurnedUsersUiDelegate:
  void ShowPaymentsChurnedUsersUI(base::OnceClosure accept_callback,
                                  base::OnceClosure cancel_callback,
                                  base::OnceClosure closed_callback) override;

  void SetAutofillPaymentsChurnedUsersBottomSheetBridgeForTesting(
      std::unique_ptr<AutofillPaymentsChurnedUsersBottomSheetBridge> bridge);

 private:
  AutofillEnableResurrectingPaymentsUsersTreatmentArm GetTreatmentArm() const;
  AutofillPaymentsChurnedUsersBottomSheetBridge*
  GetOrCreatePaymentsChurnedUsersBottomSheetBridge();

  const raw_ref<ContentAutofillClient> client_;
  // TODO(crbug.com/558874126): Wire callbacks to the bottom sheet bridge and
  // message controller.
  base::OnceClosure accept_callback_;
  base::OnceClosure cancel_callback_;
  base::OnceClosure closed_callback_;
  std::unique_ptr<AutofillPaymentsChurnedUsersBottomSheetBridge>
      autofill_payments_churned_users_bottom_sheet_bridge_;
};

}  // namespace payments
}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_PAYMENTS_CHURNED_USERS_UI_DELEGATE_ANDROID_H_
