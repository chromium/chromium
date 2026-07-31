// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_FIELD_CANDIDATES_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_FIELD_CANDIDATES_H_

#include <array>
#include <optional>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/notreached.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/autofill_parsing_utils.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/autofill/core/common/is_required.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

// Represents the source heuristic parser.
enum class HeuristicParser {
  kSearch,
  kMerchantPromoCode,
  kName,
  kLoyaltyCard,
  kPrice,
  kIban,
  kOneTimeCode,
  kCreditCard,
  kAddress,
  kTravel,
  kPhone,
  kEmail
};

// Represents the priority of a field candidate classification.
// Priorities are compared lexicographically:
// 1. By match quality: A name match or high-quality label match takes
//    precedence over a low-quality label match.
// 2. By parser precedence: if the match quality is equal, the parser with the
//    higher precedence wins.
struct FieldCandidatePriority {
  FieldCandidatePriority(bool is_name_or_high_quality_label_match,
                         HeuristicParser parser_type);
  bool is_name_or_high_quality_label_match;
  size_t parser_priority;
  auto operator<=>(const FieldCandidatePriority&) const = default;
};

// Represents a possible type for a given field.
struct FieldCandidate {
  // The associated type for this candidate.
  FieldType type = internal::IsRequired();

  // Information on whether the `type` was derived based on name or high/low
  // quality label.
  MatchInfo match_info = internal::IsRequired();

  // A non-negative number indicating how sure the type is for this specific
  // candidate. The higher the more confidence.
  FieldCandidatePriority priority = internal::IsRequired();
};

// Each field can be of different types. This class collects all these possible
// types and determines which type is the most likely.
class FieldCandidates {
 public:
  FieldCandidates();
  FieldCandidates(FieldCandidates&& other);
  FieldCandidates& operator=(FieldCandidates&& other);
  ~FieldCandidates();

  // Includes a possible `type` for a given field.
  //
  // Callers are responsible for the scores they add. FieldCandidates is
  // agnostic to the source of these scores and will select the best candidate
  // based solely on their numeric values. `BestHeuristicCandidate()` uses
  // `priority` to determine the most likely type for this given field. Please
  // see field_candidates.cc for details on how this type is actually chosen.
  void AddFieldCandidate(FieldType type,
                         MatchInfo match_info,
                         FieldCandidatePriority priority);

  // Determines the best type based on `field_candidates_` and returns
  // the corresponding `FieldCandidate`. Returns `std::nullopt` when there are
  // no candidates.
  std::optional<FieldCandidate> BestHeuristicCandidate() const;

  // The MatchAttributes responsible for determining `BestHeuristicCandidate()`.
  DenseSet<MatchAttribute> BestHeuristicTypeReason() const;

 private:
  // Internal storage for all the possible types for a given field.
  std::vector<FieldCandidate> field_candidates_;
};

// A map from the field's global ID to its possible candidates.
using FieldCandidatesMap = base::flat_map<FieldGlobalId, FieldCandidates>;

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_FIELD_CANDIDATES_H_
