// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/payments_churned_users_ui_delegate_desktop.h"

#include <utility>

#include "base/check_deref.h"
#include "base/functional/callback.h"
#include "chrome/browser/ui/autofill/payments/payments_churned_users_bubble_controller.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/tabs/public/tab_interface.h"

namespace autofill::payments {

PaymentsChurnedUsersUiDelegateDesktop::PaymentsChurnedUsersUiDelegateDesktop(
    ContentAutofillClient* client)
    : client_(CHECK_DEREF(client)) {}

PaymentsChurnedUsersUiDelegateDesktop::
    ~PaymentsChurnedUsersUiDelegateDesktop() = default;

void PaymentsChurnedUsersUiDelegateDesktop::ShowPaymentsChurnedUsersUI(
    AccountInfo account_info,
    base::OnceCallback<void(PaymentsUiClosedReason)> closed_callback) {
  tabs::TabInterface* tab_interface =
      tabs::TabInterface::MaybeGetFromContents(&client_->GetWebContents());
  if (!tab_interface) {
    return;
  }

  if (PaymentsChurnedUsersBubbleController* controller =
          PaymentsChurnedUsersBubbleController::From(*tab_interface)) {
    controller->Show(std::move(closed_callback), std::move(account_info));
  }
}

}  // namespace autofill::payments
