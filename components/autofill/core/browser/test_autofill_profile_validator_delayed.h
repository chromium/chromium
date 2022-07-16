// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_PROFILE_VALIDATOR_DELAYED_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_PROFILE_VALIDATOR_DELAYED_H_

#include <memory>
#include <string>

#include "components/autofill/core/browser/autofill_profile_validator.h"
#include "third_party/libaddressinput/chromium/chrome_address_validator.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/source.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/storage.h"

namespace autofill {

// Singleton that owns a single AutofillProfileValidator instance. It's a
// delayed validator used in tests, to make sure that the system can handle
// possible delays in the real world.
class TestAutofillProfileValidatorDelayed : public AutofillProfileValidator {
 public:
  // Takes ownership of |source| and |storage|.
  TestAutofillProfileValidatorDelayed(
      std::unique_ptr<::i18n::addressinput::Source> source,
      std::unique_ptr<::i18n::addressinput::Storage> storage);

  TestAutofillProfileValidatorDelayed(
      const TestAutofillProfileValidatorDelayed&) = delete;
  TestAutofillProfileValidatorDelayed& operator=(
      const TestAutofillProfileValidatorDelayed&) = delete;

  ~TestAutofillProfileValidatorDelayed() override;

  // Starts loading the rules for the specified |region_code|.
  void LoadRulesForRegion(const std::string& region_code) override;

 private:
  void LoadRulesInstantly(const std::string& region_code);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_PROFILE_VALIDATOR_DELAYED_H_
