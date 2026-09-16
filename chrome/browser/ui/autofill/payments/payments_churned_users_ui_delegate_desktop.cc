// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/payments_churned_users_ui_delegate_desktop.h"

#include "base/check_deref.h"
#include "base/functional/callback.h"
#include "base/notimplemented.h"
#include "components/autofill/content/browser/content_autofill_client.h"

namespace autofill::payments {

PaymentsChurnedUsersUiDelegateDesktop::PaymentsChurnedUsersUiDelegateDesktop(
    ContentAutofillClient* client)
    : client_(CHECK_DEREF(client)) {}

PaymentsChurnedUsersUiDelegateDesktop::
    ~PaymentsChurnedUsersUiDelegateDesktop() = default;

void PaymentsChurnedUsersUiDelegateDesktop::ShowPaymentsChurnedUsersUI(
    base::OnceClosure accept_callback,
    base::OnceClosure cancel_callback,
    base::OnceClosure closed_callback) {
  // TODO(crbug.com/558880336): Implement for Desktop.
  NOTIMPLEMENTED();
}

}  // namespace autofill::payments
