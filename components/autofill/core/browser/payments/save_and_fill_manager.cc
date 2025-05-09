// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/save_and_fill_manager.h"

#include "base/check_deref.h"

namespace autofill::payments {

SaveAndFillManager::SaveAndFillManager(
    PaymentsAutofillClient* payments_autofill_client)
    : payments_autofill_client_(CHECK_DEREF(payments_autofill_client)) {}

SaveAndFillManager::~SaveAndFillManager() = default;

}  // namespace autofill::payments
