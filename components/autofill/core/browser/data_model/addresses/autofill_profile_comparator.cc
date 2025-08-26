// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/addresses/autofill_profile_comparator.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_model/addresses/address.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_i18n_api.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_normalization_utils.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_component.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_name.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_utils.h"
#include "components/autofill/core/browser/data_model/addresses/contact_info.h"
#include "components/autofill/core/browser/data_model/transliterator.h"
#include "components/autofill/core/browser/data_quality/autofill_data_util.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_l10n_util.h"
#include "third_party/libphonenumber/phonenumber_api.h"

using i18n::phonenumbers::PhoneNumberUtil;

namespace autofill {
namespace {

constexpr char16_t kSpace[] = u" ";

std::ostream& operator<<(std::ostream& os,
                         const ::i18n::phonenumbers::PhoneNumber& n) {
  os << "country_code: " << n.country_code() << " "
     << "national_number: " << n.national_number();
  if (n.has_italian_leading_zero()) {
    os << " italian_leading_zero: " << n.italian_leading_zero();
  }
  if (n.has_number_of_leading_zeros()) {
    os << " number_of_leading_zeros: " << n.number_of_leading_zeros();
  }
  if (n.has_raw_input()) {
    os << " raw_input: \"" << n.raw_input() << "\"";
  }
  return os;
}

}  // namespace

AutofillProfileComparator::AutofillProfileComparator(
    std::string_view app_locale)
    : app_locale_(app_locale.data(), app_locale.size()) {}

AutofillProfileComparator::~AutofillProfileComparator() = default;

std::vector<ProfileValueDifference>
AutofillProfileComparator::GetProfileDifference(
    const AutofillProfile& first_profile,
    const AutofillProfile& second_profile,
    FieldTypeSet types,
    const std::string& app_locale) {
  std::vector<ProfileValueDifference> difference;
  difference.reserve(types.size());

  for (auto type : types) {
    const std::u16string& first_value = first_profile.GetInfo(type, app_locale);
    const std::u16string& second_value =
        second_profile.GetInfo(type, app_locale);
    if (first_value != second_value) {
      difference.emplace_back(
          ProfileValueDifference{type, first_value, second_value});
    }
  }
  return difference;
}

std::vector<ProfileValueDifference>
AutofillProfileComparator::GetSettingsVisibleProfileDifference(
    const AutofillProfile& first_profile,
    const AutofillProfile& second_profile,
    const std::string& app_locale) {
  FieldTypeSet types = first_profile.GetUserVisibleTypes();
  types.insert_all(second_profile.GetUserVisibleTypes());
  return GetProfileDifference(first_profile, second_profile, types, app_locale);
}

// static
bool AutofillProfileComparator::Compare(
    std::u16string_view text1,
    std::u16string_view text2,
    normalization::WhitespaceSpec whitespace_spec,
    std::optional<FieldType> type,
    AddressCountryCode country_code_1,
    AddressCountryCode country_code_2) {
  if (text1.empty() && text2.empty()) {
    return true;
  }
  // We transliterate the entire text as it's non-trivial to go character
  // by character (eg. a "ß" is transliterated to "ss").
  std::u16string normalized_text1 =
      RemoveDiacriticsAndConvertToLowerCase(text1, country_code_1);
  std::u16string normalized_text2 =
      RemoveDiacriticsAndConvertToLowerCase(text2, country_code_2);

  // TODO(crbug.com/359768803): Extract alternative name transliteration and
  // remove `type` parameter. Japanese alternative names are stored in Hiragana
  // only. We transliterate Katakana to ensure correct comparison.
  if (type.has_value() && IsAlternativeNameType(type.value()) &&
      base::FeatureList::IsEnabled(
          features::kAutofillSupportPhoneticNameForJP)) {
    normalized_text1 = TransliterateAlternativeName(normalized_text1);
    normalized_text2 = TransliterateAlternativeName(normalized_text2);
  }

  normalization::NormalizingIterator normalizing_iter1{normalized_text1,
                                                       whitespace_spec};
  normalization::NormalizingIterator normalizing_iter2{normalized_text2,
                                                       whitespace_spec};

  while (!normalizing_iter1.End() && !normalizing_iter2.End()) {
    if (normalizing_iter1.GetNextChar() != normalizing_iter2.GetNextChar()) {
      return false;
    }
    normalizing_iter1.Advance();
    normalizing_iter2.Advance();
  }

  if (normalizing_iter1.EndsInSkippableCharacters() &&
      normalizing_iter2.EndsInSkippableCharacters()) {
    return true;
  }

  return false;
}

bool AutofillProfileComparator::AreMergeable(const AutofillProfile& p1,
                                             const AutofillProfile& p2) const {
  // Sorted in order to relative expense of the tests to fail early and cheaply
  // if possible.
  // Emails go last, since their comparison logic triggers ICU code, which can
  // trigger the loading of locale-specific rules.
  DVLOG(1) << "Comparing profiles:\np1 = " << p1 << "\np2 = " << p2;

  if (!HaveMergeableCompanyNames(p1, p2)) {
    DVLOG(1) << "Different email company names.";
    return false;
  }

  if (!HaveMergeablePhoneNumbers(p1, p2)) {
    DVLOG(1) << "Different phone numbers.";
    return false;
  }

  if (!NameInfo::AreNamesMergeable(p1.GetNameInfo(), p1.GetAddressCountryCode(),
                                   p2.GetNameInfo(),
                                   p2.GetAddressCountryCode())) {
    DVLOG(1) << "Different names.";
    return false;
  }

  if (!NameInfo::AreAlternativeNamesMergeable(
          p1.GetNameInfo(), p1.GetAddressCountryCode(), p2.GetNameInfo(),
          p2.GetAddressCountryCode())) {
    DVLOG(1) << "Different alternative names.";
    return false;
  }

  if (!HaveMergeableAddresses(p1, p2)) {
    DVLOG(1) << "Different addresses.";
    return false;
  }

  if (!HaveMergeableEmailAddresses(p1, p2)) {
    DVLOG(1) << "Different email addresses.";
    return false;
  }

  DVLOG(1) << "Profiles are mergeable.";
  return true;
}

bool AutofillProfileComparator::MergeEmailAddresses(
    const AutofillProfile& new_profile,
    const AutofillProfile& old_profile,
    EmailInfo& email_info) const {
  DCHECK(HaveMergeableEmailAddresses(new_profile, old_profile));

  const std::u16string& e1 = new_profile.GetInfo(EMAIL_ADDRESS, app_locale_);
  const std::u16string& e2 = old_profile.GetInfo(EMAIL_ADDRESS, app_locale_);
  const std::u16string* best = nullptr;

  if (e1.empty()) {
    best = &e2;
  } else if (e2.empty()) {
    best = &e1;
  } else {
    best = old_profile.usage_history().use_date() >
                   new_profile.usage_history().use_date()
               ? &e2
               : &e1;
  }

  email_info.SetInfo(EMAIL_ADDRESS, *best, app_locale_);
  return true;
}

bool AutofillProfileComparator::MergeCompanyNames(
    const AutofillProfile& new_profile,
    const AutofillProfile& old_profile,
    CompanyInfo& company_info) const {
  const std::u16string& c1 = new_profile.GetInfo(COMPANY_NAME, app_locale_);
  const std::u16string& c2 = old_profile.GetInfo(COMPANY_NAME, app_locale_);
  const std::u16string* best = nullptr;

  DCHECK(HaveMergeableCompanyNames(new_profile, old_profile))
      << "Company names are not mergeable: '" << c1 << "' vs '" << c2 << "'";

  CompareTokensResult result =
      CompareTokens(normalization::NormalizeForComparison(c1),
                    normalization::NormalizeForComparison(c2));
  switch (result) {
    case DIFFERENT_TOKENS:
    default:
      NOTREACHED() << "Unexpected mismatch: '" << c1 << "' vs '" << c2 << "'";
    case S1_CONTAINS_S2:
      best = &c1;
      break;
    case S2_CONTAINS_S1:
      best = &c2;
      break;
    case SAME_TOKENS:
      best = old_profile.usage_history().use_date() >
                     new_profile.usage_history().use_date()
                 ? &c2
                 : &c1;
      break;
  }
  company_info.SetInfo(COMPANY_NAME, *best, app_locale_);
  return true;
}

bool AutofillProfileComparator::MergePhoneNumbers(
    const AutofillProfile& new_profile,
    const AutofillProfile& old_profile,
    PhoneNumber& phone_number) const {
  const FieldType kWholePhoneNumber = PHONE_HOME_WHOLE_NUMBER;
  const std::u16string& s1 = new_profile.GetRawInfo(kWholePhoneNumber);
  const std::u16string& s2 = old_profile.GetRawInfo(kWholePhoneNumber);

  DCHECK(HaveMergeablePhoneNumbers(new_profile, old_profile))
      << "Phone numbers are not mergeable: '" << s1 << "' vs '" << s2 << "'";

  if (normalization::HasOnlySkippableCharacters(s1) &&
      normalization::HasOnlySkippableCharacters(s2)) {
    phone_number.SetRawInfo(kWholePhoneNumber, std::u16string());
  }

  if (normalization::HasOnlySkippableCharacters(s1)) {
    phone_number.SetRawInfo(kWholePhoneNumber, s2);
    return true;
  }

  if (normalization::HasOnlySkippableCharacters(s2) || s1 == s2) {
    phone_number.SetRawInfo(kWholePhoneNumber, s1);
    return true;
  }

  // Figure out a country code hint.
  // TODO(crbug.com/40221178) `GetNonEmptyOf()` prefers `new_profile` in case
  // both are non empty.
  std::string region = base::UTF16ToUTF8(GetNonEmptyOf(
      new_profile, old_profile,
      AutofillType(ADDRESS_HOME_COUNTRY, /*is_country_code=*/true)));
  if (region.empty()) {
    region = AutofillCountry::CountryCodeForLocale(app_locale_);
  }

  // Parse the phone numbers.
  PhoneNumberUtil* phone_util = PhoneNumberUtil::GetInstance();

  ::i18n::phonenumbers::PhoneNumber n1;
  if (phone_util->ParseAndKeepRawInput(base::UTF16ToUTF8(s1), region, &n1) !=
      PhoneNumberUtil::NO_PARSING_ERROR) {
    return false;
  }

  ::i18n::phonenumbers::PhoneNumber n2;
  if (phone_util->ParseAndKeepRawInput(base::UTF16ToUTF8(s2), region, &n2) !=
      PhoneNumberUtil::NO_PARSING_ERROR) {
    return false;
  }

  // `country_code()` defaults to the provided `region`. But if one of the
  // numbers is in international format, we should prefer that country code.
  auto HasInternationalCountryCode =
      [](const ::i18n::phonenumbers::PhoneNumber& number) {
        return number.country_code_source() !=
               ::i18n::phonenumbers::PhoneNumber::FROM_DEFAULT_COUNTRY;
      };

  ::i18n::phonenumbers::PhoneNumber merged_number;
  // There are three cases for country codes:
  // - Both numbers are in international format, so because the numbers are
  //   mergeable, they are equal.
  // - Both are not in international format, so their country codes both default
  //   to `region`.
  // - One of them is in international format, so we prefer that country code.
  DCHECK(HasInternationalCountryCode(n1) != HasInternationalCountryCode(n2) ||
         n1.country_code() == n2.country_code());
  merged_number.set_country_code(
      HasInternationalCountryCode(n1) ? n1.country_code() : n2.country_code());
  merged_number.set_national_number(
      std::max(n1.national_number(), n2.national_number()));
  if (n1.has_italian_leading_zero() || n2.has_italian_leading_zero()) {
    merged_number.set_italian_leading_zero(n1.italian_leading_zero() ||
                                           n2.italian_leading_zero());
  }
  if (n1.has_number_of_leading_zeros() || n2.has_number_of_leading_zeros()) {
    merged_number.set_number_of_leading_zeros(
        std::max(n1.number_of_leading_zeros(), n2.number_of_leading_zeros()));
  }

  // Format the `merged_number` in international format only if at least one
  // of the country codes was derived from the number itself. This is done
  // consistently with `::autofill::i18n::FormatValidatedNumber()` and
  // `::autofill::i18n::ParsePhoneNumber()`, which backs the `PhoneNumber`
  // implementation.
  PhoneNumberUtil::PhoneNumberFormat format =
      HasInternationalCountryCode(n1) || HasInternationalCountryCode(n2)
          ? PhoneNumberUtil::INTERNATIONAL
          : PhoneNumberUtil::NATIONAL;

  std::string new_number;
  phone_util->Format(merged_number, format, &new_number);

  DVLOG(2) << "n1 = {" << n1 << "}";
  DVLOG(2) << "n2 = {" << n2 << "}";
  DVLOG(2) << "merged_number = {" << merged_number << "}";
  DVLOG(2) << "new_number = \"" << new_number << "\"";

  // Check if it's a North American number that's missing the area code.
  // Libphonenumber doesn't know how to format short numbers; it will still
  // include the country code prefix.
  if (merged_number.country_code() == 1 &&
      merged_number.national_number() <= 9999999 &&
      new_number.starts_with("+1")) {
    size_t offset = 2;  // The char just after "+1".
    while (offset < new_number.size() &&
           base::IsAsciiWhitespace(new_number[offset])) {
      ++offset;
    }
    new_number = new_number.substr(offset);
  }

  phone_number.SetRawInfo(kWholePhoneNumber, base::UTF8ToUTF16(new_number));
  return true;
}

bool AutofillProfileComparator::MergeAddresses(
    const AutofillProfile& new_profile,
    const AutofillProfile& old_profile,
    Address& address) const {
  DCHECK(HaveMergeableAddresses(new_profile, old_profile));
  CHECK(!(old_profile.record_type() ==
              AutofillProfile::RecordType::kAccountNameEmail &&
          new_profile.record_type() ==
              AutofillProfile::RecordType::kAccountNameEmail));

  // If one of the profiles is `kAccountNameEmail` profile, sets `address` to
  // address tree of the other profile and returns true.
  if (new_profile.record_type() ==
      AutofillProfile::RecordType::kAccountNameEmail) {
    address = old_profile.GetAddress();
    return true;
  }
  if (old_profile.record_type() ==
      AutofillProfile::RecordType::kAccountNameEmail) {
    address = new_profile.GetAddress();
    return true;
  }

  address = old_profile.GetAddress();
  return address.MergeStructuredAddress(
      new_profile.GetAddress(), old_profile.usage_history().use_date() <
                                    new_profile.usage_history().use_date());
}

std::optional<FieldTypeSet>
AutofillProfileComparator::NonMergeableSettingVisibleTypes(
    const AutofillProfile& a,
    const AutofillProfile& b) const {
  if (a.GetAddressCountryCode() != b.GetAddressCountryCode()) {
    return std::nullopt;
  }
  FieldTypeSet setting_visible_types = a.GetUserVisibleTypes();
  FieldTypeSet non_mergeable_types;
  auto maybe_add_type = [&](FieldType type, bool is_mergeable) {
    // Ensure that `type` is actually a setting-visible type.
    CHECK_EQ(setting_visible_types.erase(type), 1u);
    if (!is_mergeable) {
      non_mergeable_types.insert(type);
    }
  };
  // For most setting-visible types, a HaveMergeable* function exists. If these
  // types ever become non-settings visible, the check in `maybe_add_type` will
  // fail in the unittest.
  maybe_add_type(NAME_FULL, NameInfo::AreNamesMergeable(
                                a.GetNameInfo(), a.GetAddressCountryCode(),
                                b.GetNameInfo(), b.GetAddressCountryCode()));
  if (setting_visible_types.contains(ALTERNATIVE_FULL_NAME)) {
    maybe_add_type(ALTERNATIVE_FULL_NAME,
                   NameInfo::AreAlternativeNamesMergeable(
                       a.GetNameInfo(), a.GetAddressCountryCode(),
                       b.GetNameInfo(), b.GetAddressCountryCode()));
  }
  maybe_add_type(COMPANY_NAME, HaveMergeableCompanyNames(a, b));
  maybe_add_type(PHONE_HOME_WHOLE_NUMBER, HaveMergeablePhoneNumbers(a, b));
  maybe_add_type(EMAIL_ADDRESS, HaveMergeableEmailAddresses(a, b));
  // Now, only address-related types remain in `setting_visible_types`. Using
  // `HaveMergeableAddresses()` is not fine-grained enough, since multiple
  // address types are setting-visible (e.g. city, zip, etc). Verify differences
  // in the corresponding subtrees manually.
  for (FieldType address_type : setting_visible_types) {
    CHECK_EQ(GroupTypeOfFieldType(address_type), FieldTypeGroup::kAddress);
    if (!a.GetAddress().IsAddressFieldSettingAccessible(address_type)) {
      // `address_type` is not applicable to `a`'s country (= `b`'s country).
      continue;
    }
    if (!a.GetAddress().IsStructuredAddressMergeableForType(address_type,
                                                            b.GetAddress())) {
      non_mergeable_types.insert(address_type);
    }
  }
  return non_mergeable_types;
}

bool AutofillProfileComparator::ProfilesHaveDifferentSettingsVisibleValues(
    const AutofillProfile& p1,
    const AutofillProfile& p2,
    const std::string& app_locale) {
  if (p1.GetUserVisibleTypes() != p2.GetUserVisibleTypes()) {
    return false;
  }
  // Return true if at least one value corresponding to the settings visible
  // types is different between the two profiles.
  return std::ranges::any_of(p1.GetUserVisibleTypes(), [&](FieldType type) {
    if (IsAlternativeNameType(type) &&
        base::FeatureList::IsEnabled(
            features::kAutofillSupportPhoneticNameForJP)) {
      // Consider two alternative names that differ only in the character set
      // equal.
      const AddressCountryCode common_country_code =
          AddressComponent::GetCommonCountry(p1.GetAddressCountryCode(),
                                             p2.GetAddressCountryCode());
      return p1.GetNameInfo().GetValueForComparisonForType(
                 type, common_country_code) !=
             p2.GetNameInfo().GetValueForComparisonForType(type,
                                                           common_country_code);
    }
    return p1.GetInfo(type, app_locale) != p2.GetInfo(type, app_locale);
  });
}

// static
std::set<std::u16string_view> AutofillProfileComparator::UniqueTokens(
    std::u16string_view s) {
  std::vector<std::u16string_view> tokens = base::SplitStringPiece(
      s, kSpace, base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  return std::set<std::u16string_view>(tokens.begin(), tokens.end());
}

// static
AutofillProfileComparator::CompareTokensResult
AutofillProfileComparator::CompareTokens(std::u16string_view s1,
                                         std::u16string_view s2) {
  // Note: std::include() expects the items in each range to be in sorted order,
  // hence the use of std::set<> instead of std::unordered_set<>.
  std::set<std::u16string_view> t1 = UniqueTokens(s1);
  std::set<std::u16string_view> t2 = UniqueTokens(s2);

  // Does `s1` contain all of the tokens in `s2`? As a special case, return 0 if
  // the two sets are exactly the same.
  if (std::includes(t1.begin(), t1.end(), t2.begin(), t2.end())) {
    return t1.size() == t2.size() ? SAME_TOKENS : S1_CONTAINS_S2;
  }

  // Does `s2` contain all of the tokens in `s1`?
  if (std::includes(t2.begin(), t2.end(), t1.begin(), t1.end())) {
    return S2_CONTAINS_S1;
  }

  // Neither string contains all of the tokens from the other.
  return DIFFERENT_TOKENS;
}

std::u16string AutofillProfileComparator::GetNonEmptyOf(
    const AutofillProfile& p1,
    const AutofillProfile& p2,
    AutofillType t) const {
  const std::u16string& s1 = p1.GetInfo(t, app_locale_);
  if (!s1.empty()) {
    return s1;
  }
  return p2.GetInfo(t, app_locale_);
}

bool AutofillProfileComparator::HaveMergeableEmailAddresses(
    const AutofillProfile& p1,
    const AutofillProfile& p2) const {
  const std::u16string& email_1 = p1.GetInfo(EMAIL_ADDRESS, app_locale_);
  const std::u16string& email_2 = p2.GetInfo(EMAIL_ADDRESS, app_locale_);
  return email_1.empty() || email_2.empty() ||
         l10n::CaseInsensitiveCompare().StringsEqual(email_1, email_2);
}

bool AutofillProfileComparator::HaveMergeableCompanyNames(
    const AutofillProfile& p1,
    const AutofillProfile& p2) const {
  const std::u16string& company_name_1 = p1.GetInfo(COMPANY_NAME, app_locale_);
  const std::u16string& company_name_2 = p2.GetInfo(COMPANY_NAME, app_locale_);
  return normalization::HasOnlySkippableCharacters(company_name_1) ||
         normalization::HasOnlySkippableCharacters(company_name_2) ||
         CompareTokens(normalization::NormalizeForComparison(company_name_1),
                       normalization::NormalizeForComparison(company_name_2)) !=
             DIFFERENT_TOKENS;
}

bool AutofillProfileComparator::HaveMergeablePhoneNumbers(
    const AutofillProfile& p1,
    const AutofillProfile& p2) const {
  // We work with the raw phone numbers to avoid losing any helpful information
  // as we parse.
  const std::u16string& raw_phone_1 = p1.GetRawInfo(PHONE_HOME_WHOLE_NUMBER);
  const std::u16string& raw_phone_2 = p2.GetRawInfo(PHONE_HOME_WHOLE_NUMBER);

  // Are the two phone numbers trivially mergeable?
  if (normalization::HasOnlySkippableCharacters(raw_phone_1) ||
      normalization::HasOnlySkippableCharacters(raw_phone_2) ||
      raw_phone_1 == raw_phone_2) {
    return true;
  }

  // TODO(rogerm): Modify ::autofill::i18n::PhoneNumbersMatch to support
  // SHORT_NSN_MATCH and just call that instead of accessing the underlying
  // utility library directly?

  // Parse and compare the phone numbers.
  // The phone number util library needs the numbers in utf8.
  PhoneNumberUtil* phone_util = PhoneNumberUtil::GetInstance();
  switch (phone_util->IsNumberMatchWithTwoStrings(
      base::UTF16ToUTF8(raw_phone_1), base::UTF16ToUTF8(raw_phone_2))) {
    case PhoneNumberUtil::SHORT_NSN_MATCH:
    case PhoneNumberUtil::NSN_MATCH:
    case PhoneNumberUtil::EXACT_MATCH:
      return true;
    case PhoneNumberUtil::INVALID_NUMBER:
    case PhoneNumberUtil::NO_MATCH:
      return false;
    default:
      NOTREACHED();
  }
}

bool AutofillProfileComparator::HaveMergeableAddresses(
    const AutofillProfile& p1,
    const AutofillProfile& p2) const {
  CHECK(!(p1.record_type() == AutofillProfile::RecordType::kAccountNameEmail &&
          p2.record_type() == AutofillProfile::RecordType::kAccountNameEmail));
  if (p1.record_type() == AutofillProfile::RecordType::kAccountNameEmail ||
      p2.record_type() == AutofillProfile::RecordType::kAccountNameEmail) {
    return true;
  }
  return p2.GetAddress().IsStructuredAddressMergeable(p1.GetAddress());
}

}  // namespace autofill
