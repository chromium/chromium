// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_REDACTOR_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_REDACTOR_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "components/optimization_guide/proto/model_execution.pb.h"

namespace re2 {
class RE2;
}  // namespace re2

namespace optimization_guide {

// This structure matches the proto (RedactRule), see it for details.
struct Rule {
  Rule();
  Rule(const Rule& r);
  ~Rule();

  std::string regex;
  proto::RedactBehavior behavior;
  std::optional<std::string> replacement_string;
  std::optional<int> matching_group;
  std::optional<int> min_pattern_length;
  std::optional<int> max_pattern_length;
};

enum RedactResult {
  // Used if there was at least one rule that matched with a behavior of reject.
  kReject,

  // No rules with reject matched.
  kContinue,
};

// Used to redact (or reject) text.
class Redactor {
 public:
  explicit Redactor(const std::vector<Rule>& rules);
  ~Redactor();

  // Redacts (or rejects) the applicable text in`output`. `input` is the string
  // regexes of type REDACT_IF_ONLY_IN_OUTPUT checks.
  RedactResult Redact(const std::string& input, std::string& output) const;

 private:
  struct CachedRule {
    CachedRule();
    ~CachedRule();

    Rule rule;
    std::unique_ptr<re2::RE2> re;
  };

  static std::string GetReplacementString(const Rule& rule,
                                          std::string_view match);

  // Processes a single regex, applying any redactions to `output`.
  RedactResult ProcessRule(const CachedRule& cached_rule,
                           const std::string& input,
                           std::string& output) const;

  // Returns true if a match should be considered valid.
  static bool IsValidMatchForRule(const Rule& rule,
                                  const std::string_view& match);

  std::vector<std::unique_ptr<CachedRule>> rules_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_REDACTOR_H_
