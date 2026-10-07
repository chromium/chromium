// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_PAYMENTS_CHURNED_USERS_BOTTOM_SHEET_BRIDGE_TEST_API_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_PAYMENTS_CHURNED_USERS_BOTTOM_SHEET_BRIDGE_TEST_API_H_

#include <utility>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/android/autofill/autofill_payments_churned_users_bottom_sheet_bridge.h"
#include "components/autofill/core/browser/ui/payments/payments_ui_closed_reasons.h"

namespace autofill {

class AutofillPaymentsChurnedUsersBottomSheetBridgeTestApi {
 public:
  explicit AutofillPaymentsChurnedUsersBottomSheetBridgeTestApi(
      AutofillPaymentsChurnedUsersBottomSheetBridge& bridge)
      : bridge_(bridge) {}

  void SetCallback(base::OnceCallback<void(PaymentsUiClosedReason)> callback) {
    bridge_->closed_callback_ = std::move(callback);
  }

  void SetShowConfirmationCallback(base::OnceClosure callback) {
    bridge_->show_confirmation_callback_ = std::move(callback);
  }

 private:
  const raw_ref<AutofillPaymentsChurnedUsersBottomSheetBridge> bridge_;
};

inline AutofillPaymentsChurnedUsersBottomSheetBridgeTestApi test_api(
    AutofillPaymentsChurnedUsersBottomSheetBridge& bridge) {
  return AutofillPaymentsChurnedUsersBottomSheetBridgeTestApi(bridge);
}

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_PAYMENTS_CHURNED_USERS_BOTTOM_SHEET_BRIDGE_TEST_API_H_
