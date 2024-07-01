// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/redactor.h"

#include <algorithm>

#include "base/containers/heap_array.h"
#include "base/debug/dump_without_crashing.h"
#include "base/memory/ptr_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "components/optimization_guide/proto/redaction.pb.h"
#include "third_party/re2/src/re2/re2.h"

namespace optimization_guide {

Redactor::Rule::Rule(std::unique_ptr<re2::RE2> re,
                     Redactor::Behavior behavior,
                     std::string replacement_string,
                     int matching_group,
                     size_t min_pattern_length,
                     size_t max_pattern_length)
    : re_(std::move(re)),
      behavior_(behavior),
      replacement_string_(replacement_string),
      matching_group_(matching_group),
      min_pattern_length_(min_pattern_length),
      max_pattern_length_(max_pattern_length) {}
Redactor::Rule::Rule(Rule&& r) = default;
Redactor::Rule::~Rule() = default;

Redactor::Redactor(std::vector<Rule>&& rules) {
  rules_.swap(rules);
}

Redactor::~Redactor() = default;

// static
Redactor Redactor::FromProto(const proto::RedactRules& proto_rules) {
  std::vector<Redactor::Rule> rules;
  for (const auto& proto_rule : proto_rules.rules()) {
    if (proto_rule.regex().empty() ||
        proto_rule.group_index() < 0 || proto_rule.min_pattern_length() < 0 ||
        proto_rule.max_pattern_length() < 0) {
      base::debug::DumpWithoutCrashing();
      continue;
    }
    Behavior behavior = Redactor::Behavior::kReject;
    switch (proto_rule.behavior()) {
      case proto::RedactBehavior::REJECT:
        behavior = Redactor::Behavior::kReject;
        break;
      case proto::RedactBehavior::REDACT_IF_ONLY_IN_OUTPUT:
        behavior = Redactor::Behavior::kRedactIfOnlyInOutput;
        break;
      case proto::RedactBehavior::REDACT_ALWAYS:
        behavior = Redactor::Behavior::kRedactAlways;
        break;
      default:
        base::debug::DumpWithoutCrashing();
        continue;
    }
    auto re = std::make_unique<re2::RE2>(proto_rule.regex());
    if (!re->ok() || re->NumberOfCapturingGroups() < proto_rule.group_index()) {
      base::debug::DumpWithoutCrashing();
      continue;
    }
    rules.emplace_back(std::move(re), behavior, proto_rule.replacement_string(),
                       proto_rule.group_index(),
                       proto_rule.min_pattern_length(),
                       proto_rule.max_pattern_length());
  }
  return Redactor(std::move(rules));
}

RedactResult Redactor::Redact(const std::string& input,
                              std::string& output) const {
  for (auto& rule : rules_) {
    if (rule.Process(input, output) == RedactResult::kReject) {
      return RedactResult::kReject;
    }
  }
  return RedactResult::kContinue;
}

RedactResult Redactor::Rule::Process(const std::string& input,
                                     std::string& output) const {
  std::string_view output_view(output);
  const int group = matching_group_;
  // The first match gives the whole region.
  auto matches = base::HeapArray<std::string_view>::WithSize(group + 1);
  size_t last_match_start = 0;
  std::string new_output;
  size_t next_start_offset = 0;
  while (next_start_offset < output_view.length() &&
         re_->Match(output_view, next_start_offset, output_view.length(),
                    re2::RE2::UNANCHORED, matches.data(), group + 1)) {
    const std::string_view& match(matches[group]);
    if (IsValidMatch(match)) {
      if (behavior_ == Redactor::Behavior::kReject) {
        return RedactResult::kReject;
      }
      if (behavior_ == Redactor::Behavior::kRedactAlways ||
          input.find(match) == std::string::npos) {
        const size_t match_start_offset_in_output =
            match.data() - output_view.data();
        new_output +=
            base::StrCat({std::string_view(output).substr(
                              last_match_start,
                              match_start_offset_in_output - last_match_start),
                          GetReplacementString(match)});
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

std::string Redactor::Rule::GetReplacementString(std::string_view match) const {
  if (!replacement_string_.empty()) {
    return replacement_string_;
  }
  std::string replacement(match.length() + 2, '#');
  replacement[0] = '[';
  replacement.back() = ']';
  return replacement;
}

bool Redactor::Rule::IsValidMatch(const std::string_view& match) const {
  if (match.empty()) {
    return false;
  }
  if (match.length() < min_pattern_length_) {
    return false;
  }
  if (max_pattern_length_ && match.length() > max_pattern_length_) {
    return false;
  }
  return true;
}

}  // namespace optimization_guide
