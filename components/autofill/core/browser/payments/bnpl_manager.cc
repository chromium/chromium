// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/bnpl_manager.h"

#include "base/check_deref.h"

namespace autofill::payments {

BnplManager::BnplManager(PaymentsAutofillClient* payments_autofill_client)
    : payments_autofill_client_(CHECK_DEREF(payments_autofill_client)) {}

BnplManager::~BnplManager() = default;

}  // namespace autofill::payments
