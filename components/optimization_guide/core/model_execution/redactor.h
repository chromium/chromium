// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_REDACTOR_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_REDACTOR_H_

#include <memory>
#include <string>
#include <vector>

#include "components/optimization_guide/proto/redaction.pb.h"

namespace re2 {
class RE2;
}  // namespace re2

namespace optimization_guide {

enum class RedactResult {
  // Used if there was at least one rule that matched with a behavior of reject.
  kReject,

  // No rules with reject matched.
  kContinue,
};

// Used to redact (or reject) text.
class Redactor final {
 public:
  Redactor(Redactor&& rules);
  ~Redactor();

  // Construct a Redactor that implements the RedactRules.
  static Redactor FromProto(const proto::RedactRules& proto_rules);

  // Redacts (or rejects) the applicable text in`output`. `input` is the string
  // regexes of type REDACT_IF_ONLY_IN_OUTPUT checks.
  RedactResult Redact(const std::string& input, std::string& output) const;

 private:
  enum class Behavior {
    kReject = 1,
    kRedactIfOnlyInOutput = 2,
    kRedactAlways = 3,
  };
  // A validated and compiled proto::RedactRule, see it for details.
  class Rule final {
   public:
    Rule(std::unique_ptr<re2::RE2> re,
         Behavior behavior,
         std::string replacement_string,
         int matching_group,
         size_t min_pattern_length,
         size_t max_pattern_length);
    Rule(Rule&&);
    ~Rule();

    // Apply this rule, redacting `output`.
    RedactResult Process(const std::string& input, std::string& output) const;

   private:
    // Returns true if a match should be considered valid.
    bool IsValidMatch(const std::string_view& match) const;

    std::string GetReplacementString(std::string_view match) const;

    std::unique_ptr<re2::RE2> re_;
    Behavior behavior_;
    std::string replacement_string_;
    int matching_group_;
    size_t min_pattern_length_;
    size_t max_pattern_length_;
  };

  explicit Redactor(std::vector<Rule>&& rules);

  std::vector<Rule> rules_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_REDACTOR_H_
