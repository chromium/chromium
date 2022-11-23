// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_UTILS_VOTE_UPLOADS_TEST_MATCHERS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_UTILS_VOTE_UPLOADS_TEST_MATCHERS_H_

#include <string>

#include "base/strings/string_number_conversions.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/common/signatures.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace autofill {

// Matches a FormStructure if its signature is the same as that of the
// PasswordForm |form|.
MATCHER_P(SignatureIsSameAs,
          form,
          std::string(negation ? "signature isn't " : "signature is ") +
              autofill::FormStructure(form.form_data).FormSignatureAsStr()) {
  if (autofill::FormStructure(form.form_data).FormSignatureAsStr() ==
      arg.FormSignatureAsStr())
    return true;

  *result_listener << "signature is " << arg.FormSignatureAsStr() << " instead";
  return false;
}

MATCHER_P(SignatureIs,
          signature,
          std::string(negation ? "signature isn't " : "signature is ") +
              base::NumberToString(signature.value())) {
  if (signature == arg.form_signature())
    return true;

  *result_listener << "signature is " << arg.form_signature() << " instead";
  return false;
}

// The matcher argument is a FormStructure.
MATCHER_P(
    SubmissionEventIsSameAs,
    expected_submission_event,
    std::string(negation ? "submission event isn't " : "submission event is ") +
        base::NumberToString(static_cast<int>(expected_submission_event))) {
  autofill::FormStructureTestApi test_api(
      const_cast<autofill::FormStructure*>(&arg));
  if (expected_submission_event == test_api.get_submission_event())
    return true;

  *result_listener << "submission event is " << test_api.get_submission_event()
                   << " instead";
  return false;
}

MATCHER_P(UploadedAutofillTypesAre, expected_types, "") {
  size_t fields_matched_type_count = 0;
  bool conflict_found = false;
  for (const auto& field : arg) {
    fields_matched_type_count +=
        expected_types.find(field->name) == expected_types.end() ? 0 : 1;
    if (field->possible_types().size() > 1) {
      *result_listener << (conflict_found ? ", " : "") << "Field "
                       << field->name << ": has several possible types";
      conflict_found = true;
    }

    autofill::ServerFieldType expected_vote =
        expected_types.find(field->name) == expected_types.end()
            ? autofill::UNKNOWN_TYPE
            : expected_types.find(field->name)->second;
    autofill::ServerFieldType actual_vote =
        field->possible_types().empty() ? autofill::UNKNOWN_TYPE
                                        : *field->possible_types().begin();
    if (expected_vote != actual_vote) {
      *result_listener << (conflict_found ? ", " : "") << "Field "
                       << field->name << ": expected vote " << expected_vote
                       << " but found " << actual_vote;
      conflict_found = true;
    }
  }
  if (expected_types.size() != fields_matched_type_count) {
    *result_listener << (conflict_found ? ", " : "")
                     << "Some types were expected but not found in the vote";
    return false;
  }

  return !conflict_found;
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_UTILS_VOTE_UPLOADS_TEST_MATCHERS_H_
