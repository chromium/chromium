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

static constexpr auto kStandardizedAttributes =
    base::MakeFixedFlatMap<std::string_view, HtmlFieldType>({
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

// If an autocomplete attribute length is larger than this cap, there is no need
// to bother checking if the developer made an honest mistake.
static constexpr int kMaxAutocompleteLengthToCheckForWellIntendedUsage = 70;

static constexpr std::string_view kWellIntendedAutocompleteValuesKeywords[] = {
    "street", "password", "address", "bday",     "cc-",         "family",
    "name",   "country",  "tel",     "phone",    "transaction", "code",
    "zip",    "state",    "city",    "shipping", "billing"};

static constexpr std::string_view
    kNegativeMatchWellIntendedAutocompleteValuesKeywords[] = {
        "off", "disabled", "nope", "noop", "fake", "false", "new"};

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

// Chrome Autofill supports a subset of the field types listed at
// http://is.gd/whatwg_autocomplete. Returns the corresponding HtmlFieldType, if
// `value` matches any of them.
std::optional<HtmlFieldType> ParseStandardizedAutocompleteAttribute(
    std::string_view value) {
  auto it = kStandardizedAttributes.find(value);
  return it != kStandardizedAttributes.end()
             ? std::optional<HtmlFieldType>(it->second)
             : std::nullopt;
}

// Maps `value`s that Autofill has proposed for the HTML autocomplete standard,
// but which are not standardized, to their HtmlFieldType.
std::optional<HtmlFieldType> ParseProposedAutocompleteAttribute(
    std::string_view value) {
  static constexpr auto proposed_attributes =
      base::MakeFixedFlatMap<std::string_view, HtmlFieldType>({
          {"address", HtmlFieldType::kStreetAddress},
          {"coupon-code", HtmlFieldType::kMerchantPromoCode},
          // TODO(crbug.com/40234618): Investigate if this mapping makes sense.
          {"username", HtmlFieldType::kEmail},
      });

  auto it = proposed_attributes.find(value);
  return it != proposed_attributes.end()
             ? std::optional<HtmlFieldType>(it->second)
             : std::nullopt;
}

// Maps non-standardized `value`s for the HTML autocomplete attribute to an
// HtmlFieldType. This is primarily a list of "reasonable guesses".
std::optional<HtmlFieldType> ParseNonStandarizedAutocompleteAttribute(
    std::string_view value) {
  static constexpr auto non_standardized_attributes =
      base::MakeFixedFlatMap<std::string_view, HtmlFieldType>({
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
      });

  auto it = non_standardized_attributes.find(value);
  return it != non_standardized_attributes.end()
             ? std::optional<HtmlFieldType>(it->second)
             : std::nullopt;
}

}  // namespace

std::string AutocompleteParsingResult::ToString() const {
  return base::StrCat({"section='", section, "' ", "mode='",
                       HtmlFieldModeToStringView(mode), "' ", "field_type='",
                       FieldTypeToStringView(field_type), "' ", "webauthn='",
                       webauthn ? "true" : "false", "'"});
}

bool AutocompleteParsingResult::operator==(
    const AutocompleteParsingResult&) const = default;

HtmlFieldType FieldTypeFromAutocompleteAttributeValue(std::string value) {
  if (value.empty())
    return HtmlFieldType::kUnspecified;

  // We are lenient and accept '_' instead of '-' as a separator. E.g.
  // "given_name" is treated like "given-name".
  base::ReplaceChars(value, "_", "-", &value);
  // We accept e.g. "phone-country" instead of "tel-country".
  if (value.starts_with("phone")) {
    base::ReplaceFirstSubstringAfterOffset(&value, 0, "phone", "tel");
  }

  std::optional<HtmlFieldType> type =
      ParseStandardizedAutocompleteAttribute(value);
  if (!type.has_value()) {
    type = ParseProposedAutocompleteAttribute(value);
    if (!type.has_value())
      type = ParseNonStandarizedAutocompleteAttribute(value);
  }

  if (type.has_value())
    return *type;

  // `value` cannot be mapped to any HtmlFieldType. By classifying the field
  // as HtmlFieldType::kUnrecognized Autofill is effectively disabled.
  return HtmlFieldType::kUnrecognized;
}

std::optional<AutocompleteParsingResult> ParseAutocompleteAttribute(
    std::string_view autocomplete_attribute) {
  std::vector<std::string> tokens =
      LowercaseAndTokenizeAttributeString(autocomplete_attribute);

  // The autocomplete attribute is overloaded: it can specify either a field
  // type hint or whether autocomplete should be enabled at all. Ignore the
  // latter type of attribute value.
  if (tokens.empty() ||
      (tokens.size() == 1 && ShouldIgnoreAutocompleteAttribute(tokens[0]))) {
    return std::nullopt;
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
  result.field_type = FieldTypeFromAutocompleteAttributeValue(field_type_token);

  // (2) The preceding token, if any, may be a type hint.
  if (!tokens.empty() && IsContactTypeHint(tokens.back())) {
    // If it is, it must match the field type; otherwise, abort.
    // Note that an invalid token invalidates the entire attribute value, even
    // if the other tokens are valid.
    if (!ContactTypeHintMatchesFieldType(tokens.back(), result.field_type))
      return std::nullopt;
    // Chrome Autofill ignores these type hints.
    tokens.pop_back();
  }

  // (3) The preceding token, if any, may be a fixed string that is either
  // "shipping" or "billing".
  if (!tokens.empty()) {
    for (HtmlFieldMode mode :
         {HtmlFieldMode::kBilling, HtmlFieldMode::kShipping})
      if (tokens.back() == HtmlFieldModeToStringView(mode)) {
        result.mode = mode;
        tokens.pop_back();
        break;
      }
  }

  // (4) The preceding token, if any, may be a named section.
  constexpr std::string_view kSectionPrefix = "section-";
  if (!tokens.empty() && tokens.back().starts_with(kSectionPrefix)) {
    // Prepend this section name to the suffix set in the preceding block.
    result.section = tokens.back().substr(kSectionPrefix.size());
    tokens.pop_back();
  }

  // (5) No other tokens are allowed. If there are any remaining, abort.
  if (!tokens.empty())
    return std::nullopt;

  return result;
}

bool IsAutocompleteTypeWrongButWellIntended(
    std::string_view autocomplete_attribute) {
  if (autocomplete_attribute.size() >=
      kMaxAutocompleteLengthToCheckForWellIntendedUsage) {
    return false;
  }
  std::vector<std::string> tokens =
      LowercaseAndTokenizeAttributeString(autocomplete_attribute);

  // The autocomplete attribute is overloaded: it can specify either a field
  // type hint or whether autocomplete should be enabled at all. Ignore the
  // latter type of attribute value.
  if (tokens.empty() ||
      (tokens.size() == 1 && ShouldIgnoreAutocompleteAttribute(tokens[0]))) {
    return false;
  }

  // Parse the "webauthn" token.
  if (tokens.back() == "webauthn") {
    tokens.pop_back();
    if (tokens.empty()) {
      return false;
    }
  }

  std::string field_type_token = tokens.back();

  // Autofill does not recognize password inputs, so we have to manually check
  // for them.
  bool is_field_type_password = field_type_token == "new-password" ||
                                field_type_token == "current-password";
  if (is_field_type_password ||
      FieldTypeFromAutocompleteAttributeValue(field_type_token) !=
          HtmlFieldType::kUnrecognized) {
    return false;
  }

  auto contains_field_type_token = [&](std::string_view s) {
    return std::string_view(field_type_token).find(s) != std::string::npos;
  };
  bool token_is_wrong_but_has_well_intended_usage_keyword = std::ranges::any_of(
      kWellIntendedAutocompleteValuesKeywords, contains_field_type_token);
  bool developer_likely_tried_to_disable_autofill =
      std::ranges::any_of(kNegativeMatchWellIntendedAutocompleteValuesKeywords,
                          contains_field_type_token);
  return token_is_wrong_but_has_well_intended_usage_keyword &&
         !developer_likely_tried_to_disable_autofill;
}

bool ShouldIgnoreAutocompleteAttribute(std::string_view autocomplete) {
  return autocomplete == "on" || autocomplete == "off" ||
         autocomplete == "false";
}

}  // namespace autofill
