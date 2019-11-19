// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_FIELD_CANDIDATES_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_FIELD_CANDIDATES_H_

#include <unordered_map>
#include <vector>

#include "components/autofill/core/browser/field_types.h"

namespace autofill {

// Represents a possible type for a given field.
struct FieldCandidate {
  FieldCandidate(ServerFieldType field_type, float field_score);

  // The associated type for this candidate.
  ServerFieldType type = UNKNOWN_TYPE;

  // A non-negative number indicating how sure the type is for this specific
  // candidate. The higher the more confidence.
  float score = 0.0f;
};

// Each field can be of different types. This class collects all these possible
// types and determines which type is the most likely.
class FieldCandidates {
 public:
  FieldCandidates();
  FieldCandidates(const FieldCandidates& other);
  ~FieldCandidates();

  // Includes a possible |type| for a given field.
  //
  // Callers are responsible for the scores they add. FieldCandidates is
  // agnostic to the source of these scores and will select the best candidate
  // based solely on their numeric values. BestHeuristicType() uses |score| to
  // determine the most likely type for this given field. Please see
  // field_candidates.cc for details on how this type is actually chosen.
  void AddFieldCandidate(ServerFieldType type, float score);

  // Determines the best type based on the current possible types.
  ServerFieldType BestHeuristicType() const;

 private:
  // Internal storage for all the possible types for a given field.
  std::vector<FieldCandidate> field_candidates_;
};

// A map from the field's unique name to its possible candidates.
using FieldCandidatesMap = std::unordered_map<base::string16, FieldCandidates>;

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_FIELD_CANDIDATES_H_
