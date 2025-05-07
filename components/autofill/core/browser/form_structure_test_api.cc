// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_structure_test_api.h"

#include "base/types/zip.h"
#include "components/autofill/core/browser/form_parsing/field_candidates.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

using ::testing::_;
using ::testing::Contains;
using ::testing::Each;
using ::testing::Pair;
using FieldPrediction =
    AutofillQueryResponse::FormSuggestion::FieldSuggestion::FieldPrediction;

void FormStructureTestApi::SetFieldTypes(
    const std::vector<std::vector<std::pair<HeuristicSource, FieldType>>>&
        heuristic_types,
    const std::vector<FieldPrediction>& server_types) {
  ASSERT_EQ(form_structure_->field_count(), heuristic_types.size());
  ASSERT_EQ(form_structure_->field_count(), server_types.size());
  ASSERT_THAT(heuristic_types,
              Each(Contains(Pair(GetActiveHeuristicSource(), _))))
      << "There must be a default heuristic prediction for every field.";

  for (auto [field, heuristic_type, server_type] :
       base::zip(form_structure_->fields(), heuristic_types, server_types)) {
    for (const auto& [source, type] : heuristic_type) {
      field->set_heuristic_type(source, type);
    }
    field->set_server_predictions({server_type});
  }
}

void FormStructureTestApi::SetFieldTypes(
    const std::vector<std::vector<std::pair<HeuristicSource, FieldType>>>&
        heuristic_types,
    const std::vector<FieldType>& server_types) {
  std::vector<FieldPrediction> server_predictions;
  for (FieldType type : server_types) {
    server_predictions.push_back(test::CreateFieldPrediction(type));
  }
  SetFieldTypes(heuristic_types, server_predictions);
}

void FormStructureTestApi::SetFieldTypes(
    const std::vector<FieldType>& heuristic_types,
    const std::vector<FieldType>& server_types) {
  std::vector<std::vector<std::pair<HeuristicSource, FieldType>>>
      all_heuristic_types;
  for (FieldType type : heuristic_types) {
    all_heuristic_types.push_back({{GetActiveHeuristicSource(), type}});
  }
  SetFieldTypes(all_heuristic_types, server_types);
}

}  // namespace autofill
