// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_DATA_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_DATA_UTIL_H_

#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "base/strings/string_piece_forward.h"
#include "components/autofill/core/browser/field_types.h"

namespace autofill {

class AutofillProfile;

namespace data_util {

struct NameParts {
  base::string16 given;
  base::string16 middle;
  base::string16 family;
};

namespace bit_field_type_groups {

// Bits for FieldTypeGroup options.
// The form has a field associated with the NAME_HOME or NAME_BILLING
// FieldTypeGroups.
constexpr uint32_t kName = 1 << 0;
// The form has a field associated with the ADDRESS_HOME or ADDRESS_BILLING
// FieldTypeGroups.
constexpr uint32_t kAddress = 1 << 1;
// The form has a field associated with the EMAIL FieldTypeGroup.
constexpr uint32_t kEmail = 1 << 2;
// The form has a field associated with the PHONE_HOME or PHONE_BILLING
// FieldTypeGroups.
constexpr uint32_t kPhone = 1 << 3;

}  // namespace bit_field_type_groups

// Returns true if kName is set in |groups|.
bool ContainsName(uint32_t groups);

// Returns true if kAddress is set in |groups|.
bool ContainsAddress(uint32_t groups);

// Returns true if kEmail is set in |groups|.
bool ContainsEmail(uint32_t groups);

// Returns true if kPhone is set in |groups|.
bool ContainsPhone(uint32_t groups);

// Returns a bitmask indicating which of the name, address, email address, and
// phone number FieldTypeGroups are associated with the given |types|.
uint32_t DetermineGroups(const std::vector<ServerFieldType>& types);

// Returns true if a form has address fields or has least two supported
// non-address fields.
bool IsSupportedFormType(uint32_t groups);

// Returns the histogram suffix corresponding to the given |bitmask|.
std::string GetSuffixForProfileFormType(uint32_t bitmask);

// Truncates a string to the nearest UTF-8 character that will leave
// the string less than or equal to the specified byte size.
std::string TruncateUTF8(const std::string& data);

bool IsCreditCardExpirationType(ServerFieldType type);

// Used to map Chrome card issuer networks to Payment Request API basic card
// payment spec issuer networks and icons.
// https://w3c.github.io/webpayments-methods-card/#method-id
struct PaymentRequestData {
  const char* issuer_network;
  const char* basic_card_issuer_network;
  const int icon_resource_id;
  const int a11y_label_resource_id;
};

// Returns true if |name| looks like a CJK name (or some kind of mish-mash of
// the three, at least).
bool IsCJKName(base::StringPiece16 name);

// TODO(crbug.com/586510): Investigate the use of app_locale to do better name
// splitting.
// Returns the different name parts (given, middle and family names) of the full
// |name| passed as a parameter.
NameParts SplitName(base::StringPiece16 name);

// Concatenates the name parts together in the correct order (based on script),
// and returns the result.
base::string16 JoinNameParts(base::StringPiece16 given,
                             base::StringPiece16 middle,
                             base::StringPiece16 family);

// Returns the Payment Request API basic card payment spec data for the provided
// autofill credit card |network|.  Will set the network and the icon to
// "generic" for any unrecognized type.
const PaymentRequestData& GetPaymentRequestData(
    const std::string& issuer_network);

// Returns the autofill credit card issuer network string for the provided
// Payment Request API basic card payment spec |basic_card_card_issuer_network|.
const char* GetIssuerNetworkForBasicCardIssuerNetwork(
    const std::string& basic_card_issuer_network);

// Returns whether the specified |basic_card_issuer_network| is a valid basic
// card network or not. Note that 'generic' is not considered valid.
bool IsValidBasicCardIssuerNetwork(
    const std::string& basic_card_issuer_network);

// Returns whether the specified |country_code| is a valid country code.
bool IsValidCountryCode(const std::string& country_code);
bool IsValidCountryCode(const base::string16& country_code);

// Returns a country code to be used when validating this profile. If the
// profile has a valid country code set, it is returned. If not, a country code
// associated with |app_locale| is used as a fallback.
std::string GetCountryCodeWithFallback(const autofill::AutofillProfile& profile,
                                       const std::string& app_locale);

}  // namespace data_util
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_DATA_UTIL_H_
