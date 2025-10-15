// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/payments/select_bnpl_issuer_dialog_controller_impl.h"

#include "base/test/mock_callback.h"
#include "components/autofill/core/browser/data_model/payments/bnpl_issuer.h"
#include "components/autofill/core/browser/payments/bnpl_util.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/ui/payments/select_bnpl_issuer_view.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using l10n_util::GetStringUTF16;
namespace autofill::payments {

using IssuerId = autofill::BnplIssuer::IssuerId;

class SelectBnplIssuerDialogControllerImplTest : public testing::Test {
 public:
  SelectBnplIssuerDialogControllerImplTest() = default;
  ~SelectBnplIssuerDialogControllerImplTest() override = default;

  void InitController() {
    controller_ = std::make_unique<SelectBnplIssuerDialogControllerImpl>();
    controller_->ShowDialog(
        create_view_callback_.Get(), issuer_contexts_, /*app_locale=*/"en-US",
        selected_issuer_callback_.Get(), cancel_callback_.Get());
  }

  void SetIssuerContexts(std::vector<BnplIssuerContext> issuer_contexts) {
    issuer_contexts_ = std::move(issuer_contexts);
  }

 protected:
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

// This test checks the `TextWithLink` returned from the `GetLinkText()` method.
// On Android, `GetLinkText()` does not return a `TextWithLink` so this test is
// not applicable.
#if !BUILDFLAG(IS_ANDROID)
TEST_F(SelectBnplIssuerDialogControllerImplTest, GetLinkText) {
  InitController();

  EXPECT_FALSE(controller_->GetLinkText().text.empty());
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace autofill::payments
