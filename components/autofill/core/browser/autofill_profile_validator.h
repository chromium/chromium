// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_PROFILE_VALIDATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_PROFILE_VALIDATOR_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "third_party/libaddressinput/chromium/chrome_address_validator.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/source.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/storage.h"

namespace autofill {

using AutofillProfileValidatorCallback =
    base::OnceCallback<void(const AutofillProfile*)>;

// AutofillProfileValidator Loads Rules from the server and validates an
// autofill profile. For a given autofill profile, it will set the ValidityState
// of several fields in the profile such as country, administrative area, etc.
// See implementation for more details.
class AutofillProfileValidator : public autofill::LoadRulesListener {
 public:
  // Takes ownership of |source| and |storage|.
  AutofillProfileValidator(
      std::unique_ptr<::i18n::addressinput::Source> source,
      std::unique_ptr<::i18n::addressinput::Storage> storage);

  ~AutofillProfileValidator() override;

  // If the rule corresponding to the |profile| is loaded, this validates the
  // profile, synchronously. If it is not loaded yet, it sets up a
  // task to validate the profile when the rule is loaded (asynchronous). If the
  // loading has not yet started, it will also start loading the rules.
  void StartProfileValidation(const AutofillProfile* profile,
                              AutofillProfileValidatorCallback cb);

 protected:
  // Starts loading the rules for the specified |region_code|.
  virtual void LoadRulesForRegion(const std::string& region_code);

  // The address validator used to load rules.
  AddressValidator address_validator_;

 private:
  // ValidationRequest loads Rules from the server and validates various fields
  // in an autofill profile.
  class ValidationRequest {
   public:
    ValidationRequest(base::WeakPtr<const AutofillProfile> profile,
                      AddressValidator* validator,
                      AutofillProfileValidatorCallback on_validated);

    ~ValidationRequest();

    // Validates various fields of the |profile_|, and calls |on_validated_|.
    void OnRulesLoaded();

   private:
    AutofillProfile profile_;

    // Not owned. Outlives this object.
    AddressValidator* validator_;

    AutofillProfileValidatorCallback on_validated_;

    bool has_responded_ = false;
    base::CancelableOnceCallback<void()> on_timeout_;
    base::WeakPtrFactory<ValidationRequest> weak_factory_{this};
    DISALLOW_COPY_AND_ASSIGN(ValidationRequest);
  };

  friend class AutofillProfileValidatorTest;

  // Returns whether the rules for the specified |region_code| is loaded.
  bool AreRulesLoadedForRegion(const std::string& region_code);

  // Implementation of the LoadRulesListener interface. Called when the address
  // rules for the |region_code| have finished loading.
  void OnAddressValidationRulesLoaded(const std::string& region_code,
                                      bool success) override;

  // A map of the region code and the pending requests for that region code.
  std::map<std::string, std::vector<std::unique_ptr<ValidationRequest>>>
      pending_requests_;

  DISALLOW_COPY_AND_ASSIGN(AutofillProfileValidator);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_PROFILE_VALIDATOR_H_
