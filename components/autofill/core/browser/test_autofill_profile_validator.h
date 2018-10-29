// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_PROFILE_VALIDATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_PROFILE_VALIDATOR_H_

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "components/autofill/core/browser/autofill_profile_validator.h"

namespace autofill {

// Singleton that owns a single AutofillProfileValidator instance.
class TestAutofillProfileValidator {
 public:
  static AutofillProfileValidator* GetInstance();

 private:
  friend struct base::LazyInstanceTraitsBase<TestAutofillProfileValidator>;

  TestAutofillProfileValidator();
  ~TestAutofillProfileValidator();

  // The only instance that exists.
  AutofillProfileValidator autofill_profile_validator_;

  DISALLOW_COPY_AND_ASSIGN(TestAutofillProfileValidator);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_PROFILE_VALIDATOR_H_
