// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_FORM_STRUCTURE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_FORM_STRUCTURE_H_

#include <vector>

#include "components/autofill/core/browser/form_parsing/field_candidates.h"
#include "components/autofill/core/browser/form_structure.h"

namespace autofill {

class TestFormStructure : public FormStructure {
 public:
  explicit TestFormStructure(const FormData& form);

  TestFormStructure(const TestFormStructure&) = delete;
  TestFormStructure& operator=(const TestFormStructure&) = delete;

  ~TestFormStructure() override;

  // Set the heuristic and server types for each field. The `heuristic_types`
  // and `server_types` vectors must be aligned with the indices of the fields
  // in the form.
  void SetFieldTypes(const std::vector<ServerFieldType>& heuristic_types,
                     const std::vector<ServerFieldType>& server_types);

  // Set the heuristic and server types for each field. The `heuristic_types`
  // and `server_types` vectors must be aligned with the indices of the fields
  // in the form. For each field in `heuristic_types` there must be exactly one
  // `GetActivePatternSource()` prediction and any number of alternative
  // predictions.
  void SetFieldTypes(
      const std::vector<std::vector<std::pair<PatternSource, ServerFieldType>>>&
          heuristic_types,
      const std::vector<ServerFieldType>& server_types);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_FORM_STRUCTURE_H_
