// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/field_formatter.h"

#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/state_names.h"
#include "components/autofill_assistant/browser/generic_ui.pb.h"
#include "third_party/re2/src/re2/re2.h"
#include "third_party/re2/src/re2/stringpiece.h"

namespace {
// Regex to find placeholders of the form ${key}, where key is an arbitrary
// string that does not contain curly braces. The first capture group is for
// the prefix before the key, the second for the key itself.
const char kPlaceholderExtractor[] = R"re((.*?)\$\{([^{}]+)\})re";

absl::optional<std::string> GetFieldValue(
    const std::map<std::string, std::string>& mappings,
    const std::string& key) {
  auto it = mappings.find(key);
  if (it == mappings.end()) {
    return absl::nullopt;
  }
  return it->second;
}

std::map<std::string, std::string> CreateFormGroupMappings(
    const autofill::FormGroup& form_group,
    const std::string& locale) {
  std::map<std::string, std::string> mappings;
  autofill::ServerFieldTypeSet available_fields;
  form_group.GetNonEmptyTypes(locale, &available_fields);
  for (const auto& field : available_fields) {
    mappings.emplace(base::NumberToString(static_cast<int>(field)),
                     base::UTF16ToUTF8(form_group.GetInfo(
                         autofill::AutofillType(field), locale)));
  }
  return mappings;
}

}  // namespace

namespace autofill_assistant {
namespace field_formatter {

absl::optional<std::string> FormatString(
    const std::string& pattern,
    const std::map<std::string, std::string>& mappings,
    bool strict) {
  if (pattern.empty()) {
    return std::string();
  }

  std::string out;
  re2::StringPiece input(pattern);
  std::string prefix;
  std::string key;
  while (
      re2::RE2::FindAndConsume(&input, kPlaceholderExtractor, &prefix, &key)) {
    auto rewrite_value = GetFieldValue(mappings, key);
    if (!rewrite_value.has_value()) {
      if (strict) {
        VLOG(2) << "No value for " << key << " in " << pattern;
        return absl::nullopt;
      }
      // Leave placeholder unchanged.
      rewrite_value = "${" + key + "}";
    }

    out = out + prefix + *rewrite_value;
  }
  // Append remaining unmatched suffix (if any).
  out = out + input.ToString();

  return out;
}

ClientStatus FormatExpression(
    const ValueExpression& value_expression,
    const std::map<std::string, std::string>& mappings,
    bool quote_meta,
    std::string* out_value) {
  out_value->clear();
  for (const auto& chunk : value_expression.chunk()) {
    switch (chunk.chunk_case()) {
      case ValueExpression::Chunk::kText:
        out_value->append(chunk.text());
        break;
      case ValueExpression::Chunk::kKey: {
        auto rewrite_value =
            GetFieldValue(mappings, base::NumberToString(chunk.key()));
        if (!rewrite_value.has_value()) {
          return ClientStatus(AUTOFILL_INFO_NOT_AVAILABLE);
        }
        if (quote_meta) {
          out_value->append(re2::RE2::QuoteMeta(*rewrite_value));
        } else {
          out_value->append(*rewrite_value);
        }
        break;
      }
      case ValueExpression::Chunk::CHUNK_NOT_SET:
        return ClientStatus(INVALID_ACTION);
    }
  }

  return OkClientStatus();
}

std::string GetHumanReadableValueExpression(
    const ValueExpression& value_expression) {
  std::string out;
  for (const auto& chunk : value_expression.chunk()) {
    switch (chunk.chunk_case()) {
      case ValueExpression::Chunk::kText:
        out += chunk.text();
        break;
      case ValueExpression::Chunk::kKey:
        out += "${" + base::NumberToString(chunk.key()) + "}";
        break;
      case ValueExpression::Chunk::CHUNK_NOT_SET:
        out += "<CHUNK_NOT_SET>";
        break;
    }
  }
  return out;
}

template <>
std::map<std::string, std::string>
CreateAutofillMappings<autofill::AutofillProfile>(
    const autofill::AutofillProfile& profile,
    const std::string& locale) {
  auto mappings = CreateFormGroupMappings(profile, locale);

  auto state = profile.GetInfo(
      autofill::AutofillType(autofill::ADDRESS_HOME_STATE), locale);
  if (!state.empty()) {
    std::u16string full_name;
    std::u16string abbreviation;
    autofill::state_names::GetNameAndAbbreviation(state, &full_name,
                                                  &abbreviation);
    mappings[base::NumberToString(
        static_cast<int>(AutofillFormatProto::ADDRESS_HOME_STATE_NAME))] =
        base::UTF16ToUTF8(
            full_name.length() > 1
                ? base::StrCat({base::i18n::ToUpper(full_name.substr(0, 1)),
                                full_name.substr(1)})
                : base::i18n::ToUpper(full_name));
    if (abbreviation.empty()) {
      mappings.erase(
          base::NumberToString(static_cast<int>(autofill::ADDRESS_HOME_STATE)));
    } else {
      mappings[base::NumberToString(
          static_cast<int>(autofill::ADDRESS_HOME_STATE))] =
          base::UTF16ToUTF8(base::i18n::ToUpper(abbreviation));
    }
  }

  return mappings;
}

template <>
std::map<std::string, std::string> CreateAutofillMappings<autofill::CreditCard>(
    const autofill::CreditCard& credit_card,
    const std::string& locale) {
  auto mappings = CreateFormGroupMappings(credit_card, locale);

  auto network = std::string(
      autofill::data_util::GetPaymentRequestData(credit_card.network())
          .basic_card_issuer_network);
  if (!network.empty()) {
    mappings[base::NumberToString(
        static_cast<int>(AutofillFormatProto::CREDIT_CARD_NETWORK))] = network;
  }
  auto network_for_display = base::UTF16ToUTF8(credit_card.NetworkForDisplay());
  if (!network_for_display.empty()) {
    mappings[base::NumberToString(static_cast<int>(
        AutofillFormatProto::CREDIT_CARD_NETWORK_FOR_DISPLAY))] =
        network_for_display;
  }
  auto last_four_digits = base::UTF16ToUTF8(credit_card.LastFourDigits());
  if (!last_four_digits.empty()) {
    mappings[base::NumberToString(static_cast<int>(
        AutofillFormatProto::CREDIT_CARD_NUMBER_LAST_FOUR_DIGITS))] =
        last_four_digits;
  }
  int month;
  if (base::StringToInt(
          credit_card.GetInfo(autofill::CREDIT_CARD_EXP_MONTH, locale),
          &month)) {
    mappings[base::NumberToString(static_cast<int>(
        AutofillFormatProto::CREDIT_CARD_NON_PADDED_EXP_MONTH))] =
        base::NumberToString(month);
  }

  return mappings;
}

}  // namespace field_formatter

std::ostream& operator<<(std::ostream& out,
                         const ValueExpression& value_expression) {
  return out << field_formatter::GetHumanReadableValueExpression(
             value_expression);
}

}  // namespace autofill_assistant
