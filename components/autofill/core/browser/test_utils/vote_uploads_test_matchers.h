// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_UTILS_VOTE_UPLOADS_TEST_MATCHERS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_UTILS_VOTE_UPLOADS_TEST_MATCHERS_H_

#include <string>

#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "base/strings/string_number_conversions.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/common/signatures.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace autofill {

using ::testing::ContainerEq;
using ::testing::Contains;
using ::testing::Field;
using ::testing::Property;
using ::testing::ResultOf;

inline FormStructureTestApi test_api(const FormStructure* form) {
  return FormStructureTestApi(const_cast<FormStructure*>(form));
}

inline auto SignatureIsSameAs(const FormData& form) {
  return Property("form_signature", &FormStructure::form_signature,
                  CalculateFormSignature(form));
}

inline auto SignatureIs(FormSignature sig) {
  return Property("form_signature", &FormStructure::form_signature, sig);
}

inline auto SubmissionEventIsSameAs(mojom::SubmissionIndicatorEvent exp) {
  auto get_submission_event = [](const FormStructure& form) {
    return test_api(&form).get_submission_event();
  };
  return ResultOf("get_submission_event", get_submission_event, exp);
}

inline auto UploadedAutofillTypesAre(
    std::map<std::u16string, ServerFieldTypeSet> expected_types) {
  // Normalize the actual and expected sets by removing all UNKNOWN_TYPEs.
  auto get_possible_field_types = [](const FormStructure& actual) {
    std::map<std::u16string, ServerFieldTypeSet> type_map;
    for (const auto& field : actual) {
      ServerFieldTypeSet types = field->possible_types();
      types.erase(UNKNOWN_TYPE);
      if (!types.empty())
        type_map[field->name] = types;
    }
    return type_map;
  };
  for (auto& [field_name, types] : expected_types)
    types.erase(UNKNOWN_TYPE);
  base::EraseIf(expected_types, [](const auto& p) { return p.second.empty(); });
  return ResultOf("get_possible_field_types", get_possible_field_types,
                  ContainerEq(expected_types));
}

inline auto UploadedAutofillTypesAre(
    const std::map<std::u16string, ServerFieldType>& expected_types) {
  std::map<std::u16string, ServerFieldTypeSet> map;
  for (const auto& [field_name, type] : expected_types)
    map[field_name] = {type};
  return UploadedAutofillTypesAre(map);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_UTILS_VOTE_UPLOADS_TEST_MATCHERS_H_
