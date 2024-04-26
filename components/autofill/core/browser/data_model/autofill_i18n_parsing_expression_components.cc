// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_i18n_parsing_expression_components.h"

#include "base/strings/strcat.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_utils.h"

namespace autofill::i18n_model_definition {
namespace {

inline std::string RemoveVersionSuffix(const std::string& token) {
  return token.substr(0, token.find("__"));
}

std::optional<base::flat_map<std::string, std::string>> ParseUsingRegex(
    std::string_view value,
    std::string_view pattern) {
  const RE2* regex = Re2RegExCache::Instance()->GetRegEx(pattern);
  if (!regex || !regex->ok()) {
    return std::nullopt;
  }

  // Get the number of capturing groups in the expression.
  // Note, the capturing group for the full match is not counted.
  size_t number_of_capturing_groups = regex->NumberOfCapturingGroups() + 1;

  // Create result vectors to get the matches for the capturing groups.
  std::vector<std::string> results(number_of_capturing_groups);
  std::vector<RE2::Arg> match_results(number_of_capturing_groups);
  std::vector<RE2::Arg*> match_results_ptr(number_of_capturing_groups);

  // Note, the capturing group for the full match is not counted by
  // |NumberOfCapturingGroups|.
  for (size_t i = 0; i < number_of_capturing_groups; ++i) {
    match_results[i] = &results[i];
    match_results_ptr[i] = &match_results[i];
  }

  // One capturing group is not counted since it holds the full match.
  if (!RE2::PartialMatchN(value, *regex, match_results_ptr.data(),
                          number_of_capturing_groups - 1)) {
    return std::nullopt;
  }

  // If successful, write the values into the results map.
  // Note, the capturing group for the full match creates an off-by-one scenario
  // in the indexing.
  std::vector<std::pair<std::string, std::string>> matches;
  for (const auto& group : regex->NamedCapturingGroups()) {
    const auto& [name, index] = group;
    if (results[index - 1].empty()) {
      continue;
    }
    // TODO(crbug.com/40275657): Remove unknown type special handling once field
    // types for unit-type and unit-name tokens are introduced.
    std::string parsed_name = RemoveVersionSuffix(name);
    if (parsed_name == "UNKNOWN_TYPE") {
      continue;
    }
    matches.emplace_back(parsed_name, results[index - 1]);
  }

  return base::MakeFlatMap<std::string, std::string>(std::move(matches));
}

// Check that the condition regex is matched if exist.
bool ConditionIsMatched(std::string_view condition_regex,
                        std::string_view value) {
  if (condition_regex.empty()) {
    return true;
  }
  const RE2* regex = Re2RegExCache::Instance()->GetRegEx(condition_regex);
  return RE2::PartialMatch(value, *regex);
}
}  // namespace

ValueParsingResults Decomposition::Parse(std::string_view value) const {
  std::string_view prefix = anchor_beginning_ ? "^" : "";
  std::string_view suffix = anchor_end_ ? "$" : "";
  std::string regex = base::StrCat({prefix, parsing_regex_, suffix});
  return ParseUsingRegex(value, regex);
}

ValueParsingResults DecompositionCascade::Parse(std::string_view value) const {
  if (!ConditionIsMatched(condition_regex_, value)) {
    return std::nullopt;
  }

  for (const auto* alternative : alternatives_) {
    auto result = alternative->Parse(value);
    if (result.has_value()) {
      return result;
    }
  }
  return std::nullopt;
}

ValueParsingResults ExtractPart::Parse(std::string_view value) const {
  if (!ConditionIsMatched(condition_regex_, value)) {
    return std::nullopt;
  }

  return ParseUsingRegex(value, parsing_regex_);
}

ValueParsingResults ExtractParts::Parse(std::string_view value) const {
  if (!ConditionIsMatched(condition_regex_, value)) {
    return std::nullopt;
  }
  base::flat_map<std::string, std::string> result;
  for (const auto* piece : pieces_) {
    auto piece_match = piece->Parse(value);
    if (piece_match.has_value()) {
      for (const auto& [field_type_str, matched_string] : *piece_match) {
        result.insert_or_assign(field_type_str, matched_string);
      }
    }
  }
  if (!result.empty()) {
    return result;
  }
  return std::nullopt;
}

}  // namespace autofill::i18n_model_definition
