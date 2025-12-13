// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_UTILS_TEST_PROFILES_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_UTILS_TEST_PROFILES_H_

#include <ranges>

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_test_utils.h"

namespace autofill::test {

// Defines the |value| and |verification_status| for a specific Autofill
// |field_type|
struct ProfileTestData {
  FieldType field_type;
  std::string value;
  VerificationStatus verification_status = VerificationStatus::kNoStatus;
};

// Set the values and verification statuses for the field types in
// |profile_test_data|. If |finalize|, the finalization routine of the
// AutofillProfile is called subsequently.
void SetProfileTestValues(AutofillProfile* profile,
                          const std::vector<ProfileTestData>& profile_test_data,
                          bool finalize = true);

// Copies the GUID from |from| to |to|.
void CopyGUID(const AutofillProfile& from, AutofillProfile* to);

// Convenience function to set the test data but with a verification status that
// is set to |kObserved| for all values.
void SetProfileObservedTestValues(AutofillProfile* profile,
                                  const std::vector<ProfileTestData>& test_data,
                                  bool finalize = true);

// A standard AutofillProfile. All subsequent profiles are defined with respect
// to this one.
AutofillProfile StandardProfile(
    AddressCountryCode country_code = AddressCountryCode("US"));

// This profile is similar to the standard profile defined above.
// Here, the verification status for the name is 'only' observed. When merged
// with the `StandardProfile()`, this should result in a silent update.
AutofillProfile UpdateableStandardProfile();

// This profile is similar to the standard profile defined above.
// This profile is both lacking a city and a ZIP code and should be merged with
// the `StandardProfile(}`.
AutofillProfile SubsetOfStandardProfile();

// This profile that is not similar to the standard profile.
AutofillProfile DifferentFromStandardProfile();

// Basic profile with alternative name in Hiragana and the address related field
// values missing.
AutofillProfile HiraganaProfile();

// Copy of `HiraganaProfile()` with address fields set.
AutofillProfile ExtendedHiraganaProfile();

// Copy of `HiraganaProfile()` but alternative name is in Katakana.
AutofillProfile KatakanaProfile1();

// A new Katakana profile
AutofillProfile KatakanaProfile2();

// A new `kAccountNameEmail` profile
AutofillProfile AccountNameEmailProfile();

// A superset profile of a profile acquired from the `AccountNameEmailProfile`
// method, with `kAccount` `record_type`.
AutofillProfile AccountNameEmailProfileSuperset();

// A new profile that contains only address data.
AutofillProfile OnlyAddressProfile(AutofillProfile::RecordType record_type =
                                       AutofillProfile::RecordType::kAccount);

// A new profile obtained through repeated merging of data from `profiles`, with
// a specific record type.
AutofillProfile SupersetProfileOf(
    base::span<const AutofillProfile> profiles,
    std::string_view app_locale,
    AutofillProfile::RecordType type = AutofillProfile::RecordType::kAccount,
    AddressCountryCode country_code = AddressCountryCode("US"));
}  // namespace autofill::test

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_UTILS_TEST_PROFILES_H_
