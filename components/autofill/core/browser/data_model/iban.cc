// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/iban.h"

#include <string>

#include "base/containers/fixed_flat_map.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_model/autofill_metadata.h"
#include "components/autofill/core/common/autofill_regexes.h"

namespace autofill {

namespace {

// IBAN lengths taken from:
// https://en.wikipedia.org/wiki/International_Bank_Account_Number#IBAN_formats_by_country.
static constexpr auto kCountryToIbanLength =
    base::MakeFixedFlatMap<base::StringPiece, size_t>({
        {"AD", 24},  // Andorra
        {"AE", 23},  // United Arab Emirates
        {"AL", 28},  // Albania
        {"AT", 20},  // Austria
        {"AZ", 28},  // Azerbaijan
        {"BA", 20},  // Bosnia and Herzegovina
        {"BE", 16},  // Belgium
        {"BG", 22},  // Bulgaria
        {"BH", 22},  // Bahrain
        {"BR", 29},  // Brazil
        {"BY", 28},  // Belarus
        {"CH", 21},  // Switzerland
        {"CR", 22},  // Costa Rica
        {"CY", 28},  // Cyprus
        {"CZ", 24},  // Czech Republic
        {"DE", 22},  // Germany
        {"DK", 18},  // Denmark
        {"DO", 28},  // Dominican Republic
        {"EE", 20},  // Estonia
        {"EG", 29},  // Egypt
        {"ES", 24},  // Spain
        {"FI", 18},  // Finland
        {"FO", 18},  // Faroe Islands
        {"FR", 27},  // France
        {"GE", 22},  // Georgia
        {"GB", 22},  // United Kingdom
        {"GI", 23},  // Gibraltar
        {"GL", 18},  // Greenland
        {"GR", 27},  // Greece
        {"GT", 28},  // Guatemala
        {"HR", 21},  // Croatia
        {"HU", 28},  // Hungary
        {"IL", 23},  // Israel
        {"IQ", 23},  // Iraq
        {"IS", 26},  // Iceland
        {"IT", 27},  // Italy
        {"JO", 30},  // Jordan
        {"KW", 30},  // Kuwait
        {"KZ", 20},  // Kazakhstan
        {"LB", 28},  // Lebanon
        {"LC", 32},  // Saint Lucia
        {"LI", 21},  // Liechtenstein
        {"LT", 20},  // Lithuania
        {"LU", 20},  // Luxembourg
        {"LY", 25},  // Libya
        {"LV", 21},  // Latvia
        {"MC", 27},  // Monaco
        {"MD", 24},  // Moldova
        {"ME", 22},  // Montenegro
        {"MK", 19},  // North Macedonia
        {"MR", 27},  // Mauritania
        {"MT", 31},  // Malta
        {"MU", 30},  // Mauritius
        {"NL", 18},  // Netherlands
        {"PK", 24},  // Pakistan
        {"PL", 28},  // Poland
        {"PS", 29},  // Palestinian territories
        {"PT", 25},  // Portugal
        {"QA", 29},  // Qatar
        {"RO", 24},  // Romania
        {"RU", 33},  // Russia
        {"RS", 22},  // Serbia
        {"SA", 24},  // Saudi Arabia
        {"SC", 31},  // Seychelles
        {"SD", 18},  // Sudan
        {"SE", 24},  // Sweden
        {"SI", 19},  // Slovenia
        {"SK", 24},  // Slovakia
        {"SM", 27},  // San Marino
        {"ST", 25},  // São Tomé and Príncipe
        {"TN", 24},  // Tunisia
        {"TR", 26},  // Turkey
        {"SV", 28},  // El Salvador
        {"TL", 23},  // East Timor
        {"UA", 29},  // Ukraine
        {"VA", 22},  // Vatican City
        {"VG", 24},  // Virgin Islands, British
        {"XK", 20},  // Kosovo
    });

int GetIbanCountryToLength(base::StringPiece country_code) {
  auto* it = kCountryToIbanLength.find(country_code);
  if (it == kCountryToIbanLength.end()) {
    return 0;
  }
  return it->second;
}

// This method does the following steps:
// 1. Move the four initial characters to the end of the string.
// 2. Replace each letter in with two digits, thereby expanding the string,
//    where 'A' = 10, 'B' = 11, ..., 'Z' = 35.
// 3. Treat the converted string as decimal and get the remainder of it on
//    division by 97.
//
// The algorithm is from:
// https://en.wikipedia.org/wiki/International_Bank_Account_Number#Modulo_operation_on_IBAN
int GetRemainderOfIbanValue(const std::u16string& stripped_value) {
  // Move the four initial characters to the end of the string.
  // E.g., GB82WEST12345698765432 -> WEST12345698765432GB82
  std::string rearranged_value =
      base::UTF16ToUTF8(stripped_value.substr(4) + stripped_value.substr(0, 4));

  // Replace each letter in with two digits where 'A' = 10, 'B' = 11, ...,
  // 'Z' = 35.
  std::string iban_decimal_string;
  for (char iban_character : rearranged_value) {
    if (iban_character - 'A' >= 0 && iban_character - 'A' < 26) {
      iban_decimal_string.append(
          base::NumberToString(iban_character - 'A' + 10));
    } else {
      iban_decimal_string.push_back(iban_character);
    }
  }

  // Returns the remainder of `iban_decimal_string` on division by 97.
  // This function returns remainder of `iban_decimal_string` because of the
  // followings:
  // 1) 10^9 <= 2^32. Max int value is 2147483647 which has 10 digits, so 10^9
  //    <= 2^32.
  // 2) a % 97 < 10^2. The remainder of a given number divided by 97 must be
  // less than 10^2, otherwise, it can be divided further.
  // 3) If a, b and c are integers, then (a + b) % c = ((a % c) + b) % c.
  auto mod97 = [](base::StringPiece s) {
    DCHECK_LE(s.length(), 9u);
    uint32_t i = 0;
    bool success = base::StringToUint(s, &i);
    DCHECK(success);
    return i % 97;
  };
  int remainder = mod97(iban_decimal_string.substr(0, 9));
  std::string fragment = iban_decimal_string.substr(9);
  for (size_t i = 0; i < fragment.length(); i += 7) {
    remainder = mod97(base::NumberToString(remainder) + fragment.substr(i, 7));
  }
  return remainder;
}

std::u16string RemoveIbanSeparators(base::StringPiece16 value) {
  std::u16string stripped_value;
  base::RemoveChars(value, base::kWhitespaceUTF16, &stripped_value);
  return stripped_value;
}

}  // namespace

constexpr char16_t kCapitalizedIbanPattern[] =
    u"^[A-Z]{2}[0-9]{2}[A-Z0-9]{4}[0-9]{7}[A-Z0-9]{0,18}$";
// Unicode characters used in IBAN value obfuscation:
//  - \u2022 - Bullet.
//  - \u2006 - SIX-PER-EM SPACE (small space between bullets).
constexpr char16_t kEllipsisOneDot = u'\u2022';
constexpr char16_t kEllipsisOneSpace = u'\u2006';

IBAN::IBAN(const std::string& guid) : AutofillDataModel(guid) {}

IBAN::IBAN() : IBAN(base::Uuid::GenerateRandomV4().AsLowercaseString()) {}

IBAN::IBAN(const IBAN& iban) : IBAN() {
  operator=(iban);
}

IBAN::~IBAN() = default;

IBAN& IBAN::operator=(const IBAN& iban) = default;

AutofillMetadata IBAN::GetMetadata() const {
  AutofillMetadata metadata = AutofillDataModel::GetMetadata();
  metadata.id = guid();
  return metadata;
}

// static
bool IBAN::IsValid(const std::u16string& value) {
  std::u16string iban_value = RemoveIbanSeparators(value);
  iban_value = base::i18n::ToUpper(iban_value);
  // IBANs must be at least 16 digits and at most 33 digits long.
  if (iban_value.length() < 16 || iban_value.length() > 33) {
    return false;
  }

  // IBAN must match the regex pattern. Note that we made the IBAN uppercased,
  // so we only need to check against an uppercased pattern.
  if (!MatchesRegex<kCapitalizedIbanPattern>(iban_value)) {
    return false;
  }

  // IBAN length must match the length of IBANs in the country the IBAN is from.
  size_t iban_value_length =
      GetIbanCountryToLength(base::UTF16ToUTF8(iban_value.substr(0, 2)));
  if (iban_value_length == 0 || iban_value_length != iban_value.length()) {
    return false;
  }

  // IBAN decimal value must have a remainder of 1 when divided by 97.
  return GetRemainderOfIbanValue(iban_value) == 1;
}

// static
bool IBAN::IsIbanApplicableInCountry(const std::string& country_code) {
  auto* it = kCountryToIbanLength.find(country_code);
  return it != kCountryToIbanLength.end();
}

bool IBAN::SetMetadata(const AutofillMetadata& metadata) {
  // Make sure the ids match.
  return metadata.id != guid() && AutofillDataModel::SetMetadata(metadata);
}

bool IBAN::IsDeletable() const {
  return false;
}

std::u16string IBAN::GetRawInfo(ServerFieldType type) const {
  if (type == IBAN_VALUE) {
    return value_;
  }

  NOTREACHED();
  return std::u16string();
}

void IBAN::SetRawInfoWithVerificationStatus(ServerFieldType type,
                                            const std::u16string& value,
                                            VerificationStatus status) {
  if (type == IBAN_VALUE) {
    set_value(value);
  } else {
    NOTREACHED() << "Attempting to set unknown info-type" << type;
  }
}

void IBAN::GetSupportedTypes(ServerFieldTypeSet* supported_types) const {
  supported_types->insert(IBAN_VALUE);
}

bool IBAN::IsEmpty(const std::string& app_locale) const {
  ServerFieldTypeSet types;
  GetNonEmptyTypes(app_locale, &types);
  return types.empty();
}

int IBAN::Compare(const IBAN& iban) const {
  int comparison = server_id_.compare(iban.server_id_);
  if (comparison != 0) {
    return comparison;
  }

  comparison = nickname_.compare(iban.nickname_);
  if (comparison != 0) {
    return comparison;
  }

  return value_.compare(iban.value_);
}

bool IBAN::operator==(const IBAN& iban) const {
  return guid() == iban.guid() && Compare(iban) == 0;
}

bool IBAN::operator!=(const IBAN& iban) const {
  return !operator==(iban);
}

void IBAN::set_value(const std::u16string& value) {
  if (!IsValid(value)) {
    return;
  }
  // Get rid of all separators in the value before storing.
  value_ = RemoveIbanSeparators(value);
}

void IBAN::set_nickname(const std::u16string& nickname) {
  // First replace all tabs and newlines with whitespaces and store it as
  // |nickname_|.
  base::ReplaceChars(nickname, u"\t\r\n", u" ", &nickname_);
  // An additional step to collapse whitespaces, this step does:
  // 1. Trim leading and trailing whitespaces.
  // 2. All other whitespace sequences are converted to a single space.
  nickname_ =
      base::CollapseWhitespace(nickname_,
                               /*trim_sequences_with_line_breaks=*/true);
}

std::u16string IBAN::GetIdentifierStringForAutofillDisplay(
    bool is_value_masked) const {
  DCHECK(!value_.empty());
  const std::u16string stripped_value = GetStrippedValue();
  size_t value_length = stripped_value.size();
  auto ShouldMask = [&](size_t i) {
    // The first 2-letter country code and 2 IBAN check digits will stay
    // unmasked, the last four digits will be shown as-is too. The rest of the
    // digits will not be masked if `is_value_masked` is false
    return 4 <= i && i < value_length - 4 && is_value_masked;
  };

  std::u16string output;
  output.reserve(stripped_value.size() + (stripped_value.size() - 1) / 4);
  for (size_t i = 0; i < stripped_value.size(); ++i) {
    if (i % 4 == 0 && i > 0)
      output.push_back(kEllipsisOneSpace);
    output.push_back(ShouldMask(i) ? kEllipsisOneDot : stripped_value[i]);
  }
  return output;
}

std::u16string IBAN::GetStrippedValue() const {
  return value_;
}

}  // namespace autofill
