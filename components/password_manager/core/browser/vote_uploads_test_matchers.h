// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_VOTE_UPLOADS_TEST_MATCHERS_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_VOTE_UPLOADS_TEST_MATCHERS_H_

#include <string>

#include "components/autofill/core/browser/form_structure.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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
              base::NumberToString(signature)) {
  if (signature == arg.form_signature())
    return true;

  *result_listener << "signature is " << arg.form_signature() << " instead";
  return false;
}

MATCHER_P(SubmissionEventIsSameAs,
          expected_submission_event,
          std::string(negation ? "submission event isn't "
                               : "submission event is ") +
              std::to_string(static_cast<int>(expected_submission_event))) {
  if (expected_submission_event == arg.get_submission_event_for_testing())
    return true;

  *result_listener << "submission event is "
                   << arg.get_submission_event_for_testing() << " instead";
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

MATCHER_P(HasGenerationVote, expect_generation_vote, "") {
  bool found_generation_vote = false;
  for (const auto& field : arg) {
    if (field->generation_type() !=
        autofill::AutofillUploadContents::Field::NO_GENERATION) {
      found_generation_vote = true;
      break;
    }
  }
  return found_generation_vote == expect_generation_vote;
}

// Matches if all fields with a vote type are described in |expected_vote_types|
// and all votes from |expected_vote_types| are found in a field.
MATCHER_P(VoteTypesAre, expected_vote_types, "") {
  size_t matched_count = 0;
  bool conflict_found = false;
  for (const auto& field : arg) {
    auto expectation = expected_vote_types.find(field->name);
    if (expectation == expected_vote_types.end()) {
      if (field->vote_type() !=
          autofill::AutofillUploadContents::Field::NO_INFORMATION) {
        *result_listener << (conflict_found ? ", " : "") << "field "
                         << field->name << ": unexpected vote type "
                         << field->vote_type();
        conflict_found = true;
      }
      continue;
    }

    matched_count++;
    if (expectation->second != field->vote_type()) {
      *result_listener << (conflict_found ? ", " : "") << "field "
                       << field->name << ": expected vote type "
                       << expectation->second << " but has "
                       << field->vote_type();
      conflict_found = true;
    }
  }
  if (expected_vote_types.size() != matched_count) {
    *result_listener
        << (conflict_found ? ", " : "")
        << "some vote types were expected but not found in the vote";
    conflict_found = true;
  }

  return !conflict_found;
}

MATCHER_P2(UploadedGenerationTypesAre,
           expected_generation_types,
           generated_password_changed,
           "") {
  for (const auto& field : arg) {
    if (expected_generation_types.find(field->name) ==
        expected_generation_types.end()) {
      if (field->generation_type() !=
          autofill::AutofillUploadContents::Field::NO_GENERATION) {
        // Unexpected generation type.
        *result_listener << "Expected no generation type for the field "
                         << field->name << ", but found "
                         << field->generation_type();
        return false;
      }
    } else {
      if (expected_generation_types.find(field->name)->second !=
          field->generation_type()) {
        // Wrong generation type.
        *result_listener << "Expected generation type for the field "
                         << field->name << " is "
                         << expected_generation_types.find(field->name)->second
                         << ", but found " << field->generation_type();
        return false;
      }

      if (field->generation_type() !=
          autofill::AutofillUploadContents::Field::IGNORED_GENERATION_POPUP) {
        if (generated_password_changed != field->generated_password_changed())
          return false;
      }
    }
  }
  return true;
}

MATCHER_P(PasswordsWereRevealed, passwords_were_revealed, "") {
  return passwords_were_revealed == arg.passwords_were_revealed();
}

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_VOTE_UPLOADS_TEST_MATCHERS_H_
