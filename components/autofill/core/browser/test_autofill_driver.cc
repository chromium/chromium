// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/test_autofill_driver.h"

#include "base/check_deref.h"
#include "components/autofill/core/browser/autofill_manager_test_api.h"

namespace autofill {

TestAutofillDriver::TestAutofillDriver(TestAutofillClient* client)
    : autofill_client_(CHECK_DEREF(client)) {}

TestAutofillDriver::~TestAutofillDriver() {
  test_api(*this).SetLifecycleState(
      AutofillDriver::LifecycleState::kPendingDeletion);
}

TestAutofillClient& TestAutofillDriver::GetAutofillClient() {
  return *autofill_client_;
}

AutofillManager& TestAutofillDriver::GetAutofillManager() {
  return *autofill_manager_;
}

ukm::SourceId TestAutofillDriver::GetPageUkmSourceId() const {
  // This test implementation does not correctly simulate production code, where
  // the UKM source IDs of inactive drivers and active drivers differ.
  //
  // Simulating the production code is difficult because UKM source IDs are
  // controlled by navigations, but TestAutofillClient and TestAutofillDriver
  // have no access to simulated navigation.
  return autofill_client_->GetActivePageUkmSourceId();
}

}  // namespace autofill
