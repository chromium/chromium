// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_CONFIGS_STRING_SUBSTITUTION_BUILDER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_CONFIGS_STRING_SUBSTITUTION_BUILDER_H_

#include <string>

#include "components/optimization_guide/proto/substitution.pb.h"

namespace optimization_guide {

// Builder for constructing proto::StringSubstitution instances.
// This builder translates hierarchical conditional branching into a flat
// protobuf candidate list.
//
// Example:
//   StringSubstitutionBuilder()
//       .If(IsEnglish(), "Hello")
//       .If(IsSpanish(), "Hola")
//       .Else("Welcome")   // <-- Unconditional fallback is added last
//
// Nesting Example:
//   StringSubstitutionBuilder()
//       .If(AddBullets(), StringSubstitutionBuilder()
//                             .If(BeConcise(), "Add 3 bullet points.")
//                             .Else("Add 5 bullet points."))
class StringSubstitutionBuilder {
 public:
  StringSubstitutionBuilder();
  explicit StringSubstitutionBuilder(proto::StringArg arg);
  explicit StringSubstitutionBuilder(std::string raw_string);
  explicit StringSubstitutionBuilder(proto::StringSubstitution substitution);
  StringSubstitutionBuilder(const StringSubstitutionBuilder&);
  StringSubstitutionBuilder& operator=(const StringSubstitutionBuilder&);
  StringSubstitutionBuilder(StringSubstitutionBuilder&&);
  StringSubstitutionBuilder& operator=(StringSubstitutionBuilder&&);
  ~StringSubstitutionBuilder();

  bool empty() const { return substitution_.candidates().empty(); }

  // Appends a single candidate conditionally.
  StringSubstitutionBuilder& If(proto::Condition condition,
                                proto::StringArg arg);
  StringSubstitutionBuilder& If(proto::Condition condition,
                                std::string raw_string);

  // Prepends `condition` (as an AND clause) to every single candidate present
  // in `other`, effectively distributing the condition downwards before
  // appending.
  StringSubstitutionBuilder& If(proto::Condition condition,
                                StringSubstitutionBuilder other);
  StringSubstitutionBuilder& If(proto::Condition condition,
                                proto::StringSubstitution other);

  // Appends a single candidate unconditionally.
  StringSubstitutionBuilder& Else(proto::StringArg arg);
  StringSubstitutionBuilder& Else(std::string raw_string);

  // Appends the candidates from `other` directly to the end of the candidate
  // list.
  // NOTE: Internal conditions within `other` are preserved as-is. This enables
  // "chaining" or "falling back" to a distinct group of evaluations.
  StringSubstitutionBuilder& Else(const proto::StringSubstitution& other);
  StringSubstitutionBuilder& Else(const StringSubstitutionBuilder& other);

  // Prepends `condition` to the AND-evaluated condition list of every
  // candidate currently in this builder.
  // NOTE: Existing candidates must not have OR-evaluated conditions
  // (`CONDITION_EVALUATION_TYPE_OR`), since `proto::ConditionList` does not
  // support nested boolean expressions and flattening an OR list into an AND
  // list would change the logical evaluation.
  StringSubstitutionBuilder& PrependCondition(proto::Condition condition);

  // Returns the built substitution (copies lvalues, moves rvalues).
  proto::StringSubstitution Build() const&;
  proto::StringSubstitution Build() &&;

 private:
  proto::StringSubstitution substitution_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_CONFIGS_STRING_SUBSTITUTION_BUILDER_H_
