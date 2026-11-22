// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/foundations/test_autofill_client.h"

#include "base/check.h"
#include "components/autofill/core/browser/foundations/autofill_driver.h"

namespace autofill {

TestAutofillClientBase::TestAutofillClientBase(
    base::PassKey<TestAutofillClient>) {}

TestAutofillClientBase::~TestAutofillClientBase() = default;

TestAutofillDriverFactory& TestAutofillClientBase::GetAutofillDriverFactory() {
  return autofill_driver_factory_;
}

AutofillManager*
TestAutofillClientBase::GetAutofillManagerForPrimaryMainFrame() {
  AutofillDriver* driver = nullptr;
  size_t num_matches = 0;
  for (AutofillDriver* d : GetAutofillDriverFactory().GetExistingDrivers()) {
    if (d->IsActive() && !d->IsEmbedded() && !d->GetParent() &&
        d->GetLifecycleState() == AutofillDriver::LifecycleState::kActive) {
      driver = d;
      ++num_matches;
    }
  }
  CHECK(allow_null_primary_main_frame_driver_ || num_matches > 0)
      << "No active primary main frame AutofillDriver found.\n\n"
         "If your test is using `WithTestAutofillClientDriverManager`, call "
         "`CreateAndActivateAutofillDriver()` or "
         "`ActivateAutofillDriver()`.\n\n"
         "Otherwise, make sure it satisfies `IsActive()` and "
         "`GetLifecycleState() == kActive` and `!IsEmbedded()` and "
         "`GetParent() == nullptr`.\n\n"
         "If you specifically want to test a scenario without a primary "
         "main frame, you can disable this CHECK with "
         "`AllowNullPrimaryMainFrameDriver(true)`.";
  CHECK(allow_multiple_primary_main_frame_drivers_ || num_matches <= 1)
      << "Found " << num_matches
      << " active primary main frame AutofillDrivers.\n\n"
         "If your test is using `WithTestAutofillClientDriverManager`, make "
         "sure at most one driver is activated (via "
         "`CreateAndActivateAutofillDriver()` or "
         "`ActivateAutofillDriver()`).\n\n"
         "Otherwise, make sure at most one driver satisfies `IsActive()` and "
         "`GetLifecycleState() == kActive` and `!IsEmbedded()` and "
         "`GetParent() == nullptr`.\n\n"
         "If you specifically want to test a scenario with multiple primary "
         "main frames, you can disable this CHECK with "
         "`AllowMultiplePrimaryMainFrameDrivers(true)`.";
  return driver ? &driver->GetAutofillManager() : nullptr;
}

TestAutofillClient::TestAutofillClient()
    : TestAutofillClientTemplate<TestAutofillClientBase>(/*pass_key=*/{}) {}

TestAutofillClient::~TestAutofillClient() {
  // Ensure that AutofillDrivers and AutofillManagers are destroyed while the
  // AutofillClient is still functional.
  //
  // This is necessary because they may call into `this` during their
  // destruction. In production code, that's safe because drivers are destroyed
  // before clients (crbug.com/587056767).
  //
  // If you come here from a test's crash trace:
  //
  // If the crashing test uses a subclass of TestAutofillClient, you may need to
  // call DeleteAll() in that subclass's destructor.
  //
  // The reason is that virtual dispatch does not work during destruction. So if
  // the below DeleteAll() indirectly calls a virtual function of
  // AutofillClient, this won't dispatch to the overrides of that subclass of
  // TestAutofillClient.
  GetAutofillDriverFactory().DeleteAll();
}

}  // namespace autofill
