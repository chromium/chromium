// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_tapjacking_protector_impl.h"

#include <string>

#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/autofill/autofill_dialog_controller.h"
#include "chrome/browser/ui/autofill/mock_autofill_dialog_controller.h"
#include "components/autofill/core/browser/ui/autofill_tapjacking_protector.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {
namespace {

using ::base::test::RunOnceCallback;
using ::base::test::TestFuture;
using ::testing::_;

class AutofillTapjackingProtectorImplTest : public testing::Test {
 public:
  AutofillTapjackingProtectorImplTest() = default;
  ~AutofillTapjackingProtectorImplTest() override = default;

  MockAutofillDialogController& dialog_controller() {
    return dialog_controller_;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  MockAutofillDialogController dialog_controller_;
};

TEST_F(AutofillTapjackingProtectorImplTest, Show_Payments_Accepted) {
  EXPECT_CALL(
      dialog_controller(),
      Show(
          l10n_util::GetStringUTF16(
              IDS_AUTOFILL_TAPJACKING_CREDIT_CARD_CONFIRMATION_DIALOG_TITLE),
          l10n_util::GetStringUTF16(
              IDS_AUTOFILL_TAPJACKING_CREDIT_CARD_CONFIRMATION_DIALOG_DESCRIPTION),
          l10n_util::GetStringUTF16(
              IDS_AUTOFILL_TAPJACKING_CONFIRMATION_DIALOG_POSITIVE_BUTTON),
          l10n_util::GetStringUTF16(
              IDS_AUTOFILL_TAPJACKING_CONFIRMATION_DIALOG_NEGATIVE_BUTTON),
          _))
      .WillOnce(
          RunOnceCallback<4>(AutofillDialogController::Result::kAccepted));

  AutofillTapjackingProtectorImpl protector(&dialog_controller());
  TestFuture<AutofillTapjackingProtector::AuthorizationResult> result_future;

  protector.Show(AutofillTapjackingProtector::AuthorizationType::kPayments,
                 result_future.GetCallback());

  EXPECT_EQ(result_future.Get(),
            AutofillTapjackingProtector::AuthorizationResult::kSuccess);
}

TEST_F(AutofillTapjackingProtectorImplTest, Show_Payments_Declined) {
  EXPECT_CALL(
      dialog_controller(),
      Show(
          l10n_util::GetStringUTF16(
              IDS_AUTOFILL_TAPJACKING_CREDIT_CARD_CONFIRMATION_DIALOG_TITLE),
          l10n_util::GetStringUTF16(
              IDS_AUTOFILL_TAPJACKING_CREDIT_CARD_CONFIRMATION_DIALOG_DESCRIPTION),
          l10n_util::GetStringUTF16(
              IDS_AUTOFILL_TAPJACKING_CONFIRMATION_DIALOG_POSITIVE_BUTTON),
          l10n_util::GetStringUTF16(
              IDS_AUTOFILL_TAPJACKING_CONFIRMATION_DIALOG_NEGATIVE_BUTTON),
          _))
      .WillOnce(
          RunOnceCallback<4>(AutofillDialogController::Result::kDeclined));

  AutofillTapjackingProtectorImpl protector(&dialog_controller());
  TestFuture<AutofillTapjackingProtector::AuthorizationResult> result_future;

  protector.Show(AutofillTapjackingProtector::AuthorizationType::kPayments,
                 result_future.GetCallback());

  EXPECT_EQ(result_future.Get(),
            AutofillTapjackingProtector::AuthorizationResult::kCancelled);
}

TEST_F(AutofillTapjackingProtectorImplTest, Show_Payments_Dismissed) {
  EXPECT_CALL(
      dialog_controller(),
      Show(
          l10n_util::GetStringUTF16(
              IDS_AUTOFILL_TAPJACKING_CREDIT_CARD_CONFIRMATION_DIALOG_TITLE),
          l10n_util::GetStringUTF16(
              IDS_AUTOFILL_TAPJACKING_CREDIT_CARD_CONFIRMATION_DIALOG_DESCRIPTION),
          l10n_util::GetStringUTF16(
              IDS_AUTOFILL_TAPJACKING_CONFIRMATION_DIALOG_POSITIVE_BUTTON),
          l10n_util::GetStringUTF16(
              IDS_AUTOFILL_TAPJACKING_CONFIRMATION_DIALOG_NEGATIVE_BUTTON),
          _))
      .WillOnce(RunOnceCallback<4>(AutofillDialogController::Result::kUnknown));

  AutofillTapjackingProtectorImpl protector(&dialog_controller());
  TestFuture<AutofillTapjackingProtector::AuthorizationResult> result_future;

  protector.Show(AutofillTapjackingProtector::AuthorizationType::kPayments,
                 result_future.GetCallback());

  EXPECT_EQ(result_future.Get(),
            AutofillTapjackingProtector::AuthorizationResult::kUnknown);
}

TEST_F(AutofillTapjackingProtectorImplTest, Show_NoDialogController) {
  AutofillTapjackingProtectorImpl protector(nullptr);
  TestFuture<AutofillTapjackingProtector::AuthorizationResult> result_future;

  protector.Show(AutofillTapjackingProtector::AuthorizationType::kPayments,
                 result_future.GetCallback());

  EXPECT_EQ(result_future.Get(),
            AutofillTapjackingProtector::AuthorizationResult::kUnknown);
}

}  // namespace
}  // namespace autofill
