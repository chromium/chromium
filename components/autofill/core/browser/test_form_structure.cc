// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/test_form_structure.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

TestFormStructure::TestFormStructure(const FormData& form)
    : FormStructure(form) {}

TestFormStructure::~TestFormStructure() {}

void TestFormStructure::SetFieldTypes(
    const std::vector<ServerFieldType>& heuristic_types,
    const std::vector<ServerFieldType>& server_types) {
  ASSERT_EQ(field_count(), heuristic_types.size());
  ASSERT_EQ(field_count(), server_types.size());

  for (size_t i = 0; i < field_count(); ++i) {
    AutofillField* form_field = field(i);
    ASSERT_TRUE(form_field);
    form_field->set_heuristic_type(heuristic_types[i]);
    AutofillQueryResponse::FormSuggestion::FieldSuggestion::FieldPrediction
        prediction;
    prediction.set_type(server_types[i]);
    form_field->set_server_predictions({prediction});
  }

  UpdateAutofillCount();
}

}  // namespace autofill
