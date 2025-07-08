// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/undo_password_change_controller.h"

#include <optional>
#include <string>

#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {
const std::u16string kUsername = u"username";
const std::u16string kBackupPassword = u"backup_password";

autofill::PasswordAndMetadata GetPasswordAndMetadata(
    const std::u16string& username = kUsername,
    const std::optional<std::u16string>& backup_password = kBackupPassword) {
  autofill::PasswordAndMetadata credential;
  credential.username_value = username;
  credential.backup_password_value = backup_password;
  return credential;
}

autofill::Suggestion::PasswordSuggestionDetails GetSuggestionDetails(
    autofill::PasswordAndMetadata credential) {
  autofill::Suggestion::PasswordSuggestionDetails password_details;
  password_details.username = credential.username_value;
  password_details.signon_realm = credential.realm;
  return password_details;
}
}  // namespace

TEST(UndoPasswordChangeController, EmptyState) {
  UndoPasswordChangeController controller;

  EXPECT_EQ(controller.GetState(kUsername),
            PasswordRecoveryState::kRegularFlow);
}

TEST(UndoPasswordChangeController, OnSuggestionSelected) {
  UndoPasswordChangeController controller;
  const auto credential = GetPasswordAndMetadata();

  controller.OnSuggestionSelected(credential);

  EXPECT_EQ(controller.GetState(credential.username_value),
            PasswordRecoveryState::kTroubleSigningIn);
}

TEST(UndoPasswordChangeController,
     OnSuggestionSelectedNoBackupPasswordIgnored) {
  UndoPasswordChangeController controller;
  auto credential = GetPasswordAndMetadata(kUsername,
                                           /*backup_password=*/std::nullopt);

  controller.OnSuggestionSelected(credential);

  EXPECT_EQ(controller.GetState(credential.username_value),
            PasswordRecoveryState::kRegularFlow);
}

TEST(UndoPasswordChangeController, OnSuggestionSelectedNoBackupResetsFlow) {
  UndoPasswordChangeController controller;
  const auto credential_1 =
      GetPasswordAndMetadata(u"username_2", kBackupPassword);
  const auto credential_2 =
      GetPasswordAndMetadata(kUsername,
                             /*backup_password=*/std::nullopt);

  controller.OnSuggestionSelected(credential_1);
  controller.OnSuggestionSelected(credential_2);

  EXPECT_EQ(controller.GetState(credential_1.username_value),
            PasswordRecoveryState::kRegularFlow);
  EXPECT_EQ(controller.GetState(credential_2.username_value),
            PasswordRecoveryState::kRegularFlow);
}

TEST(UndoPasswordChangeController, OnSuggestionSelectedTwice) {
  UndoPasswordChangeController controller;
  auto credential = GetPasswordAndMetadata();

  controller.OnSuggestionSelected(credential);
  controller.OnSuggestionSelected(credential);

  EXPECT_EQ(controller.GetState(credential.username_value),
            PasswordRecoveryState::kTroubleSigningIn);
}

TEST(UndoPasswordChangeController, OnTroubleSigningIn) {
  UndoPasswordChangeController controller;
  const auto credential = GetPasswordAndMetadata();
  const auto password_details = GetSuggestionDetails(credential);

  controller.OnSuggestionSelected(credential);
  controller.OnTroubleSigningInClicked(password_details);

  EXPECT_EQ(controller.GetState(credential.username_value),
            PasswordRecoveryState::kIncludeBackup);
}

TEST(UndoPasswordChangeController, DifferentUsernameResetsFlow) {
  UndoPasswordChangeController controller;
  const auto credential_1 = GetPasswordAndMetadata();
  const auto password_details = GetSuggestionDetails(credential_1);
  const auto credential_2 =
      GetPasswordAndMetadata(u"username2", kBackupPassword);

  controller.OnSuggestionSelected(credential_1);
  controller.OnTroubleSigningInClicked(password_details);
  controller.OnSuggestionSelected(credential_2);

  EXPECT_EQ(controller.GetState(credential_1.username_value),
            PasswordRecoveryState::kRegularFlow);
  EXPECT_EQ(controller.GetState(credential_2.username_value),
            PasswordRecoveryState::kTroubleSigningIn);
}

TEST(UndoPasswordChangeController, CredentialWithIncludeBackupStateClicked) {
  UndoPasswordChangeController controller;
  const auto credential = GetPasswordAndMetadata();
  const auto password_details = GetSuggestionDetails(credential);

  controller.OnSuggestionSelected(credential);
  controller.OnTroubleSigningInClicked(password_details);
  controller.OnSuggestionSelected(credential);

  EXPECT_EQ(controller.GetState(credential.username_value),
            PasswordRecoveryState::kIncludeBackup);
}

TEST(UndoPasswordChangeController, FullFlowMultipleCredentials) {
  UndoPasswordChangeController controller;
  const auto credential = GetPasswordAndMetadata();
  const auto credential_2 = GetPasswordAndMetadata(u"username2");

  controller.OnSuggestionSelected(credential);
  controller.OnSuggestionSelected(credential_2);
  controller.OnTroubleSigningInClicked(GetSuggestionDetails(credential_2));

  EXPECT_EQ(controller.GetState(credential.username_value),
            PasswordRecoveryState::kRegularFlow);
  EXPECT_EQ(controller.GetState(credential_2.username_value),
            PasswordRecoveryState::kIncludeBackup);
}

}  // namespace password_manager
