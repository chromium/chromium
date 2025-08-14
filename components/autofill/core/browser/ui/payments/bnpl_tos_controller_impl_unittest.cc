// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/payments/bnpl_tos_controller_impl.h"

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/json/json_reader.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/data_model/payments/bnpl_issuer.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/ui/payments/bnpl_tos_view.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using base::MockCallback;
using base::MockOnceClosure;
using base::OnceCallback;
using base::UTF8ToUTF16;
using l10n_util::GetStringFUTF16;
using l10n_util::GetStringUTF16;
using std::u16string;
using testing::ByMove;
using testing::FieldsAre;
using testing::Return;
using testing::Test;

namespace autofill {

namespace {
constexpr std::string_view kWalletLinkText = "wallet.google.com";
constexpr std::string_view kWalletUrlString = "https://wallet.google.com/";
}  // namespace

class BnplTosControllerImplTest : public Test {
 public:
  void SetUp() override {
    test_autofill_client_ = std::make_unique<TestAutofillClient>();
    controller_ =
        std::make_unique<BnplTosControllerImpl>(test_autofill_client_.get());

    std::unique_ptr<BnplTosView> view = std::make_unique<BnplTosView>();
    view_ = view.get();
    ON_CALL(create_view_callback_, Run)
        .WillByDefault(Return(ByMove(std::move(view))));

    account_info_ =
        test_autofill_client_->identity_test_environment()
            .MakePrimaryAccountAvailable("somebody@example.test",
                                         signin::ConsentLevel::kSignin);
    static_cast<TestPaymentsDataManager&>(
        test_autofill_client_->GetPaymentsAutofillClient()
            ->GetPaymentsDataManager())
        .SetAccountInfoForPayments(account_info_);

    issuer_ = BnplIssuer(/*instrument_id=*/std::nullopt,
                         BnplIssuer::IssuerId::kBnplAffirm,
                         std::vector<BnplIssuer::EligiblePriceRange>{});
    LegalMessageLine::Parse(
        base::JSONReader::Read(
            "{ \"line\" : [ { \"template\": \"This is a legal message with"
            "{0}.\", \"template_parameter\": [ { \"display_text\": "
            "\"a link\", \"url\": \"http://www.example.com/\" "
            "} ] }] }")
            ->GetDict(),
        &legal_message_lines_, true);

    BnplTosModel model;
    model.issuer = issuer_;
    model.legal_message_lines = legal_message_lines_;

    EXPECT_CALL(create_view_callback_, Run());
    controller_->Show(create_view_callback_.Get(), std::move(model),
                      accept_callback_.Get(), cancel_callback_.Get());
    EXPECT_EQ(View(), view_);
  }

  void TearDown() override { view_ = nullptr; }

  BnplTosView* View() { return controller_->view_.get(); }

  u16string IssuerName() { return controller_->model_.issuer.GetDisplayName(); }

  base::test::TaskEnvironment task_environment_;
  MockCallback<OnceCallback<std::unique_ptr<BnplTosView>()>>
      create_view_callback_;
  AccountInfo account_info_;
  BnplIssuer issuer_;
  LegalMessageLines legal_message_lines_;
  MockOnceClosure accept_callback_;
  MockOnceClosure cancel_callback_;

  std::unique_ptr<TestAutofillClient> test_autofill_client_;
  std::unique_ptr<BnplTosControllerImpl> controller_;
  raw_ptr<BnplTosView> view_;
};

TEST_F(BnplTosControllerImplTest, ShowView_MultipleTimes) {
  // Show() was already called once during Setup().
  MockCallback<OnceCallback<std::unique_ptr<BnplTosView>()>>
      new_create_view_callback_;

  EXPECT_CALL(new_create_view_callback_, Run()).Times(0);
  controller_->Show(new_create_view_callback_.Get(), BnplTosModel(),
                    accept_callback_.Get(), cancel_callback_.Get());
}

TEST_F(BnplTosControllerImplTest, Dismiss) {
  // The view should start out as being shown.
  EXPECT_TRUE(View() != nullptr);
  // Avoid dangling pointer.
  view_ = nullptr;

  controller_->Dismiss();

  EXPECT_EQ(View(), nullptr);
}

TEST_F(BnplTosControllerImplTest, OnUserAccepted) {
  EXPECT_CALL(accept_callback_, Run());
  controller_->OnUserAccepted();

  // View should still be shown.
  EXPECT_TRUE(View() != nullptr);
}

TEST_F(BnplTosControllerImplTest, OnUserCancelled) {
  // Avoid dangling pointer.
  view_ = nullptr;

  EXPECT_CALL(cancel_callback_, Run());
  controller_->OnUserCancelled();

  // View should be dismissed.
  EXPECT_TRUE(View() == nullptr);
}

TEST_F(BnplTosControllerImplTest, GetOkButtonLabel) {
  EXPECT_EQ(controller_->GetOkButtonLabel(),
            GetStringUTF16(IDS_AUTOFILL_BNPL_TOS_OK_BUTTON_LABEL));
}

TEST_F(BnplTosControllerImplTest, GetCancelButtonLabel) {
  EXPECT_EQ(controller_->GetCancelButtonLabel(),
            GetStringUTF16(IDS_AUTOFILL_BNPL_TOS_CANCEL_BUTTON_LABEL));
}

TEST_F(BnplTosControllerImplTest, GetTitle) {
  EXPECT_EQ(controller_->GetTitle(),
            GetStringFUTF16(IDS_AUTOFILL_BNPL_TOS_TITLE, IssuerName()));
}

TEST_F(BnplTosControllerImplTest, GetReviewText) {
  EXPECT_EQ(controller_->GetReviewText(),
            GetStringFUTF16(IDS_AUTOFILL_BNPL_TOS_REVIEW_TEXT, IssuerName()));
}

TEST_F(BnplTosControllerImplTest, GetApproveText) {
  EXPECT_EQ(controller_->GetApproveText(),
            GetStringFUTF16(IDS_AUTOFILL_BNPL_TOS_APPROVE_TEXT, IssuerName()));
}

TEST_F(BnplTosControllerImplTest, GetLinkText) {
  std::vector<size_t> offsets;
  u16string text =
      GetStringFUTF16(IDS_AUTOFILL_BNPL_TOS_LINK_TEXT, IssuerName(),
                      UTF8ToUTF16(kWalletLinkText), &offsets);

  EXPECT_THAT(
      controller_->GetLinkText(),
      FieldsAre(text,
                gfx::Range(offsets[1], offsets[1] + kWalletLinkText.length()),
                GURL(kWalletUrlString)));
}

TEST_F(BnplTosControllerImplTest, GetLegalMessageLines) {
  EXPECT_EQ(controller_->GetLegalMessageLines(), legal_message_lines_);
}

TEST_F(BnplTosControllerImplTest, GetAccountInfo) {
  EXPECT_EQ(controller_->GetAccountInfo().email, account_info_.email);
}

TEST_F(BnplTosControllerImplTest, GetIssuerId) {
  EXPECT_EQ(controller_->GetIssuerId(), issuer_.issuer_id());
}

}  // namespace autofill
