// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/payments_churned_users_ui_delegate_android.h"

#include <utility>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "chrome/browser/android/preferences/autofill/settings_navigation_helper.h"
#include "chrome/browser/ui/android/autofill/autofill_payments_churned_users_bottom_sheet_bridge.h"
#include "chrome/browser/ui/autofill/autofill_message_controller_impl.h"
#include "chrome/browser/ui/autofill/autofill_message_model.h"
#include "chrome/browser/ui/autofill/autofill_snackbar_controller_impl.h"
#include "chrome/browser/ui/autofill/autofill_snackbar_type.h"
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
    base::OnceCallback<void(PaymentsUiClosedReason)> closed_callback) {
  const AutofillEnableResurrectingPaymentsUsersTreatmentArm treatment_arm =
      GetTreatmentArm();
  switch (treatment_arm) {
    case AutofillEnableResurrectingPaymentsUsersTreatmentArm::kSecurity:
    case AutofillEnableResurrectingPaymentsUsersTreatmentArm::kConvenience:
      closed_callback_ = std::move(closed_callback);
      if (auto* bridge = GetOrCreatePaymentsChurnedUsersBottomSheetBridge()) {
        bridge->RequestShowContent(treatment_arm);
      } else {
        std::move(closed_callback_).Run(PaymentsUiClosedReason::kUnknown);
      }
      break;
    case AutofillEnableResurrectingPaymentsUsersTreatmentArm::kMessage:
      if (is_showing_message_) {
        return;
      }
      closed_callback_ = std::move(closed_callback);
      is_showing_message_ = true;
      GetOrCreateAutofillMessageController().Show(
          AutofillMessageModel::CreateForResurrectChurnedUsers(
              base::BindOnce(
                  &PaymentsChurnedUsersUiDelegateAndroid::OnMessageAccepted,
                  weak_ptr_factory_.GetWeakPtr()),
              base::BindOnce(
                  &PaymentsChurnedUsersUiDelegateAndroid::OnMessageDismissed,
                  weak_ptr_factory_.GetWeakPtr())));
      break;
  }
}

void PaymentsChurnedUsersUiDelegateAndroid::
    SetAutofillPaymentsChurnedUsersBottomSheetBridgeForTesting(
        std::unique_ptr<AutofillPaymentsChurnedUsersBottomSheetBridge> bridge) {
  autofill_payments_churned_users_bottom_sheet_bridge_ = std::move(bridge);
}

void PaymentsChurnedUsersUiDelegateAndroid::
    SetAutofillMessageControllerForTesting(
        std::unique_ptr<AutofillMessageController> controller) {
  autofill_message_controller_ = std::move(controller);
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

AutofillMessageController&
PaymentsChurnedUsersUiDelegateAndroid::GetOrCreateAutofillMessageController() {
  if (!autofill_message_controller_) {
    autofill_message_controller_ =
        std::make_unique<AutofillMessageControllerImpl>(
            &client_->GetWebContents());
  }
  return *autofill_message_controller_;
}

void PaymentsChurnedUsersUiDelegateAndroid::OnMessageAccepted() {
  if (!closed_callback_) {
    return;
  }
  std::move(closed_callback_).Run(PaymentsUiClosedReason::kAccepted);
  if (auto* snackbar_controller = client_->GetAutofillSnackbarController()) {
    snackbar_controller->Show(
        AutofillSnackbarType::kResurrectChurnedUsers,
        base::BindOnce(
            [](base::WeakPtr<content::WebContents> web_contents) {
              if (!web_contents) {
                return;
              }
              ShowAutofillCreditCardSettings(web_contents.get());
            },
            client_->GetWebContents().GetWeakPtr()));
  }
}

void PaymentsChurnedUsersUiDelegateAndroid::OnMessageDismissed(
    messages::DismissReason dismiss_reason) {
  is_showing_message_ = false;
  if (!closed_callback_) {
    return;
  }
  switch (dismiss_reason) {
    case messages::DismissReason::PRIMARY_ACTION:
      // Primary action is handled in `OnMessageAccepted`.
      break;
    case messages::DismissReason::GESTURE:
      // Since the message banner only has a primary action button, swiping the
      // message away is treated as an explicit rejection (`kCancelled`).
      std::move(closed_callback_).Run(PaymentsUiClosedReason::kCancelled);
      break;
    default:
      std::move(closed_callback_).Run(PaymentsUiClosedReason::kNotInteracted);
      break;
  }
  closed_callback_.Reset();
}

}  // namespace autofill::payments
