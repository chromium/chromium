// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/field_candidates.h"

#include <algorithm>
#include <vector>

#include "base/logging.h"

namespace autofill {

FieldCandidate::FieldCandidate(ServerFieldType field_type, float field_score)
    : type(field_type), score(field_score) {}

FieldCandidates::FieldCandidates() {}

FieldCandidates::FieldCandidates(const FieldCandidates& other) = default;

FieldCandidates::~FieldCandidates() {}

void FieldCandidates::AddFieldCandidate(ServerFieldType type, float score) {
  field_candidates_.emplace_back(type, score);
}

// We currently select the type with the biggest sum.
ServerFieldType FieldCandidates::BestHeuristicType() const {
  if (field_candidates_.empty())
    return UNKNOWN_TYPE;

  // Scores for each type. The index is their ServerFieldType enum value.
  std::vector<float> type_scores(MAX_VALID_FIELD_TYPE, 0.0f);

  for (const auto& field_candidate : field_candidates_) {
    VLOG(1) << "type: " << field_candidate.type
            << " score: " << field_candidate.score;
    type_scores[field_candidate.type] += field_candidate.score;
  }

  const auto best_type_iter =
      std::max_element(type_scores.begin(), type_scores.end());
  const size_t index = std::distance(type_scores.begin(), best_type_iter);

  return static_cast<ServerFieldType>(index);
}

}  // namespace autofill
