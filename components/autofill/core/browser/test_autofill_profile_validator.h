// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_PROFILE_VALIDATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_PROFILE_VALIDATOR_H_

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "components/autofill/core/browser/autofill_profile_validator.h"
#include "components/autofill/core/browser/test_autofill_profile_validator_delayed.h"

namespace autofill {

// Singleton that owns a single AutofillProfileValidator instance.
class TestAutofillProfileValidator {
 public:
  static AutofillProfileValidator* GetInstance();
  static TestAutofillProfileValidatorDelayed* GetDelayedInstance();

  TestAutofillProfileValidator(const TestAutofillProfileValidator&) = delete;
  TestAutofillProfileValidator& operator=(const TestAutofillProfileValidator&) =
      delete;

 private:
  friend struct base::LazyInstanceTraitsBase<TestAutofillProfileValidator>;

  TestAutofillProfileValidator();
  ~TestAutofillProfileValidator();

  // The only instance that exists of normal and delayed validators.
  AutofillProfileValidator autofill_profile_validator_;
  TestAutofillProfileValidatorDelayed autofill_profile_validator_delayed_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_PROFILE_VALIDATOR_H_
