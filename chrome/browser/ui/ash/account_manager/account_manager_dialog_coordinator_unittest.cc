// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/account_manager/account_manager_dialog_coordinator.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/ash/account_manager/fake_account_manager_dialog.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/account_addition_options.h"
#include "components/account_manager_core/account_manager_metrics.h"
#include "components/account_manager_core/account_upsertion_result.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

const GaiaId::Literal kFakeGaiaId("fake-gaia-id");
const char kFakeEmail[] = "fake_email@example.com";
constexpr account_manager::AccountAdditionSource kTestAddAccountSource =
    account_manager::AccountAdditionSource::kSettingsAddAccountButton;
constexpr account_manager::AccountAdditionSource kTestReauthAccountSource =
    account_manager::AccountAdditionSource::kSettingsReauthAccountButton;

account_manager::Account FakeAccount() {
  return {account_manager::AccountKey::FromGaiaId(kFakeGaiaId), kFakeEmail};
}

void Increment(int* value) {
  ++*value;
}

class AccountManagerDialogCoordinatorTest : public testing::Test {
 public:
  AccountManagerDialogCoordinatorTest() = default;
  AccountManagerDialogCoordinatorTest(
      const AccountManagerDialogCoordinatorTest&) = delete;
  AccountManagerDialogCoordinatorTest& operator=(
      const AccountManagerDialogCoordinatorTest&) = delete;
  ~AccountManagerDialogCoordinatorTest() override = default;

 protected:
  void SetUp() override {
    reset_dialog_callbacks_ = coordinator_.InstallDialogCallbacksForTesting(
        base::BindRepeating(
            &test::FakeAccountManagerDialog::ShowAddAccountDialog,
            base::Unretained(&fake_account_manager_dialog_)),
        base::BindRepeating(
            &test::FakeAccountManagerDialog::ShowReauthAccountDialog,
            base::Unretained(&fake_account_manager_dialog_)),
        base::BindRepeating(&test::FakeAccountManagerDialog::IsDialogShown,
                            base::Unretained(&fake_account_manager_dialog_)));
  }

  void TearDown() override { reset_dialog_callbacks_.RunAndReset(); }

  test::FakeAccountManagerDialog& fake_account_manager_dialog() {
    return fake_account_manager_dialog_;
  }

  AccountManagerDialogCoordinator& coordinator() { return coordinator_; }

 private:
  AccountManagerDialogCoordinator coordinator_;
  test::FakeAccountManagerDialog fake_account_manager_dialog_;
  base::ScopedClosureRunner reset_dialog_callbacks_;
};

TEST_F(AccountManagerDialogCoordinatorTest,
       ShowAddAccountDialogOpensDialogAndPassesOptions) {
  account_manager::AccountAdditionOptions options;
  options.is_available_in_arc = true;
  options.show_arc_availability_picker = true;
  base::test::TestFuture<const account_manager::AccountUpsertionResult&> future;

  coordinator().ShowAddAccountDialog(kTestAddAccountSource, options,
                                     future.GetCallback());

  EXPECT_EQ(1,
            fake_account_manager_dialog().show_account_addition_dialog_calls());
  ASSERT_TRUE(fake_account_manager_dialog().last_add_account_options());
  EXPECT_TRUE(fake_account_manager_dialog()
                  .last_add_account_options()
                  ->is_available_in_arc);
  EXPECT_TRUE(fake_account_manager_dialog()
                  .last_add_account_options()
                  ->show_arc_availability_picker);

  fake_account_manager_dialog().CloseDialog();
  EXPECT_EQ(account_manager::AccountUpsertionResult::Status::kCancelledByUser,
            future.Take().status());
}

TEST_F(AccountManagerDialogCoordinatorTest,
       ShowReauthAccountDialogOpensDialogAndPassesEmail) {
  base::test::TestFuture<const account_manager::AccountUpsertionResult&> future;

  coordinator().ShowReauthAccountDialog(kTestReauthAccountSource, kFakeEmail,
                                        future.GetCallback());

  EXPECT_EQ(1, fake_account_manager_dialog()
                   .show_account_reauthentication_dialog_calls());
  ASSERT_TRUE(fake_account_manager_dialog().last_reauth_email());
  EXPECT_EQ(kFakeEmail, *fake_account_manager_dialog().last_reauth_email());

  fake_account_manager_dialog().CloseDialog();
  EXPECT_EQ(account_manager::AccountUpsertionResult::Status::kCancelledByUser,
            future.Take().status());
}

TEST_F(AccountManagerDialogCoordinatorTest,
       DuplicateAddAccountDialogReturnsAlreadyInProgress) {
  base::test::TestFuture<const account_manager::AccountUpsertionResult&> future;
  coordinator().ShowAddAccountDialog(kTestAddAccountSource,
                                     account_manager::AccountAdditionOptions{},
                                     future.GetCallback());

  base::test::TestFuture<const account_manager::AccountUpsertionResult&>
      duplicate_future;
  coordinator().ShowAddAccountDialog(kTestAddAccountSource,
                                     account_manager::AccountAdditionOptions{},
                                     duplicate_future.GetCallback());

  EXPECT_EQ(account_manager::AccountUpsertionResult::Status::kAlreadyInProgress,
            duplicate_future.Take().status());
  EXPECT_EQ(1,
            fake_account_manager_dialog().show_account_addition_dialog_calls());

  fake_account_manager_dialog().CloseDialog();
  EXPECT_EQ(account_manager::AccountUpsertionResult::Status::kCancelledByUser,
            future.Take().status());
}

TEST_F(AccountManagerDialogCoordinatorTest,
       DuplicateReauthAccountDialogReturnsAlreadyInProgress) {
  base::test::TestFuture<const account_manager::AccountUpsertionResult&> future;
  coordinator().ShowReauthAccountDialog(kTestReauthAccountSource, kFakeEmail,
                                        future.GetCallback());

  base::test::TestFuture<const account_manager::AccountUpsertionResult&>
      duplicate_future;
  coordinator().ShowReauthAccountDialog(kTestReauthAccountSource, kFakeEmail,
                                        duplicate_future.GetCallback());

  EXPECT_EQ(account_manager::AccountUpsertionResult::Status::kAlreadyInProgress,
            duplicate_future.Take().status());
  EXPECT_EQ(1, fake_account_manager_dialog()
                   .show_account_reauthentication_dialog_calls());

  fake_account_manager_dialog().CloseDialog();
  EXPECT_EQ(account_manager::AccountUpsertionResult::Status::kCancelledByUser,
            future.Take().status());
}

TEST_F(AccountManagerDialogCoordinatorTest,
       InlineLoginCompletionReturnsReportedResult) {
  base::test::TestFuture<const account_manager::AccountUpsertionResult&> future;
  coordinator().ShowAddAccountDialog(kTestAddAccountSource,
                                     account_manager::AccountAdditionOptions{},
                                     future.GetCallback());
  const account_manager::Account fake_account = FakeAccount();

  coordinator().CreateInlineLoginAccountUpsertionFinishedCallback().Run(
      account_manager::AccountUpsertionResult::FromAccount(fake_account));

  const account_manager::AccountUpsertionResult result = future.Take();
  EXPECT_EQ(account_manager::AccountUpsertionResult::Status::kSuccess,
            result.status());
  ASSERT_TRUE(result.account());
  EXPECT_EQ(fake_account.key, result.account()->key);
  EXPECT_EQ(fake_account.raw_email, result.account()->raw_email);

  fake_account_manager_dialog().CloseDialog();
}

TEST_F(AccountManagerDialogCoordinatorTest,
       AddAccountDialogRecordsSourceAndResultUMA) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<const account_manager::AccountUpsertionResult&> future;
  int dialog_flow_finished_calls = 0;
  coordinator().SetDialogFlowFinishedCallback(
      base::BindRepeating(&Increment, &dialog_flow_finished_calls));

  coordinator().ShowAddAccountDialog(
      account_manager::AccountAdditionSource::kArc,
      account_manager::AccountAdditionOptions{}, future.GetCallback());

  fake_account_manager_dialog().CloseDialog();
  EXPECT_EQ(account_manager::AccountUpsertionResult::Status::kCancelledByUser,
            future.Take().status());
  histogram_tester.ExpectUniqueSample(
      account_manager::kAccountAdditionSourceHistogramName,
      account_manager::AccountAdditionSource::kArc, 1);
  histogram_tester.ExpectUniqueSample(
      account_manager::kAccountUpsertionResultStatusHistogramName,
      account_manager::AccountUpsertionResult::Status::kCancelledByUser, 1);
  EXPECT_EQ(1, dialog_flow_finished_calls);
}

TEST_F(AccountManagerDialogCoordinatorTest,
       ReauthAccountDialogRecordsSourceAndResultUMA) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<const account_manager::AccountUpsertionResult&> future;
  int dialog_flow_finished_calls = 0;
  coordinator().SetDialogFlowFinishedCallback(
      base::BindRepeating(&Increment, &dialog_flow_finished_calls));

  coordinator().ShowReauthAccountDialog(
      account_manager::AccountAdditionSource::kChromeOSProjectorAppReauth,
      kFakeEmail, future.GetCallback());

  coordinator().CreateInlineLoginAccountUpsertionFinishedCallback().Run(
      account_manager::AccountUpsertionResult::FromAccount(FakeAccount()));
  EXPECT_EQ(account_manager::AccountUpsertionResult::Status::kSuccess,
            future.Take().status());
  fake_account_manager_dialog().CloseDialog();

  histogram_tester.ExpectUniqueSample(
      account_manager::kAccountAdditionSourceHistogramName,
      account_manager::AccountAdditionSource::kChromeOSProjectorAppReauth, 1);
  histogram_tester.ExpectUniqueSample(
      account_manager::kAccountUpsertionResultStatusHistogramName,
      account_manager::AccountUpsertionResult::Status::kSuccess, 1);
  EXPECT_EQ(1, dialog_flow_finished_calls);
}

TEST_F(AccountManagerDialogCoordinatorTest,
       SequentialDialogCallsWorkAfterFirstFlowFinishes) {
  base::test::TestFuture<const account_manager::AccountUpsertionResult&>
      first_future;
  coordinator().ShowAddAccountDialog(kTestAddAccountSource,
                                     account_manager::AccountAdditionOptions{},
                                     first_future.GetCallback());
  coordinator().CreateInlineLoginAccountUpsertionFinishedCallback().Run(
      account_manager::AccountUpsertionResult::FromAccount(FakeAccount()));
  EXPECT_EQ(account_manager::AccountUpsertionResult::Status::kSuccess,
            first_future.Take().status());
  fake_account_manager_dialog().CloseDialog();

  base::test::TestFuture<const account_manager::AccountUpsertionResult&>
      second_future;
  coordinator().ShowAddAccountDialog(kTestAddAccountSource,
                                     account_manager::AccountAdditionOptions{},
                                     second_future.GetCallback());
  fake_account_manager_dialog().CloseDialog();

  EXPECT_EQ(account_manager::AccountUpsertionResult::Status::kCancelledByUser,
            second_future.Take().status());
  EXPECT_EQ(2,
            fake_account_manager_dialog().show_account_addition_dialog_calls());
}

}  // namespace

}  // namespace ash
