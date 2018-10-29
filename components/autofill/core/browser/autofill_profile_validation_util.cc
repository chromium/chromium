// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_profile_validation_util.h"

#include <string>
#include <utility>

#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/address_i18n.h"
#include "components/autofill/core/browser/country_data.h"
#include "components/autofill/core/browser/validation.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_data.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_validator.h"
#include "third_party/libphonenumber/dist/cpp/src/phonenumbers/phonenumberutil.h"

namespace autofill {

namespace {

using ::i18n::addressinput::COUNTRY;
using ::i18n::addressinput::ADMIN_AREA;
using ::i18n::addressinput::LOCALITY;
using ::i18n::addressinput::DEPENDENT_LOCALITY;
using ::i18n::addressinput::SORTING_CODE;
using ::i18n::addressinput::POSTAL_CODE;
using ::i18n::addressinput::STREET_ADDRESS;
using ::i18n::addressinput::RECIPIENT;

using ::i18n::addressinput::AddressData;
using ::i18n::addressinput::AddressField;
using ::i18n::addressinput::AddressProblem;
using ::i18n::addressinput::FieldProblemMap;

using ::i18n::addressinput::INVALID_FORMAT;
using ::i18n::addressinput::MISMATCHING_VALUE;
using ::i18n::addressinput::MISSING_REQUIRED_FIELD;
using ::i18n::addressinput::UNEXPECTED_FIELD;
using ::i18n::addressinput::UNKNOWN_VALUE;

using ::i18n::phonenumbers::PhoneNumberUtil;

const AddressField kFields[] = {COUNTRY, ADMIN_AREA, LOCALITY,
                                DEPENDENT_LOCALITY, POSTAL_CODE};
const AddressProblem kProblems[] = {UNEXPECTED_FIELD, MISSING_REQUIRED_FIELD,
                                    UNKNOWN_VALUE, INVALID_FORMAT,
                                    MISMATCHING_VALUE};

// If the |address_field| is valid, set the validity state of the
// |address_field| in the |profile| to the |state| and return true.
// Otherwise, return false.
bool SetValidityStateForAddressField(AutofillProfile* profile,
                                     AddressField address_field,
                                     AutofillProfile::ValidityState state) {
  ServerFieldType server_field = i18n::TypeForField(address_field,
                                                    /*billing=*/false);
  if (server_field == UNKNOWN_TYPE)
    return false;
  DCHECK(profile);
  profile->SetValidityState(server_field, state, AutofillProfile::CLIENT);
  return true;
}

// Set the validity state of all address fields in the |profile| to |state|.
void SetAllAddressValidityStates(AutofillProfile* profile,
                                 AutofillProfile::ValidityState state) {
  DCHECK(profile);
  for (auto field : kFields)
    SetValidityStateForAddressField(profile, field, state);
}

// Returns all relevant pairs of (field, problem), where field is in
// |kFields|, and problem is in |kProblems|.
FieldProblemMap* CreateFieldProblemMap() {
  FieldProblemMap* filter = new FieldProblemMap();
  for (auto field : kFields) {
    for (auto problem : kProblems) {
      filter->insert(std::make_pair(field, problem));
    }
  }
  return filter;
}

// GetFilter() will make sure that the validation only returns problems that
// are relevant.
const FieldProblemMap* GetFilter() {
  static const FieldProblemMap* const filter = CreateFieldProblemMap();
  return filter;
}

// Initializes |address| data from the address info in the |profile|.
void InitializeAddressFromProfile(const AutofillProfile& profile,
                                  AddressData* address) {
  address->region_code =
      base::UTF16ToUTF8(profile.GetRawInfo(ADDRESS_HOME_COUNTRY));
  address->administrative_area =
      base::UTF16ToUTF8(profile.GetRawInfo(ADDRESS_HOME_STATE));
  address->locality = base::UTF16ToUTF8(profile.GetRawInfo(ADDRESS_HOME_CITY));
  address->dependent_locality =
      base::UTF16ToUTF8(profile.GetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY));
  // The validation is case insensitive, and the postal codes are always upper
  // case.
  address->postal_code = base::UTF16ToUTF8(
      base::i18n::ToUpper(profile.GetRawInfo(ADDRESS_HOME_ZIP)));
}

void SetEmptyValidityIfEmpty(AutofillProfile* profile) {
  if (profile->GetRawInfo(ADDRESS_HOME_COUNTRY).empty())
    profile->SetValidityState(ADDRESS_HOME_COUNTRY, AutofillProfile::EMPTY,
                              AutofillProfile::CLIENT);
  if (profile->GetRawInfo(ADDRESS_HOME_STATE).empty())
    profile->SetValidityState(ADDRESS_HOME_STATE, AutofillProfile::EMPTY,
                              AutofillProfile::CLIENT);
  if (profile->GetRawInfo(ADDRESS_HOME_CITY).empty())
    profile->SetValidityState(ADDRESS_HOME_CITY, AutofillProfile::EMPTY,
                              AutofillProfile::CLIENT);
  if (profile->GetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY).empty())
    profile->SetValidityState(ADDRESS_HOME_DEPENDENT_LOCALITY,
                              AutofillProfile::EMPTY, AutofillProfile::CLIENT);
  if (profile->GetRawInfo(ADDRESS_HOME_ZIP).empty())
    profile->SetValidityState(ADDRESS_HOME_ZIP, AutofillProfile::EMPTY,
                              AutofillProfile::CLIENT);
}

void SetInvalidIfUnvalidated(AutofillProfile* profile) {
  if (profile->GetValidityState(ADDRESS_HOME_COUNTRY,
                                AutofillProfile::CLIENT) ==
      AutofillProfile::UNVALIDATED) {
    profile->SetValidityState(ADDRESS_HOME_COUNTRY, AutofillProfile::INVALID,
                              AutofillProfile::CLIENT);
  }

  if (profile->GetValidityState(ADDRESS_HOME_STATE, AutofillProfile::CLIENT) ==
      AutofillProfile::UNVALIDATED) {
    profile->SetValidityState(ADDRESS_HOME_STATE, AutofillProfile::INVALID,
                              AutofillProfile::CLIENT);
  }

  if (profile->GetValidityState(ADDRESS_HOME_CITY, AutofillProfile::CLIENT) ==
      AutofillProfile::UNVALIDATED) {
    profile->SetValidityState(ADDRESS_HOME_CITY, AutofillProfile::INVALID,
                              AutofillProfile::CLIENT);
  }

  if (profile->GetValidityState(ADDRESS_HOME_DEPENDENT_LOCALITY,
                                AutofillProfile::CLIENT) ==
      AutofillProfile::UNVALIDATED) {
    profile->SetValidityState(ADDRESS_HOME_DEPENDENT_LOCALITY,
                              AutofillProfile::INVALID,
                              AutofillProfile::CLIENT);
  }

  if (profile->GetValidityState(ADDRESS_HOME_ZIP, AutofillProfile::CLIENT) ==
      AutofillProfile::UNVALIDATED) {
    profile->SetValidityState(ADDRESS_HOME_ZIP, AutofillProfile::INVALID,
                              AutofillProfile::CLIENT);
  }
}

void MaybeApplyValidToFields(AutofillProfile* profile) {
  // The metadata works from top to bottom. Therefore, a so far UNVALIDATED
  // subregion can only be validated if its super-region is VALID. In  this
  // case, it's VALID if it has not been marked as INVALID or EMPTY.

  if (profile->GetValidityState(ADDRESS_HOME_STATE, AutofillProfile::CLIENT) ==
      AutofillProfile::UNVALIDATED) {
    profile->SetValidityState(ADDRESS_HOME_STATE, AutofillProfile::VALID,
                              AutofillProfile::CLIENT);
  }

  if (profile->GetValidityState(ADDRESS_HOME_CITY, AutofillProfile::CLIENT) ==
          AutofillProfile::UNVALIDATED &&
      profile->GetValidityState(ADDRESS_HOME_STATE, AutofillProfile::CLIENT) ==
          AutofillProfile::VALID) {
    profile->SetValidityState(ADDRESS_HOME_CITY, AutofillProfile::VALID,
                              AutofillProfile::CLIENT);
  }

  if (profile->GetValidityState(ADDRESS_HOME_DEPENDENT_LOCALITY,
                                AutofillProfile::CLIENT) ==
          AutofillProfile::UNVALIDATED &&
      profile->GetValidityState(ADDRESS_HOME_CITY, AutofillProfile::CLIENT) ==
          AutofillProfile::VALID) {
    profile->SetValidityState(ADDRESS_HOME_DEPENDENT_LOCALITY,
                              AutofillProfile::VALID, AutofillProfile::CLIENT);
  }

  // ZIP only depends on COUNTRY. If it's not so far marked as INVALID or EMPTY,
  // then it's VALID.
  if (profile->GetValidityState(ADDRESS_HOME_ZIP, AutofillProfile::CLIENT) ==
      AutofillProfile::UNVALIDATED) {
    profile->SetValidityState(ADDRESS_HOME_ZIP, AutofillProfile::VALID,
                              AutofillProfile::CLIENT);
  }
}

void ApplyValidOnlyIfAllChildrenNotInvalid(AutofillProfile* profile) {
  if (profile->GetValidityState(ADDRESS_HOME_STATE, AutofillProfile::CLIENT) ==
          AutofillProfile::INVALID &&
      profile->GetValidityState(ADDRESS_HOME_ZIP, AutofillProfile::CLIENT) ==
          AutofillProfile::INVALID) {
    profile->SetValidityState(ADDRESS_HOME_COUNTRY, AutofillProfile::INVALID,
                              AutofillProfile::CLIENT);
  }

  if (profile->GetValidityState(ADDRESS_HOME_CITY, AutofillProfile::CLIENT) ==
      AutofillProfile::INVALID) {
    profile->SetValidityState(ADDRESS_HOME_STATE, AutofillProfile::INVALID,
                              AutofillProfile::CLIENT);
  }

  if (profile->GetValidityState(ADDRESS_HOME_DEPENDENT_LOCALITY,
                                AutofillProfile::CLIENT) ==
      AutofillProfile::INVALID) {
    profile->SetValidityState(ADDRESS_HOME_CITY, AutofillProfile::INVALID,
                              AutofillProfile::CLIENT);
  }
}

}  // namespace

namespace profile_validation_util {

void ValidateProfile(AutofillProfile* profile,
                     AddressValidator* address_validator) {
  DCHECK(address_validator);
  DCHECK(profile);
  ValidateAddressStrictly(profile, address_validator);
  ValidatePhoneNumber(profile);
  ValidateEmailAddress(profile);
}

AddressValidator::Status ValidateAddress(AutofillProfile* profile,
                                         AddressValidator* address_validator) {
  DCHECK(address_validator);
  DCHECK(profile);

  SetAllAddressValidityStates(profile, AutofillProfile::UNVALIDATED);

  if (!base::ContainsValue(
          CountryDataMap::GetInstance()->country_codes(),
          base::UTF16ToUTF8(profile->GetRawInfo(ADDRESS_HOME_COUNTRY)))) {
    // If the country code is not in the database, the country code and the
    // profile are invalid, and other fields cannot be validated, because it is
    // unclear which, if any, rule should apply.
    SetValidityStateForAddressField(profile, COUNTRY, AutofillProfile::INVALID);
    SetEmptyValidityIfEmpty(profile);
    return AddressValidator::SUCCESS;
  }

  // The COUNTRY was already listed in the CountryDataMap, therefore it's valid.
  SetValidityStateForAddressField(profile, COUNTRY, AutofillProfile::VALID);

  AddressData address;
  InitializeAddressFromProfile(*profile, &address);
  FieldProblemMap problems;
  // status denotes if the rule was successfully loaded before validation.
  AddressValidator::Status status =
      address_validator->ValidateAddress(address, GetFilter(), &problems);

  for (auto problem : problems)
    SetValidityStateForAddressField(profile, problem.first,
                                    AutofillProfile::INVALID);

  SetEmptyValidityIfEmpty(profile);

  // Fields (except COUNTRY) could be VALID, only if the rules were available.
  if (status == AddressValidator::SUCCESS)
    MaybeApplyValidToFields(profile);

  return status;
}

void ValidateAddressStrictly(AutofillProfile* profile,
                             AddressValidator* address_validator) {
  DCHECK(address_validator);
  DCHECK(profile);

  // If the rules were loaded successfully, add a second layer of validation:
  // 1. For a field to stay valid after the first run, all the fields that
  // depend on that field for validation need to not be invalid on the first
  // run, otherwise there is a chance that the data on that field was also
  // invalid (incorrect.)
  // Example: 1225 Notre-Dame Ouest, Montreal, Quebec, H3C 2A3, United States.
  // A human validator can see that the country is most probably the invalid
  // field. The first step helps us validate the rules interdependently.
  // 2. All the address fields that could not be validated (UNVALIDATED),
  // should be considered as invalid.

  if (ValidateAddress(profile, address_validator) ==
      AddressValidator::SUCCESS) {
    ApplyValidOnlyIfAllChildrenNotInvalid(profile);
    SetInvalidIfUnvalidated(profile);
  }
}

void ValidateEmailAddress(AutofillProfile* profile) {
  const base::string16& email = profile->GetRawInfo(EMAIL_ADDRESS);
  if (email.empty()) {
    profile->SetValidityState(EMAIL_ADDRESS, AutofillProfile::EMPTY,
                              AutofillProfile::CLIENT);
    return;
  }

  profile->SetValidityState(EMAIL_ADDRESS,
                            autofill::IsValidEmailAddress(email)
                                ? AutofillProfile::VALID
                                : AutofillProfile::INVALID,
                            AutofillProfile::CLIENT);
}

void ValidatePhoneNumber(AutofillProfile* profile) {
  const std::string& phone_number =
      base::UTF16ToUTF8(profile->GetRawInfo(PHONE_HOME_WHOLE_NUMBER));
  if (phone_number.empty()) {
    profile->SetValidityState(PHONE_HOME_WHOLE_NUMBER, AutofillProfile::EMPTY,
                              AutofillProfile::CLIENT);
    return;
  }

  const std::string& country_code =
      base::UTF16ToUTF8(profile->GetRawInfo(ADDRESS_HOME_COUNTRY));
  if (!base::ContainsValue(CountryDataMap::GetInstance()->country_codes(),
                           country_code)) {
    // If the country code is not in the database, the phone number cannot be
    // validated.
    profile->SetValidityState(PHONE_HOME_WHOLE_NUMBER,
                              AutofillProfile::UNVALIDATED,
                              AutofillProfile::CLIENT);
    return;
  }

  PhoneNumberUtil* phone_util = PhoneNumberUtil::GetInstance();
  profile->SetValidityState(
      PHONE_HOME_WHOLE_NUMBER,
      phone_util->IsPossibleNumberForString(phone_number, country_code)
          ? AutofillProfile::VALID
          : AutofillProfile::INVALID,
      AutofillProfile::CLIENT);
}

}  // namespace profile_validation_util
}  // namespace autofill
