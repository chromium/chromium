// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_data_util.h"

#include <array>
#include <iterator>
#include <string_view>
#include <vector>

#include "base/containers/contains.h"
#include "base/i18n/char_iterator.h"
#include "base/no_destructor.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/webdata/autofill_table_utils.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/credit_card_network_identifiers.h"
#include "components/grit/components_scaled_resources.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/icu/source/common/unicode/uscript.h"
#include "third_party/re2/src/re2/re2.h"

namespace autofill::data_util {

using bit_field_type_groups::kAddress;
using bit_field_type_groups::kEmail;
using bit_field_type_groups::kName;
using bit_field_type_groups::kPhone;

namespace {

// Mappings from Chrome card networks to Payment Request API basic card payment
// spec networks and icons. Note that "generic" is not in the spec.
// https://w3c.github.io/webpayments-methods-card/#method-id
constexpr PaymentRequestData kPaymentRequestData[]{
    {autofill::kAmericanExpressCard, "amex", IDR_AUTOFILL_CC_AMEX,
     IDS_AUTOFILL_CC_AMEX},
    {autofill::kDinersCard, "diners", IDR_AUTOFILL_CC_DINERS,
     IDS_AUTOFILL_CC_DINERS},
    {autofill::kDiscoverCard, "discover", IDR_AUTOFILL_CC_DISCOVER,
     IDS_AUTOFILL_CC_DISCOVER},
    {autofill::kEloCard, "elo", IDR_AUTOFILL_CC_ELO, IDS_AUTOFILL_CC_ELO},
    {autofill::kJCBCard, "jcb", IDR_AUTOFILL_CC_JCB, IDS_AUTOFILL_CC_JCB},
    {autofill::kMasterCard, "mastercard", IDR_AUTOFILL_CC_MASTERCARD,
     IDS_AUTOFILL_CC_MASTERCARD},
    {autofill::kMirCard, "mir", IDR_AUTOFILL_CC_MIR, IDS_AUTOFILL_CC_MIR},
    {autofill::kTroyCard, "troy", IDR_AUTOFILL_CC_TROY, IDS_AUTOFILL_CC_TROY},
    {autofill::kUnionPay, "unionpay", IDR_AUTOFILL_CC_UNIONPAY,
     IDS_AUTOFILL_CC_UNION_PAY},
    {autofill::kVerveCard, "verve", IDR_AUTOFILL_CC_VERVE,
     IDS_AUTOFILL_CC_VERVE},
    {autofill::kVisaCard, "visa", IDR_AUTOFILL_CC_VISA, IDS_AUTOFILL_CC_VISA},
};

constexpr PaymentRequestData kPaymentRequestDataForNewNetworkImages[]{
    {autofill::kAmericanExpressCard, "amex", IDR_AUTOFILL_METADATA_CC_AMEX,
     IDS_AUTOFILL_CC_AMEX},
    {autofill::kDinersCard, "diners", IDR_AUTOFILL_METADATA_CC_DINERS,
     IDS_AUTOFILL_CC_DINERS},
    {autofill::kDiscoverCard, "discover", IDR_AUTOFILL_METADATA_CC_DISCOVER,
     IDS_AUTOFILL_CC_DISCOVER},
    {autofill::kEloCard, "elo", IDR_AUTOFILL_METADATA_CC_ELO,
     IDS_AUTOFILL_CC_ELO},
    {autofill::kJCBCard, "jcb", IDR_AUTOFILL_METADATA_CC_JCB,
     IDS_AUTOFILL_CC_JCB},
    {autofill::kMasterCard, "mastercard", IDR_AUTOFILL_METADATA_CC_MASTERCARD,
     IDS_AUTOFILL_CC_MASTERCARD},
    {autofill::kMirCard, "mir", IDR_AUTOFILL_METADATA_CC_MIR,
     IDS_AUTOFILL_CC_MIR},
    {autofill::kTroyCard, "troy", IDR_AUTOFILL_METADATA_CC_TROY,
     IDS_AUTOFILL_CC_TROY},
    {autofill::kUnionPay, "unionpay", IDR_AUTOFILL_METADATA_CC_UNIONPAY,
     IDS_AUTOFILL_CC_UNION_PAY},
    {autofill::kVerveCard, "verve", IDR_AUTOFILL_METADATA_CC_VERVE,
     IDS_AUTOFILL_CC_VERVE},
    {autofill::kVisaCard, "visa", IDR_AUTOFILL_METADATA_CC_VISA,
     IDS_AUTOFILL_CC_VISA},
};

constexpr PaymentRequestData kGenericPaymentRequestData = {
    autofill::kGenericCard, "generic", IDR_AUTOFILL_CC_GENERIC,
    IDS_AUTOFILL_CC_GENERIC};

constexpr PaymentRequestData kGenericPaymentRequestDataForNewNetworkImages = {
    autofill::kGenericCard, "generic", IDR_AUTOFILL_METADATA_CC_GENERIC,
    IDS_AUTOFILL_CC_GENERIC};

constexpr auto kNamePrefixes = std::to_array<std::string_view>(
    {"1lt",     "1st", "2lt", "2nd",    "3rd",  "admiral", "capt",
     "captain", "col", "cpt", "dr",     "gen",  "general", "lcdr",
     "lt",      "ltc", "ltg", "ltjg",   "maj",  "major",   "mg",
     "mr",      "mrs", "ms",  "pastor", "prof", "rep",     "reverend",
     "rev",     "sen", "st"});

constexpr auto kNameSuffixes = std::to_array<std::string_view>(
    {"b.a", "ba", "d.d.s", "dds", "i",   "ii",   "iii", "iv",
     "ix",  "jr", "m.a",   "m.d", "ma",  "md",   "ms",  "ph.d",
     "phd", "sr", "v",     "vi",  "vii", "viii", "x"});

constexpr auto kFamilyNamePrefixes = std::to_array<std::string_view>(
    {"d'", "de", "del", "den", "der", "di", "la", "le", "mc", "san", "st",
     "ter", "van", "von"});

// The common and non-ambiguous CJK surnames (last names) that have more than
// one character.
constexpr auto kCommonCjkMultiCharSurnames = std::to_array<std::u16string_view>(
    {// Korean, taken from the list of surnames:
     // https://ko.wikipedia.org/wiki/%ED%95%9C%EA%B5%AD%EC%9D%98_%EC%84%B1%EC%94%A8_%EB%AA%A9%EB%A1%9D
     u"남궁", u"사공", u"서문", u"선우", u"제갈", u"황보", u"독고", u"망절",
     // Chinese, taken from the top 10 Chinese 2-character surnames:
     // https://zh.wikipedia.org/wiki/%E8%A4%87%E5%A7%93#.E5.B8.B8.E8.A6.8B.E7.9A.84.E8.A4.87.E5.A7.93
     // Simplified Chinese (mostly mainland China)
     u"欧阳", u"令狐", u"皇甫", u"上官", u"司徒", u"诸葛", u"司马", u"宇文",
     u"呼延", u"端木",
     // Traditional Chinese (mostly Taiwan)
     u"張簡", u"歐陽", u"諸葛", u"申屠", u"尉遲", u"司馬", u"軒轅", u"夏侯"});

// All Korean surnames that have more than one character, even the
// rare/ambiguous ones.
constexpr auto kKoreanMultiCharSurnames = std::to_array<std::u16string_view>(
    {u"강전", u"남궁", u"독고", u"동방", u"망절", u"사공", u"서문", u"선우",
     u"소봉", u"어금", u"장곡", u"제갈", u"황목", u"황보"});

// Returns true if `set` contains `element`, modulo a final period.
bool ContainsString(base::span<const std::string_view> set,
                    std::u16string_view element) {
  if (!base::IsStringASCII(element))
    return false;

  std::u16string_view trimmed_element =
      base::TrimString(element, u".", base::TRIM_ALL);
  return std::ranges::any_of(
      set, [trimmed_element](std::string_view set_element) {
        return base::EqualsCaseInsensitiveASCII(trimmed_element, set_element);
      });
}

// Removes common name prefixes from `name_tokens`.
void StripPrefixes(std::vector<std::u16string_view>* name_tokens) {
  auto iter = name_tokens->begin();
  while (iter != name_tokens->end()) {
    if (!ContainsString(kNamePrefixes, *iter)) {
      break;
    }
    ++iter;
  }

  std::vector<std::u16string_view> copy_vector;
  copy_vector.assign(iter, name_tokens->end());
  *name_tokens = copy_vector;
}

// Removes common name suffixes from `name_tokens`.
void StripSuffixes(std::vector<std::u16string_view>* name_tokens) {
  while (!name_tokens->empty()) {
    if (!ContainsString(kNameSuffixes, name_tokens->back())) {
      break;
    }
    name_tokens->pop_back();
  }
}

// Find whether `name` starts with any of the strings from the array
// `prefixes`. The returned value is the length of the prefix found, or 0 if
// none is found.
size_t StartsWithAny(std::u16string_view name,
                     base::span<const std::u16string_view> prefixes) {
  for (std::u16string_view prefix : prefixes) {
    if (name.starts_with(prefix)) {
      return prefix.size();
    }
  }
  return 0;
}

// Returns true if |c| is a CJK (Chinese, Japanese, Korean) character, for any
// of the CJK alphabets.
bool IsCJKCharacter(UChar32 c) {
  UErrorCode error = U_ZERO_ERROR;
  switch (uscript_getScript(c, &error)) {
    case USCRIPT_HAN:  // CJK logographs, used by all 3 (but rarely for Korean)
    case USCRIPT_HANGUL:    // Korean alphabet
    case USCRIPT_KATAKANA:  // A Japanese syllabary
    case USCRIPT_HIRAGANA:  // A Japanese syllabary
    case USCRIPT_BOPOMOFO:  // Chinese semisyllabary, rarely used
      return true;
    default:
      return false;
  }
}

// Returns true if |c| is a Korean Hangul character.
bool IsHangulCharacter(UChar32 c) {
  UErrorCode error = U_ZERO_ERROR;
  return uscript_getScript(c, &error) == USCRIPT_HANGUL;
}

// Returns true if |name| looks like a Korean name, made up entirely of Hangul
// characters or spaces. |name| should already be confirmed to be a CJK name, as
// per |IsCJKName()|.
bool IsHangulName(std::u16string_view name) {
  for (base::i18n::UTF16CharIterator iter(name); !iter.end(); iter.Advance()) {
    UChar32 c = iter.get();
    if (!IsHangulCharacter(c) && !base::IsUnicodeWhitespace(c)) {
      return false;
    }
  }
  return true;
}

// Tries to split a Chinese, Japanese, or Korean name into its given name &
// surname parts, and puts the result in |parts|. If splitting did not work for
// whatever reason, returns false.
bool SplitCJKName(const std::vector<std::u16string_view>& name_tokens,
                  NameParts* parts) {
  // The convention for CJK languages is to put the surname (last name) first,
  // and the given name (first name) second. In a continuous text, there is
  // normally no space between the two parts of the name. When entering their
  // name into a field, though, some people add a space to disambiguate. CJK
  // names (almost) never have a middle name.
  if (name_tokens.size() == 1) {
    // There is no space between the surname and given name. Try to infer where
    // to separate between the two. Most Chinese and Korean surnames have only
    // one character, but there are a few that have 2. If the name does not
    // start with a surname from a known list, default to 1 character.
    //
    // TODO(crbug.com/40596226): Japanese names with no space will be mis-split,
    // since we don't have a list of Japanese last names. In the Han alphabet,
    // it might also be difficult for us to differentiate between Chinese &
    // Japanese names.
    const std::u16string_view& name = name_tokens.front();
    const bool is_korean = IsHangulName(name);
    const size_t surname_length = [&] {
      if (is_korean && name.size() > 3) {
        // 4-character Korean names are more likely to be 2/2 than 1/3, so use
        // the full list of Korean 2-char surnames. (instead of only the common
        // ones)
        return std::max<size_t>(1,
                                StartsWithAny(name, kKoreanMultiCharSurnames));
      }
      // Default to 1 character if the surname is not in
      // `kCommonCjkMultiCharSurnames`.
      return std::max<size_t>(1,
                              StartsWithAny(name, kCommonCjkMultiCharSurnames));
    }();
    parts->family = std::u16string(name.substr(0, surname_length));
    parts->given = std::u16string(name.substr(surname_length));
    return true;
  }
  if (name_tokens.size() == 2) {
    // The user entered a space between the two name parts. This makes our job
    // easier. Family name first, given name second.
    parts->family = std::u16string(name_tokens[0]);
    parts->given = std::u16string(name_tokens[1]);
    return true;
  }
  // We don't know what to do if there are more than 2 tokens.
  return false;
}

void AddGroupToBitmask(uint32_t* group_bitmask, FieldType type) {
  const FieldTypeGroup group = GroupTypeOfFieldType(type);
  switch (group) {
    case autofill::FieldTypeGroup::kName:
      *group_bitmask |= kName;
      break;
    case autofill::FieldTypeGroup::kAddress:
      *group_bitmask |= kAddress;
      break;
    case autofill::FieldTypeGroup::kEmail:
      *group_bitmask |= kEmail;
      break;
    case autofill::FieldTypeGroup::kPhone:
      *group_bitmask |= kPhone;
      break;
    default:
      break;
  }
}

}  // namespace

bool ContainsName(uint32_t groups) {
  return groups & kName;
}

bool ContainsAddress(uint32_t groups) {
  return groups & kAddress;
}

bool ContainsEmail(uint32_t groups) {
  return groups & kEmail;
}

bool ContainsPhone(uint32_t groups) {
  return groups & kPhone;
}

uint32_t DetermineGroups(const FormStructure& form) {
  uint32_t group_bitmask = 0;
  for (const auto& field : form) {
    FieldType type = field->Type().GetStorableType();
    AddGroupToBitmask(&group_bitmask, type);
  }
  return group_bitmask;
}

uint32_t DetermineGroups(const FieldTypeSet& types) {
  uint32_t group_bitmask = 0;
  for (const FieldType type : types) {
    AddGroupToBitmask(&group_bitmask, type);
  }
  return group_bitmask;
}

bool IsSupportedFormType(uint32_t groups) {
  return ContainsAddress(groups) ||
         ContainsName(groups) + ContainsEmail(groups) + ContainsPhone(groups) >=
             2;
}

std::string GetSuffixForProfileFormType(uint32_t bitmask) {
  switch (bitmask) {
    case kAddress | kEmail | kPhone:
    case kName | kAddress | kEmail | kPhone:
      return ".AddressPlusEmailPlusPhone";
    case kAddress | kPhone:
    case kName | kAddress | kPhone:
      return ".AddressPlusPhone";
    case kAddress | kEmail:
    case kName | kAddress | kEmail:
      return ".AddressPlusEmail";
    case kAddress:
    case kName | kAddress:
      return ".AddressOnly";
    case kEmail | kPhone:
    case kName | kEmail | kPhone:
    case kName | kEmail:
    case kName | kPhone:
      return ".ContactOnly";
    case kPhone:
      return ".PhoneOnly";
    default:
      return ".Other";
  }
}

std::string TruncateUTF8(const std::string& data) {
  std::string trimmed_value;
  base::TruncateUTF8ToByteSize(data, kMaxDataLengthForDatabase, &trimmed_value);
  return trimmed_value;
}

bool IsCreditCardExpirationType(FieldType type) {
  return type == CREDIT_CARD_EXP_MONTH ||
         type == CREDIT_CARD_EXP_2_DIGIT_YEAR ||
         type == CREDIT_CARD_EXP_4_DIGIT_YEAR ||
         type == CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR ||
         type == CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR;
}

bool IsCJKName(std::u16string_view name) {
  // The name is considered to be a CJK name if it is only CJK characters,
  // spaces, and "middle dot" separators, with at least one CJK character, and
  // no more than 2 words.
  //
  // Chinese and Japanese names are usually spelled out using the Han characters
  // (logographs), which constitute the "CJK Unified Ideographs" block in
  // Unicode, also referred to as Unihan. Korean names are usually spelled out
  // in the Korean alphabet (Hangul), although they do have a Han equivalent as
  // well.
  //
  // The middle dot is used as a separator for foreign names in Japanese.
  static constexpr char16_t kKatakanaMiddleDot = u'\u30FB';
  // A (common?) typo for 'KATAKANA MIDDLE DOT' (U+30FB).
  static constexpr char16_t kMiddleDot = u'\u00B7';
  bool previous_was_cjk = false;
  size_t word_count = 0;
  for (base::i18n::UTF16CharIterator iter(name); !iter.end(); iter.Advance()) {
    UChar32 c = iter.get();
    const bool is_cjk = IsCJKCharacter(c);
    if (!is_cjk && !base::IsUnicodeWhitespace(c) && c != kKatakanaMiddleDot &&
        c != kMiddleDot) {
      return false;
    }
    if (is_cjk && !previous_was_cjk) {
      word_count++;
    }
    previous_was_cjk = is_cjk;
  }
  return word_count > 0 && word_count < 3;
}

NameParts SplitName(std::u16string_view name) {
  // u' ',       // ASCII space.
  // u',',       // ASCII comma.
  // u'\u3000',  // 'IDEOGRAPHIC SPACE' (U+3000).
  // u'\u30FB',  // 'KATAKANA MIDDLE DOT' (U+30FB).
  // u'\u00B7',  // 'MIDDLE DOT' (U+00B7).
  static constexpr std::u16string_view kWordSeparators =
      u" ,\u3000\u30FB\u00B7";
  std::vector<std::u16string_view> name_tokens = base::SplitStringPiece(
      name, kWordSeparators, base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  StripPrefixes(&name_tokens);

  NameParts parts;

  // TODO(crbug.com/40596226): Hungarian, Tamil, Telugu, and Vietnamese also
  // have the given name before the surname, and should be treated as special
  // cases too.

  // Treat CJK names differently.
  if (IsCJKName(name) && SplitCJKName(name_tokens, &parts)) {
    return parts;
  }

  // Don't assume "Ma" is a suffix in John Ma.
  if (name_tokens.size() > 2)
    StripSuffixes(&name_tokens);

  if (name_tokens.empty()) {
    // Bad things have happened; just assume the whole thing is a given name.
    parts.given = std::u16string(name);
    return parts;
  }

  // Only one token, assume given name.
  if (name_tokens.size() == 1) {
    parts.given = std::u16string(name_tokens[0]);
    return parts;
  }

  // 2 or more tokens. Grab the family, which is the last word plus any
  // recognizable family prefixes.
  std::vector<std::u16string_view> reverse_family_tokens;
  reverse_family_tokens.push_back(name_tokens.back());
  name_tokens.pop_back();
  while (name_tokens.size() >= 1 &&
         ContainsString(kFamilyNamePrefixes, name_tokens.back())) {
    reverse_family_tokens.push_back(name_tokens.back());
    name_tokens.pop_back();
  }

  std::vector<std::u16string_view> family_tokens(reverse_family_tokens.rbegin(),
                                                 reverse_family_tokens.rend());
  parts.family = base::JoinString(family_tokens, u" ");

  // Take the last remaining token as the middle name (if there are at least 2
  // tokens).
  if (name_tokens.size() >= 2) {
    parts.middle = std::u16string(name_tokens.back());
    name_tokens.pop_back();
  }

  // Remainder is given name.
  parts.given = base::JoinString(name_tokens, u" ");

  return parts;
}

std::u16string JoinNameParts(std::u16string_view given,
                             std::u16string_view middle,
                             std::u16string_view family) {
  // First Middle Last
  std::vector<std::u16string_view> full_name;
  if (!given.empty())
    full_name.push_back(given);

  if (!middle.empty())
    full_name.push_back(middle);

  if (!family.empty())
    full_name.push_back(family);

  const char* separator = " ";
  if (IsCJKName(given) && IsCJKName(family) && middle.empty()) {
    // LastFirst
    std::reverse(full_name.begin(), full_name.end());
    separator = "";
  }

  return base::JoinString(full_name, base::ASCIIToUTF16(separator));
}

const PaymentRequestData& GetPaymentRequestData(
    const std::string& issuer_network) {
  bool use_new_data = base::FeatureList::IsEnabled(
      autofill::features::kAutofillEnableNewCardArtAndNetworkImages);

  for (const PaymentRequestData& data :
       use_new_data ? kPaymentRequestDataForNewNetworkImages
                    : kPaymentRequestData) {
    if (issuer_network == data.issuer_network)
      return data;
  }
  return use_new_data ? kGenericPaymentRequestDataForNewNetworkImages
                      : kGenericPaymentRequestData;
}

const char* GetIssuerNetworkForBasicCardIssuerNetwork(
    const std::string& basic_card_issuer_network) {
  bool use_new_data = base::FeatureList::IsEnabled(
      autofill::features::kAutofillEnableNewCardArtAndNetworkImages);

  for (const PaymentRequestData& data :
       use_new_data ? kPaymentRequestDataForNewNetworkImages
                    : kPaymentRequestData) {
    if (basic_card_issuer_network == data.basic_card_issuer_network) {
      return data.issuer_network;
    }
  }
  return use_new_data
             ? kGenericPaymentRequestDataForNewNetworkImages.issuer_network
             : kGenericPaymentRequestData.issuer_network;
}

bool IsValidBasicCardIssuerNetwork(
    const std::string& basic_card_issuer_network) {
  bool use_new_data = base::FeatureList::IsEnabled(
      autofill::features::kAutofillEnableNewCardArtAndNetworkImages);

  return base::Contains(use_new_data ? kPaymentRequestDataForNewNetworkImages
                                     : kPaymentRequestData,
                        basic_card_issuer_network,
                        &PaymentRequestData::basic_card_issuer_network);
}

bool IsValidCountryCode(const std::string& country_code) {
  if (country_code.size() != 2)
    return false;

  static const base::NoDestructor<re2::RE2> country_code_regex("^[A-Z]{2}$");
  return re2::RE2::FullMatch(country_code, *country_code_regex.get());
}

bool IsValidCountryCode(const std::u16string& country_code) {
  return IsValidCountryCode(base::UTF16ToUTF8(country_code));
}

std::string GetCountryCodeWithFallback(const autofill::AutofillProfile& profile,
                                       const std::string& app_locale) {
  std::string country_code =
      base::UTF16ToUTF8(profile.GetRawInfo(autofill::ADDRESS_HOME_COUNTRY));
  if (!IsValidCountryCode(country_code))
    country_code = AutofillCountry::CountryCodeForLocale(app_locale);
  return country_code;
}

}  // namespace autofill::data_util
