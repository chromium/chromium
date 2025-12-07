// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/foundations/test_autofill_client.h"

namespace autofill {

TestAutofillClientBase::TestAutofillClientBase(
    base::PassKey<TestAutofillClient>) {}

TestAutofillClientBase::~TestAutofillClientBase() = default;

TestAutofillDriverFactory& TestAutofillClientBase::GetAutofillDriverFactory() {
  return autofill_driver_factory_;
}

TestAutofillClient::TestAutofillClient()
    : TestAutofillClientTemplate<TestAutofillClientBase>(/*pass_key=*/{}) {}

TestAutofillClient::~TestAutofillClient() {
  // Ensure that AutofillDrivers and AutofillManagers are destroyed while the
  // AutofillClient is still functional.
  //
  // This is necessary because they may call into `this` during their
  // destruction. In production code, that's safe because drivers are destroyed
  // before clients.
  //
  // If you come here from a test's crash trace:
  //
  // If the crashing test uses a subclass of TestAutofillClient, you may need to
  // call DeleteAll() in that subclass's destructor.
  //
  // The reason is that virtual dispatch does not work during destruction. So if
  // the below DeleteAll() indirectly calls a virtual function of
  // AutofillDriver, this won't dispatch to the overrides of that subclass of
  // TestAutofillClient.
  GetAutofillDriverFactory().DeleteAll();
}

}  // namespace autofill
