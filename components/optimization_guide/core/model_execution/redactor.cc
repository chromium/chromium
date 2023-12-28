// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/redactor.h"

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "third_party/re2/src/re2/re2.h"

namespace optimization_guide {

Rule::Rule() = default;
Rule::Rule(const Rule& r) = default;
Rule::~Rule() = default;

Redactor::RuleImpl::RuleImpl() = default;

Redactor::RuleImpl::~RuleImpl() = default;

Redactor::Redactor(const std::vector<Rule>& rules) {
  for (const auto& rule : rules) {
    if (rule.behavior == proto::RedactBehavior::REDACT_BEHAVIOR_UNSPECIFIED) {
      continue;
    }
    auto rule_impl = std::make_unique<RuleImpl>();
    rule_impl->re = std::make_unique<re2::RE2>(rule.regex);
    rule_impl->behavior = rule.behavior;
    rule_impl->replacement_string = rule.replacement_string;
    rules_.push_back(std::move(rule_impl));
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

RedactResult Redactor::ProcessRule(const RuleImpl& rule,
                                   const std::string& input,
                                   std::string& output) const {
  // This is rather expensive, only enter the main loop if there is at least one
  // match.
  absl::string_view output_view(output);
  absl::string_view match;
  if (!rule.re->Match(output_view, 0, output_view.length(),
                      re2::RE2::UNANCHORED, &match, 1)) {
    return RedactResult::kContinue;
  }

  if (rule.behavior == proto::RedactBehavior::REJECT) {
    return RedactResult::kReject;
  }

  size_t last_match_start = 0;
  std::string new_output;
  size_t next_start_offset;
  do {
    next_start_offset = match.data() - output_view.data() + match.length();
    if (rule.behavior == proto::RedactBehavior::REDACT_ALWAYS ||
        input.find(match) == std::string::npos) {
      const size_t match_start_offset_in_output =
          match.data() - output_view.data();
      new_output +=
          base::StrCat({std::string_view(output).substr(
                            last_match_start,
                            match_start_offset_in_output - last_match_start),
                        GetReplacementString(rule, match)});
      last_match_start = match_start_offset_in_output + match.length();
    }
  } while (next_start_offset < output_view.length() &&
           rule.re->Match(output_view, next_start_offset, output_view.length(),
                          re2::RE2::UNANCHORED, &match, 1));
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

std::string Redactor::GetReplacementString(const RuleImpl& rule,
                                           absl::string_view match) const {
  if (rule.replacement_string.has_value()) {
    return *rule.replacement_string;
  }
  std::string replacement(match.length() + 2, '#');
  replacement[0] = '[';
  replacement.back() = ']';
  return replacement;
}

}  // namespace optimization_guide
