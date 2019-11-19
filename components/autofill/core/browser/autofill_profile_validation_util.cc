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
#include "components/autofill/core/browser/geo/address_i18n.h"
#include "components/autofill/core/browser/geo/country_data.h"
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

using i18nAddressData = ::i18n::addressinput::AddressData;
using i18nAddressField = ::i18n::addressinput::AddressField;
using i18nAddressProblem = ::i18n::addressinput::AddressProblem;
using i18nFieldProblemMap = ::i18n::addressinput::FieldProblemMap;

using ::i18n::addressinput::INVALID_FORMAT;
using ::i18n::addressinput::MISMATCHING_VALUE;
using ::i18n::addressinput::MISSING_REQUIRED_FIELD;
using ::i18n::addressinput::UNEXPECTED_FIELD;
using ::i18n::addressinput::UNKNOWN_VALUE;
using ::i18n::addressinput::UNSUPPORTED_FIELD;

using ::i18n::phonenumbers::PhoneNumberUtil;

const i18nAddressField kFields[] = {COUNTRY, ADMIN_AREA, LOCALITY,
                                    DEPENDENT_LOCALITY, POSTAL_CODE};
const i18nAddressProblem kProblems[] = {
    UNEXPECTED_FIELD, MISSING_REQUIRED_FIELD, UNKNOWN_VALUE,
    INVALID_FORMAT,   MISMATCHING_VALUE,      UNSUPPORTED_FIELD};

// If the |address_field| is valid, set the validity state of the
// |address_field| in the |profile| to the |state| and return true.
// Otherwise, return false.
bool SetValidityStateForAddressField(const AutofillProfile* profile,
                                     i18nAddressField address_field,
                                     AutofillDataModel::ValidityState state) {
  ServerFieldType server_field = i18n::TypeForField(address_field,
                                                    /*billing=*/false);
  if (server_field == UNKNOWN_TYPE)
    return false;
  DCHECK(profile);
  profile->SetValidityState(server_field, state, AutofillDataModel::CLIENT);
  return true;
}

// Set the validity state of all address fields in the |profile| to |state|.
void SetAllAddressValidityStates(const AutofillProfile* profile,
                                 AutofillDataModel::ValidityState state) {
  DCHECK(profile);
  for (auto field : kFields)
    SetValidityStateForAddressField(profile, field, state);
}

// Returns all relevant pairs of (field, problem), where field is in
// |kFields|, and problem is in |kProblems|.
i18nFieldProblemMap* CreateFieldProblemMap() {
  i18nFieldProblemMap* filter = new i18nFieldProblemMap();
  for (auto field : kFields) {
    for (auto problem : kProblems) {
      filter->insert(std::make_pair(field, problem));
    }
  }
  return filter;
}

// GetFilter() will make sure that the validation only returns problems that
// are relevant.
const i18nFieldProblemMap* GetFilter() {
  static const i18nFieldProblemMap* const filter = CreateFieldProblemMap();
  return filter;
}

// Initializes |address| data from the address info in the |profile|.
void InitializeAddressFromProfile(const AutofillProfile& profile,
                                  i18nAddressData* address) {
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

void SetEmptyValidityIfEmpty(const AutofillProfile* profile) {
  if (profile->GetRawInfo(ADDRESS_HOME_COUNTRY).empty())
    profile->SetValidityState(ADDRESS_HOME_COUNTRY, AutofillDataModel::EMPTY,
                              AutofillDataModel::CLIENT);
  if (profile->GetRawInfo(ADDRESS_HOME_STATE).empty())
    profile->SetValidityState(ADDRESS_HOME_STATE, AutofillDataModel::EMPTY,
                              AutofillDataModel::CLIENT);
  if (profile->GetRawInfo(ADDRESS_HOME_CITY).empty())
    profile->SetValidityState(ADDRESS_HOME_CITY, AutofillDataModel::EMPTY,
                              AutofillDataModel::CLIENT);
  if (profile->GetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY).empty())
    profile->SetValidityState(ADDRESS_HOME_DEPENDENT_LOCALITY,
                              AutofillDataModel::EMPTY,
                              AutofillDataModel::CLIENT);
  if (profile->GetRawInfo(ADDRESS_HOME_ZIP).empty())
    profile->SetValidityState(ADDRESS_HOME_ZIP, AutofillDataModel::EMPTY,
                              AutofillDataModel::CLIENT);
}

void SetInvalidIfUnvalidated(const AutofillProfile* profile) {
  if (profile->GetValidityState(ADDRESS_HOME_COUNTRY,
                                AutofillDataModel::CLIENT) ==
      AutofillDataModel::UNVALIDATED) {
    profile->SetValidityState(ADDRESS_HOME_COUNTRY, AutofillDataModel::INVALID,
                              AutofillDataModel::CLIENT);
  }

  if (profile->GetValidityState(ADDRESS_HOME_STATE,
                                AutofillDataModel::CLIENT) ==
      AutofillDataModel::UNVALIDATED) {
    profile->SetValidityState(ADDRESS_HOME_STATE, AutofillDataModel::INVALID,
                              AutofillDataModel::CLIENT);
  }

  if (profile->GetValidityState(ADDRESS_HOME_CITY, AutofillDataModel::CLIENT) ==
      AutofillDataModel::UNVALIDATED) {
    profile->SetValidityState(ADDRESS_HOME_CITY, AutofillDataModel::INVALID,
                              AutofillDataModel::CLIENT);
  }

  if (profile->GetValidityState(ADDRESS_HOME_DEPENDENT_LOCALITY,
                                AutofillDataModel::CLIENT) ==
      AutofillDataModel::UNVALIDATED) {
    profile->SetValidityState(ADDRESS_HOME_DEPENDENT_LOCALITY,
                              AutofillDataModel::INVALID,
                              AutofillDataModel::CLIENT);
  }

  if (profile->GetValidityState(ADDRESS_HOME_ZIP, AutofillDataModel::CLIENT) ==
      AutofillDataModel::UNVALIDATED) {
    profile->SetValidityState(ADDRESS_HOME_ZIP, AutofillDataModel::INVALID,
                              AutofillDataModel::CLIENT);
  }
}

void MaybeApplyValidToFields(const AutofillProfile* profile) {
  // The metadata works from top to bottom. Therefore, a so far UNVALIDATED
  // subregion can only be validated if its super-region is VALID. In  this
  // case, it's VALID if it has not been marked as INVALID or EMPTY.

  if (profile->GetValidityState(ADDRESS_HOME_STATE,
                                AutofillDataModel::CLIENT) ==
      AutofillDataModel::UNVALIDATED) {
    profile->SetValidityState(ADDRESS_HOME_STATE, AutofillDataModel::VALID,
                              AutofillDataModel::CLIENT);
  }

  if (profile->GetValidityState(ADDRESS_HOME_CITY, AutofillDataModel::CLIENT) ==
          AutofillDataModel::UNVALIDATED &&
      profile->GetValidityState(ADDRESS_HOME_STATE,
                                AutofillDataModel::CLIENT) ==
          AutofillDataModel::VALID) {
    profile->SetValidityState(ADDRESS_HOME_CITY, AutofillDataModel::VALID,
                              AutofillDataModel::CLIENT);
  }

  if (profile->GetValidityState(ADDRESS_HOME_DEPENDENT_LOCALITY,
                                AutofillDataModel::CLIENT) ==
          AutofillDataModel::UNVALIDATED &&
      profile->GetValidityState(ADDRESS_HOME_CITY, AutofillDataModel::CLIENT) ==
          AutofillDataModel::VALID) {
    profile->SetValidityState(ADDRESS_HOME_DEPENDENT_LOCALITY,
                              AutofillDataModel::VALID,
                              AutofillDataModel::CLIENT);
  }

  // ZIP only depends on COUNTRY. If it's not so far marked as INVALID or EMPTY,
  // then it's VALID.
  if (profile->GetValidityState(ADDRESS_HOME_ZIP, AutofillDataModel::CLIENT) ==
      AutofillDataModel::UNVALIDATED) {
    profile->SetValidityState(ADDRESS_HOME_ZIP, AutofillDataModel::VALID,
                              AutofillDataModel::CLIENT);
  }
}

void ApplyValidOnlyIfAllChildrenNotInvalid(const AutofillProfile* profile) {
  if (profile->GetValidityState(ADDRESS_HOME_STATE,
                                AutofillDataModel::CLIENT) ==
          AutofillDataModel::INVALID &&
      profile->GetValidityState(ADDRESS_HOME_ZIP, AutofillDataModel::CLIENT) ==
          AutofillDataModel::INVALID) {
    profile->SetValidityState(ADDRESS_HOME_COUNTRY, AutofillDataModel::INVALID,
                              AutofillDataModel::CLIENT);
  }

  if (profile->GetValidityState(ADDRESS_HOME_CITY, AutofillDataModel::CLIENT) ==
      AutofillDataModel::INVALID) {
    profile->SetValidityState(ADDRESS_HOME_STATE, AutofillDataModel::INVALID,
                              AutofillDataModel::CLIENT);
  }

  if (profile->GetValidityState(ADDRESS_HOME_DEPENDENT_LOCALITY,
                                AutofillDataModel::CLIENT) ==
      AutofillDataModel::INVALID) {
    profile->SetValidityState(ADDRESS_HOME_CITY, AutofillDataModel::INVALID,
                              AutofillDataModel::CLIENT);
  }
}

}  // namespace

namespace profile_validation_util {

void ValidateProfile(const AutofillProfile* profile,
                     AddressValidator* address_validator) {
  DCHECK(address_validator);
  DCHECK(profile);
  ValidateAddressStrictly(profile, address_validator);
  ValidatePhoneNumber(profile);
  ValidateEmailAddress(profile);
}

AddressValidator::Status ValidateAddress(const AutofillProfile* profile,
                                         AddressValidator* address_validator) {
  DCHECK(address_validator);
  DCHECK(profile);

  SetAllAddressValidityStates(profile, AutofillDataModel::UNVALIDATED);

  if (!base::Contains(
          CountryDataMap::GetInstance()->country_codes(),
          base::UTF16ToUTF8(profile->GetRawInfo(ADDRESS_HOME_COUNTRY)))) {
    // If the country code is not in the database, the country code and the
    // profile are invalid, and other fields cannot be validated, because it is
    // unclear which, if any, rule should apply.
    SetValidityStateForAddressField(profile, COUNTRY,
                                    AutofillDataModel::INVALID);
    SetEmptyValidityIfEmpty(profile);
    return AddressValidator::SUCCESS;
  }

  // The COUNTRY was already listed in the CountryDataMap, therefore it's valid.
  SetValidityStateForAddressField(profile, COUNTRY, AutofillDataModel::VALID);

  i18nAddressData address;
  InitializeAddressFromProfile(*profile, &address);
  i18nFieldProblemMap problems;
  // status denotes if the rule was successfully loaded before validation.
  AddressValidator::Status status =
      address_validator->ValidateAddress(address, GetFilter(), &problems);

  // The address fields for which validation is not supported by the metadata
  // will be marked as UNSUPPORTED_FIELDs. These fields should be treated like
  // VALID fields to stay consistent. INVALID_FORMATs, MISMATCHING_VALUEs or
  // UNKNOWN_VALUEs are INVALID. MISSING_REQUIRED_FIELD would be marked as EMPTY
  // along other empty fields. UNEXPECTED_FIELD would mean that there is also no
  // metadata for validation, therefore, they are also UNSUPPORTED_FIELDs, and
  // thus they would be treated as VALID fields.
  for (auto problem : problems) {
    if (problem.second == UNSUPPORTED_FIELD) {
      SetValidityStateForAddressField(profile, problem.first,
                                      AutofillDataModel::VALID);

    } else if (problem.second == INVALID_FORMAT ||
               problem.second == MISMATCHING_VALUE ||
               problem.second == UNKNOWN_VALUE) {
      SetValidityStateForAddressField(profile, problem.first,
                                      AutofillDataModel::INVALID);
    }
  }

  SetEmptyValidityIfEmpty(profile);

  // Fields (except COUNTRY) could be VALID, only if the rules were available.
  if (status == AddressValidator::SUCCESS)
    MaybeApplyValidToFields(profile);

  return status;
}

void ValidateAddressStrictly(const AutofillProfile* profile,
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

void ValidateEmailAddress(const AutofillProfile* profile) {
  const base::string16& email = profile->GetRawInfo(EMAIL_ADDRESS);
  if (email.empty()) {
    profile->SetValidityState(EMAIL_ADDRESS, AutofillDataModel::EMPTY,
                              AutofillDataModel::CLIENT);
    return;
  }

  profile->SetValidityState(EMAIL_ADDRESS,
                            autofill::IsValidEmailAddress(email)
                                ? AutofillDataModel::VALID
                                : AutofillDataModel::INVALID,
                            AutofillDataModel::CLIENT);
}

void ValidatePhoneNumber(const AutofillProfile* profile) {
  const std::string& phone_number =
      base::UTF16ToUTF8(profile->GetRawInfo(PHONE_HOME_WHOLE_NUMBER));
  if (phone_number.empty()) {
    profile->SetValidityState(PHONE_HOME_WHOLE_NUMBER, AutofillDataModel::EMPTY,
                              AutofillDataModel::CLIENT);
    return;
  }

  const std::string& country_code =
      base::UTF16ToUTF8(profile->GetRawInfo(ADDRESS_HOME_COUNTRY));
  if (!base::Contains(CountryDataMap::GetInstance()->country_codes(),
                      country_code)) {
    // If the country code is not in the database, the phone number cannot be
    // validated.
    profile->SetValidityState(PHONE_HOME_WHOLE_NUMBER,
                              AutofillDataModel::UNVALIDATED,
                              AutofillDataModel::CLIENT);
    return;
  }

  PhoneNumberUtil* phone_util = PhoneNumberUtil::GetInstance();
  profile->SetValidityState(
      PHONE_HOME_WHOLE_NUMBER,
      phone_util->IsPossibleNumberForString(phone_number, country_code)
          ? AutofillDataModel::VALID
          : AutofillDataModel::INVALID,
      AutofillDataModel::CLIENT);
}

}  // namespace profile_validation_util
}  // namespace autofill
