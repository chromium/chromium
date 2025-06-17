// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/autofill/core/browser/payments/save_and_fill_manager_impl.h"

#include "base/check_deref.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"

namespace autofill::payments {

SaveAndFillManagerImpl::SaveAndFillManagerImpl(
    PaymentsAutofillClient* payments_autofill_client)
    : payments_autofill_client_(CHECK_DEREF(payments_autofill_client)) {}

SaveAndFillManagerImpl::~SaveAndFillManagerImpl() = default;

void SaveAndFillManagerImpl::OnDidAcceptCreditCardSaveAndFillSuggestion() {
  payments_autofill_client_->ShowCreditCardSaveAndFillDialog();
}

}  // namespace autofill::payments
