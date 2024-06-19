// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/geo/phone_number_i18n.h"

#include <memory>
#include <string_view>
#include <utility>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "third_party/libphonenumber/phonenumber_api.h"

namespace autofill {

namespace {

using ::i18n::phonenumbers::PhoneNumberUtil;

// Formats the |phone_number| to the specified |format|. Returns the original
// number if the operation is not possible.
std::string FormatPhoneNumber(const std::string& phone_number,
                              const std::string& country_code,
                              PhoneNumberUtil::PhoneNumberFormat format) {
  ::i18n::phonenumbers::PhoneNumber parsed_number;
  PhoneNumberUtil* phone_number_util = PhoneNumberUtil::GetInstance();
  if (phone_number_util->Parse(phone_number, country_code, &parsed_number) !=
      PhoneNumberUtil::NO_PARSING_ERROR) {
    return phone_number;
  }

  std::string formatted_number;
  phone_number_util->Format(parsed_number, format, &formatted_number);
  return formatted_number;
}

std::string SanitizeRegion(const std::string& region,
                           const std::string& app_locale) {
  if (region.length() == 2)
    return region;

  return AutofillCountry::CountryCodeForLocale(app_locale);
}

// Formats the given |number| as a human-readable string, and writes the result
// into |formatted_number|.  Also, normalizes the formatted number, and writes
// that result into |normalized_number|.  This function should only be called
// with numbers already known to be valid, i.e. validation should be done prior
// to calling this function.  Note that the |country_code|, which determines
// whether to format in the national or in the international format, is passed
// in explicitly, as |number| might have an implicit country code set, even
// though the original input lacked a country code.
void FormatValidatedNumber(const ::i18n::phonenumbers::PhoneNumber& number,
                           const std::u16string& country_code,
                           std::u16string* formatted_number,
                           std::u16string* normalized_number) {
  PhoneNumberUtil::PhoneNumberFormat format =
      country_code.empty() ? PhoneNumberUtil::NATIONAL
                           : PhoneNumberUtil::INTERNATIONAL;

  PhoneNumberUtil* phone_util = PhoneNumberUtil::GetInstance();
  std::string processed_number;
  phone_util->Format(number, format, &processed_number);

  std::string region_code;
  phone_util->GetRegionCodeForNumber(number, &region_code);

  // Drop the leading '+' for US/CA numbers as some sites can't handle the "+",
  // and in these regions dialing "+1..." is the same as dialing "1...".
  // TODO(crbug.com/40311205): Investigate whether the leading "+" is desirable
  // in other regions. Closed bug crbug/98911 contains additional context.
  std::string prefix;
  if (processed_number[0] == '+') {
    processed_number = processed_number.substr(1);
    if (region_code != "US" && region_code != "CA")
      prefix = "+";
  }

  if (formatted_number)
    *formatted_number = base::UTF8ToUTF16(prefix + processed_number);

  if (normalized_number) {
    phone_util->NormalizeDigitsOnly(&processed_number);
    *normalized_number = base::UTF8ToUTF16(prefix + processed_number);
  }
}

// Returns false iff |str| contains any characters from the range 0-31
// (inclusive) or the "delete" character (127). This allows the characters in
// 128-255 because |str| is assumed to be in UTF-8. Note that for all
// multi-byte UTF-8 characters, every byte has its most significant bit set
// (i.e., is in the range 128-255, inclusive), so all bytes <=127 are
// single-byte characters.
bool IsPrintable(std::string_view str) {
  for (unsigned char c : str) {
    if (c < 32 || c == 127)
      return false;
  }
  return true;
}

}  // namespace

namespace i18n {

const size_t kMaxPhoneNumberSize = 40u;

// Returns true if |phone_number| is a possible number.
bool IsPossiblePhoneNumber(
    const ::i18n::phonenumbers::PhoneNumber& phone_number) {
  PhoneNumberUtil* phone_util = PhoneNumberUtil::GetInstance();
  return phone_util->IsPossibleNumber(phone_number);
}

bool IsPossiblePhoneNumber(const std::string& phone_number,
                           const std::string& country_code) {
  ::i18n::phonenumbers::PhoneNumber parsed_number;
  PhoneNumberUtil* phone_util = PhoneNumberUtil::GetInstance();
  auto result = phone_util->ParseAndKeepRawInput(phone_number, country_code,
                                                 &parsed_number);

  return result == ::i18n::phonenumbers::PhoneNumberUtil::NO_PARSING_ERROR &&
         phone_util->IsPossibleNumber(parsed_number);
}

bool IsValidPhoneNumber(const std::string& phone_number,
                        const std::string& country_code) {
  ::i18n::phonenumbers::PhoneNumber parsed_number;
  PhoneNumberUtil* phone_util = PhoneNumberUtil::GetInstance();
  auto result = phone_util->ParseAndKeepRawInput(phone_number, country_code,
                                                 &parsed_number);

  return result == ::i18n::phonenumbers::PhoneNumberUtil::NO_PARSING_ERROR &&
         phone_util->IsValidNumber(parsed_number);
}

// Parses the number stored in |value| as it should be interpreted in the given
// |default_region|, and stores the results into the remaining arguments.
// The |default_region| should be sanitized prior to calling this function.
bool ParsePhoneNumber(const std::u16string& value,
                      const std::string& default_region,
                      std::u16string* country_code,
                      std::u16string* city_code,
                      std::u16string* number,
                      std::string* inferred_region,
                      ::i18n::phonenumbers::PhoneNumber* i18n_number) {
  country_code->clear();
  city_code->clear();
  number->clear();
  *i18n_number = ::i18n::phonenumbers::PhoneNumber();

  if (value.size() > kMaxPhoneNumberSize)
    return false;

  std::string number_text;
  if (!base::UTF16ToUTF8(value.data(), value.size(), &number_text) ||
      !base::IsStringUTF8(number_text) || !IsPrintable(number_text)) {
    return false;
  }

  // Parse phone number based on the region.
  PhoneNumberUtil* phone_util = PhoneNumberUtil::GetInstance();

  // The |default_region| should already be sanitized.
  DCHECK_EQ(2U, default_region.size());
  if (phone_util->ParseAndKeepRawInput(number_text, default_region,
                                       i18n_number) !=
      PhoneNumberUtil::NO_PARSING_ERROR) {
    return false;
  }

  if (!IsPossiblePhoneNumber(*i18n_number))
    return false;

  std::string national_significant_number;
  phone_util->GetNationalSignificantNumber(*i18n_number,
                                           &national_significant_number);

  int area_length = phone_util->GetLengthOfGeographicalAreaCode(*i18n_number);
  int destination_length =
      phone_util->GetLengthOfNationalDestinationCode(*i18n_number);
  // Some phones have a destination code in lieu of area code: mobile operators
  // in Europe, toll and toll-free numbers in USA, etc. From our point of view
  // these two types of codes are the same.
  if (destination_length > area_length)
    area_length = destination_length;

  if (area_length >= static_cast<int>(national_significant_number.size())) {
    // For some non-ASCII strings |destination_length| is bigger than phone
    // string size. It might be because of incorrect treating of non-ASCII
    // characters.
    return false;
  }

  std::string area_code;
  std::string subscriber_number;
  if (area_length > 0) {
    area_code = national_significant_number.substr(0, area_length);
    subscriber_number = national_significant_number.substr(area_length);
  } else {
    subscriber_number = national_significant_number;
  }
  *number = base::UTF8ToUTF16(subscriber_number);
  *city_code = base::UTF8ToUTF16(area_code);

  // Check if parsed number has a country code that was not inferred from the
  // region.
  if (i18n_number->has_country_code() &&
      i18n_number->country_code_source() !=
          ::i18n::phonenumbers::PhoneNumber::FROM_DEFAULT_COUNTRY) {
    *country_code =
        base::UTF8ToUTF16(base::NumberToString(i18n_number->country_code()));
  }

  // The region might be different from what we started with.
  phone_util->GetRegionCodeForNumber(*i18n_number, inferred_region);

  return true;
}

std::u16string NormalizePhoneNumber(const std::u16string& value,
                                    const std::string& region) {
  DCHECK_EQ(2u, region.size());
  std::u16string country_code, unused_city_code, unused_number;
  std::string unused_region;
  ::i18n::phonenumbers::PhoneNumber phone_number;
  if (!ParsePhoneNumber(value, region, &country_code, &unused_city_code,
                        &unused_number, &unused_region, &phone_number)) {
    return std::u16string();  // Parsing failed - do not store phone.
  }

  std::u16string normalized_number;
  FormatValidatedNumber(phone_number, country_code, nullptr,
                        &normalized_number);
  return normalized_number;
}

bool ConstructPhoneNumber(const std::u16string& input_whole_number,
                          const std::string& region,
                          std::u16string* output_whole_number) {
  DCHECK_EQ(2u, region.size());
  output_whole_number->clear();

  std::u16string parsed_country_code, unused_city_code, unused_number;
  std::string unused_region;
  ::i18n::phonenumbers::PhoneNumber phone_number;
  if (!ParsePhoneNumber(input_whole_number, region, &parsed_country_code,
                        &unused_city_code, &unused_number, &unused_region,
                        &phone_number)) {
    return false;
  }

  // We pass the parsed_country_code so that if the phone number contained a
  // country code, the formatted phone number is returned in international
  // format with a country code as well.
  FormatValidatedNumber(phone_number, parsed_country_code, output_whole_number,
                        nullptr);
  return true;
}

bool PhoneNumbersMatch(const std::u16string& number_a,
                       const std::u16string& number_b,
                       const std::string& raw_region,
                       const std::string& app_locale) {
  if (number_a.empty() && number_b.empty()) {
    return true;
  }

  if (number_a.empty() || number_b.empty()) {
    return false;
  }

  // Sanitize the provided |raw_region| before trying to use it for parsing.
  const std::string region = SanitizeRegion(raw_region, app_locale);

  PhoneNumberUtil* phone_util = PhoneNumberUtil::GetInstance();

  // Parse phone numbers based on the region
  ::i18n::phonenumbers::PhoneNumber i18n_number1;
  if (phone_util->Parse(base::UTF16ToUTF8(number_a), region, &i18n_number1) !=
      PhoneNumberUtil::NO_PARSING_ERROR) {
    return false;
  }

  ::i18n::phonenumbers::PhoneNumber i18n_number2;
  if (phone_util->Parse(base::UTF16ToUTF8(number_b), region, &i18n_number2) !=
      PhoneNumberUtil::NO_PARSING_ERROR) {
    return false;
  }

  switch (phone_util->IsNumberMatch(i18n_number1, i18n_number2)) {
    case PhoneNumberUtil::INVALID_NUMBER:
    case PhoneNumberUtil::NO_MATCH:
      return false;
    case PhoneNumberUtil::SHORT_NSN_MATCH:
      return false;
    case PhoneNumberUtil::NSN_MATCH:
    case PhoneNumberUtil::EXACT_MATCH:
      return true;
  }

  NOTREACHED_IN_MIGRATION();
  return false;
}

std::u16string GetFormattedPhoneNumberForDisplay(const AutofillProfile& profile,
                                                 const std::string& locale) {
  // Since the "+" is removed for some country's phone numbers, try to add a "+"
  // and see if it is a valid phone number for a country.
  // Having two "+" in front of a number has no effect on the formatted number.
  // The reason for this is international phone numbers for another country. For
  // example, without adding a "+", the US number 1-415-123-1234 for an AU
  // address would be wrongly formatted as +61 1-415-123-1234 which is invalid.
  std::string phone =
      base::UTF16ToUTF8(profile.GetInfo(PHONE_HOME_WHOLE_NUMBER, locale));
  std::string tentative_intl_phone = "+" + phone;

  // Always favor the tentative international phone number if it's determined as
  // being a valid number.
  const std::string country_code =
      data_util::GetCountryCodeWithFallback(profile, locale);
  if (IsValidPhoneNumber(tentative_intl_phone, country_code)) {
    return base::UTF8ToUTF16(
        FormatPhoneNumber(tentative_intl_phone, country_code,
                          PhoneNumberUtil::PhoneNumberFormat::INTERNATIONAL));
  }

  if (IsValidPhoneNumber(phone, country_code)) {
    return base::UTF8ToUTF16(
        FormatPhoneNumber(phone, country_code,
                          PhoneNumberUtil::PhoneNumberFormat::INTERNATIONAL));
  }

  return base::UTF8ToUTF16(phone);
}

std::string FormatPhoneNationallyForDisplay(const std::string& phone_number,
                                            const std::string& country_code) {
  if (IsValidPhoneNumber(phone_number, country_code)) {
    return FormatPhoneNumber(phone_number, country_code,
                             PhoneNumberUtil::PhoneNumberFormat::NATIONAL);
  }
  return phone_number;
}

std::string FormatPhoneForDisplay(const std::string& phone_number,
                                  const std::string& country_code) {
  if (IsValidPhoneNumber(phone_number, country_code)) {
    return FormatPhoneNumber(phone_number, country_code,
                             PhoneNumberUtil::PhoneNumberFormat::INTERNATIONAL);
  }
  return phone_number;
}

std::string FormatPhoneForResponse(const std::string& phone_number,
                                   const std::string& country_code) {
  if (IsValidPhoneNumber(phone_number, country_code)) {
    return FormatPhoneNumber(phone_number, country_code,
                             PhoneNumberUtil::PhoneNumberFormat::E164);
  }
  return phone_number;
}

PhoneObject::PhoneObject(const std::u16string& number,
                         const std::string& region,
                         bool infer_country_code) {
  DCHECK_EQ(2u, region.size());

  std::unique_ptr<::i18n::phonenumbers::PhoneNumber> i18n_number(
      new ::i18n::phonenumbers::PhoneNumber);
  if (ParsePhoneNumber(number, region, &country_code_, &city_code_, &number_,
                       &region_, i18n_number.get())) {
    // The phone number was successfully parsed, so store the parsed version.
    // The formatted and normalized versions will be set on the first call to
    // the coresponding methods.
    i18n_number_ = std::move(i18n_number);
    // `ParsePhoneNumber()` only sets `country_code_` for internationally
    // formatted numbers. `i18n_number_`'s country_code defaults to `region` in,
    // this case. If `infer_country_code` is true, fall back to that.
    if (infer_country_code && country_code_.empty() &&
        i18n_number_->has_country_code()) {
      country_code_ = base::NumberToString16(i18n_number_->country_code());
    }
    // Autofill doesn't support filling extensions, so we should not store them.
    i18n_number_->clear_extension();
  } else {
    // Parsing failed. Store passed phone "as is" into |whole_number_|.
    whole_number_ = number;
    // We have no way of removing any extensions.
  }
}

PhoneObject::PhoneObject(const PhoneObject& other) {
  *this = other;
}

PhoneObject::PhoneObject() = default;

PhoneObject::~PhoneObject() = default;

const std::u16string& PhoneObject::GetFormattedNumber() const {
  if (i18n_number_ && formatted_number_.empty()) {
    FormatValidatedNumber(*i18n_number_, country_code_, &formatted_number_,
                          &whole_number_);
  }

  return formatted_number_;
}

std::u16string PhoneObject::GetNationallyFormattedNumber() const {
  std::u16string formatted = whole_number_;
  if (i18n_number_)
    FormatValidatedNumber(*i18n_number_, std::u16string(), &formatted, nullptr);

  return formatted;
}

const std::u16string& PhoneObject::GetWholeNumber() const {
  if (i18n_number_ && whole_number_.empty()) {
    FormatValidatedNumber(*i18n_number_, country_code_, &formatted_number_,
                          &whole_number_);
  }

  return whole_number_;
}

PhoneObject& PhoneObject::operator=(const PhoneObject& other) {
  if (this == &other)
    return *this;

  region_ = other.region_;

  if (other.i18n_number_) {
    i18n_number_ = std::make_unique<::i18n::phonenumbers::PhoneNumber>(
        *other.i18n_number_);
  } else {
    i18n_number_.reset();
  }

  country_code_ = other.country_code_;
  city_code_ = other.city_code_;
  number_ = other.number_;

  formatted_number_ = other.formatted_number_;
  whole_number_ = other.whole_number_;

  return *this;
}

}  // namespace i18n
}  // namespace autofill
