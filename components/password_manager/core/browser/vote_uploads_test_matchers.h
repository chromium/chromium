// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_VOTE_UPLOADS_TEST_MATCHERS_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_VOTE_UPLOADS_TEST_MATCHERS_H_

#include <optional>
#include <string>
#include <vector>

#include "components/autofill/core/browser/proto/server.pb.h"
#include "components/autofill/core/browser/test_utils/vote_uploads_test_matchers.h"
#include "components/autofill/core/common/signatures.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace upload_contents_matchers {

// Returns a matcher that checks whether the `AutofillUploadContents` has a
// `password_length` field that was set.
inline ::testing::Matcher<autofill::AutofillUploadContents>
HasPasswordLength() {
  return ::testing::Property(
      "has_password_length",
      &autofill::AutofillUploadContents::has_password_length, true);
}

inline ::testing::Matcher<autofill::AutofillUploadContents>
LoginFormSignatureIs(autofill::FormSignature signature) {
  return ::testing::Property(
      "login_form_signature",
      &autofill::AutofillUploadContents::login_form_signature, *signature);
}

inline ::testing::Matcher<autofill::AutofillUploadContents> PasswordsRevealedIs(
    bool revealed) {
  return ::testing::Property(
      "passwords_revealed",
      &autofill::AutofillUploadContents::passwords_revealed, revealed);
}

// Matchers for `AutofillUploadContents::Field`.
inline ::testing::Matcher<autofill::AutofillUploadContents::Field>
FieldGenerationTypeIs(
    autofill::AutofillUploadContents::Field::PasswordGenerationType type) {
  return ::testing::Property(
      "generation_type",
      &autofill::AutofillUploadContents::Field::generation_type, type);
}

inline ::testing::Matcher<autofill::AutofillUploadContents::Field>
FieldIsMostRecentSingleUsernameCandidateIs(std::optional<bool> value) {
  return ::testing::AllOf(
      ::testing::Property("has_is_most_recent_single_username_candidate",
                          &autofill::AutofillUploadContents::Field::
                              has_is_most_recent_single_username_candidate,
                          value.has_value()),
      ::testing::Property("is_most_recent_single_username_candidate",
                          &autofill::AutofillUploadContents::Field::
                              is_most_recent_single_username_candidate,
                          value.value_or(false)));
}

inline ::testing::Matcher<autofill::AutofillUploadContents::Field>
FieldSingleUsernameVoteTypeIs(
    autofill::AutofillUploadContents::Field::SingleUsernameVoteType vote_type) {
  return ::testing::Property(
      "single_username_vote_type",
      &autofill::AutofillUploadContents::Field::single_username_vote_type,
      vote_type);
}

inline ::testing::Matcher<autofill::AutofillUploadContents::Field>
FieldVoteTypeIs(autofill::AutofillUploadContents::Field::VoteType vote_type) {
  return ::testing::Property(
      "vote_type", &autofill::AutofillUploadContents::Field::vote_type,
      vote_type);
}

// Creates a matcher for the type of
// `std::vector<autofill::AutofillUploadContents>` that is expected from
// password manager calls to `AutofillCrowdsourceManager::StartUploadRequest`.
inline ::testing::Matcher<std::vector<autofill::AutofillUploadContents>>
IsPasswordUpload(auto... matchers) {
  return FirstElementIs(::testing::AllOf(
      autofill::upload_contents_matchers::AutofillUsedIs(false),
      autofill::upload_contents_matchers::ObservedSubmissionIs(true),
      matchers...));
}

}  // namespace upload_contents_matchers

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_VOTE_UPLOADS_TEST_MATCHERS_H_
