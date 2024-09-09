// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/phone_number.h"

#include <limits.h>

#include <algorithm>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/data_model_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/geo/phone_number_i18n.h"
#include "components/autofill/core/common/autofill_features.h"
#include "third_party/abseil-cpp/absl/strings/ascii.h"

namespace autofill {

namespace {

// Returns the region code for this phone number, which is an ISO 3166 2-letter
// country code.  The returned value is based on the |profile|; if the |profile|
// does not have a country code associated with it, falls back to the country
// code corresponding to the |app_locale|.
std::string GetRegion(const AutofillProfile& profile,
                      const std::string& app_locale) {
  std::u16string country_code = profile.GetRawInfo(ADDRESS_HOME_COUNTRY);
  if (!country_code.empty())
    return base::UTF16ToASCII(country_code);

  return AutofillCountry::CountryCodeForLocale(app_locale);
}

}  // namespace

PhoneNumber::PhoneNumber(const AutofillProfile* profile) : profile_(profile) {}

PhoneNumber::PhoneNumber(const PhoneNumber& number) : profile_(nullptr) {
  *this = number;
}

PhoneNumber::~PhoneNumber() = default;

PhoneNumber& PhoneNumber::operator=(const PhoneNumber& number) {
  if (this == &number)
    return *this;

  number_ = number.number_;
  profile_ = number.profile_;
  cached_parsed_phone_ = number.cached_parsed_phone_;
  return *this;
}

bool PhoneNumber::operator==(const PhoneNumber& other) const {
  if (this == &other)
    return true;

  return number_ == other.number_ && profile_ == other.profile_;
}

void PhoneNumber::GetSupportedTypes(FieldTypeSet* supported_types) const {
  supported_types->insert(PHONE_HOME_WHOLE_NUMBER);
  supported_types->insert(PHONE_HOME_NUMBER);
  supported_types->insert(PHONE_HOME_NUMBER_PREFIX);
  supported_types->insert(PHONE_HOME_NUMBER_SUFFIX);
  supported_types->insert(PHONE_HOME_CITY_CODE);
  supported_types->insert(PHONE_HOME_CITY_AND_NUMBER);
  supported_types->insert(PHONE_HOME_COUNTRY_CODE);
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableSupportForPhoneNumberTrunkTypes)) {
    supported_types->insert(PHONE_HOME_CITY_CODE_WITH_TRUNK_PREFIX);
    supported_types->insert(PHONE_HOME_CITY_AND_NUMBER_WITHOUT_TRUNK_PREFIX);
  }
}

std::u16string PhoneNumber::GetRawInfo(FieldType type) const {
  DCHECK_EQ(FieldTypeGroup::kPhone, GroupTypeOfFieldType(type));
  if (type == PHONE_HOME_WHOLE_NUMBER)
    return number_;

  // Only the whole number is available as raw data.  All of the other types are
  // parsed from this raw info, and parsing requires knowledge of the phone
  // number's region, which is only available via GetInfo().
  return std::u16string();
}

void PhoneNumber::SetRawInfoWithVerificationStatus(FieldType type,
                                                   const std::u16string& value,
                                                   VerificationStatus status) {
  DCHECK_EQ(FieldTypeGroup::kPhone, GroupTypeOfFieldType(type));
  if (type != PHONE_HOME_WHOLE_NUMBER) {
    // Only full phone numbers should be set directly. The browser is
    // intentionally caused to crash to prevent all users from setting raw info
    // to the non-storable fields.
    NOTREACHED();
  }

  number_ = value;

  // Invalidate the cached number.
  cached_parsed_phone_ = i18n::PhoneObject();
}

void PhoneNumber::GetMatchingTypesWithProfileSources(
    const std::u16string& text,
    const std::string& app_locale,
    FieldTypeSet* matching_types,
    PossibleProfileValueSources* profile_value_sources) const {
  // Strip the common phone number non numerical characters before calling the
  // base matching type function. For example, the |text| "(514) 121-1523"
  // would become the stripped text "5141211523". Since the base matching
  // function only does simple canonicalization to match against the stored
  // data, some domain specific cases will be covered below.
  std::u16string stripped_text = text;
  base::RemoveChars(stripped_text, u" .()-", &stripped_text);
  FormGroup::GetMatchingTypesWithProfileSources(
      stripped_text, app_locale, matching_types, profile_value_sources);

  // TODO(crbug.com/41236729): Investigate the use of PhoneNumberUtil when
  // matching phone numbers for upload.
  // If there is not already a match for PHONE_HOME_WHOLE_NUMBER, normalize the
  // |text| based on the app_locale before comparing it to the whole number. For
  // example, the France number "33 2 49 19 70 70" would be normalized to
  // "+33249197070" whereas the US number "+1 (234) 567-8901" would be
  // normalized to "12345678901".
  if (!matching_types->contains(PHONE_HOME_WHOLE_NUMBER)) {
    std::u16string whole_number = GetInfo(PHONE_HOME_WHOLE_NUMBER, app_locale);
    if (!whole_number.empty()) {
      std::u16string normalized_number =
          i18n::NormalizePhoneNumber(text, GetRegion(*profile_, app_locale));
      if (normalized_number == whole_number)
        matching_types->insert(PHONE_HOME_WHOLE_NUMBER);
    }
  }

  // `PHONE_HOME_COUNTRY_CODE` is added to the set of the `matching_types` when
  // the digits extracted from the `stripped_text` match the `country_code`.
  std::u16string candidate =
      data_util::FindPossiblePhoneCountryCode(stripped_text);
  std::u16string country_code = GetInfo(PHONE_HOME_COUNTRY_CODE, app_locale);
  if (candidate.size() > 0 && candidate == country_code)
    matching_types->insert(PHONE_HOME_COUNTRY_CODE);

  // The following pairs of types coincide in countries without trunk prefixes:
  // - PHONE_HOME_CITY_CODE, PHONE_HOME_CITY_CODE_WITH_TRUNK_PREFIX
  // - PHONE_HOME_CITY_AND_NUMBER,
  //   PHONE_HOME_CITY_AND_NUMBER_WITHOUT_TRUNK_PREFIX
  // We explicitly keep both matches, as the type prediction doesn't make a
  // difference for these countries. Votes from other countries can then tip
  // the counts to the right type.
  // This is only applicable when
  // `kAutofillEnableSupportForPhoneNumberTrunkTypes` is enabled.
  //
  // When the phone number is stored without a country code,
  // PHONE_HOME_WHOLE_NUMBER and PHONE_HOME_CITY_AND_NUMBER coincide (and
  // potentially PHONE_HOME_CITY_AND_NUMBER_WITHOUT_TRUNK_PREFIX too, as
  // indicated above).
  // Since PHONE_HOME_WHOLE_NUMBER is meant to represent an international
  // number, it is not voted in this case.
  if (matching_types->contains(PHONE_HOME_WHOLE_NUMBER) &&
      matching_types->contains_any(
          {PHONE_HOME_CITY_AND_NUMBER,
           PHONE_HOME_CITY_AND_NUMBER_WITHOUT_TRUNK_PREFIX})) {
    matching_types->erase(PHONE_HOME_WHOLE_NUMBER);
  }
}

// Normalize phones if |type| is a whole number:
//   (650)2345678 -> 6502345678
//   1-800-FLOWERS -> 18003569377
// If the phone cannot be normalized, returns the stored value verbatim.
std::u16string PhoneNumber::GetInfoImpl(const AutofillType& type,
                                        const std::string& app_locale) const {
  FieldType storable_type = type.GetStorableType();
  UpdateCacheIfNeeded(app_locale);

  // When the phone number autofill has stored cannot be normalized, it
  // responds to queries for complete numbers with whatever the raw stored value
  // is, and simply return empty string for any queries for phone components.
  if (!cached_parsed_phone_.IsValidNumber()) {
    if (storable_type == PHONE_HOME_WHOLE_NUMBER ||
        storable_type == PHONE_HOME_CITY_AND_NUMBER) {
      return cached_parsed_phone_.GetWholeNumber();
    }
    return std::u16string();
  }

  auto GetTrunkPrefix = [&] {
    const std::u16string national_number =
        GetInfo(PHONE_HOME_CITY_AND_NUMBER, app_locale);
    // Everything before the city code in the nationally formatted number.
    return national_number.substr(
        0, national_number.find(cached_parsed_phone_.city_code()));
  };

  switch (storable_type) {
    case PHONE_HOME_WHOLE_NUMBER:
      return cached_parsed_phone_.GetWholeNumber();

    case PHONE_HOME_NUMBER:
      return cached_parsed_phone_.number();

    case PHONE_HOME_NUMBER_PREFIX: {
      const std::u16string number = GetInfo(PHONE_HOME_NUMBER, app_locale);
      const std::u16string number_suffix =
          GetInfo(PHONE_HOME_NUMBER_SUFFIX, app_locale);
      DCHECK(number.size() >= number_suffix.size());
      // As PHONE_HOME_NUMBER = PHONE_HOME_NUMBER_PREFIX +
      // PHONE_HOME_NUMBER_SUFFIX, extract the appropriate prefix from `number`.
      return number.substr(0, number.size() - number_suffix.size());
    }

    case PHONE_HOME_NUMBER_SUFFIX: {
      const std::u16string number = GetInfo(PHONE_HOME_NUMBER, app_locale);
      // Libphonenumber doesn't provide functionality to split PHONE_HOME_NUMBER
      // further, and the HTML standard doesn't specify which suffix
      // autocomplete="tel-local-suffix" corresponds to. In all countries using
      // this format that we are aware of (see unit tests), the suffix consists
      // of the last 4 digits, while the length of the prefix varies.
      constexpr size_t kHomePhoneNumberSuffixLength = 4;
      return number.size() >= kHomePhoneNumberSuffixLength
                 ? number.substr(number.size() - kHomePhoneNumberSuffixLength)
                 : number;
    }

    case PHONE_HOME_CITY_CODE_WITH_TRUNK_PREFIX:
      return GetTrunkPrefix() + cached_parsed_phone_.city_code();

    case PHONE_HOME_CITY_CODE:
      return cached_parsed_phone_.city_code();

    case PHONE_HOME_COUNTRY_CODE:
      return cached_parsed_phone_.country_code();

    case PHONE_HOME_CITY_AND_NUMBER: {
      // Just concatenating city code and phone number is insufficient because
      // a number of non-US countries (e.g. Germany and France) use a leading 0
      // to indicate that the next digits represent a city code.
      std::u16string national_number =
          cached_parsed_phone_.GetNationallyFormattedNumber();
      // GetNationallyFormattedNumber optimizes for screen display, e.g. it
      // shows a US number as (888) 123-1234. The following retains only the
      // digits.
      std::erase_if(national_number, [](char16_t c) {
        return c > UCHAR_MAX ||
               !absl::ascii_isdigit(static_cast<unsigned char>(c));
      });
      return national_number;
    }

    case PHONE_HOME_CITY_AND_NUMBER_WITHOUT_TRUNK_PREFIX: {
      // Strip the trunk prefix from the nationally formatted number.
      const std::u16string national_number =
          GetInfo(PHONE_HOME_CITY_AND_NUMBER, app_locale);
      const std::size_t trunk_prefix_len = GetTrunkPrefix().length();
      DCHECK(trunk_prefix_len <= national_number.length());
      return national_number.substr(trunk_prefix_len);
    }

    case PHONE_HOME_EXTENSION:
      // Autofill doesn't support filling extensions, but some basic local
      // heuristics classify them.
      return std::u16string();

    default:
      NOTREACHED_IN_MIGRATION();
      return std::u16string();
  }
}

bool PhoneNumber::SetInfoWithVerificationStatusImpl(
    const AutofillType& type,
    const std::u16string& value,
    const std::string& app_locale,
    VerificationStatus status) {
  SetRawInfoWithVerificationStatus(type.GetStorableType(), value, status);

  if (number_.empty())
    return true;

  // `SetRawInfoWithVerificationStatus()` invalidated `cached_parsed_phone_` and
  // calling `UpdateCacheIfNeeded()` will thus try parsing the `number_` here.
  UpdateCacheIfNeeded(app_locale);
  // If the number invalid, setting fails and `GetRawInfo()` and `GetInfo()`
  // should return an empty string. Clear both representations of the number.
  if (!cached_parsed_phone_.IsValidNumber()) {
    number_.clear();
    cached_parsed_phone_ = i18n::PhoneObject();
    return false;
  }
  // Store a formatted (i.e., pretty printed) version of the number if it
  // doesn't contain formatting marks.
  if (base::ContainsOnlyChars(number_, u"+0123456789") ||
      base::FeatureList::IsEnabled(
          features::kAutofillPreferParsedPhoneNumber)) {
    number_ = cached_parsed_phone_.GetFormattedNumber();
  } else {
    // Strip `number_` of extensions, e.g. "(123)-123 ext. 123" -> "(123)-123".
    // In the if case, this is done by `GetFormattedNumber()` already. To
    // preserve the formatting, everything after the last digit of the whole
    // number is removed manually here. The whole number only consists of digits
    // and has any extensions removed already.
    size_t i = 0;
    for (auto digit : cached_parsed_phone_.GetWholeNumber())
      i = number_.find(digit, i) + 1;  // Skip `digit`.
    number_ = number_.substr(0, i);
  }
  return true;
}

void PhoneNumber::UpdateCacheIfNeeded(const std::string& app_locale) const {
  std::string region = GetRegion(*profile_, app_locale);
  if (!number_.empty() && cached_parsed_phone_.region() != region) {
    // To enable filling of country calling codes for nationally formatted
    // numbers, infer it from the `profile_`'s country information while parsing
    // the number.
    cached_parsed_phone_ = i18n::PhoneObject(
        number_, region,
        /*infer_country_code=*/profile_->HasInfo(ADDRESS_HOME_COUNTRY) &&
            base::FeatureList::IsEnabled(
                features::kAutofillInferCountryCallingCode));
  }
}

PhoneNumber::PhoneCombineHelper::PhoneCombineHelper() = default;

PhoneNumber::PhoneCombineHelper::~PhoneCombineHelper() = default;

void PhoneNumber::PhoneCombineHelper::SetInfo(FieldType field_type,
                                              const std::u16string& value) {
  CHECK_EQ(GroupTypeOfFieldType(field_type), FieldTypeGroup::kPhone);
  switch (field_type) {
    case PHONE_HOME_COUNTRY_CODE:
      country_ = value;
      return;
    case PHONE_HOME_CITY_CODE:
    case PHONE_HOME_CITY_CODE_WITH_TRUNK_PREFIX:
      city_ = value;
      return;
    case PHONE_HOME_CITY_AND_NUMBER:
    case PHONE_HOME_CITY_AND_NUMBER_WITHOUT_TRUNK_PREFIX:
      phone_ = value;
      return;
    case PHONE_HOME_WHOLE_NUMBER:
      whole_number_ = value;
      return;
    case PHONE_HOME_NUMBER:
    case PHONE_HOME_NUMBER_PREFIX:
      phone_ = value;
      return;
    case PHONE_HOME_NUMBER_SUFFIX:
      phone_.append(value);
      return;
    case PHONE_HOME_EXTENSION:
      // PHONE_HOME_EXTENSION is not stored or filled, but it's still classified
      // to prevent misclassifying such fields as something else.
      return;
    default:
      NOTREACHED();
  }
}

bool PhoneNumber::PhoneCombineHelper::ParseNumber(
    const AutofillProfile& profile,
    const std::string& app_locale,
    std::u16string* value) const {
  if (IsEmpty())
    return false;

  if (!whole_number_.empty()) {
    *value = whole_number_;
    return true;
  }

  return i18n::ConstructPhoneNumber(country_ + city_ + phone_,
                                    GetRegion(profile, app_locale), value);
}

// static
bool PhoneNumber::ImportPhoneNumberToProfile(
    const PhoneNumber::PhoneCombineHelper& combined_phone,
    const std::string& app_locale,
    AutofillProfile& profile) {
  std::u16string constructed_number;
  // If the phone number only consists of a single component, the
  // `PhoneCombineHelper` won't try to parse it. This happens during `SetInfo()`
  // in this case.
  bool parsed_successfully =
      combined_phone.ParseNumber(profile, app_locale, &constructed_number) &&
      profile.SetInfoWithVerificationStatus(PHONE_HOME_WHOLE_NUMBER,
                                            constructed_number, app_locale,
                                            VerificationStatus::kObserved);
  return parsed_successfully;
}

bool PhoneNumber::PhoneCombineHelper::IsEmpty() const {
  return phone_.empty() && whole_number_.empty();
}

}  // namespace autofill
