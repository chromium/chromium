// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_profile_comparator.h"

#include <algorithm>
#include <optional>
#include <string_view>
#include <vector>

#include "base/i18n/char_iterator.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_utils.h"
#include "components/autofill/core/browser/data_model/borrowed_transliterator.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/common/autofill_clock.h"
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
  if (n.has_italian_leading_zero())
    os << " italian_leading_zero: " << n.italian_leading_zero();
  if (n.has_number_of_leading_zeros())
    os << " number_of_leading_zeros: " << n.number_of_leading_zeros();
  if (n.has_raw_input())
    os << " raw_input: \"" << n.raw_input() << "\"";
  return os;
}

bool IsPunctuationOrWhitespace(const int8_t character) {
  switch (character) {
    // Punctuation
    case U_DASH_PUNCTUATION:
    case U_START_PUNCTUATION:
    case U_END_PUNCTUATION:
    case U_CONNECTOR_PUNCTUATION:
    case U_OTHER_PUNCTUATION:
    // Whitespace
    case U_CONTROL_CHAR:  // To escape the '\n' character.
    case U_SPACE_SEPARATOR:
    case U_LINE_SEPARATOR:
    case U_PARAGRAPH_SEPARATOR:
      return true;

    default:
      return false;
  }
}

// Iterator for a string that processes punctuation and white space according to
// |collapse_skippable_|.
class NormalizingIterator {
 public:
  NormalizingIterator(
      std::u16string_view text,
      AutofillProfileComparator::WhitespaceSpec whitespace_spec);
  ~NormalizingIterator();

  // Advances to the next non-skippable character in the string. Whether a
  // punctuation or white space character is skippable depends on
  // |collapse_skippable_|. Returns false if the end of the string has been
  // reached.
  void Advance();

  // Returns true if the iterator has reached the end of the string.
  bool End();

  // Returns true if the iterator ends in skippable characters or if the
  // iterator has reached the end of the string. Has the side effect of
  // advancing the iterator to either the first skippable character or to the
  // end of the string.
  bool EndsInSkippableCharacters();

  // Returns the next character that should be considered.
  int32_t GetNextChar();

 private:
  // When |collapse_skippable_| is false, this member is initialized to false
  // and is not updated.
  //
  // When |collapse_skippable_| is true, this member indicates whether the
  // previous character was punctuation or white space so that one or more
  // consecutive embedded punctuation and white space characters can be
  // collapsed to a single white space.
  bool previous_was_skippable_ = false;

  // True if punctuation and white space within the string should be collapsed
  // to a single white space.
  bool collapse_skippable_;

  base::i18n::UTF16CharIterator iter_;
};

NormalizingIterator::NormalizingIterator(
    std::u16string_view text,
    AutofillProfileComparator::WhitespaceSpec whitespace_spec)
    : collapse_skippable_(whitespace_spec ==
                          AutofillProfileComparator::RETAIN_WHITESPACE),
      iter_(text) {
  int32_t character = iter_.get();

  while (!iter_.end() && IsPunctuationOrWhitespace(u_charType(character))) {
    iter_.Advance();
    character = iter_.get();
  }
}

NormalizingIterator::~NormalizingIterator() = default;

void NormalizingIterator::Advance() {
  if (!iter_.Advance()) {
    return;
  }

  while (!End()) {
    int32_t character = iter_.get();
    bool is_punctuation_or_whitespace =
        IsPunctuationOrWhitespace(u_charType(character));

    if (!is_punctuation_or_whitespace) {
      previous_was_skippable_ = false;
      return;
    }

    if (is_punctuation_or_whitespace && !previous_was_skippable_ &&
        collapse_skippable_) {
      // Punctuation or white space within the string was found, e.g. the "," in
      // the string "Hotel Schmotel, 3 Old Rd", and is after a non-skippable
      // character.
      previous_was_skippable_ = true;
      return;
    }

    iter_.Advance();
  }
}

bool NormalizingIterator::End() {
  return iter_.end();
}

bool NormalizingIterator::EndsInSkippableCharacters() {
  while (!End()) {
    int32_t character = iter_.get();
    if (!IsPunctuationOrWhitespace(u_charType(character))) {
      return false;
    }
    iter_.Advance();
  }
  return true;
}

int32_t NormalizingIterator::GetNextChar() {
  if (End()) {
    return 0;
  }

  if (previous_was_skippable_) {
    return ' ';
  }

  return iter_.get();
}

}  // namespace

FieldTypeSet GetUserVisibleTypes() {
  static const FieldTypeSet user_visible_type = {
      NAME_FULL,
      ADDRESS_HOME_STREET_ADDRESS,
      ADDRESS_HOME_CITY,
      ADDRESS_HOME_DEPENDENT_LOCALITY,
      ADDRESS_HOME_STATE,
      ADDRESS_HOME_ZIP,
      ADDRESS_HOME_COUNTRY,
      ADDRESS_HOME_ADMIN_LEVEL2,
      EMAIL_ADDRESS,
      PHONE_HOME_WHOLE_NUMBER,
      COMPANY_NAME};
  return user_visible_type;
}

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

base::flat_map<FieldType, std::pair<std::u16string, std::u16string>>
AutofillProfileComparator::GetProfileDifferenceMap(
    const AutofillProfile& first_profile,
    const AutofillProfile& second_profile,
    FieldTypeSet types,
    const std::string& app_locale) {
  std::vector<std::pair<FieldType, std::pair<std::u16string, std::u16string>>>
      result;
  result.reserve(types.size());

  for (auto& diff : AutofillProfileComparator::GetProfileDifference(
           first_profile, second_profile, types, app_locale)) {
    result.push_back(
        {diff.type,
         {std::move(diff.first_value), std::move(diff.second_value)}});
  }
  return base::flat_map<FieldType, std::pair<std::u16string, std::u16string>>(
      std::move(result));
}

std::vector<ProfileValueDifference>
AutofillProfileComparator::GetSettingsVisibleProfileDifference(
    const AutofillProfile& first_profile,
    const AutofillProfile& second_profile,
    const std::string& app_locale) {
  return GetProfileDifference(first_profile, second_profile,
                              GetUserVisibleTypes(), app_locale);
}

base::flat_map<FieldType, std::pair<std::u16string, std::u16string>>
AutofillProfileComparator::GetSettingsVisibleProfileDifferenceMap(
    const AutofillProfile& first_profile,
    const AutofillProfile& second_profile,
    const std::string& app_locale) {
  return GetProfileDifferenceMap(first_profile, second_profile,
                                 GetUserVisibleTypes(), app_locale);
}

bool AutofillProfileComparator::Compare(std::u16string_view text1,
                                        std::u16string_view text2,
                                        WhitespaceSpec whitespace_spec) const {
  if (text1.empty() && text2.empty()) {
    return true;
  }

  // We transliterate the entire text as it's non-trivial to go character
  // by character (eg. a "ß" is transliterated to "ss").
  std::u16string normalized_text1 =
      RemoveDiacriticsAndConvertToLowerCase(text1);
  std::u16string normalized_text2 =
      RemoveDiacriticsAndConvertToLowerCase(text2);

  NormalizingIterator normalizing_iter1{normalized_text1, whitespace_spec};
  NormalizingIterator normalizing_iter2{normalized_text2, whitespace_spec};

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

bool AutofillProfileComparator::HasOnlySkippableCharacters(
    std::u16string_view text) const {
  if (text.empty()) {
    return true;
  }

  return NormalizingIterator(text,
                             AutofillProfileComparator::DISCARD_WHITESPACE)
      .End();
}

// static
std::u16string AutofillProfileComparator::NormalizeForComparison(
    std::u16string_view text,
    AutofillProfileComparator::WhitespaceSpec whitespace_spec) {
  // This algorithm is not designed to be perfect, we could get arbitrarily
  // fancy here trying to canonicalize address lines. Instead, this is designed
  // to handle common cases for all types of data (addresses and names) without
  // needing domain-specific logic.
  //
  // 1. Convert punctuation to spaces and normalize all whitespace to spaces if
  //    |whitespace_spec| is RETAIN_WHITESPACE.
  //    This will convert "Mid-Island Plz." -> "Mid Island Plz " (the trailing
  //    space will be trimmed off outside of the end of the loop).
  //
  // 2. Collapse consecutive punctuation/whitespace characters to a single
  //    space. We pretend the string has already started with whitespace in
  //    order to trim leading spaces.
  //
  // 3. Remove diacritics (accents and other non-spacing marks) and perform
  //    case folding to lower-case.
  std::u16string result;
  result.reserve(text.length());
  bool previous_was_whitespace = (whitespace_spec == RETAIN_WHITESPACE);
  for (base::i18n::UTF16CharIterator iter(text); !iter.end(); iter.Advance()) {
    if (IsPunctuationOrWhitespace(u_charType(iter.get()))) {
      if (!previous_was_whitespace && whitespace_spec == RETAIN_WHITESPACE) {
        result.push_back(' ');
        previous_was_whitespace = true;
      }
    } else {
      previous_was_whitespace = false;
      base::WriteUnicodeCharacter(iter.get(), &result);
    }
  }

  // Trim off trailing whitespace if we left one.
  if (previous_was_whitespace && !result.empty())
    result.resize(result.size() - 1);

  return RemoveDiacriticsAndConvertToLowerCase(result);
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

  if (!HaveMergeableNames(p1, p2)) {
    DVLOG(1) << "Different names.";
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

bool AutofillProfileComparator::MergeNames(const AutofillProfile& p1,
                                           const AutofillProfile& p2,
                                           NameInfo& name_info) const {
  DCHECK(HaveMergeableNames(p1, p2));

  const std::u16string full_name_1 = p1.GetInfo(NAME_FULL, app_locale_);
  const std::u16string full_name_2 = p2.GetInfo(NAME_FULL, app_locale_);

  // At this state it is already determined that the two names are mergeable.
  // This can mean of of the following things:
  // * One name is empty. In this scenario the non-empty name is used.
  // * The names are token equivalent: In this scenario a merge of the tree
  // structure should be possible.
  // * One name is a variant of the other. In this scenario, use the non-variant
  // name. Note, p1 is the newer profile.
  // First, set info to the original profile.
  name_info = p2.GetNameInfo();
  // If the name of the |p1| is empty, just keep the state of p2.
  if (HasOnlySkippableCharacters(full_name_1))
    return true;
  // Vice verse set name to the one of |p1| if |p2| has an empty name
  if (HasOnlySkippableCharacters(full_name_2)) {
    name_info = p1.GetNameInfo();
    return true;
  }
  // Try to apply a direct merging.
  if (name_info.MergeStructuredName(p1.GetNameInfo()))
    return true;
  // If the name in |p2| is a variant of |p1| use the one in |p1|.
  if (IsNameVariantOf(NormalizeForComparison(full_name_1),
                      NormalizeForComparison(full_name_2))) {
    name_info = p1.GetNameInfo();
    return true;
  }
  // The only left case is that |p1| is a variant of |p2|.
  DCHECK(IsNameVariantOf(NormalizeForComparison(full_name_2),
                         NormalizeForComparison(full_name_1)));
  return true;
}

bool AutofillProfileComparator::MergeCJKNames(const AutofillProfile& p1,
                                              const AutofillProfile& p2,
                                              NameInfo& info) const {
  DCHECK(data_util::IsCJKName(p1.GetInfo(NAME_FULL, app_locale_)));
  DCHECK(data_util::IsCJKName(p2.GetInfo(NAME_FULL, app_locale_)));

  struct Name {
    std::u16string given;
    std::u16string surname;
    std::u16string full;
  };

  Name name1 = {p1.GetRawInfo(NAME_FIRST), p1.GetRawInfo(NAME_LAST),
                p1.GetRawInfo(NAME_FULL)};
  Name name2 = {p2.GetRawInfo(NAME_FIRST), p2.GetRawInfo(NAME_LAST),
                p2.GetRawInfo(NAME_FULL)};

  const Name* most_recent_name =
      p2.use_date() >= p1.use_date() ? &name2 : &name1;

  // The two |NameInfo| objects might disagree about what the full name looks
  // like. If only one of the two has an explicit (user-entered) full name, use
  // that as ground truth. Otherwise, use the most recent profile.
  const Name* full_name_candidate;
  if (name1.full.empty()) {
    full_name_candidate = &name2;
  } else if (name2.full.empty()) {
    full_name_candidate = &name1;
  } else {
    full_name_candidate = most_recent_name;
  }

  // The two |NameInfo| objects might disagree about how the name is split into
  // given/surname. If only one of the two has an explicit (user-entered)
  // given/surname pair, use that as ground truth. Otherwise, use the most
  // recent profile.
  const Name* name_parts_candidate;
  if (name1.given.empty() || name1.surname.empty()) {
    name_parts_candidate = &name2;
  } else if (name2.given.empty() || name2.surname.empty()) {
    name_parts_candidate = &name1;
  } else {
    name_parts_candidate = most_recent_name;
  }

  if (name_parts_candidate->given.empty() ||
      name_parts_candidate->surname.empty()) {
    // The name was not split correctly into a given/surname, so use the logic
    // from |SplitName()|.
    info.SetInfo(NAME_FULL, full_name_candidate->full, app_locale_);
  } else {
    // The name was already split into a given/surname, so keep those intact.
    if (!full_name_candidate->full.empty()) {
      info.SetRawInfo(NAME_FULL, full_name_candidate->full);
    }
    info.SetRawInfo(NAME_FIRST, name_parts_candidate->given);
    info.SetRawInfo(NAME_LAST, name_parts_candidate->surname);
  }

  return true;
}

bool AutofillProfileComparator::IsNameVariantOf(
    const std::u16string& full_name_1,
    const std::u16string& full_name_2) const {
  data_util::NameParts name_1_parts = data_util::SplitName(full_name_1);

  // Build the variants of full_name_1`s given, middle and family names.
  //
  // TODO(rogerm): Figure out whether or not we should break apart a compound
  // family name into variants (crbug/619051)
  const std::set<std::u16string> given_name_variants =
      GetNamePartVariants(name_1_parts.given);
  const std::set<std::u16string> middle_name_variants =
      GetNamePartVariants(name_1_parts.middle);
  std::u16string_view family_name = name_1_parts.family;

  // Iterate over all full name variants of profile 2 and see if any of them
  // match the full name from profile 1.
  for (const auto& given_name : given_name_variants) {
    for (const auto& middle_name : middle_name_variants) {
      std::u16string candidate = base::CollapseWhitespace(
          base::JoinString({given_name, middle_name, family_name}, kSpace),
          true);
      if (candidate == full_name_2)
        return true;
    }
  }

  // Also check if the name is just composed of the user's initials. For
  // example, "thomas jefferson miller" could be composed as "tj miller".
  if (!name_1_parts.given.empty() && !name_1_parts.middle.empty()) {
    std::u16string initials;
    initials.push_back(name_1_parts.given[0]);
    initials.push_back(name_1_parts.middle[0]);
    std::u16string candidate = base::CollapseWhitespace(
        base::JoinString({initials, family_name}, kSpace), true);
    if (candidate == full_name_2)
      return true;
  }

  // There was no match found.
  return false;
}

bool AutofillProfileComparator::MergeEmailAddresses(
    const AutofillProfile& p1,
    const AutofillProfile& p2,
    EmailInfo& email_info) const {
  DCHECK(HaveMergeableEmailAddresses(p1, p2));

  const std::u16string& e1 = p1.GetInfo(EMAIL_ADDRESS, app_locale_);
  const std::u16string& e2 = p2.GetInfo(EMAIL_ADDRESS, app_locale_);
  const std::u16string* best = nullptr;

  if (e1.empty()) {
    best = &e2;
  } else if (e2.empty()) {
    best = &e1;
  } else {
    best = p2.use_date() > p1.use_date() ? &e2 : &e1;
  }

  email_info.SetInfo(EMAIL_ADDRESS, *best, app_locale_);
  return true;
}

bool AutofillProfileComparator::MergeCompanyNames(
    const AutofillProfile& p1,
    const AutofillProfile& p2,
    CompanyInfo& company_info) const {
  const std::u16string& c1 = p1.GetInfo(COMPANY_NAME, app_locale_);
  const std::u16string& c2 = p2.GetInfo(COMPANY_NAME, app_locale_);
  const std::u16string* best = nullptr;

  DCHECK(HaveMergeableCompanyNames(p1, p2))
      << "Company names are not mergeable: '" << c1 << "' vs '" << c2 << "'";

  CompareTokensResult result =
      CompareTokens(NormalizeForComparison(c1), NormalizeForComparison(c2));
  switch (result) {
    case DIFFERENT_TOKENS:
    default:
      NOTREACHED_IN_MIGRATION()
          << "Unexpected mismatch: '" << c1 << "' vs '" << c2 << "'";
      return false;
    case S1_CONTAINS_S2:
      best = &c1;
      break;
    case S2_CONTAINS_S1:
      best = &c2;
      break;
    case SAME_TOKENS:
      best = p2.use_date() > p1.use_date() ? &c2 : &c1;
      break;
  }
  company_info.SetInfo(COMPANY_NAME, *best, app_locale_);
  return true;
}

bool AutofillProfileComparator::MergePhoneNumbers(
    const AutofillProfile& p1,
    const AutofillProfile& p2,
    PhoneNumber& phone_number) const {
  const FieldType kWholePhoneNumber = PHONE_HOME_WHOLE_NUMBER;
  const std::u16string& s1 = p1.GetRawInfo(kWholePhoneNumber);
  const std::u16string& s2 = p2.GetRawInfo(kWholePhoneNumber);

  DCHECK(HaveMergeablePhoneNumbers(p1, p2))
      << "Phone numbers are not mergeable: '" << s1 << "' vs '" << s2 << "'";

  if (HasOnlySkippableCharacters(s1) && HasOnlySkippableCharacters(s2)) {
    phone_number.SetRawInfo(kWholePhoneNumber, std::u16string());
  }

  if (HasOnlySkippableCharacters(s1)) {
    phone_number.SetRawInfo(kWholePhoneNumber, s2);
    return true;
  }

  if (HasOnlySkippableCharacters(s2) || s1 == s2) {
    phone_number.SetRawInfo(kWholePhoneNumber, s1);
    return true;
  }

  // Figure out a country code hint.
  // TODO(crbug.com/40221178) |GetNonEmptyOf()| prefers |p1| in case both are
  // non empty.
  std::string region = base::UTF16ToUTF8(
      GetNonEmptyOf(p1, p2, AutofillType(HtmlFieldType::kCountryCode)));
  if (region.empty())
    region = AutofillCountry::CountryCodeForLocale(app_locale_);

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

bool AutofillProfileComparator::MergeAddresses(const AutofillProfile& p1,
                                               const AutofillProfile& p2,
                                               Address& address) const {
  DCHECK(HaveMergeableAddresses(p1, p2));

  // Note that p1 is the newer address. Using p2 as the base.
  address = p2.GetAddress();
  return address.MergeStructuredAddress(p1.GetAddress(),
                                        p2.use_date() < p1.use_date());
}

std::optional<FieldTypeSet>
AutofillProfileComparator::NonMergeableSettingVisibleTypes(
    const AutofillProfile& a,
    const AutofillProfile& b) const {
  if (a.GetAddressCountryCode() != b.GetAddressCountryCode()) {
    return std::nullopt;
  }
  FieldTypeSet setting_visible_types = GetUserVisibleTypes();
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
  maybe_add_type(NAME_FULL, HaveMergeableNames(a, b));
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
  // Return true if at least one value corresponding to the settings visible
  // types is different between the two profiles.
  return std::ranges::any_of(GetUserVisibleTypes(), [&](const auto type) {
    return p1.GetInfo(type, app_locale) != p2.GetInfo(type, app_locale);
  });
}

bool AutofillProfileComparator::IsMergeCandidate(
    const AutofillProfile& existing_profile,
    const AutofillProfile& new_profile,
    const std::string& app_locale) {
  // If the existing profile is not mergeable with the new profile, it is
  // certainly not a merge candidate.
  if (!AreMergeable(existing_profile, new_profile)) {
    return false;
  }

  // Merge the two profiles. The return value from |MergeDataFrom()| indicates
  // if the existing profile was modified during the merge.
  AutofillProfile merged_profile = existing_profile;
  if (!merged_profile.MergeDataFrom(new_profile, app_locale)) {
    return false;
  }

  // If the two profiles have at least one settings-visible value that is
  // different, |existing_profile| is a merge candidate.
  return ProfilesHaveDifferentSettingsVisibleValues(
      merged_profile, existing_profile, app_locale);
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

  // Does s1 contain all of the tokens in s2? As a special case, return 0 if the
  // two sets are exactly the same.
  if (std::includes(t1.begin(), t1.end(), t2.begin(), t2.end()))
    return t1.size() == t2.size() ? SAME_TOKENS : S1_CONTAINS_S2;

  // Does s2 contain all of the tokens in s1?
  if (std::includes(t2.begin(), t2.end(), t1.begin(), t1.end()))
    return S2_CONTAINS_S1;

  // Neither string contains all of the tokens from the other.
  return DIFFERENT_TOKENS;
}

std::u16string AutofillProfileComparator::GetNonEmptyOf(
    const AutofillProfile& p1,
    const AutofillProfile& p2,
    AutofillType t) const {
  const std::u16string& s1 = p1.GetInfo(t, app_locale_);
  if (!s1.empty())
    return s1;
  return p2.GetInfo(t, app_locale_);
}

// static
std::set<std::u16string> AutofillProfileComparator::GetNamePartVariants(
    const std::u16string& name_part) {
  const size_t kMaxSupportedSubNames = 8;

  std::vector<std::u16string_view> sub_names = base::SplitStringPiece(
      name_part, kSpace, base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  // Limit the number of sub-names we support (to constrain memory usage);
  if (sub_names.size() > kMaxSupportedSubNames)
    return {name_part};

  // Start with the empty string as a variant.
  std::set<std::u16string> variants = {{}};

  // For each sub-name, add a variant of all the already existing variants that
  // appends this sub-name and one that appends the initial of this sub-name.
  // Duplicates will be discarded when they're added to the variants set.
  for (const auto& sub_name : sub_names) {
    if (sub_name.empty())
      continue;
    std::vector<std::u16string> new_variants;
    for (const std::u16string& variant : variants) {
      new_variants.push_back(base::CollapseWhitespace(
          base::JoinString({variant, sub_name}, kSpace), true));
      new_variants.push_back(base::CollapseWhitespace(
          base::JoinString({variant, sub_name.substr(0, 1)}, kSpace), true));
    }
    variants.insert(new_variants.begin(), new_variants.end());
  }

  // As a common case, also add the variant that just concatenates all of the
  // initials.
  std::u16string initials;
  for (const auto& sub_name : sub_names) {
    if (sub_name.empty())
      continue;
    initials.push_back(sub_name[0]);
  }
  variants.insert(initials);

  // And, we're done.
  return variants;
}

bool AutofillProfileComparator::HaveMergeableNames(
    const AutofillProfile& p1,
    const AutofillProfile& p2) const {
  std::u16string full_name_1 = p1.GetInfo(NAME_FULL, app_locale_);
  std::u16string full_name_2 = p2.GetInfo(NAME_FULL, app_locale_);

  if (HasOnlySkippableCharacters(full_name_1) ||
      HasOnlySkippableCharacters(full_name_2) ||
      Compare(full_name_1, full_name_2)) {
    return true;
  }

  // If the two names are just a permutation of each other, they are mergeable
  // for structured names.
  if (AreStringTokenEquivalent(full_name_1, full_name_2)) {
    return true;
  }

  std::u16string canon_full_name_1 = NormalizeForComparison(full_name_1);
  std::u16string canon_full_name_2 = NormalizeForComparison(full_name_2);

  // Is it reasonable to merge the names from p1 and p2.
  bool result = IsNameVariantOf(canon_full_name_1, canon_full_name_2) ||
                IsNameVariantOf(canon_full_name_2, canon_full_name_1);
  return result;
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
  return HasOnlySkippableCharacters(company_name_1) ||
         HasOnlySkippableCharacters(company_name_2) ||
         CompareTokens(NormalizeForComparison(company_name_1),
                       NormalizeForComparison(company_name_2)) !=
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
  if (HasOnlySkippableCharacters(raw_phone_1) ||
      HasOnlySkippableCharacters(raw_phone_2) || raw_phone_1 == raw_phone_2) {
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
      NOTREACHED_IN_MIGRATION();
      return false;
  }
}

bool AutofillProfileComparator::HaveMergeableAddresses(
    const AutofillProfile& p1,
    const AutofillProfile& p2) const {
  // Note that p1 is the newer address. Using p2 as the base.
  return p2.GetAddress().IsStructuredAddressMergeable(p1.GetAddress());
}

}  // namespace autofill
