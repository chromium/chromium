// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/payments/select_bnpl_issuer_dialog_controller_impl.h"

#include <memory>

#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/data_model/payments/bnpl_issuer.h"
#include "components/autofill/core/browser/foundations/with_test_autofill_client_driver_manager.h"
#include "components/autofill/core/browser/payments/bnpl_util.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/ui/payments/select_bnpl_issuer_view.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using l10n_util::GetStringUTF16;
namespace autofill::payments {

namespace {

using IssuerId = autofill::BnplIssuer::IssuerId;
using ::testing::_;
using ::testing::ByMove;
using ::testing::Eq;
using ::testing::NotNull;
using ::testing::Return;

class MockSelectBnplIssuerView : public SelectBnplIssuerView {
 public:
  MOCK_METHOD(void, UpdateDialogWithIssuers, (), (override));
};

class SelectBnplIssuerDialogControllerImplTest
    : public testing::Test,
      public WithTestAutofillClientDriverManager<> {
 public:
  SelectBnplIssuerDialogControllerImplTest() = default;
  ~SelectBnplIssuerDialogControllerImplTest() override = default;

  void InitController() {
    InitAutofillClient();
    controller_ = std::make_unique<SelectBnplIssuerDialogControllerImpl>(
        &payments_autofill_client());
    controller_->ShowDialog(
        create_view_callback_.Get(), issuer_contexts_, /*app_locale=*/"en-US",
        selected_issuer_callback_.Get(), cancel_callback_.Get());
  }

  void SetIssuerContexts(std::vector<BnplIssuerContext> issuer_contexts) {
    issuer_contexts_ = std::move(issuer_contexts);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<SelectBnplIssuerDialogControllerImpl> controller_;
  std::vector<BnplIssuerContext> issuer_contexts_;
  base::MockCallback<
      base::OnceCallback<std::unique_ptr<SelectBnplIssuerView>()>>
      create_view_callback_;
  base::MockOnceCallback<void(BnplIssuer)> selected_issuer_callback_;
  base::MockOnceClosure cancel_callback_;
};

TEST_F(SelectBnplIssuerDialogControllerImplTest, Getters) {
  SetIssuerContexts(
      {BnplIssuerContext(test::GetTestLinkedBnplIssuer(),
                         BnplIssuerEligibilityForPage::kIsEligible)});
  InitController();
  EXPECT_EQ(controller_->GetIssuerContexts(), issuer_contexts_);
  EXPECT_CALL(selected_issuer_callback_, Run(issuer_contexts_[0].issuer));
  controller_->OnIssuerSelected(issuer_contexts_[0].issuer);
  EXPECT_CALL(cancel_callback_, Run());
  controller_->OnUserCancelled();
}

TEST_F(SelectBnplIssuerDialogControllerImplTest, GetTitle) {
  InitController();
  EXPECT_EQ(controller_->GetTitle(),
            GetStringUTF16(IDS_AUTOFILL_CARD_BNPL_SELECT_PROVIDER_TITLE));
}

TEST_F(SelectBnplIssuerDialogControllerImplTest, GetSelectionOptionText) {
  SetIssuerContexts(
      {BnplIssuerContext(test::GetTestLinkedBnplIssuer(),
                         BnplIssuerEligibilityForPage::kIsEligible)});
  InitController();

  EXPECT_FALSE(
      controller_->GetSelectionOptionText(BnplIssuer::IssuerId::kBnplAffirm)
          .empty());
}

TEST_F(SelectBnplIssuerDialogControllerImplTest,
       UpdateWithIssuers_CallsUpdateDialogWithIssuers) {
  SetIssuerContexts(
      {BnplIssuerContext(test::GetTestLinkedBnplIssuer(),
                         BnplIssuerEligibilityForPage::kIsEligible)});
  raw_ptr<MockSelectBnplIssuerView> mock_view = nullptr;
  EXPECT_CALL(create_view_callback_, Run).WillOnce([&mock_view]() {
    auto view = std::make_unique<MockSelectBnplIssuerView>();
    mock_view = view.get();
    return view;
  });
  InitController();

  ASSERT_THAT(mock_view, NotNull());

  std::vector<BnplIssuerContext> new_contexts = {
      BnplIssuerContext(test::GetTestLinkedBnplIssuer(),
                        BnplIssuerEligibilityForPage::kIsEligible),
      BnplIssuerContext(test::GetTestUnlinkedBnplIssuer(),
                        BnplIssuerEligibilityForPage::kIsEligible)};

  EXPECT_CALL(*mock_view, UpdateDialogWithIssuers);

  // Call `UpdateDialogWithIssuers` from the controller. This should trigger
  // `UpdateDialogWithIssuers` in `SelectBnplIssuerView`.
  controller_->UpdateDialogWithIssuers(new_contexts);

  // Verify that the controller's `issuer_contexts_` is updated.
  EXPECT_EQ(controller_->GetIssuerContexts(), new_contexts);
  // Verify that the new `selected_issuer_callback` is now stored.
  BnplIssuer selected_issuer = new_contexts[0].issuer;
  EXPECT_CALL(selected_issuer_callback_,
              Run(selected_issuer));  // .Times(1) is default
  controller_->OnIssuerSelected(selected_issuer);
}

// This test checks the `TextWithLink` returned from the `GetLinkText()` method.
// On Android, `GetLinkText()` does not return a `TextWithLink` so this test is
// not applicable.
#if !BUILDFLAG(IS_ANDROID)
TEST_F(SelectBnplIssuerDialogControllerImplTest, GetLinkText) {
  InitController();

  EXPECT_THAT(controller_->GetLinkText(),
              Field(&TextWithLink::text, Not(testing::IsEmpty())));
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace

}  // namespace autofill::payments
