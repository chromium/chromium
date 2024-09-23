// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/iban.h"

#include <string_view>

#include "base/containers/fixed_flat_map.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_model/payments_metadata.h"
#include "components/autofill/core/browser/metrics/payments/iban_metrics.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_regexes.h"

namespace autofill {

namespace {

// This prefix and suffix length are for local-based IBANs only. Server-based
// IBANs should generally use the same value but the client will respect
// whatever it receives from the server.
static constexpr int kPrefixLength = 2;
static constexpr int kSuffixLength = 4;

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
  auto mod97 = [](std::string_view s) {
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

std::u16string RemoveIbanSeparators(std::u16string_view value) {
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

PaymentsMetadata Iban::GetMetadata() const {
  CHECK_NE(record_type_, Iban::kUnknown);
  PaymentsMetadata metadata(*this);
  metadata.id = record_type_ == Iban::kLocalIban
                    ? guid()
                    : base::NumberToString(instrument_id());
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
  const std::string country_code = base::UTF16ToUTF8(iban_value.substr(0, 2));
  if (!IsIbanApplicableInCountry(country_code) ||
      GetLengthOfIbanCountry(GetIbanSupportedCountry(country_code)) !=
          iban_value.length()) {
    return false;
  }

  // IBAN decimal value must have a remainder of 1 when divided by 97.
  return GetRemainderOfIbanValue(iban_value) == 1;
}

// static
std::string Iban::GetCountryCode(const std::u16string& iban_value) {
  CHECK(iban_value.length() >= 2);
  return base::UTF16ToUTF8(base::i18n::ToUpper(iban_value.substr(0, 2)));
}

// static
bool Iban::IsIbanApplicableInCountry(const std::string& country_code) {
  return GetIbanSupportedCountry(country_code) !=
         IbanSupportedCountry::kUnsupported;
}

// static
Iban::IbanSupportedCountry Iban::GetIbanSupportedCountry(
    std::string_view country_code) {
  if (country_code == "AD") {
    return IbanSupportedCountry::kAD;
  } else if (country_code == "AE") {
    return IbanSupportedCountry::kAE;
  } else if (country_code == "AL") {
    return IbanSupportedCountry::kAL;
  } else if (country_code == "AT") {
    return IbanSupportedCountry::kAT;
  } else if (country_code == "AZ") {
    return IbanSupportedCountry::kAZ;
  } else if (country_code == "BA") {
    return IbanSupportedCountry::kBA;
  } else if (country_code == "BE") {
    return IbanSupportedCountry::kBE;
  } else if (country_code == "BG") {
    return IbanSupportedCountry::kBG;
  } else if (country_code == "BH") {
    return IbanSupportedCountry::kBH;
  } else if (country_code == "BR") {
    return IbanSupportedCountry::kBR;
  } else if (country_code == "BY") {
    return IbanSupportedCountry::kBY;
  } else if (country_code == "CH") {
    return IbanSupportedCountry::kCH;
  } else if (country_code == "CR") {
    return IbanSupportedCountry::kCR;
  } else if (country_code == "CY") {
    return IbanSupportedCountry::kCY;
  } else if (country_code == "CZ") {
    return IbanSupportedCountry::kCZ;
  } else if (country_code == "DE") {
    return IbanSupportedCountry::kDE;
  } else if (country_code == "DK") {
    return IbanSupportedCountry::kDK;
  } else if (country_code == "DO") {
    return IbanSupportedCountry::kDO;
  } else if (country_code == "EE") {
    return IbanSupportedCountry::kEE;
  } else if (country_code == "EG") {
    return IbanSupportedCountry::kEG;
  } else if (country_code == "ES") {
    return IbanSupportedCountry::kES;
  } else if (country_code == "FI") {
    return IbanSupportedCountry::kFI;
  } else if (country_code == "FO") {
    return IbanSupportedCountry::kFO;
  } else if (country_code == "FR") {
    return IbanSupportedCountry::kFR;
  } else if (country_code == "GB") {
    return IbanSupportedCountry::kGB;
  } else if (country_code == "GE") {
    return IbanSupportedCountry::kGE;
  } else if (country_code == "GI") {
    return IbanSupportedCountry::kGI;
  } else if (country_code == "GL") {
    return IbanSupportedCountry::kGL;
  } else if (country_code == "GR") {
    return IbanSupportedCountry::kGR;
  } else if (country_code == "GT") {
    return IbanSupportedCountry::kGT;
  } else if (country_code == "HR") {
    return IbanSupportedCountry::kHR;
  } else if (country_code == "HU") {
    return IbanSupportedCountry::kHU;
  } else if (country_code == "IL") {
    return IbanSupportedCountry::kIL;
  } else if (country_code == "IQ") {
    return IbanSupportedCountry::kIQ;
  } else if (country_code == "IS") {
    return IbanSupportedCountry::kIS;
  } else if (country_code == "IT") {
    return IbanSupportedCountry::kIT;
  } else if (country_code == "JO") {
    return IbanSupportedCountry::kJO;
  } else if (country_code == "KW") {
    return IbanSupportedCountry::kKW;
  } else if (country_code == "KZ") {
    return IbanSupportedCountry::kKZ;
  } else if (country_code == "LB") {
    return IbanSupportedCountry::kLB;
  } else if (country_code == "LC") {
    return IbanSupportedCountry::kLC;
  } else if (country_code == "LI") {
    return IbanSupportedCountry::kLI;
  } else if (country_code == "LT") {
    return IbanSupportedCountry::kLT;
  } else if (country_code == "LU") {
    return IbanSupportedCountry::kLU;
  } else if (country_code == "LV") {
    return IbanSupportedCountry::kLV;
  } else if (country_code == "LY") {
    return IbanSupportedCountry::kLY;
  } else if (country_code == "MC") {
    return IbanSupportedCountry::kMC;
  } else if (country_code == "MD") {
    return IbanSupportedCountry::kMD;
  } else if (country_code == "ME") {
    return IbanSupportedCountry::kME;
  } else if (country_code == "MK") {
    return IbanSupportedCountry::kMK;
  } else if (country_code == "MR") {
    return IbanSupportedCountry::kMR;
  } else if (country_code == "MT") {
    return IbanSupportedCountry::kMT;
  } else if (country_code == "MU") {
    return IbanSupportedCountry::kMU;
  } else if (country_code == "NL") {
    return IbanSupportedCountry::kNL;
  } else if (country_code == "PK") {
    return IbanSupportedCountry::kPK;
  } else if (country_code == "PL") {
    return IbanSupportedCountry::kPL;
  } else if (country_code == "PS") {
    return IbanSupportedCountry::kPS;
  } else if (country_code == "PT") {
    return IbanSupportedCountry::kPT;
  } else if (country_code == "QA") {
    return IbanSupportedCountry::kQA;
  } else if (country_code == "RO") {
    return IbanSupportedCountry::kRO;
  } else if (country_code == "RS") {
    return IbanSupportedCountry::kRS;
  } else if (country_code == "RU") {
    return IbanSupportedCountry::kRU;
  } else if (country_code == "SA") {
    return IbanSupportedCountry::kSA;
  } else if (country_code == "SC") {
    return IbanSupportedCountry::kSC;
  } else if (country_code == "SD") {
    return IbanSupportedCountry::kSD;
  } else if (country_code == "SE") {
    return IbanSupportedCountry::kSE;
  } else if (country_code == "SI") {
    return IbanSupportedCountry::kSI;
  } else if (country_code == "SK") {
    return IbanSupportedCountry::kSK;
  } else if (country_code == "SM") {
    return IbanSupportedCountry::kSM;
  } else if (country_code == "ST") {
    return IbanSupportedCountry::kST;
  } else if (country_code == "SV") {
    return IbanSupportedCountry::kSV;
  } else if (country_code == "TL") {
    return IbanSupportedCountry::kTL;
  } else if (country_code == "TN") {
    return IbanSupportedCountry::kTN;
  } else if (country_code == "TR") {
    return IbanSupportedCountry::kTR;
  } else if (country_code == "UA") {
    return IbanSupportedCountry::kUA;
  } else if (country_code == "VA") {
    return IbanSupportedCountry::kVA;
  } else if (country_code == "VG") {
    return IbanSupportedCountry::kVG;
  } else if (country_code == "XK") {
    return IbanSupportedCountry::kXK;
  } else {
    return IbanSupportedCountry::kUnsupported;
  }
}

// static
// IBAN lengths taken from:
// https://en.wikipedia.org/wiki/International_Bank_Account_Number#IBAN_formats_by_country.
size_t Iban::GetLengthOfIbanCountry(IbanSupportedCountry supported_country) {
  switch (supported_country) {
    case IbanSupportedCountry::kAD:
      return 24;
    case IbanSupportedCountry::kAE:
      return 23;
    case IbanSupportedCountry::kAL:
      return 28;
    case IbanSupportedCountry::kAT:
      return 20;
    case IbanSupportedCountry::kAZ:
      return 28;
    case IbanSupportedCountry::kBA:
      return 20;
    case IbanSupportedCountry::kBE:
      return 16;
    case IbanSupportedCountry::kBG:
      return 22;
    case IbanSupportedCountry::kBH:
      return 22;
    case IbanSupportedCountry::kBR:
      return 29;
    case IbanSupportedCountry::kBY:
      return 28;
    case IbanSupportedCountry::kCH:
      return 21;
    case IbanSupportedCountry::kCR:
      return 22;
    case IbanSupportedCountry::kCY:
      return 28;
    case IbanSupportedCountry::kCZ:
      return 24;
    case IbanSupportedCountry::kDE:
      return 22;
    case IbanSupportedCountry::kDK:
      return 18;
    case IbanSupportedCountry::kDO:
      return 28;
    case IbanSupportedCountry::kEE:
      return 20;
    case IbanSupportedCountry::kEG:
      return 29;
    case IbanSupportedCountry::kES:
      return 24;
    case IbanSupportedCountry::kFI:
      return 18;
    case IbanSupportedCountry::kFO:
      return 18;
    case IbanSupportedCountry::kFR:
      return 27;
    case IbanSupportedCountry::kGB:
      return 22;
    case IbanSupportedCountry::kGE:
      return 22;
    case IbanSupportedCountry::kGI:
      return 23;
    case IbanSupportedCountry::kGL:
      return 18;
    case IbanSupportedCountry::kGR:
      return 27;
    case IbanSupportedCountry::kGT:
      return 28;
    case IbanSupportedCountry::kHR:
      return 21;
    case IbanSupportedCountry::kHU:
      return 28;
    case IbanSupportedCountry::kIL:
      return 23;
    case IbanSupportedCountry::kIQ:
      return 23;
    case IbanSupportedCountry::kIS:
      return 26;
    case IbanSupportedCountry::kIT:
      return 27;
    case IbanSupportedCountry::kJO:
      return 30;
    case IbanSupportedCountry::kKW:
      return 30;
    case IbanSupportedCountry::kKZ:
      return 20;
    case IbanSupportedCountry::kLB:
      return 28;
    case IbanSupportedCountry::kLC:
      return 32;
    case IbanSupportedCountry::kLI:
      return 21;
    case IbanSupportedCountry::kLT:
      return 20;
    case IbanSupportedCountry::kLU:
      return 20;
    case IbanSupportedCountry::kLV:
      return 21;
    case IbanSupportedCountry::kLY:
      return 25;
    case IbanSupportedCountry::kMC:
      return 27;
    case IbanSupportedCountry::kMD:
      return 24;
    case IbanSupportedCountry::kME:
      return 22;
    case IbanSupportedCountry::kMK:
      return 19;
    case IbanSupportedCountry::kMR:
      return 27;
    case IbanSupportedCountry::kMT:
      return 31;
    case IbanSupportedCountry::kMU:
      return 30;
    case IbanSupportedCountry::kNL:
      return 18;
    case IbanSupportedCountry::kPK:
      return 24;
    case IbanSupportedCountry::kPL:
      return 28;
    case IbanSupportedCountry::kPS:
      return 29;
    case IbanSupportedCountry::kPT:
      return 25;
    case IbanSupportedCountry::kQA:
      return 29;
    case IbanSupportedCountry::kRO:
      return 24;
    case IbanSupportedCountry::kRU:
      return 33;
    case IbanSupportedCountry::kRS:
      return 22;
    case IbanSupportedCountry::kSA:
      return 24;
    case IbanSupportedCountry::kSC:
      return 31;
    case IbanSupportedCountry::kSD:
      return 18;
    case IbanSupportedCountry::kSE:
      return 24;
    case IbanSupportedCountry::kSI:
      return 19;
    case IbanSupportedCountry::kSK:
      return 24;
    case IbanSupportedCountry::kSM:
      return 27;
    case IbanSupportedCountry::kST:
      return 25;
    case IbanSupportedCountry::kSV:
      return 28;
    case IbanSupportedCountry::kTL:
      return 23;
    case IbanSupportedCountry::kTN:
      return 24;
    case IbanSupportedCountry::kTR:
      return 26;
    case IbanSupportedCountry::kUA:
      return 29;
    case IbanSupportedCountry::kVA:
      return 22;
    case IbanSupportedCountry::kVG:
      return 24;
    case IbanSupportedCountry::kXK:
      return 20;
    case IbanSupportedCountry::kUnsupported:
      NOTREACHED();
  }
}

bool Iban::SetMetadata(const PaymentsMetadata& metadata) {
  // Make sure the ids match.
  if (metadata.id != (record_type_ == RecordType::kLocalIban
                          ? guid()
                          : base::NumberToString(instrument_id()))) {
    return false;
  }
  set_use_count(metadata.use_count);
  set_use_date(metadata.use_date);
  return true;
}

std::u16string Iban::GetRawInfo(FieldType type) const {
  if (type == IBAN_VALUE) {
    return value_;
  }

  NOTREACHED_IN_MIGRATION();
  return std::u16string();
}

void Iban::SetRawInfoWithVerificationStatus(FieldType type,
                                            const std::u16string& value,
                                            VerificationStatus status) {
  if (type == IBAN_VALUE) {
    set_value(value);
  } else {
    NOTREACHED_IN_MIGRATION() << "Attempting to set unknown info-type" << type;
  }
}

void Iban::GetSupportedTypes(FieldTypeSet* supported_types) const {
  supported_types->insert(IBAN_VALUE);
}

bool Iban::IsEmpty(const std::string& app_locale) const {
  FieldTypeSet types;
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

  if (record_type_ != iban.record_type_) {
    return 1;
  }
  return 0;
}

bool Iban::operator==(const Iban& iban) const {
  return Compare(iban) == 0;
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

int64_t Iban::instrument_id() const {
  CHECK(absl::holds_alternative<InstrumentId>(identifier_));
  return absl::get<InstrumentId>(identifier_).value();
}

void Iban::set_value(const std::u16string& value) {
  if (!IsValid(value)) {
    return;
  }
  CHECK_NE(record_type_, Iban::kServerIban);
  // Get rid of all separators in the value and capitalize them before storing.
  value_ = RemoveIbanSeparators(value);
  value_ = base::ToUpperASCII(value_);
  // The `IsValid()` call above ensures we have a valid IBAN length. We should
  // never set the `kPrefixLength` and `kSuffixLength` in a way where they can
  // be longer than the total length of the IBAN.
  CHECK(value_.length() >= kPrefixLength + kSuffixLength);
  prefix_ = value_.substr(0, kPrefixLength);
  suffix_ = value_.substr(value_.length() - kSuffixLength);
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
  std::u16string capitalized_prefix = base::ToUpperASCII(prefix);
  prefix_ = std::move(capitalized_prefix);
}

void Iban::set_suffix(std::u16string suffix) {
  CHECK_NE(record_type_, Iban::kLocalIban);
  std::u16string capitalized_suffix = base::ToUpperASCII(suffix);
  suffix_ = std::move(capitalized_suffix);
}

bool Iban::IsValid() {
  CHECK_NE(record_type_, RecordType::kUnknown);
  return record_type_ == kServerIban || IsValid(value_);
}

std::string Iban::GetCountryCode() const {
  CHECK(prefix_.length() >= 2);
  return GetCountryCode(prefix_);
}

void Iban::RecordAndLogUse() {
  autofill_metrics::LogDaysSinceLastIbanUse(*this);
  RecordUseDate(AutofillClock::Now());
  set_use_count(use_count() + 1);
}

std::u16string Iban::GetIdentifierStringForAutofillDisplay(
    bool is_value_masked) const {
  // `value_` is expected to be empty for server-based IBANs. For local IBANs,
  // it might be empty in rare situations (e.g., keychain is locked).
  if (value_.empty() && record_type_ == kLocalIban) {
    return value_;
  }

  if (is_value_masked) {
    const std::u16string one_space = std::u16string(1, kEllipsisOneSpace);
    const std::u16string two_dots = std::u16string(2, kEllipsisOneDot);
    return base::StrCat({prefix(), one_space, two_dots, suffix()});
  }

  // Displaying the full IBAN value is not possible for server-based IBANs.
  CHECK(record_type_ != Iban::kServerIban);

  // Add space separators into the IBAN identifier to display it in groups of
  // four characters. For example, an IBAN with value of DE91100000000123456789
  // will be displayed as: DE91 1000 0000 0123 4567 89.
  std::u16string output;
  output.reserve(value_.length() + (value_.length() - 1) / 4);
  for (size_t i = 0; i < value_.length(); ++i) {
    if (i % 4 == 0 && i > 0) {
      output.push_back(kEllipsisOneSpace);
    }
    output.push_back(value_[i]);
  }

  return output;
}

bool Iban::MatchesPrefixAndSuffix(const Iban& iban) const {
  // Unlike the `Compare()` function, which seeks an exact match between
  // `prefix_` and `suffix_`, the comparison performed here involves matching
  // the prefixes between each other and similarly comparing the suffixes
  // between each other This approach is adopted because the `prefix` and
  // `suffix` received from the server are considered the source of truth.
  // Therefore, even if the values of `kPrefixLength` or `kSuffixLength` change
  // later, leading to differences in length between the client and server, it
  // remains essential to match substrings and identify the matched IBAN.
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

  return true;
}

std::ostream& operator<<(std::ostream& os, const Iban& iban) {
  return os << "[id: "
            << (iban.record_type() == Iban::RecordType::kLocalIban
                    ? iban.guid()
                    : base::NumberToString(iban.instrument_id()))
            << ", record_type: "
            << (iban.record_type() == Iban::RecordType::kLocalIban
                    ? "Local IBAN"
                    : "Server IBAN")
            << ", value: " << base::UTF16ToUTF8(iban.GetRawInfo(IBAN_VALUE))
            << ", prefix: " << base::UTF16ToUTF8(iban.prefix())
            << ", suffix: " << base::UTF16ToUTF8(iban.suffix())
            << ", nickname: " << base::UTF16ToUTF8(iban.nickname()) << "]";
}

}  // namespace autofill
