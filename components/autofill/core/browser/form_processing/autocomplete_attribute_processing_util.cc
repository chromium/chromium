// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/autofill/core/browser/form_processing/autocomplete_attribute_processing_util.h"

#include <vector>

#include "base/containers/fixed_flat_map.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_regexes.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_util.h"

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
    return field_type == HTML_TYPE_EMAIL ||
           (field_type >= HTML_TYPE_TEL &&
            field_type <= HTML_TYPE_TEL_LOCAL_SUFFIX);
  }

  // The "mobile" type hint is only appropriate for phone number field types.
  // Note that "fax" and "pager" are intentionally ignored, as Chrome does not
  // support filling either type of information.
  if (token == "mobile") {
    return field_type >= HTML_TYPE_TEL &&
           field_type <= HTML_TYPE_TEL_LOCAL_SUFFIX;
  }

  return false;
}

// Rationalizes the HTML `type` of `field`, based on the fields properties. At
// the moment only `max_length` is considered. For example, a max_length of 4
// might indicate a 4 digit year.
// In case no rationalization rule applies, the original type is returned.
HtmlFieldType RationalizeAutocompleteType(HtmlFieldType type,
                                          const AutofillField& field) {
  // (original-type, max-length) -> new-type
  static constexpr auto rules =
      base::MakeFixedFlatMap<std::pair<HtmlFieldType, uint64_t>, HtmlFieldType>(
          {
              {{HTML_TYPE_ADDITIONAL_NAME, 1},
               HTML_TYPE_ADDITIONAL_NAME_INITIAL},
              {{HTML_TYPE_CREDIT_CARD_EXP, 5},
               HTML_TYPE_CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR},
              {{HTML_TYPE_CREDIT_CARD_EXP, 7},
               HTML_TYPE_CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR},
              {{HTML_TYPE_CREDIT_CARD_EXP_YEAR, 2},
               HTML_TYPE_CREDIT_CARD_EXP_2_DIGIT_YEAR},
              {{HTML_TYPE_CREDIT_CARD_EXP_YEAR, 4},
               HTML_TYPE_CREDIT_CARD_EXP_4_DIGIT_YEAR},
          });

  auto* it = rules.find(std::make_pair(type, field.max_length));
  return it == rules.end() ? type : it->second;
}

// Chrome Autofill supports a subset of the field types listed at
// http://is.gd/whatwg_autocomplete. Returns the corresponding HtmlFieldType, if
// `value` matches any of them.
absl::optional<HtmlFieldType> ParseStandardizedAutocompleteAttribute(
    base::StringPiece value) {
  static constexpr auto standardized_attributes =
      base::MakeFixedFlatMap<base::StringPiece, HtmlFieldType>({
          {"additional-name", HTML_TYPE_ADDITIONAL_NAME},
          {"address-level1", HTML_TYPE_ADDRESS_LEVEL1},
          {"address-level2", HTML_TYPE_ADDRESS_LEVEL2},
          {"address-level3", HTML_TYPE_ADDRESS_LEVEL3},
          {"address-line1", HTML_TYPE_ADDRESS_LINE1},
          {"address-line2", HTML_TYPE_ADDRESS_LINE2},
          {"address-line3", HTML_TYPE_ADDRESS_LINE3},
          {"bday-day", HTML_TYPE_BIRTHDATE_DAY},
          {"bday-month", HTML_TYPE_BIRTHDATE_MONTH},
          {"bday-year", HTML_TYPE_BIRTHDATE_YEAR},
          {"cc-csc", HTML_TYPE_CREDIT_CARD_VERIFICATION_CODE},
          {"cc-exp", HTML_TYPE_CREDIT_CARD_EXP},
          {"cc-exp-month", HTML_TYPE_CREDIT_CARD_EXP_MONTH},
          {"cc-exp-year", HTML_TYPE_CREDIT_CARD_EXP_YEAR},
          {"cc-family-name", HTML_TYPE_CREDIT_CARD_NAME_LAST},
          {"cc-given-name", HTML_TYPE_CREDIT_CARD_NAME_FIRST},
          {"cc-name", HTML_TYPE_CREDIT_CARD_NAME_FULL},
          {"cc-number", HTML_TYPE_CREDIT_CARD_NUMBER},
          {"cc-type", HTML_TYPE_CREDIT_CARD_TYPE},
          {"country", HTML_TYPE_COUNTRY_CODE},
          {"country-name", HTML_TYPE_COUNTRY_NAME},
          {"email", HTML_TYPE_EMAIL},
          {"family-name", HTML_TYPE_FAMILY_NAME},
          {"given-name", HTML_TYPE_GIVEN_NAME},
          {"honorific-prefix", HTML_TYPE_HONORIFIC_PREFIX},
          {"name", HTML_TYPE_NAME},
          {"one-time-code", HTML_TYPE_ONE_TIME_CODE},
          {"organization", HTML_TYPE_ORGANIZATION},
          {"postal-code", HTML_TYPE_POSTAL_CODE},
          {"street-address", HTML_TYPE_STREET_ADDRESS},
          {"tel-area-code", HTML_TYPE_TEL_AREA_CODE},
          {"tel-country-code", HTML_TYPE_TEL_COUNTRY_CODE},
          {"tel-extension", HTML_TYPE_TEL_EXTENSION},
          {"tel", HTML_TYPE_TEL},
          {"tel-local", HTML_TYPE_TEL_LOCAL},
          {"tel-local-prefix", HTML_TYPE_TEL_LOCAL_PREFIX},
          {"tel-local-suffix", HTML_TYPE_TEL_LOCAL_SUFFIX},
          {"tel-national", HTML_TYPE_TEL_NATIONAL},
          {"transaction-amount", HTML_TYPE_TRANSACTION_AMOUNT},
          {"transaction-currency", HTML_TYPE_TRANSACTION_CURRENCY},
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
          {"address", HTML_TYPE_STREET_ADDRESS},
          {"coupon-code", HTML_TYPE_MERCHANT_PROMO_CODE},
          {"username", HTML_TYPE_EMAIL},
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
          {"company", HTML_TYPE_ORGANIZATION},
          {"first-name", HTML_TYPE_GIVEN_NAME},
          {"gift-code", HTML_TYPE_MERCHANT_PROMO_CODE},
          {"locality", HTML_TYPE_ADDRESS_LEVEL2},
          {"promo-code", HTML_TYPE_MERCHANT_PROMO_CODE},
          {"promotional-code", HTML_TYPE_MERCHANT_PROMO_CODE},
          {"promotion-code", HTML_TYPE_MERCHANT_PROMO_CODE},
          {"region", HTML_TYPE_ADDRESS_LEVEL1},
          {"tel-ext", HTML_TYPE_TEL_EXTENSION},
          {"upi", HTML_TYPE_UPI_VPA},
          {"upi-vpa", HTML_TYPE_UPI_VPA},
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
// `ParseFieldTypesFromAutocompleteAttributes()`.
bool ShouldIgnoreAutocompleteValue(const std::string& value) {
  return MatchesPattern(base::UTF8ToUTF16(value), u"address");
}

// Returns the Chrome Autofill-supported field type corresponding to a given
// autocomplete `value`, if there is one, in the context of the given
// `field`.
HtmlFieldType FieldTypeFromAutocompleteAttributeValue(
    std::string value,
    const AutofillField& field) {
  if (value.empty())
    return HTML_TYPE_UNSPECIFIED;

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
    return RationalizeAutocompleteType(type.value(), field);

  // `value` cannot be mapped to any HtmlFieldType. By classifying the field
  // as HTML_TYPE_UNRECOGNIZED Autofill is effectively disabled. Instead, check
  // if we have reason to ignore the value and treat the field as
  // HTML_TYPE_UNSPECIFIED. This makes us ignore the autocomplete value.
  return ShouldIgnoreAutocompleteValue(value) &&
                 base::FeatureList::IsEnabled(
                     features::kAutofillIgnoreUnmappableAutocompleteValues)
             ? HTML_TYPE_UNSPECIFIED
             : HTML_TYPE_UNRECOGNIZED;
}

}  // namespace

absl::optional<AutocompleteParsingResult> ParseAutocompleteAttribute(
    const AutofillField& field) {
  std::vector<std::string> tokens =
      LowercaseAndTokenizeAttributeString(field.autocomplete_attribute);

  // The autocomplete attribute is overloaded: it can specify either a field
  // type hint or whether autocomplete should be enabled at all. Ignore the
  // latter type of attribute value.
  if (tokens.empty() ||
      (tokens.size() == 1 &&
       (tokens[0] == "on" || tokens[0] == "off" || tokens[0] == "false"))) {
    return absl::nullopt;
  }

  AutocompleteParsingResult result;

  // The "webauthn" token is unused by Autofill, but skipped to parse the type
  // correctly.
  if (tokens.back() == "webauthn") {
    tokens.pop_back();
    if (tokens.empty())
      return absl::nullopt;
  }

  // (1) The final token must be the field type.
  std::string field_type_token = tokens.back();
  tokens.pop_back();
  result.field_type =
      FieldTypeFromAutocompleteAttributeValue(field_type_token, field);

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
  result.mode = HTML_MODE_NONE;
  if (!tokens.empty()) {
    for (HtmlFieldMode mode : {HTML_MODE_BILLING, HTML_MODE_SHIPPING})
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

base::StringPiece HtmlFieldModeToStringPiece(HtmlFieldMode mode) {
  if (mode == HTML_MODE_BILLING)
    return "billing";
  if (mode == HTML_MODE_SHIPPING)
    return "shipping";
  NOTREACHED();
  return "";
}

}  // namespace autofill
