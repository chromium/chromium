// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_PHONE_NUMBER_I18N_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_PHONE_NUMBER_I18N_H_

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/strings/string16.h"

namespace i18n {
namespace phonenumbers {
class PhoneNumber;
}  // namespace phonenumbers
}  // namespace i18n

namespace autofill {

class AutofillProfile;

// Utilities to process, normalize and compare international phone numbers.
namespace i18n {

// No reasonable phone number should need more than |kMaxPhoneNumberSize|
// characters. Longer inputs might be an error or an attack and processing them
// takes non-trivial time (parsing with regex), so will be ignored.
extern const size_t kMaxPhoneNumberSize;

// Return true if the given |phone_number| object is likely to be a phone number
// This method uses IsPossibleNumber from libphonenumber, instead of
// IsValidNumber. IsPossibleNumber does a less strict check, it will not try to
// check for carrier code validility.
bool IsPossiblePhoneNumber(
    const ::i18n::phonenumbers::PhoneNumber& phone_number);

// Return true if the given |phone_number| is likely to be a phone number for
// the |country_code|. This method uses IsPossibleNumber from libphonenumber,
// instead of IsValidNumber. IsPossibleNumber does a less strict check, it
// will not try to check for carrier code validility.
bool IsPossiblePhoneNumber(const std::string& phone_number,
                           const std::string& country_code);

// Most of the following functions require |region| to operate. The |region| is
// a ISO 3166 standard code ("US" for USA, "CZ" for Czech Republic, etc.).

// Parses the number stored in |value| as a phone number interpreted in the
// given |default_region|, and stores the results into the remaining arguments.
// The |default_region| should be a 2-letter country code.  |inferred_region| is
// set to the actual region of the number (which may be different than
// |default_region| if |value| has an international country code, for example).
// This is an internal function, exposed in the header file so that it can be
// tested.
bool ParsePhoneNumber(const base::string16& value,
                      const std::string& default_region,
                      base::string16* country_code,
                      base::string16* city_code,
                      base::string16* number,
                      std::string* inferred_region,
                      ::i18n::phonenumbers::PhoneNumber* i18n_number)
    WARN_UNUSED_RESULT;

// Normalizes phone number, by changing digits in the extended fonts
// (such as \xFF1x) into '0'-'9'. Also strips out non-digit characters.
base::string16 NormalizePhoneNumber(const base::string16& value,
                                    const std::string& default_region);

// Constructs whole phone number from parts.
// |city_code| - area code, could be empty.
// |country_code| - country code, could be empty.
// |number| - local number, should not be empty.
// |region| - current region, the parsing is based on.
// |whole_number| - constructed whole number.
// Separator characters are stripped before parsing the digits.
// Returns true if parsing was successful, false otherwise.
bool ConstructPhoneNumber(const base::string16& country_code,
                          const base::string16& city_code,
                          const base::string16& number,
                          const std::string& default_region,
                          base::string16* whole_number) WARN_UNUSED_RESULT;

// Returns true if |number_a| and |number_b| parse to the same phone number in
// the given |region|.
bool PhoneNumbersMatch(const base::string16& number_a,
                       const base::string16& number_b,
                       const std::string& region,
                       const std::string& app_locale);

// Returns the phone number from the given |profile| formatted for display.
// If it's a valid number for the profile's country or for the |locale| given
// as a fallback, returns the number in international format; otherwise returns
// the raw number string from profile.
base::string16 GetFormattedPhoneNumberForDisplay(const AutofillProfile& profile,
                                                 const std::string& locale);

// Returns |phone_number| in i18n::phonenumbers::PhoneNumberUtil::
// PhoneNumberFormat::NATIONAL format if the number is valid for
// |country_code|. Otherwise, returns the given |phone_number|.
std::string FormatPhoneNationallyForDisplay(const std::string& phone_number,
                                            const std::string& country_code);

// Formats the given number |phone_number| to
// i18n::phonenumbers::PhoneNumberUtil::PhoneNumberFormat::INTERNATIONAL format
// by using i18n::phonenumbers::PhoneNumberUtil::Format.
std::string FormatPhoneForDisplay(const std::string& phone_number,
                                  const std::string& country_code);

// Formats the given number |phone_number| to
// i18n::phonenumbers::PhoneNumberUtil::PhoneNumberFormat::E164 format by using
// i18n::phonenumbers::PhoneNumberUtil::Format, as defined in the Payment
// Request spec
// (https://w3c.github.io/browser-payment-api/#paymentrequest-updated-algorithm)
// if the number is a valid number for the given country code.
// Returns the given_number without formatting if the number is invalid.
std::string FormatPhoneForResponse(const std::string& phone_number,
                                   const std::string& country_code);

// The cached phone number, does parsing only once, improves performance.
class PhoneObject {
 public:
  PhoneObject(const base::string16& number, const std::string& default_region);
  PhoneObject(const PhoneObject&);
  PhoneObject();
  ~PhoneObject();

  const std::string& region() const { return region_; }

  const base::string16& country_code() const { return country_code_; }
  const base::string16& city_code() const { return city_code_; }
  const base::string16& number() const { return number_; }

  const base::string16& GetFormattedNumber() const;
  base::string16 GetNationallyFormattedNumber() const;
  const base::string16& GetWholeNumber() const;

  PhoneObject& operator=(const PhoneObject& other);

  bool IsValidNumber() const { return i18n_number_ != NULL; }

 private:
  // The region code for this phone number, inferred during parsing.
  std::string region_;

  // The parsed number and its components.
  //
  std::unique_ptr<::i18n::phonenumbers::PhoneNumber> i18n_number_;
  base::string16 city_code_;
  base::string16 country_code_;
  base::string16 number_;

  // Pretty printed version of the whole number, or empty if parsing failed.
  // Set on first request.
  mutable base::string16 formatted_number_;

  // The whole number, normalized to contain only digits if possible.
  // Set on first request.
  mutable base::string16 whole_number_;
};

}  // namespace i18n
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_PHONE_NUMBER_I18N_H_
