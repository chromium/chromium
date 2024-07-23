// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/test_autofill_driver.h"

#include "base/check_deref.h"
#include "components/autofill/core/browser/autofill_manager_test_api.h"

namespace autofill {

TestAutofillDriver::TestAutofillDriver(AutofillClient* client)
    : autofill_client_(CHECK_DEREF(client)) {}

TestAutofillDriver::~TestAutofillDriver() {
  if (autofill_manager_) {
    test_api(*autofill_manager_)
        .SetLifecycleState(AutofillManager::LifecycleState::kPendingDeletion);
  }
}

AutofillClient& TestAutofillDriver::GetAutofillClient() {
  return *autofill_client_;
}

AutofillManager& TestAutofillDriver::GetAutofillManager() {
  return *autofill_manager_;
}

}  // namespace autofill
