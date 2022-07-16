// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_FORM_STRUCTURE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_FORM_STRUCTURE_H_

#include <vector>

#include "components/autofill/core/browser/form_structure.h"

namespace autofill {

class TestFormStructure : public FormStructure {
 public:
  explicit TestFormStructure(const FormData& form);

  TestFormStructure(const TestFormStructure&) = delete;
  TestFormStructure& operator=(const TestFormStructure&) = delete;

  ~TestFormStructure() override;

  void SetFieldTypes(const std::vector<ServerFieldType>& heuristic_types,
                     const std::vector<ServerFieldType>& server_types);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_FORM_STRUCTURE_H_
