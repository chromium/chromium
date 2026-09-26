// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/payments_churned_users_ui_delegate_android.h"

#include <utility>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/notimplemented.h"
#include "chrome/browser/ui/android/autofill/autofill_payments_churned_users_bottom_sheet_bridge.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "content/public/browser/web_contents.h"

namespace autofill::payments {

PaymentsChurnedUsersUiDelegateAndroid::PaymentsChurnedUsersUiDelegateAndroid(
    ContentAutofillClient* client)
    : client_(CHECK_DEREF(client)) {}

PaymentsChurnedUsersUiDelegateAndroid::
    ~PaymentsChurnedUsersUiDelegateAndroid() = default;

void PaymentsChurnedUsersUiDelegateAndroid::ShowPaymentsChurnedUsersUI(
    base::OnceClosure accept_callback,
    base::OnceClosure cancel_callback,
    base::OnceClosure closed_callback) {
  // TODO(crbug.com/558874126): Consider replacing the 3 callbacks with a
  // single callback taking `PaymentsUiClosedReason`.
  accept_callback_ = std::move(accept_callback);
  cancel_callback_ = std::move(cancel_callback);
  closed_callback_ = std::move(closed_callback);

  const AutofillEnableResurrectingPaymentsUsersTreatmentArm treatment_arm =
      GetTreatmentArm();
  switch (treatment_arm) {
    case AutofillEnableResurrectingPaymentsUsersTreatmentArm::kSecurity:
    case AutofillEnableResurrectingPaymentsUsersTreatmentArm::kConvenience:
      if (auto* bridge = GetOrCreatePaymentsChurnedUsersBottomSheetBridge()) {
        bridge->RequestShowContent(treatment_arm);
      }
      break;
    case AutofillEnableResurrectingPaymentsUsersTreatmentArm::kMessage:
      // TODO(crbug.com/558874126): Route `kMessage` to
      // `AutofillMessageController` once the Android Message arm is wired up.
      NOTIMPLEMENTED();
      break;
  }
}

void PaymentsChurnedUsersUiDelegateAndroid::
    SetAutofillPaymentsChurnedUsersBottomSheetBridgeForTesting(
        std::unique_ptr<AutofillPaymentsChurnedUsersBottomSheetBridge> bridge) {
  autofill_payments_churned_users_bottom_sheet_bridge_ = std::move(bridge);
}

AutofillEnableResurrectingPaymentsUsersTreatmentArm
PaymentsChurnedUsersUiDelegateAndroid::GetTreatmentArm() const {
  CHECK(base::FeatureList::IsEnabled(
      features::kAutofillEnableResurrectingPaymentsUsers));
  switch (features::kAutofillEnableResurrectingPaymentsUsersTreatment.Get()) {
    case 1:
      return AutofillEnableResurrectingPaymentsUsersTreatmentArm::kSecurity;
    case 2:
      return AutofillEnableResurrectingPaymentsUsersTreatmentArm::kConvenience;
    case 3:
      return AutofillEnableResurrectingPaymentsUsersTreatmentArm::kMessage;
    default:
      return AutofillEnableResurrectingPaymentsUsersTreatmentArm::kSecurity;
  }
}

AutofillPaymentsChurnedUsersBottomSheetBridge*
PaymentsChurnedUsersUiDelegateAndroid::
    GetOrCreatePaymentsChurnedUsersBottomSheetBridge() {
  if (!autofill_payments_churned_users_bottom_sheet_bridge_) {
    if (auto* window_android =
            client_->GetWebContents().GetTopLevelNativeWindow()) {
      autofill_payments_churned_users_bottom_sheet_bridge_ =
          std::make_unique<AutofillPaymentsChurnedUsersBottomSheetBridge>(
              window_android);
    }
  }
  return autofill_payments_churned_users_bottom_sheet_bridge_.get();
}

}  // namespace autofill::payments
