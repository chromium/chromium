// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/autofill/core/common/autocomplete_parsing_util.h"

#include <vector>

#include "base/containers/fixed_flat_map.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_regexes.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/html_field_types.h"

namespace autofill {

namespace {

// Returns true iff the `token` is a type hint for a contact field, as
// specified in the implementation section of http://is.gd/whatwg_autocomplete
// Note that "fax" and "pager" are intentionally ignored, as Chrome does not
// support filling either type of information.
bool IsContactTypeHint(const std::string& token) {
  return token == "home" || token == "work" || token == "mobile";
}

// Returns true iff the `token` is a type hint appropriate for a field of the
// given `field_type`, as specified in the implementation section of
// http://is.gd/whatwg_autocomplete
bool ContactTypeHintMatchesFieldType(const std::string& token,
                                     HtmlFieldType field_type) {
  // The "home" and "work" type hints are only appropriate for email and phone
  // number field types.
  if (token == "home" || token == "work") {
    return field_type == HtmlFieldType::kEmail ||
           (field_type >= HtmlFieldType::kTel &&
            field_type <= HtmlFieldType::kTelLocalSuffix);
  }

  // The "mobile" type hint is only appropriate for phone number field types.
  // Note that "fax" and "pager" are intentionally ignored, as Chrome does not
  // support filling either type of information.
  if (token == "mobile") {
    return field_type >= HtmlFieldType::kTel &&
           field_type <= HtmlFieldType::kTelLocalSuffix;
  }

  return false;
}

// Rationalizes the HTML `type` of `field`, based on the fields properties. At
// the moment only `max_length` is considered. For example, a max_length of 4
// might indicate a 4 digit year.
// In case no rationalization rule applies, the original type is returned.
HtmlFieldType RationalizeAutocompleteType(HtmlFieldType type,
                                          uint64_t field_max_length) {
  // (original-type, max-length) -> new-type
  static constexpr auto rules =
      base::MakeFixedFlatMap<std::pair<HtmlFieldType, uint64_t>, HtmlFieldType>(
          {
              {{HtmlFieldType::kAdditionalName, 1},
               HtmlFieldType::kAdditionalNameInitial},
              {{HtmlFieldType::kCreditCardExp, 5},
               HtmlFieldType::kCreditCardExpDate2DigitYear},
              {{HtmlFieldType::kCreditCardExp, 7},
               HtmlFieldType::kCreditCardExpDate4DigitYear},
              {{HtmlFieldType::kCreditCardExpYear, 2},
               HtmlFieldType::kCreditCardExp2DigitYear},
              {{HtmlFieldType::kCreditCardExpYear, 4},
               HtmlFieldType::kCreditCardExp4DigitYear},
          });

  auto* it = rules.find(std::make_pair(type, field_max_length));
  return it == rules.end() ? type : it->second;
}

// Chrome Autofill supports a subset of the field types listed at
// http://is.gd/whatwg_autocomplete. Returns the corresponding HtmlFieldType, if
// `value` matches any of them.
absl::optional<HtmlFieldType> ParseStandardizedAutocompleteAttribute(
    base::StringPiece value) {
  static constexpr auto standardized_attributes =
      base::MakeFixedFlatMap<base::StringPiece, HtmlFieldType>({
          {"additional-name", HtmlFieldType::kAdditionalName},
          {"address-level1", HtmlFieldType::kAddressLevel1},
          {"address-level2", HtmlFieldType::kAddressLevel2},
          {"address-level3", HtmlFieldType::kAddressLevel3},
          {"address-line1", HtmlFieldType::kAddressLine1},
          {"address-line2", HtmlFieldType::kAddressLine2},
          {"address-line3", HtmlFieldType::kAddressLine3},
          {"bday-day", HtmlFieldType::kBirthdateDay},
          {"bday-month", HtmlFieldType::kBirthdateMonth},
          {"bday-year", HtmlFieldType::kBirthdateYear},
          {"cc-csc", HtmlFieldType::kCreditCardVerificationCode},
          {"cc-exp", HtmlFieldType::kCreditCardExp},
          {"cc-exp-month", HtmlFieldType::kCreditCardExpMonth},
          {"cc-exp-year", HtmlFieldType::kCreditCardExpYear},
          {"cc-family-name", HtmlFieldType::kCreditCardNameLast},
          {"cc-given-name", HtmlFieldType::kCreditCardNameFirst},
          {"cc-name", HtmlFieldType::kCreditCardNameFull},
          {"cc-number", HtmlFieldType::kCreditCardNumber},
          {"cc-type", HtmlFieldType::kCreditCardType},
          {"country", HtmlFieldType::kCountryCode},
          {"country-name", HtmlFieldType::kCountryName},
          {"email", HtmlFieldType::kEmail},
          {"family-name", HtmlFieldType::kFamilyName},
          {"given-name", HtmlFieldType::kGivenName},
          {"honorific-prefix", HtmlFieldType::kHonorificPrefix},
          {"name", HtmlFieldType::kName},
          {"one-time-code", HtmlFieldType::kOneTimeCode},
          {"organization", HtmlFieldType::kOrganization},
          {"postal-code", HtmlFieldType::kPostalCode},
          {"street-address", HtmlFieldType::kStreetAddress},
          {"tel-area-code", HtmlFieldType::kTelAreaCode},
          {"tel-country-code", HtmlFieldType::kTelCountryCode},
          {"tel-extension", HtmlFieldType::kTelExtension},
          {"tel", HtmlFieldType::kTel},
          {"tel-local", HtmlFieldType::kTelLocal},
          {"tel-local-prefix", HtmlFieldType::kTelLocalPrefix},
          {"tel-local-suffix", HtmlFieldType::kTelLocalSuffix},
          {"tel-national", HtmlFieldType::kTelNational},
          {"transaction-amount", HtmlFieldType::kTransactionAmount},
          {"transaction-currency", HtmlFieldType::kTransactionCurrency},
      });

  auto* it = standardized_attributes.find(value);
  return it != standardized_attributes.end()
             ? absl::optional<HtmlFieldType>(it->second)
             : absl::nullopt;
}

// Maps `value`s that Autofill has proposed for the HTML autocomplete standard,
// but which are not standardized, to their HtmlFieldType.
absl::optional<HtmlFieldType> ParseProposedAutocompleteAttribute(
    base::StringPiece value) {
  static constexpr auto proposed_attributes =
      base::MakeFixedFlatMap<base::StringPiece, HtmlFieldType>({
          {"address", HtmlFieldType::kStreetAddress},
          {"coupon-code", HtmlFieldType::kMerchantPromoCode},
          // TODO(crbug.com/1351760): Investigate if this mapping makes sense.
          {"username", HtmlFieldType::kEmail},
      });

  auto* it = proposed_attributes.find(value);
  return it != proposed_attributes.end()
             ? absl::optional<HtmlFieldType>(it->second)
             : absl::nullopt;
}

// Maps non-standardized `value`s for the HTML autocomplete attribute to an
// HtmlFieldType. This is primarily a list of "reasonable guesses".
absl::optional<HtmlFieldType> ParseNonStandarizedAutocompleteAttribute(
    base::StringPiece value) {
  static constexpr auto non_standardized_attributes =
      base::MakeFixedFlatMap<base::StringPiece, HtmlFieldType>({
          {"company", HtmlFieldType::kOrganization},
          {"first-name", HtmlFieldType::kGivenName},
          {"gift-code", HtmlFieldType::kMerchantPromoCode},
          {"iban", HtmlFieldType::kIban},
          {"locality", HtmlFieldType::kAddressLevel2},
          {"promo-code", HtmlFieldType::kMerchantPromoCode},
          {"promotional-code", HtmlFieldType::kMerchantPromoCode},
          {"promotion-code", HtmlFieldType::kMerchantPromoCode},
          {"region", HtmlFieldType::kAddressLevel1},
          {"tel-ext", HtmlFieldType::kTelExtension},
          {"upi", HtmlFieldType::kUpiVpa},
          {"upi-vpa", HtmlFieldType::kUpiVpa},
      });

  auto* it = non_standardized_attributes.find(value);
  return it != non_standardized_attributes.end()
             ? absl::optional<HtmlFieldType>(it->second)
             : absl::nullopt;
}

// If the autocomplete `value` doesn't match any of Autofill's supported values,
// Autofill should remain enabled for good intended values. This function checks
// if there is reason to believe so, by matching `value` against patterns like
// "address".
// Ignoring autocomplete="off" and alike is treated separately in
// `ParseAutocompleteAttribute()`.
bool ShouldIgnoreAutocompleteValue(base::StringPiece value) {
  static constexpr char16_t kRegex[] = u"address";
  return MatchesRegex<kRegex>(base::UTF8ToUTF16(value));
}

}  // namespace

bool operator==(const AutocompleteParsingResult& a,
                const AutocompleteParsingResult& b) {
  return std::tie(a.section, a.mode, a.field_type) ==
         std::tie(b.section, b.mode, b.field_type);
}
bool operator!=(const AutocompleteParsingResult& a,
                const AutocompleteParsingResult& b) {
  return !(a == b);
}

std::string AutocompleteParsingResult::ToString() const {
  return base::StrCat({"section='", section, "' ", "mode='",
                       HtmlFieldModeToStringPiece(mode), "' ", "field_type='",
                       FieldTypeToStringPiece(field_type), "'"});
}

HtmlFieldType FieldTypeFromAutocompleteAttributeValue(
    std::string value,
    uint64_t field_max_length) {
  if (value.empty())
    return HtmlFieldType::kUnspecified;

  // We are lenient and accept '_' instead of '-' as a separator. E.g.
  // "given_name" is treated like "given-name".
  base::ReplaceChars(value, "_", "-", &value);
  // We accept e.g. "phone-country" instead of "tel-country".
  if (base::StartsWith(value, "phone"))
    base::ReplaceFirstSubstringAfterOffset(&value, 0, "phone", "tel");

  absl::optional<HtmlFieldType> type =
      ParseStandardizedAutocompleteAttribute(value);
  if (!type.has_value()) {
    type = ParseProposedAutocompleteAttribute(value);
    if (!type.has_value())
      type = ParseNonStandarizedAutocompleteAttribute(value);
  }

  if (type.has_value())
    return RationalizeAutocompleteType(type.value(), field_max_length);

  // `value` cannot be mapped to any HtmlFieldType. By classifying the field
  // as HtmlFieldType::kUnrecognized Autofill is effectively disabled.
  // Instead, check if we have reason to ignore the value and treat the field as
  // HtmlFieldType::kUnspecified. This makes us ignore the autocomplete
  // value.
  return ShouldIgnoreAutocompleteValue(value) &&
                 base::FeatureList::IsEnabled(
                     features::kAutofillIgnoreUnmappableAutocompleteValues)
             ? HtmlFieldType::kUnspecified
             : HtmlFieldType::kUnrecognized;
}

absl::optional<AutocompleteParsingResult> ParseAutocompleteAttribute(
    base::StringPiece autocomplete_attribute,
    uint64_t field_max_length) {
  std::vector<std::string> tokens =
      LowercaseAndTokenizeAttributeString(autocomplete_attribute);

  // The autocomplete attribute is overloaded: it can specify either a field
  // type hint or whether autocomplete should be enabled at all. Ignore the
  // latter type of attribute value.
  if (tokens.empty() ||
      (tokens.size() == 1 && ShouldIgnoreAutocompleteAttribute(tokens[0]))) {
    return absl::nullopt;
  }

  AutocompleteParsingResult result;

  // Parse the "webauthn" token.
  if (tokens.back() == "webauthn") {
    result.webauthn = true;
    tokens.pop_back();
    if (tokens.empty()) {
      return result;
    }
  }

  // (1) The final token must be the field type.
  std::string field_type_token = tokens.back();
  tokens.pop_back();
  result.field_type = FieldTypeFromAutocompleteAttributeValue(field_type_token,
                                                              field_max_length);

  // (2) The preceding token, if any, may be a type hint.
  if (!tokens.empty() && IsContactTypeHint(tokens.back())) {
    // If it is, it must match the field type; otherwise, abort.
    // Note that an invalid token invalidates the entire attribute value, even
    // if the other tokens are valid.
    if (!ContactTypeHintMatchesFieldType(tokens.back(), result.field_type))
      return absl::nullopt;
    // Chrome Autofill ignores these type hints.
    tokens.pop_back();
  }

  // (3) The preceding token, if any, may be a fixed string that is either
  // "shipping" or "billing".
  if (!tokens.empty()) {
    for (HtmlFieldMode mode :
         {HtmlFieldMode::kBilling, HtmlFieldMode::kShipping})
      if (tokens.back() == HtmlFieldModeToStringPiece(mode)) {
        result.mode = mode;
        tokens.pop_back();
        break;
      }
  }

  // (4) The preceding token, if any, may be a named section.
  constexpr base::StringPiece kSectionPrefix = "section-";
  if (!tokens.empty() && base::StartsWith(tokens.back(), kSectionPrefix,
                                          base::CompareCase::SENSITIVE)) {
    // Prepend this section name to the suffix set in the preceding block.
    result.section = tokens.back().substr(kSectionPrefix.size());
    tokens.pop_back();
  }

  // (5) No other tokens are allowed. If there are any remaining, abort.
  if (!tokens.empty())
    return absl::nullopt;

  return result;
}

bool ShouldIgnoreAutocompleteAttribute(base::StringPiece autocomplete) {
  return autocomplete == "on" || autocomplete == "off" ||
         autocomplete == "false";
}

}  // namespace autofill
