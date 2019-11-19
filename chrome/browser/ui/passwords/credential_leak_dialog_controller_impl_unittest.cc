// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/credential_leak_dialog_controller_impl.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/passwords/password_dialog_prompts.h"
#include "chrome/browser/ui/passwords/passwords_leak_dialog_delegate_mock.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using password_manager::CreateLeakType;
using password_manager::IsReused;
using password_manager::IsSaved;
using password_manager::IsSyncing;
using password_manager::metrics_util::LeakDialogDismissalReason;
using testing::StrictMock;

class MockCredentialLeakPrompt : public CredentialLeakPrompt {
 public:
  MockCredentialLeakPrompt() = default;

  MOCK_METHOD0(ShowCredentialLeakPrompt, void());
  MOCK_METHOD0(ControllerGone, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCredentialLeakPrompt);
};

class CredentialLeakDialogControllerTest : public testing::Test {
 public:
  void SetUpController(password_manager::CredentialLeakType leak_type) {
    controller_ = std::make_unique<CredentialLeakDialogControllerImpl>(
        &ui_controller_mock_, leak_type, GURL("https://example.com"));
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  PasswordsLeakDialogDelegateMock& ui_controller_mock() {
    return ui_controller_mock_;
  }

  MockCredentialLeakPrompt& leak_prompt() { return leak_prompt_; }

  CredentialLeakDialogControllerImpl& controller() { return *controller_; }

 private:
  base::HistogramTester histogram_tester_;
  StrictMock<PasswordsLeakDialogDelegateMock> ui_controller_mock_;
  StrictMock<MockCredentialLeakPrompt> leak_prompt_;
  std::unique_ptr<CredentialLeakDialogControllerImpl> controller_;
};

TEST_F(CredentialLeakDialogControllerTest, CredentialLeakDialogClose) {
  SetUpController(
      CreateLeakType(IsSaved(false), IsReused(false), IsSyncing(false)));

  EXPECT_CALL(leak_prompt(), ShowCredentialLeakPrompt());
  controller().ShowCredentialLeakPrompt(&leak_prompt());

  EXPECT_CALL(ui_controller_mock(), OnLeakDialogHidden());
  controller().OnCloseDialog();

  histogram_tester().ExpectUniqueSample(
      "PasswordManager.LeakDetection.DialogDismissalReason",
      LeakDialogDismissalReason::kNoDirectInteraction, 1);

  histogram_tester().ExpectUniqueSample(
      "PasswordManager.LeakDetection.DialogDismissalReason.Change",
      LeakDialogDismissalReason::kNoDirectInteraction, 1);

  EXPECT_CALL(leak_prompt(), ControllerGone());
}

TEST_F(CredentialLeakDialogControllerTest, CredentialLeakDialogOk) {
  SetUpController(
      CreateLeakType(IsSaved(true), IsReused(true), IsSyncing(false)));

  EXPECT_CALL(leak_prompt(), ShowCredentialLeakPrompt());
  controller().ShowCredentialLeakPrompt(&leak_prompt());

  EXPECT_CALL(ui_controller_mock(), OnLeakDialogHidden());
  controller().OnAcceptDialog();

  histogram_tester().ExpectUniqueSample(
      "PasswordManager.LeakDetection.DialogDismissalReason",
      LeakDialogDismissalReason::kClickedOk, 1);

  histogram_tester().ExpectUniqueSample(
      "PasswordManager.LeakDetection.DialogDismissalReason.Change",
      LeakDialogDismissalReason::kClickedOk, 1);

  EXPECT_CALL(leak_prompt(), ControllerGone());
}

TEST_F(CredentialLeakDialogControllerTest, CredentialLeakDialogCancel) {
  SetUpController(
      CreateLeakType(IsSaved(false), IsReused(true), IsSyncing(true)));

  EXPECT_CALL(leak_prompt(), ShowCredentialLeakPrompt());
  controller().ShowCredentialLeakPrompt(&leak_prompt());

  EXPECT_CALL(ui_controller_mock(), OnLeakDialogHidden());
  controller().OnCancelDialog();

  histogram_tester().ExpectUniqueSample(
      "PasswordManager.LeakDetection.DialogDismissalReason",
      LeakDialogDismissalReason::kClickedClose, 1);

  histogram_tester().ExpectUniqueSample(
      "PasswordManager.LeakDetection.DialogDismissalReason.CheckupAndChange",
      LeakDialogDismissalReason::kClickedClose, 1);

  EXPECT_CALL(leak_prompt(), ControllerGone());
}

TEST_F(CredentialLeakDialogControllerTest, CredentialLeakDialogCheckPasswords) {
  SetUpController(
      CreateLeakType(IsSaved(true), IsReused(true), IsSyncing(true)));

  EXPECT_CALL(leak_prompt(), ShowCredentialLeakPrompt());
  controller().ShowCredentialLeakPrompt(&leak_prompt());

  EXPECT_CALL(ui_controller_mock(), NavigateToPasswordCheckup());
  EXPECT_CALL(ui_controller_mock(), OnLeakDialogHidden());
  controller().OnAcceptDialog();

  histogram_tester().ExpectUniqueSample(
      "PasswordManager.LeakDetection.DialogDismissalReason",
      LeakDialogDismissalReason::kClickedCheckPasswords, 1);

  histogram_tester().ExpectUniqueSample(
      "PasswordManager.LeakDetection.DialogDismissalReason.Checkup",
      LeakDialogDismissalReason::kClickedCheckPasswords, 1);

  EXPECT_CALL(leak_prompt(), ControllerGone());
}

}  // namespace
