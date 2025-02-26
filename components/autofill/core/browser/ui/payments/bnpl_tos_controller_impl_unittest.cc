// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/payments/bnpl_tos_controller_impl.h"

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "components/autofill/core/browser/ui/payments/bnpl_tos_view.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using base::MockCallback;
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
    controller_ = std::make_unique<BnplTosControllerImpl>();
    std::unique_ptr<BnplTosView> view = std::make_unique<BnplTosView>();
    view_ = view.get();
    ON_CALL(mock_callback_, Run).WillByDefault(Return(ByMove(std::move(view))));
  }

  void TearDown() override {
    // Avoid dangling pointer.
    view_ = nullptr;
  }

  void ShowView() { controller_->Show(mock_callback_.Get()); }

  BnplTosView* View() { return controller_->view_.get(); }

  u16string IssuerName() { return controller_->issuer_name_; }

  const LegalMessageLines& LegalMessageLines() {
    return controller_->legal_message_lines_;
  }

  std::unique_ptr<BnplTosControllerImpl> controller_;
  MockCallback<OnceCallback<std::unique_ptr<BnplTosView>()>> mock_callback_;
  raw_ptr<BnplTosView> view_;
};

TEST_F(BnplTosControllerImplTest, ShowView) {
  EXPECT_CALL(mock_callback_, Run());
  ShowView();
  EXPECT_EQ(View(), view_);
}

TEST_F(BnplTosControllerImplTest, ShowView_MultipleTimes) {
  EXPECT_CALL(mock_callback_, Run()).Times(1);
  ShowView();
  ShowView();
}

TEST_F(BnplTosControllerImplTest, OnClosed) {
  EXPECT_CALL(mock_callback_, Run());
  ShowView();

  // Avoid dangling pointer.
  view_ = nullptr;

  controller_->OnViewClosing(true);
  EXPECT_EQ(View(), nullptr);
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
  EXPECT_EQ(controller_->GetLegalMessageLines(), LegalMessageLines());
}

TEST_F(BnplTosControllerImplTest, GetAccountInfo) {
  // TODO: crbug.com/391141123 - Check both email and profile avatar when
  // real account info is implemented.
  EXPECT_EQ(controller_->GetAccountInfo().email, "somebody@example.test");
}

}  // namespace autofill
