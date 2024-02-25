// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/redactor.h"

#include <algorithm>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "third_party/re2/src/re2/re2.h"

namespace optimization_guide {

Rule::Rule() = default;
Rule::Rule(const Rule& r) = default;
Rule::~Rule() = default;

Redactor::CachedRule::CachedRule() = default;

Redactor::CachedRule::~CachedRule() = default;

Redactor::Redactor(const std::vector<Rule>& rules) {
  for (const auto& rule : rules) {
    if (rule.behavior == proto::RedactBehavior::REDACT_BEHAVIOR_UNSPECIFIED) {
      continue;
    }
    auto cached_rule = std::make_unique<CachedRule>();
    cached_rule->re = std::make_unique<re2::RE2>(rule.regex);
    if (cached_rule->re->ok() && rule.matching_group.value_or(0) >= 0 &&
        rule.min_pattern_length.value_or(0) >= 0 &&
        rule.max_pattern_length.value_or(0) >= 0 &&
        rule.matching_group.value_or(0) >= 0 &&
        cached_rule->re->NumberOfCapturingGroups() >=
            rule.matching_group.value_or(0)) {
      cached_rule->rule = rule;
      rules_.push_back(std::move(cached_rule));
    }
  }
}

Redactor::~Redactor() = default;

RedactResult Redactor::Redact(const std::string& input,
                              std::string& output) const {
  for (auto& rule : rules_) {
    if (ProcessRule(*rule, input, output) == RedactResult::kReject) {
      return RedactResult::kReject;
    }
  }
  return RedactResult::kContinue;
}

RedactResult Redactor::ProcessRule(const CachedRule& cached_rule,
                                   const std::string& input,
                                   std::string& output) const {
  std::string_view output_view(output);
  const int group = cached_rule.rule.matching_group.value_or(0);
  // The first match gives the whole region.
  std::string_view matches[group + 1];
  size_t last_match_start = 0;
  std::string new_output;
  size_t next_start_offset = 0;
  while (next_start_offset < output_view.length() &&
         cached_rule.re->Match(output_view, next_start_offset,
                               output_view.length(), re2::RE2::UNANCHORED,
                               matches, group + 1)) {
    const std::string_view& match(matches[group]);
    if (IsValidMatchForRule(cached_rule.rule, match)) {
      if (cached_rule.rule.behavior == proto::RedactBehavior::REJECT) {
        return RedactResult::kReject;
      }
      if (cached_rule.rule.behavior == proto::RedactBehavior::REDACT_ALWAYS ||
          input.find(match) == std::string::npos) {
        const size_t match_start_offset_in_output =
            match.data() - output_view.data();
        new_output +=
            base::StrCat({std::string_view(output).substr(
                              last_match_start,
                              match_start_offset_in_output - last_match_start),
                          GetReplacementString(cached_rule.rule, match)});
        last_match_start = match_start_offset_in_output + match.length();
      }
    }
    // Always skip the match even if not valid. If this only skipped the first
    // character on an invalid match, then the shortening might trigger a match.
    // It's possible for a regex to match, but the length is zero. Ensure we
    // skip at least 1 character, otherwise this code could loop infinitely.
    next_start_offset = match.data() - output_view.data() +
                        std::max(static_cast<size_t>(1), match.length());
  }
  if (last_match_start == 0) {
    // No replacement happened, nothing to do.
    return RedactResult::kContinue;
  }
  if (last_match_start != output.length()) {
    new_output += output.substr(last_match_start);
  }
  std::swap(output, new_output);
  return RedactResult::kContinue;
}

// static
std::string Redactor::GetReplacementString(const Rule& rule,
                                           std::string_view match) {
  if (rule.replacement_string.has_value()) {
    return *rule.replacement_string;
  }
  std::string replacement(match.length() + 2, '#');
  replacement[0] = '[';
  replacement.back() = ']';
  return replacement;
}

// static
bool Redactor::IsValidMatchForRule(const Rule& rule,
                                   const std::string_view& match) {
  if (match.empty()) {
    return false;
  }

  if (match.length() <
      static_cast<size_t>(rule.min_pattern_length.value_or(0))) {
    return false;
  }
  if (rule.max_pattern_length &&
      match.length() > static_cast<size_t>(*rule.max_pattern_length)) {
    return false;
  }
  return true;
}

}  // namespace optimization_guide
