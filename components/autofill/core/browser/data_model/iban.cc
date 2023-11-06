// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/iban.h"

#include <string>

#include "base/containers/fixed_flat_map.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
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

// This prefix and suffix length are for local-based IBANs only. Server-based
// IBANs should generally use the same value but the client will respect
// whatever it receives from the server.
static constexpr int kPrefixLength = 4;
static constexpr int kSuffixLength = 4;

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

Iban::Iban() : record_type_(RecordType::kUnknown) {}

Iban::Iban(const Guid& guid)
    : identifier_(guid), record_type_(RecordType::kLocalIban) {}

Iban::Iban(const InstrumentId& instrument_id)
    : identifier_(instrument_id), record_type_(RecordType::kServerIban) {}

Iban::Iban(const Iban& iban) : Iban() {
  operator=(iban);
}

Iban::~Iban() = default;

Iban& Iban::operator=(const Iban& iban) = default;

AutofillMetadata Iban::GetMetadata() const {
  CHECK_NE(record_type_, Iban::kUnknown);
  AutofillMetadata metadata = AutofillDataModel::GetMetadata();
  metadata.id = record_type_ == Iban::kLocalIban ? guid() : instrument_id();
  return metadata;
}

// static
bool Iban::IsValid(const std::u16string& value) {
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
bool Iban::IsIbanApplicableInCountry(const std::string& country_code) {
  auto* it = kCountryToIbanLength.find(country_code);
  return it != kCountryToIbanLength.end();
}

bool Iban::SetMetadata(const AutofillMetadata& metadata) {
  // Make sure the ids match.
  return metadata.id != guid() && AutofillDataModel::SetMetadata(metadata);
}

std::u16string Iban::GetRawInfo(ServerFieldType type) const {
  if (type == IBAN_VALUE) {
    return value_;
  }

  NOTREACHED();
  return std::u16string();
}

void Iban::SetRawInfoWithVerificationStatus(ServerFieldType type,
                                            const std::u16string& value,
                                            VerificationStatus status) {
  if (type == IBAN_VALUE) {
    set_value(value);
  } else {
    NOTREACHED() << "Attempting to set unknown info-type" << type;
  }
}

void Iban::GetSupportedTypes(ServerFieldTypeSet* supported_types) const {
  supported_types->insert(IBAN_VALUE);
}

bool Iban::IsEmpty(const std::string& app_locale) const {
  ServerFieldTypeSet types;
  GetNonEmptyTypes(app_locale, &types);
  return types.empty();
}

int Iban::Compare(const Iban& iban) const {
  if (identifier_ < iban.identifier_) {
    return -1;
  }

  if (identifier_ > iban.identifier_) {
    return 1;
  }

  int comparison = nickname_.compare(iban.nickname_);
  if (comparison != 0) {
    return comparison;
  }

  comparison = value_.compare(iban.value_);
  if (comparison != 0) {
    return comparison;
  }

  comparison = prefix_.compare(iban.prefix_);
  if (comparison != 0) {
    return comparison;
  }

  comparison = suffix_.compare(iban.suffix_);
  if (comparison != 0) {
    return comparison;
  }

  if (length_ != iban.length_) {
    return 1;
  }

  if (record_type_ != iban.record_type_) {
    return 1;
  }
  return 0;
}

bool Iban::operator==(const Iban& iban) const {
  return Compare(iban) == 0;
}

bool Iban::operator!=(const Iban& iban) const {
  return !operator==(iban);
}

void Iban::set_identifier(const absl::variant<Guid, InstrumentId>& identifier) {
  if (absl::holds_alternative<Guid>(identifier_)) {
    CHECK_NE(record_type_, kServerIban);
  } else {
    CHECK_EQ(record_type_, kServerIban);
  }
  identifier_ = identifier;
}

const std::string& Iban::guid() const {
  CHECK(absl::holds_alternative<Guid>(identifier_));
  return absl::get<Guid>(identifier_).value();
}

const std::string& Iban::instrument_id() const {
  CHECK(absl::holds_alternative<InstrumentId>(identifier_));
  return absl::get<InstrumentId>(identifier_).value();
}

void Iban::set_value(const std::u16string& value) {
  if (!IsValid(value)) {
    return;
  }
  CHECK_NE(record_type_, Iban::kServerIban);
  // Get rid of all separators in the value before storing.
  value_ = RemoveIbanSeparators(value);
  static_assert(
      base::ranges::min_element(kCountryToIbanLength, {},
                                [](const auto& entry) { return entry.second; })
          ->second >= kPrefixLength + kSuffixLength);
  // The `IsValid()` call above ensures we have a valid IBAN length. We should
  // never set the `kPrefixLength` and `kSuffixLength` in a way where they can
  // be longer than the total length of the IBAN.
  CHECK(value_.length() >= kPrefixLength + kSuffixLength);
  prefix_ = value_.substr(0, kPrefixLength);
  suffix_ = value_.substr(value_.length() - kSuffixLength);
  length_ = value_.length();
}

void Iban::set_nickname(const std::u16string& nickname) {
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

void Iban::set_prefix(std::u16string prefix) {
  CHECK_NE(record_type_, Iban::kLocalIban);
  prefix_ = std::move(prefix);
}

void Iban::set_suffix(std::u16string suffix) {
  CHECK_NE(record_type_, Iban::kLocalIban);
  suffix_ = std::move(suffix);
}

void Iban::set_length(int length) {
  CHECK_NE(record_type_, Iban::kLocalIban);
  length_ = length;
}

bool Iban::IsValid() {
  CHECK_NE(record_type_, RecordType::kUnknown);
  return record_type_ == kServerIban || IsValid(value_);
}

std::u16string Iban::GetIdentifierStringForAutofillDisplay(
    bool is_value_masked) const {
  // `value_` is expected to be empty for server-based IBANs. For local IBANs,
  // it might be empty in rare situations (e.g., keychain is locked).
  if (value_.empty() && record_type_ == kLocalIban) {
    return value_;
  }
  // Displaying the full IBAN value is not possible for server-based IBANs.
  CHECK(is_value_masked || record_type_ != Iban::kServerIban);
  CHECK(length_ >= int(prefix_.length() + suffix_.length()));

  // If masked IBAN value is needed, the IBAN identifier string can be
  // constructed by adding ellipsis dots in the middle based on the middle
  // length (which can be calculated from subtracting `prefix_.length()` and
  // `suffix_.length()` from `length_`). Otherwise, `iban_identifier` can be
  // directly set to the full value.
  std::u16string iban_identifier;
  if (is_value_masked) {
    iban_identifier = base::StrCat(
        {prefix_,
         std::u16string(length_ - prefix_.length() - suffix_.length(),
                        kEllipsisOneDot),
         suffix_});
  } else {
    iban_identifier = value_;
  }

  // Now that the IBAN identifier string has been constructed, the remaining
  // step is to add space separators.
  std::u16string output;
  output.reserve(length_ + (length_ - 1) / 4);
  for (int i = 0; i < length_; ++i) {
    if (i % 4 == 0 && i > 0) {
      output.push_back(kEllipsisOneSpace);
    }
    output.push_back(iban_identifier[i]);
  }

  return output;
}

std::u16string Iban::GetStrippedValue() const {
  return value_;
}

bool Iban::MatchesPrefixSuffixAndLength(const Iban& iban) const {
  // Unlike the `Compare()` function, which seeks an exact match between
  // `prefix_`, `suffix_`, and `length_`, the comparison performed here involves
  // matching the prefixes between each other and similarly comparing the
  // suffixes between each other This approach is adopted because the `prefix`,
  // `suffix`, and `length` received from the server are considered the
  // source of truth. Therefore, even if the values of `kPrefixLength` or
  // `kSuffixLength` change later, leading to differences in length between the
  // client and server, it remains essential to match substrings and identify
  // the matched IBAN.
  bool prefix_matched = base::StartsWith(prefix(), iban.prefix()) ||
                        base::StartsWith(iban.prefix(), prefix());
  if (!prefix_matched) {
    return false;
  }

  bool suffix_matched = base::EndsWith(suffix(), iban.suffix()) ||
                        base::EndsWith(iban.suffix(), suffix());
  if (!suffix_matched) {
    return false;
  }

  return length() == iban.length();
}

}  // namespace autofill
