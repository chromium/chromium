// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/payments/select_bnpl_issuer_dialog_controller_impl.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using base::UTF8ToUTF16;
using l10n_util::GetStringFUTF16;
using l10n_util::GetStringUTF16;
using std::u16string;
using testing::FieldsAre;
namespace autofill::payments {

namespace {
constexpr std::string_view kPaymentSettingsLinkText = "payment settings";
}  // namespace

class SelectBnplIssuerDialogControllerImplTest : public testing::Test {
 public:
  SelectBnplIssuerDialogControllerImplTest() = default;
  ~SelectBnplIssuerDialogControllerImplTest() override = default;

  void InitController() {
    controller_ = std::make_unique<SelectBnplIssuerDialogControllerImpl>(
        issuers_, selected_issuer_callback_.Get(), cancel_callback_.Get());
  }

  void SetIssuers(std::vector<BnplIssuer> issuers) {
    issuers_ = std::move(issuers);
  }

 protected:
  std::unique_ptr<SelectBnplIssuerDialogControllerImpl> controller_;
  std::vector<BnplIssuer> issuers_;
  base::MockOnceCallback<void(const std::string&)> selected_issuer_callback_;
  base::MockOnceClosure cancel_callback_;
};

TEST_F(SelectBnplIssuerDialogControllerImplTest, Getters) {
  SetIssuers({test::GetTestLinkedBnplIssuer()});
  InitController();
  EXPECT_EQ(controller_->GetIssuers(), issuers_);
  EXPECT_CALL(selected_issuer_callback_, Run(issuers_[0].issuer_id()));
  controller_->OnAccepted(issuers_[0].issuer_id());
  EXPECT_CALL(cancel_callback_, Run());
  controller_->OnCancel();
}

TEST_F(SelectBnplIssuerDialogControllerImplTest, GetTitle) {
  InitController();
  EXPECT_EQ(controller_->GetTitle(),
            GetStringUTF16(IDS_AUTOFILL_CARD_BNPL_SELECT_PROVIDER_TITLE));
}

TEST_F(SelectBnplIssuerDialogControllerImplTest, GetSelectionOptionText) {
  InitController();

  EXPECT_EQ(controller_->GetSelectionOptionText(kBnplZipIssuerId),
            GetStringUTF16(
                IDS_AUTOFILL_CARD_BNPL_SELECT_PROVIDER_PAYMENT_OPTION_ZIP));

  EXPECT_EQ(
      controller_->GetSelectionOptionText(kBnplAffirmIssuerId),
      GetStringUTF16(
          IDS_AUTOFILL_CARD_BNPL_SELECT_PROVIDER_PAYMENT_OPTION_AFFIRM_AND_AFTERPAY));

  EXPECT_EQ(
      controller_->GetSelectionOptionText(kBnplAfterpayIssuerId),
      GetStringUTF16(
          IDS_AUTOFILL_CARD_BNPL_SELECT_PROVIDER_PAYMENT_OPTION_AFFIRM_AND_AFTERPAY));
}

TEST_F(SelectBnplIssuerDialogControllerImplTest, GetLinkText) {
  InitController();
  size_t offset = 0;
  u16string text = GetStringFUTF16(
      IDS_AUTOFILL_CARD_BNPL_SELECT_PROVIDER_FOOTNOTE_HIDE_OPTION,
      UTF8ToUTF16(kPaymentSettingsLinkText), &offset);

  EXPECT_THAT(
      controller_->GetLinkText(),
      FieldsAre(text, gfx::Range(offset,
                                 offset + kPaymentSettingsLinkText.length())));
}

}  // namespace autofill::payments
