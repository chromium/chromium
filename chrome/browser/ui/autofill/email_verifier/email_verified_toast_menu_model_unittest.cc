// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/email_verifier/email_verified_toast_menu_model.h"

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/mock_callback.h"
#include "chrome/grit/generated_resources.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {
namespace {

// Verifies the menu structure (single 'Manage' item with correct label).
TEST(EmailVerifiedToastMenuModelTest, MenuStructure) {
  EmailVerifiedToastMenuModel model(base::DoNothing());

  ASSERT_EQ(model.GetItemCount(), 1u);
  EXPECT_EQ(model.GetCommandIdAt(0),
            EmailVerifiedToastMenuModel::CommandId::kManage);
  EXPECT_EQ(model.GetLabelAt(0), l10n_util::GetStringUTF16(IDS_MANAGE));
}

// Verifies that executing the command records the user action and invokes the
// settings callback.
TEST(EmailVerifiedToastMenuModelTest,
     ExecuteCommandRecordsActionAndInvokesCallback) {
  base::MockCallback<base::RepeatingClosure> open_settings_callback;
  EXPECT_CALL(open_settings_callback, Run);
  EmailVerifiedToastMenuModel model(open_settings_callback.Get());
  base::UserActionTester user_action_tester;

  EXPECT_EQ(
      user_action_tester.GetActionCount("Toast.EmailVerified.ManageClicked"),
      0);

  model.ExecuteCommand(EmailVerifiedToastMenuModel::CommandId::kManage,
                       /*event_flags=*/0);

  EXPECT_EQ(
      user_action_tester.GetActionCount("Toast.EmailVerified.ManageClicked"),
      1);
}

// Verifies that invalid command IDs are ignored and do not record metrics or
// invoke the settings callback.
TEST(EmailVerifiedToastMenuModelTest, UnknownCommandIgnored) {
  base::MockCallback<base::RepeatingClosure> open_settings_callback;
  EXPECT_CALL(open_settings_callback, Run).Times(0);
  EmailVerifiedToastMenuModel model(open_settings_callback.Get());
  base::UserActionTester user_action_tester;

  model.ExecuteCommand(/*command_id=*/999, /*event_flags=*/0);

  EXPECT_EQ(
      user_action_tester.GetActionCount("Toast.EmailVerified.ManageClicked"),
      0);
}

}  // namespace
}  // namespace autofill
