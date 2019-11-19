// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_profile_comparator.h"

#include <algorithm>
#include <vector>

#include "base/i18n/case_conversion.h"
#include "base/i18n/char_iterator.h"
#include "base/i18n/unicodestring.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/address_rewriter.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/geo/state_names.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "third_party/libphonenumber/phonenumber_api.h"

using base::UTF16ToUTF8;
using base::UTF8ToUTF16;
using i18n::phonenumbers::PhoneNumberUtil;

namespace autofill {
namespace {

const base::char16 kSpace[] = {L' ', L'\0'};

bool ContainsNewline(base::StringPiece16 text) {
  return text.find('\n') != base::StringPiece16::npos;
}

std::ostream& operator<<(std::ostream& os,
                         const ::i18n::phonenumbers::PhoneNumber& n) {
  os << "country_code: " << n.country_code() << " "
     << "national_number: " << n.national_number();
  if (n.has_extension())
    os << " extension: \"" << n.extension() << "\"";
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
      const base::StringPiece16& text,
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
  bool previous_was_skippable_;

  // True if punctuation and white space within the string should be collapsed
  // to a single white space.
  bool collapse_skippable_;

  base::i18n::UTF16CharIterator iter_;
};

NormalizingIterator::NormalizingIterator(
    const base::StringPiece16& text,
    AutofillProfileComparator::WhitespaceSpec whitespace_spec)
    : previous_was_skippable_(false),
      collapse_skippable_(whitespace_spec ==
                          AutofillProfileComparator::RETAIN_WHITESPACE),
      iter_(base::i18n::UTF16CharIterator(text.data(), text.length())) {
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

AutofillProfileComparator::AutofillProfileComparator(
    const base::StringPiece& app_locale)
    : app_locale_(app_locale.data(), app_locale.size()) {
  // Use ICU transliteration to remove diacritics and fold case.
  // See http://userguide.icu-project.org/transforms/general
  UErrorCode status = U_ZERO_ERROR;
  std::unique_ptr<icu::Transliterator> transliterator(
      icu::Transliterator::createInstance(
          "NFD; [:Nonspacing Mark:] Remove; Lower; NFC", UTRANS_FORWARD,
          status));
  if (U_FAILURE(status) || transliterator == nullptr) {
    // TODO(rogerm): Add a histogram to count how often this happens.
    LOG(ERROR) << "Failed to create ICU Transliterator: "
               << u_errorName(status);
  }

  transliterator_ = std::move(transliterator);
}

AutofillProfileComparator::~AutofillProfileComparator() {}

bool AutofillProfileComparator::Compare(base::StringPiece16 text1,
                                        base::StringPiece16 text2,
                                        WhitespaceSpec whitespace_spec) const {
  if (text1.empty() && text2.empty()) {
    return true;
  }

  NormalizingIterator normalizing_iter1{text1, whitespace_spec};
  NormalizingIterator normalizing_iter2{text2, whitespace_spec};

  while (!normalizing_iter1.End() && !normalizing_iter2.End()) {
    icu::UnicodeString char1 =
        icu::UnicodeString(normalizing_iter1.GetNextChar());
    icu::UnicodeString char2 =
        icu::UnicodeString(normalizing_iter2.GetNextChar());

    if (transliterator_ == nullptr) {
      if (char1.toLower() != char2.toLower()) {
        return false;
      }
    } else {
      transliterator_->transliterate(char1);
      transliterator_->transliterate(char2);
      if (char1 != char2) {
        return false;
      }
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
    base::StringPiece16 text) const {
  if (text.empty()) {
    return true;
  }

  return NormalizingIterator(text,
                             AutofillProfileComparator::DISCARD_WHITESPACE)
      .End();
}

base::string16 AutofillProfileComparator::NormalizeForComparison(
    base::StringPiece16 text,
    AutofillProfileComparator::WhitespaceSpec whitespace_spec) const {
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
  base::string16 result;
  result.reserve(text.length());
  bool previous_was_whitespace = (whitespace_spec == RETAIN_WHITESPACE);
  for (base::i18n::UTF16CharIterator iter(text.data(), text.length());
       !iter.end(); iter.Advance()) {
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

  if (transliterator_ == nullptr)
    return result;

  icu::UnicodeString value = icu::UnicodeString(result.data(), result.length());
  transliterator_->transliterate(value);
  return base::i18n::UnicodeStringToString16(value);
}

bool AutofillProfileComparator::AreMergeable(const AutofillProfile& p1,
                                             const AutofillProfile& p2) const {
  // Sorted in order to relative expense of the tests to fail early and cheaply
  // if possible.
  DVLOG(1) << "Comparing profiles:\np1 = " << p1 << "\np2 = " << p2;

  if (!HaveMergeableEmailAddresses(p1, p2)) {
    DVLOG(1) << "Different email addresses.";
    return false;
  }

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

  DVLOG(1) << "Profiles are mergeable.";
  return true;
}

bool AutofillProfileComparator::MergeNames(const AutofillProfile& p1,
                                           const AutofillProfile& p2,
                                           NameInfo* name_info) const {
  DCHECK(HaveMergeableNames(p1, p2));

  const AutofillType kFullName(NAME_FULL);
  const base::string16& full_name_1 = p1.GetInfo(kFullName, app_locale_);
  const base::string16& full_name_2 = p2.GetInfo(kFullName, app_locale_);

  const base::string16* best_name = nullptr;
  if (HasOnlySkippableCharacters(full_name_1)) {
    // p1 has no name, so use the name from p2.
    best_name = &full_name_2;
  } else if (HasOnlySkippableCharacters(full_name_2)) {
    // p2 has no name, so use the name from p1.
    best_name = &full_name_1;
  } else if (data_util::IsCJKName(full_name_1) &&
             data_util::IsCJKName(full_name_2)) {
    // Use a separate logic for CJK names.
    return MergeCJKNames(p1, p2, name_info);
  } else if (IsNameVariantOf(NormalizeForComparison(full_name_1),
                             NormalizeForComparison(full_name_2))) {
    // full_name_2 is a variant of full_name_1.
    best_name = &full_name_1;
  } else {
    // If the assertion that p1 and p2 have mergeable names is true, then
    // full_name_1 must be a name variant of full_name_2;
    best_name = &full_name_2;
  }

  name_info->SetInfo(AutofillType(NAME_FULL), *best_name, app_locale_);
  return true;
}

bool AutofillProfileComparator::MergeCJKNames(const AutofillProfile& p1,
                                              const AutofillProfile& p2,
                                              NameInfo* info) const {
  DCHECK(data_util::IsCJKName(p1.GetInfo(NAME_FULL, app_locale_)));
  DCHECK(data_util::IsCJKName(p2.GetInfo(NAME_FULL, app_locale_)));

  struct Name {
    base::string16 given;
    base::string16 surname;
    base::string16 full;
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
    info->SetInfo(AutofillType(NAME_FULL), full_name_candidate->full,
                  app_locale_);
  } else {
    // The name was already split into a given/surname, so keep those intact.
    if (!full_name_candidate->full.empty()) {
      info->SetRawInfo(NAME_FULL, full_name_candidate->full);
    }
    info->SetRawInfo(NAME_FIRST, name_parts_candidate->given);
    info->SetRawInfo(NAME_LAST, name_parts_candidate->surname);
  }

  return true;
}

bool AutofillProfileComparator::IsNameVariantOf(
    const base::string16& full_name_1,
    const base::string16& full_name_2) const {
  data_util::NameParts name_1_parts = data_util::SplitName(full_name_1);

  // Build the variants of full_name_1`s given, middle and family names.
  //
  // TODO(rogerm): Figure out whether or not we should break apart a compound
  // family name into variants (crbug/619051)
  const std::set<base::string16> given_name_variants =
      GetNamePartVariants(name_1_parts.given);
  const std::set<base::string16> middle_name_variants =
      GetNamePartVariants(name_1_parts.middle);
  base::StringPiece16 family_name = name_1_parts.family;

  // Iterate over all full name variants of profile 2 and see if any of them
  // match the full name from profile 1.
  for (const auto& given_name : given_name_variants) {
    for (const auto& middle_name : middle_name_variants) {
      base::string16 candidate = base::CollapseWhitespace(
          base::JoinString({given_name, middle_name, family_name}, kSpace),
          true);
      if (candidate == full_name_2)
        return true;
    }
  }

  // Also check if the name is just composed of the user's initials. For
  // example, "thomas jefferson miller" could be composed as "tj miller".
  if (!name_1_parts.given.empty() && !name_1_parts.middle.empty()) {
    base::string16 initials;
    initials.push_back(name_1_parts.given[0]);
    initials.push_back(name_1_parts.middle[0]);
    base::string16 candidate = base::CollapseWhitespace(
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
    EmailInfo* email_info) const {
  DCHECK(HaveMergeableEmailAddresses(p1, p2));

  const AutofillType kEmailAddress(EMAIL_ADDRESS);
  const base::string16& e1 = p1.GetInfo(kEmailAddress, app_locale_);
  const base::string16& e2 = p2.GetInfo(kEmailAddress, app_locale_);
  const base::string16* best = nullptr;

  if (e1.empty()) {
    best = &e2;
  } else if (e2.empty()) {
    best = &e1;
  } else {
    best = p2.use_date() > p1.use_date() ? &e2 : &e1;
  }

  email_info->SetInfo(kEmailAddress, *best, app_locale_);
  return true;
}

bool AutofillProfileComparator::MergeCompanyNames(
    const AutofillProfile& p1,
    const AutofillProfile& p2,
    CompanyInfo* company_info) const {
  const AutofillType kCompanyName(COMPANY_NAME);
  const base::string16& c1 = p1.GetInfo(kCompanyName, app_locale_);
  const base::string16& c2 = p2.GetInfo(kCompanyName, app_locale_);
  const base::string16* best = nullptr;

  DCHECK(HaveMergeableCompanyNames(p1, p2))
      << "Company names are not mergeable: '" << c1 << "' vs '" << c2 << "'";

  CompareTokensResult result =
      CompareTokens(NormalizeForComparison(c1), NormalizeForComparison(c2));
  switch (result) {
    case DIFFERENT_TOKENS:
    default:
      NOTREACHED() << "Unexpected mismatch: '" << c1 << "' vs '" << c2 << "'";
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

  company_info->SetInfo(kCompanyName, *best, app_locale_);
  return true;
}

bool AutofillProfileComparator::MergePhoneNumbers(
    const AutofillProfile& p1,
    const AutofillProfile& p2,
    PhoneNumber* phone_number) const {
  const ServerFieldType kWholePhoneNumber = PHONE_HOME_WHOLE_NUMBER;
  const base::string16& s1 = p1.GetRawInfo(kWholePhoneNumber);
  const base::string16& s2 = p2.GetRawInfo(kWholePhoneNumber);

  DCHECK(HaveMergeablePhoneNumbers(p1, p2))
      << "Phone numbers are not mergeable: '" << s1 << "' vs '" << s2 << "'";

  if (HasOnlySkippableCharacters(s1) && HasOnlySkippableCharacters(s2)) {
    phone_number->SetRawInfo(kWholePhoneNumber, base::string16());
  }

  if (HasOnlySkippableCharacters(s1)) {
    phone_number->SetRawInfo(kWholePhoneNumber, s2);
    return true;
  }

  if (HasOnlySkippableCharacters(s2) || s1 == s2) {
    phone_number->SetRawInfo(kWholePhoneNumber, s1);
    return true;
  }

  // Figure out a country code hint.
  const AutofillType kCountryCode(HTML_TYPE_COUNTRY_CODE, HTML_MODE_NONE);
  std::string region = UTF16ToUTF8(GetNonEmptyOf(p1, p2, kCountryCode));
  if (region.empty())
    region = AutofillCountry::CountryCodeForLocale(app_locale_);

  // Parse the phone numbers.
  PhoneNumberUtil* phone_util = PhoneNumberUtil::GetInstance();

  ::i18n::phonenumbers::PhoneNumber n1;
  if (phone_util->ParseAndKeepRawInput(UTF16ToUTF8(s1), region, &n1) !=
      PhoneNumberUtil::NO_PARSING_ERROR) {
    return false;
  }

  ::i18n::phonenumbers::PhoneNumber n2;
  if (phone_util->ParseAndKeepRawInput(UTF16ToUTF8(s2), region, &n2) !=
      PhoneNumberUtil::NO_PARSING_ERROR) {
    return false;
  }

  ::i18n::phonenumbers::PhoneNumber merged_number;
  DCHECK_EQ(n1.country_code(), n2.country_code());
  merged_number.set_country_code(n1.country_code());
  merged_number.set_national_number(
      std::max(n1.national_number(), n2.national_number()));
  if (n1.has_extension() && !n1.extension().empty()) {
    merged_number.set_extension(n1.extension());
  } else if (n2.has_extension() && !n2.extension().empty()) {
    merged_number.set_extension(n2.extension());
  }
  if (n1.has_italian_leading_zero() || n2.has_italian_leading_zero()) {
    merged_number.set_italian_leading_zero(n1.italian_leading_zero() ||
                                           n2.italian_leading_zero());
  }
  if (n1.has_number_of_leading_zeros() || n2.has_number_of_leading_zeros()) {
    merged_number.set_number_of_leading_zeros(
        std::max(n1.number_of_leading_zeros(), n2.number_of_leading_zeros()));
  }

  PhoneNumberUtil::PhoneNumberFormat format =
      region.empty() ? PhoneNumberUtil::NATIONAL
                     : PhoneNumberUtil::INTERNATIONAL;

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
      base::StartsWith(new_number, "+1", base::CompareCase::SENSITIVE)) {
    size_t offset = 2;  // The char just after "+1".
    while (offset < new_number.size() &&
           base::IsAsciiWhitespace(new_number[offset])) {
      ++offset;
    }
    new_number = new_number.substr(offset);
  }

  phone_number->SetRawInfo(kWholePhoneNumber, UTF8ToUTF16(new_number));
  return true;
}

bool AutofillProfileComparator::MergeAddresses(const AutofillProfile& p1,
                                               const AutofillProfile& p2,
                                               Address* address) const {
  DCHECK(HaveMergeableAddresses(p1, p2));

  // One of the countries is empty or they are the same modulo case, so we just
  // have to find the non-empty one, if any.
  const AutofillType kCountryCode(HTML_TYPE_COUNTRY_CODE, HTML_MODE_NONE);
  const base::string16& country_code =
      base::i18n::ToUpper(GetNonEmptyOf(p1, p2, kCountryCode));
  address->SetInfo(kCountryCode, country_code, app_locale_);

  // One of the zip codes is empty, they are the same, or one is a substring
  // of the other. We prefer the most recently used zip code.
  const AutofillType kZipCode(ADDRESS_HOME_ZIP);
  const base::string16& zip1 = p1.GetInfo(kZipCode, app_locale_);
  const base::string16& zip2 = p2.GetInfo(kZipCode, app_locale_);
  if (zip1.empty()) {
    address->SetInfo(kZipCode, zip2, app_locale_);
  } else if (zip2.empty()) {
    address->SetInfo(kZipCode, zip1, app_locale_);
  } else {
    address->SetInfo(kZipCode, (p2.use_date() > p1.use_date() ? zip2 : zip1),
                     app_locale_);
  }

  // One of the states is empty or one of the states has a subset of tokens from
  // the other. Pick the non-empty state that is shorter. This is usually the
  // abbreviated one.
  const AutofillType kState(ADDRESS_HOME_STATE);
  const base::string16& state1 = p1.GetInfo(kState, app_locale_);
  const base::string16& state2 = p2.GetInfo(kState, app_locale_);
  if (state1.empty()) {
    address->SetInfo(kState, state2, app_locale_);
  } else if (state2.empty()) {
    address->SetInfo(kState, state1, app_locale_);
  } else {
    address->SetInfo(kState, (state2.size() < state1.size() ? state2 : state1),
                     app_locale_);
  }

  AddressRewriter rewriter = AddressRewriter::ForCountryCode(country_code);

  // One of the cities is empty or one of the cities has a subset of tokens from
  // the other. Pick the city name with more tokens; this is usually the most
  // explicit one.
  const AutofillType kCity(ADDRESS_HOME_CITY);
  const base::string16& city1 = p1.GetInfo(kCity, app_locale_);
  const base::string16& city2 = p2.GetInfo(kCity, app_locale_);
  if (city1.empty()) {
    address->SetInfo(kCity, city2, app_locale_);
  } else if (city2.empty()) {
    address->SetInfo(kCity, city1, app_locale_);
  } else {
    // Prefer the one with more tokens, making sure to apply address
    // normalization and rewriting before doing the comparison.
    CompareTokensResult result =
        CompareTokens(rewriter.Rewrite(NormalizeForComparison(city1)),
                      rewriter.Rewrite(NormalizeForComparison(city2)));
    switch (result) {
      case SAME_TOKENS:
        // They have the same set of unique tokens. Let's pick the more recently
        // used one.
        address->SetInfo(kCity, (p2.use_date() > p1.use_date() ? city2 : city1),
                         app_locale_);
        break;
      case S1_CONTAINS_S2:
        // city1 has more unique tokens than city2.
        address->SetInfo(kCity, city1, app_locale_);
        break;
      case S2_CONTAINS_S1:
        // city2 has more unique tokens than city1.
        address->SetInfo(kCity, city2, app_locale_);
        break;
      case DIFFERENT_TOKENS:
      default:
        // The cities aren't mergeable and we shouldn't be doing any of
        // this.
        NOTREACHED() << "Unexpected mismatch: '" << city1 << "' vs '" << city2
                     << "'";
        return false;
    }
  }

  // One of the dependend localities is empty or one of the localities has a
  // subset of tokens from the other. Pick the locality name with more tokens;
  // this is usually the most explicit one.
  const AutofillType kDependentLocality(ADDRESS_HOME_DEPENDENT_LOCALITY);
  const base::string16& locality1 = p1.GetInfo(kDependentLocality, app_locale_);
  const base::string16& locality2 = p2.GetInfo(kDependentLocality, app_locale_);
  if (locality1.empty()) {
    address->SetInfo(kDependentLocality, locality2, app_locale_);
  } else if (locality2.empty()) {
    address->SetInfo(kDependentLocality, locality1, app_locale_);
  } else {
    // Prefer the one with more tokens, making sure to apply address
    // normalization and rewriting before doing the comparison.
    CompareTokensResult result =
        CompareTokens(rewriter.Rewrite(NormalizeForComparison(locality1)),
                      rewriter.Rewrite(NormalizeForComparison(locality2)));
    switch (result) {
      case SAME_TOKENS:
        // They have the same set of unique tokens. Let's pick the more recently
        // used one.
        address->SetInfo(
            kDependentLocality,
            (p2.use_date() > p1.use_date() ? locality2 : locality1),
            app_locale_);
        break;
      case S1_CONTAINS_S2:
        // locality1 has more unique tokens than locality2.
        address->SetInfo(kDependentLocality, locality1, app_locale_);
        break;
      case S2_CONTAINS_S1:
        // locality2 has more unique tokens than locality1.
        address->SetInfo(kDependentLocality, locality2, app_locale_);
        break;
      case DIFFERENT_TOKENS:
      default:
        // The localities aren't mergeable and we shouldn't be doing any of
        // this.
        NOTREACHED() << "Unexpected mismatch: '" << locality1 << "' vs '"
                     << locality2 << "'";
        return false;
    }
  }

  // One of the sorting codes is empty, they are the same, or one is a substring
  // of the other. We prefer the most recently used sorting code.
  const AutofillType kSortingCode(ADDRESS_HOME_SORTING_CODE);
  const base::string16& sorting1 = p1.GetInfo(kSortingCode, app_locale_);
  const base::string16& sorting2 = p2.GetInfo(kSortingCode, app_locale_);
  if (sorting1.empty()) {
    address->SetInfo(kSortingCode, sorting2, app_locale_);
  } else if (sorting2.empty()) {
    address->SetInfo(kSortingCode, sorting1, app_locale_);
  } else {
    address->SetInfo(kSortingCode,
                     (p2.use_date() > p1.use_date() ? sorting2 : sorting1),
                     app_locale_);
  }

  // One of the addresses is empty or one of the addresses has a subset of
  // tokens from the other. Prefer the more verbosely expressed one.
  const AutofillType kStreetAddress(ADDRESS_HOME_STREET_ADDRESS);
  const base::string16& address1 = p1.GetInfo(kStreetAddress, app_locale_);
  const base::string16& address2 = p2.GetInfo(kStreetAddress, app_locale_);
  // If one of the addresses is empty then use the other.
  if (address1.empty()) {
    address->SetInfo(kStreetAddress, address2, app_locale_);
  } else if (address2.empty()) {
    address->SetInfo(kStreetAddress, address1, app_locale_);
  } else {
    // Prefer the multi-line address if one is multi-line and the other isn't.
    bool address1_multiline = ContainsNewline(address1);
    bool address2_multiline = ContainsNewline(address2);
    if (address1_multiline && !address2_multiline) {
      address->SetInfo(kStreetAddress, address1, app_locale_);
    } else if (address2_multiline && !address1_multiline) {
      address->SetInfo(kStreetAddress, address2, app_locale_);
    } else {
      // Prefer the one with more tokens if they're both single-line or both
      // multi-line addresses, making sure to apply address normalization and
      // rewriting before doing the comparison.
      CompareTokensResult result =
          CompareTokens(rewriter.Rewrite(NormalizeForComparison(address1)),
                        rewriter.Rewrite(NormalizeForComparison(address2)));
      switch (result) {
        case SAME_TOKENS:
          // They have the same set of unique tokens. Let's pick the one that's
          // longer.
          address->SetInfo(
              kStreetAddress,
              (p2.use_date() > p1.use_date() ? address2 : address1),
              app_locale_);
          break;
        case S1_CONTAINS_S2:
          // address1 has more unique tokens than address2.
          address->SetInfo(kStreetAddress, address1, app_locale_);
          break;
        case S2_CONTAINS_S1:
          // address2 has more unique tokens than address1.
          address->SetInfo(kStreetAddress, address2, app_locale_);
          break;
        case DIFFERENT_TOKENS:
        default:
          // The addresses aren't mergeable and we shouldn't be doing any of
          // this.
          NOTREACHED() << "Unexpected mismatch: '" << address1 << "' vs '"
                       << address2 << "'";
          return false;
      }
    }
  }
  return true;
}

// static
std::string AutofillProfileComparator::MergeProfile(
    const AutofillProfile& new_profile,
    std::vector<std::unique_ptr<AutofillProfile>>* existing_profiles,
    const std::string& app_locale,
    std::vector<AutofillProfile>* merged_profiles) {
  merged_profiles->clear();

  // Sort the existing profiles in decreasing order of frecency, so the "best"
  // profiles are checked first. Put the verified profiles last so the non
  // verified profiles get deduped among themselves before reaching the verified
  // profiles.
  // TODO(crbug.com/620521): Remove the check for verified from the sort.
  base::Time comparison_time = AutofillClock::Now();
  std::sort(existing_profiles->begin(), existing_profiles->end(),
            [comparison_time](const std::unique_ptr<AutofillProfile>& a,
                              const std::unique_ptr<AutofillProfile>& b) {
              if (a->IsVerified() != b->IsVerified())
                return !a->IsVerified();
              return a->HasGreaterFrecencyThan(b.get(), comparison_time);
            });

  // Set to true if |existing_profiles| already contains an equivalent profile.
  bool matching_profile_found = false;
  std::string guid = new_profile.guid();

  // If we have already saved this address, merge in any missing values.
  // Only merge with the first match. Merging the new profile into the existing
  // one preserves the validity of credit card's billing address reference.
  AutofillProfileComparator comparator(app_locale);
  for (const auto& existing_profile : *existing_profiles) {
    if (!matching_profile_found &&
        comparator.AreMergeable(new_profile, *existing_profile) &&
        existing_profile->SaveAdditionalInfo(new_profile, app_locale)) {
      // Unverified profiles should always be updated with the newer data,
      // whereas verified profiles should only ever be overwritten by verified
      // data.  If an automatically aggregated profile would overwrite a
      // verified profile, just drop it.
      matching_profile_found = true;
      guid = existing_profile->guid();

      // We set the modification date so that immediate requests for profiles
      // will properly reflect the fact that this profile has been modified
      // recently. After writing to the database and refreshing the local copies
      // the profile will have a very slightly newer time reflecting what's
      // actually stored in the database.
      existing_profile->set_modification_date(AutofillClock::Now());
    }
    merged_profiles->push_back(*existing_profile);
  }

  // If the new profile was not merged with an existing one, add it to the list.
  if (!matching_profile_found) {
    merged_profiles->push_back(new_profile);
    // Similar to updating merged profiles above, set the modification date on
    // new profiles.
    merged_profiles->back().set_modification_date(AutofillClock::Now());
    AutofillMetrics::LogProfileActionOnFormSubmitted(
        AutofillMetrics::NEW_PROFILE_CREATED);
  }

  return guid;
}

// static
std::set<base::StringPiece16> AutofillProfileComparator::UniqueTokens(
    base::StringPiece16 s) {
  std::vector<base::StringPiece16> tokens = base::SplitStringPiece(
      s, kSpace, base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  return std::set<base::StringPiece16>(tokens.begin(), tokens.end());
}

// static
AutofillProfileComparator::CompareTokensResult
AutofillProfileComparator::CompareTokens(base::StringPiece16 s1,
                                         base::StringPiece16 s2) {
  // Note: std::include() expects the items in each range to be in sorted order,
  // hence the use of std::set<> instead of std::unordered_set<>.
  std::set<base::StringPiece16> t1 = UniqueTokens(s1);
  std::set<base::StringPiece16> t2 = UniqueTokens(s2);

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

base::string16 AutofillProfileComparator::GetNonEmptyOf(
    const AutofillProfile& p1,
    const AutofillProfile& p2,
    AutofillType t) const {
  const base::string16& s1 = p1.GetInfo(t, app_locale_);
  if (!s1.empty())
    return s1;
  return p2.GetInfo(t, app_locale_);
}

// static
std::set<base::string16> AutofillProfileComparator::GetNamePartVariants(
    const base::string16& name_part) {
  const size_t kMaxSupportedSubNames = 8;

  std::vector<base::StringPiece16> sub_names = base::SplitStringPiece(
      name_part, kSpace, base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  // Limit the number of sub-names we support (to constrain memory usage);
  if (sub_names.size() > kMaxSupportedSubNames)
    return {name_part};

  // Start with the empty string as a variant.
  std::set<base::string16> variants = {{}};

  // For each sub-name, add a variant of all the already existing variants that
  // appends this sub-name and one that appends the initial of this sub-name.
  // Duplicates will be discarded when they're added to the variants set.
  for (const auto& sub_name : sub_names) {
    if (sub_name.empty())
      continue;
    std::vector<base::string16> new_variants;
    for (const base::string16& variant : variants) {
      new_variants.push_back(base::CollapseWhitespace(
          base::JoinString({variant, sub_name}, kSpace), true));
      new_variants.push_back(base::CollapseWhitespace(
          base::JoinString({variant, sub_name.substr(0, 1)}, kSpace), true));
    }
    variants.insert(new_variants.begin(), new_variants.end());
  }

  // As a common case, also add the variant that just concatenates all of the
  // initials.
  base::string16 initials;
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
  base::string16 full_name_1 = p1.GetInfo(NAME_FULL, app_locale_);
  base::string16 full_name_2 = p2.GetInfo(NAME_FULL, app_locale_);

  if (HasOnlySkippableCharacters(full_name_1) ||
      HasOnlySkippableCharacters(full_name_2) ||
      Compare(full_name_1, full_name_2)) {
    return true;
  }

  base::string16 canon_full_name_1 = NormalizeForComparison(full_name_1);
  base::string16 canon_full_name_2 = NormalizeForComparison(full_name_2);

  // Is it reasonable to merge the names from p1 and p2.
  return IsNameVariantOf(canon_full_name_1, canon_full_name_2) ||
         IsNameVariantOf(canon_full_name_2, canon_full_name_1);
}

bool AutofillProfileComparator::HaveMergeableEmailAddresses(
    const AutofillProfile& p1,
    const AutofillProfile& p2) const {
  const base::string16& email_1 = p1.GetInfo(EMAIL_ADDRESS, app_locale_);
  const base::string16& email_2 = p2.GetInfo(EMAIL_ADDRESS, app_locale_);
  return email_1.empty() || email_2.empty() ||
         case_insensitive_compare_.StringsEqual(email_1, email_2);
}

bool AutofillProfileComparator::HaveMergeableCompanyNames(
    const AutofillProfile& p1,
    const AutofillProfile& p2) const {
  const base::string16& company_name_1 = p1.GetInfo(COMPANY_NAME, app_locale_);
  const base::string16& company_name_2 = p2.GetInfo(COMPANY_NAME, app_locale_);
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
  const base::string16& raw_phone_1 = p1.GetRawInfo(PHONE_HOME_WHOLE_NUMBER);
  const base::string16& raw_phone_2 = p2.GetRawInfo(PHONE_HOME_WHOLE_NUMBER);

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
      NOTREACHED();
      return false;
  }
}

bool AutofillProfileComparator::HaveMergeableAddresses(
    const AutofillProfile& p1,
    const AutofillProfile& p2) const {
  // If the address are not in the same country, then they're not the same. If
  // one of the address countries is unknown/invalid the comparison continues.
  const AutofillType kCountryCode(HTML_TYPE_COUNTRY_CODE, HTML_MODE_NONE);
  const base::string16& country1 = p1.GetInfo(kCountryCode, app_locale_);
  const base::string16& country2 = p2.GetInfo(kCountryCode, app_locale_);
  if (!country1.empty() && !country2.empty() &&
      !case_insensitive_compare_.StringsEqual(country1, country2)) {
    return false;
  }

  // Zip
  // ----
  // If the addresses are definitely not in the same zip/area code then we're
  // done. Otherwise,the comparison continues.
  const AutofillType kZipCode(ADDRESS_HOME_ZIP);
  const base::string16& zip1 = NormalizeForComparison(
      p1.GetInfo(kZipCode, app_locale_), DISCARD_WHITESPACE);
  const base::string16& zip2 = NormalizeForComparison(
      p2.GetInfo(kZipCode, app_locale_), DISCARD_WHITESPACE);
  if (!zip1.empty() && !zip2.empty() &&
      zip1.find(zip2) == base::string16::npos &&
      zip2.find(zip1) == base::string16::npos) {
    return false;
  }

  // Use the token rewrite rules for the (common) country of the address to
  // transform equivalent substrings to a representative token for comparison.
  AddressRewriter rewriter =
      AddressRewriter::ForCountryCode(country1.empty() ? country2 : country1);

  // State
  // ------
  // Heuristic: States are mergeable if one is a (possibly empty) bag of words
  // subset of the other.
  //
  // TODO(rogerm): If the match is between non-empty zip codes then we can infer
  // that the two state strings are intended to have the same meaning. This
  // handles the cases where we have invalid or poorly formed data in one of the
  // state values (like "Select one", or "CA - California").
  const AutofillType kState(ADDRESS_HOME_STATE);
  const base::string16& state1 =
      rewriter.Rewrite(NormalizeForComparison(p1.GetInfo(kState, app_locale_)));
  const base::string16& state2 =
      rewriter.Rewrite(NormalizeForComparison(p2.GetInfo(kState, app_locale_)));
  if (CompareTokens(state1, state2) == DIFFERENT_TOKENS) {
    return false;
  }

  // City
  // ------
  // Heuristic: Cities are mergeable if one is a (possibly empty) bag of words
  // subset of the other.
  //
  // TODO(rogerm): If the match is between non-empty zip codes then we can infer
  // that the two city strings are intended to have the same meaning. This
  // handles the cases where we have a city vs one of its suburbs.
  const AutofillType kCity(ADDRESS_HOME_CITY);
  const base::string16& city1 =
      rewriter.Rewrite(NormalizeForComparison(p1.GetInfo(kCity, app_locale_)));
  const base::string16& city2 =
      rewriter.Rewrite(NormalizeForComparison(p2.GetInfo(kCity, app_locale_)));
  if (CompareTokens(city1, city2) == DIFFERENT_TOKENS) {
    return false;
  }

  // Dependent Locality
  // -------------------
  // Heuristic: Dependent Localities are mergeable if one is a (possibly empty)
  // bag of words subset of the other.
  const AutofillType kDependentLocality(ADDRESS_HOME_DEPENDENT_LOCALITY);
  const base::string16& locality1 = rewriter.Rewrite(
      NormalizeForComparison(p1.GetInfo(kDependentLocality, app_locale_)));
  const base::string16& locality2 = rewriter.Rewrite(
      NormalizeForComparison(p2.GetInfo(kDependentLocality, app_locale_)));
  if (CompareTokens(locality1, locality2) == DIFFERENT_TOKENS) {
    return false;
  }

  // Sorting Code
  // -------------
  // Heuristic: Sorting codes are mergeable if one is empty or one is a
  // substring of the other, post normalization and whitespace removed. This
  // is similar to postal/zip codes.
  const AutofillType kSortingCode(ADDRESS_HOME_SORTING_CODE);
  const base::string16& sorting1 = NormalizeForComparison(
      p1.GetInfo(kSortingCode, app_locale_), DISCARD_WHITESPACE);
  const base::string16& sorting2 = NormalizeForComparison(
      p2.GetInfo(kSortingCode, app_locale_), DISCARD_WHITESPACE);
  if (!sorting1.empty() && !sorting2.empty() &&
      sorting1.find(sorting2) == base::string16::npos &&
      sorting2.find(sorting1) == base::string16::npos) {
    return false;
  }

  // Address
  // --------
  // Heuristic: Street addresses are mergeable if one is a (possibly empty) bag
  // of words subset of the other.
  const base::string16& address1 = rewriter.Rewrite(NormalizeForComparison(
      p1.GetInfo(ADDRESS_HOME_STREET_ADDRESS, app_locale_)));
  const base::string16& address2 = rewriter.Rewrite(NormalizeForComparison(
      p2.GetInfo(ADDRESS_HOME_STREET_ADDRESS, app_locale_)));
  if (CompareTokens(address1, address2) == DIFFERENT_TOKENS) {
    return false;
  }

  return true;
}

}  // namespace autofill
