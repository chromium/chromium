// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/field_formatter.h"

#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/state_names.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill_assistant/browser/generic_ui.pb.h"
#include "third_party/re2/src/re2/re2.h"
#include "third_party/re2/src/re2/stringpiece.h"

namespace autofill_assistant {
namespace field_formatter {
namespace {
// Regex to find placeholders of the form ${key}, where key is an arbitrary
// string that does not contain curly braces. The first capture group is for
// the prefix before the key, the second for the key itself.
const char kPlaceholderExtractor[] = R"re((.*?)\$\{([^{}]+)\})re";

template <typename T>
absl::optional<std::string> GetFieldValue(
    const base::flat_map<T, std::string>& mappings,
    const T& key) {
  auto it = mappings.find(key);
  if (it == mappings.end()) {
    return absl::nullopt;
  }
  return it->second;
}

base::flat_map<Key, std::string> CreateFormGroupMappings(
    const autofill::FormGroup& form_group,
    const std::string& locale) {
  std::vector<std::pair<Key, std::string>> mappings;
  autofill::ServerFieldTypeSet available_fields;
  form_group.GetNonEmptyTypes(locale, &available_fields);
  for (const auto field : available_fields) {
    mappings.emplace_back(Key(field),
                          base::UTF16ToUTF8(form_group.GetInfo(
                              autofill::AutofillType(field), locale)));
  }
  return base::flat_map<Key, std::string>(std::move(mappings));
}

void GetNameAndAbbreviationViaAlternativeStateNameMap(
    const std::string& country_code,
    const std::u16string& state_from_profile,
    std::u16string* name,
    std::u16string* abbreviation) {
  absl::optional<autofill::StateEntry> state_entry =
      autofill::AlternativeStateNameMap::GetInstance()->GetEntry(
          autofill::AlternativeStateNameMap::CountryCode(country_code),
          autofill::AlternativeStateNameMap::StateName(state_from_profile));
  if (!state_entry) {
    // Name and abbreviation are already prefilled.
    return;
  }
  if (state_entry->has_canonical_name() &&
      !state_entry->canonical_name().empty()) {
    std::u16string full = base::ASCIIToUTF16(state_entry->canonical_name());
    std::u16string abbr;
    size_t curr_min_abbr_size = INT_MAX;
    for (const auto& it_abbr : state_entry->abbreviations()) {
      if (!it_abbr.empty() && it_abbr.size() < curr_min_abbr_size) {
        abbr = base::ASCIIToUTF16(it_abbr);
        curr_min_abbr_size = it_abbr.size();
      }
    }
    if (name) {
      name->swap(full);
    }
    if (abbreviation) {
      abbreviation->swap(abbr);
    }
  }
}

std::string ApplyChunkReplacement(
    const google::protobuf::Map<std::string, std::string>& replacements,
    const std::string& value) {
  const auto& it = replacements.find(value);
  if (it != replacements.end()) {
    return it->second;
  }
  return value;
}

std::string GetMaybeQuotedChunk(const std::string& value, bool quote_meta) {
  if (quote_meta) {
    return re2::RE2::QuoteMeta(value);
  }
  return value;
}

}  // namespace

Key::Key(int key) : int_key(key) {}
Key::Key(AutofillFormatProto::AutofillAssistantCustomField custom_field)
    : int_key(static_cast<int>(custom_field)) {}
Key::Key(autofill::ServerFieldType autofill_field)
    : int_key(static_cast<int>(autofill_field)) {}
Key::Key(std::string key) : string_key(key) {}

Key::~Key() = default;
Key::Key(const Key&) = default;

bool Key::operator<(const Key& other) const {
  return std::make_tuple(this->int_key.value_or(0),
                         this->string_key.value_or(std::string())) <
         std::make_tuple(other.int_key.value_or(0),
                         other.string_key.value_or(std::string()));
}

bool Key::operator==(const Key& other) const {
  return !(*this < other) && !(other < *this);
}

absl::optional<std::string> FormatString(
    const std::string& pattern,
    const base::flat_map<std::string, std::string>& mappings,
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

ClientStatus FormatExpression(const ValueExpression& value_expression,
                              const base::flat_map<Key, std::string>& mappings,
                              bool quote_meta,
                              std::string* out_value) {
  out_value->clear();
  for (const auto& chunk : value_expression.chunk()) {
    std::string chunk_value;
    switch (chunk.chunk_case()) {
      case ValueExpression::Chunk::kText:
        chunk_value = chunk.text();
        break;
      case ValueExpression::Chunk::kKey: {
        auto rewrite_value = GetFieldValue(mappings, Key(chunk.key()));
        if (!rewrite_value.has_value()) {
          return ClientStatus(AUTOFILL_INFO_NOT_AVAILABLE);
        }
        chunk_value = GetMaybeQuotedChunk(*rewrite_value, quote_meta);
        break;
      }
      case ValueExpression::Chunk::kMemoryKey: {
        auto rewrite_value = GetFieldValue(mappings, Key(chunk.memory_key()));
        if (!rewrite_value.has_value()) {
          return ClientStatus(CLIENT_MEMORY_KEY_NOT_AVAILABLE);
        }
        chunk_value = GetMaybeQuotedChunk(*rewrite_value, quote_meta);
        break;
      }
      case ValueExpression::Chunk::CHUNK_NOT_SET:
        return ClientStatus(INVALID_ACTION);
    }
    out_value->append(ApplyChunkReplacement(chunk.replacements(), chunk_value));
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
        out += base::StrCat({"${", base::NumberToString(chunk.key()), "}"});
        break;
      case ValueExpression::Chunk::kMemoryKey:
        out += base::StrCat({"${", chunk.memory_key(), "}"});
        break;
      case ValueExpression::Chunk::CHUNK_NOT_SET:
        out += "<CHUNK_NOT_SET>";
        break;
    }
  }
  return out;
}

template <>
base::flat_map<Key, std::string>
CreateAutofillMappings<autofill::AutofillProfile>(
    const autofill::AutofillProfile& profile,
    const std::string& locale) {
  auto mappings = CreateFormGroupMappings(profile, locale);

  std::string country_code =
      base::UTF16ToUTF8(profile.GetRawInfo(autofill::ADDRESS_HOME_COUNTRY));
  if (!country_code.empty()) {
    mappings.emplace(Key(AutofillFormatProto::ADDRESS_HOME_COUNTRY_CODE),
                     country_code);
  }
  auto state = profile.GetInfo(
      autofill::AutofillType(autofill::ADDRESS_HOME_STATE), locale);
  if (!state.empty()) {
    std::u16string full_name;
    std::u16string abbreviation;
    autofill::state_names::GetNameAndAbbreviation(state, &full_name,
                                                  &abbreviation);
    DCHECK(!full_name.empty());
    full_name = full_name.length() > 1
                    ? base::StrCat({base::i18n::ToUpper(full_name.substr(0, 1)),
                                    full_name.substr(1)})
                    : base::i18n::ToUpper(full_name);
    if (abbreviation.empty() && !country_code.empty() &&
        base::FeatureList::IsEnabled(
            autofill::features::kAutofillUseAlternativeStateNameMap)) {
      GetNameAndAbbreviationViaAlternativeStateNameMap(
          country_code, state, &full_name, &abbreviation);
    }
    mappings.emplace(Key(AutofillFormatProto::ADDRESS_HOME_STATE_NAME),
                     base::UTF16ToUTF8(full_name));
    if (abbreviation.empty()) {
      mappings.erase(Key(autofill::ADDRESS_HOME_STATE));
    } else {
      mappings[Key(autofill::ADDRESS_HOME_STATE)] =
          base::UTF16ToUTF8(base::i18n::ToUpper(abbreviation));
    }
  }
  return mappings;
}

template <>
base::flat_map<Key, std::string> CreateAutofillMappings<autofill::CreditCard>(
    const autofill::CreditCard& credit_card,
    const std::string& locale) {
  auto mappings = CreateFormGroupMappings(credit_card, locale);

  auto network = std::string(
      autofill::data_util::GetPaymentRequestData(credit_card.network())
          .basic_card_issuer_network);
  if (!network.empty()) {
    mappings.emplace(Key(AutofillFormatProto::CREDIT_CARD_NETWORK), network);
  }
  auto network_for_display = base::UTF16ToUTF8(credit_card.NetworkForDisplay());
  if (!network_for_display.empty()) {
    mappings.emplace(Key(AutofillFormatProto::CREDIT_CARD_NETWORK_FOR_DISPLAY),
                     network_for_display);
  }
  auto last_four_digits = base::UTF16ToUTF8(credit_card.LastFourDigits());
  if (!last_four_digits.empty()) {
    mappings.emplace(
        Key(AutofillFormatProto::CREDIT_CARD_NUMBER_LAST_FOUR_DIGITS),
        last_four_digits);
  }
  int month;
  if (base::StringToInt(
          credit_card.GetInfo(autofill::CREDIT_CARD_EXP_MONTH, locale),
          &month)) {
    mappings.emplace(Key(AutofillFormatProto::CREDIT_CARD_NON_PADDED_EXP_MONTH),
                     base::NumberToString(month));
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
