// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/payments/select_bnpl_issuer_dialog_controller_impl.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "components/autofill/core/browser/data_model/payments/bnpl_issuer.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/ui/payments/select_bnpl_issuer_view.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using base::UTF8ToUTF16;
using l10n_util::GetStringFUTF16;
using l10n_util::GetStringUTF16;
using std::u16string;
using testing::FieldsAre;
namespace autofill::payments {

using IssuerId = autofill::BnplIssuer::IssuerId;

namespace {
constexpr std::string_view kPaymentSettingsLinkText = "payment settings";
}  // namespace

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

TEST_F(SelectBnplIssuerDialogControllerImplTest,
       GetSelectionOptionText_IsEligible) {
  SetIssuerContexts(
      {BnplIssuerContext(test::GetTestLinkedBnplIssuer(),
                         BnplIssuerEligibilityForPage::kIsEligible),
       BnplIssuerContext(test::GetTestLinkedBnplIssuer(IssuerId::kBnplZip),
                         BnplIssuerEligibilityForPage::kIsEligible),
       BnplIssuerContext(test::GetTestLinkedBnplIssuer(IssuerId::kBnplAfterpay),
                         BnplIssuerEligibilityForPage::kIsEligible)});
  InitController();

  EXPECT_EQ(controller_->GetSelectionOptionText(IssuerId::kBnplZip),
            GetStringUTF16(
                IDS_AUTOFILL_CARD_BNPL_SELECT_PROVIDER_PAYMENT_OPTION_ZIP));

  EXPECT_EQ(
      controller_->GetSelectionOptionText(IssuerId::kBnplAffirm),
      GetStringUTF16(
          IDS_AUTOFILL_CARD_BNPL_SELECT_PROVIDER_PAYMENT_OPTION_AFFIRM_AND_AFTERPAY));

  EXPECT_EQ(
      controller_->GetSelectionOptionText(IssuerId::kBnplAfterpay),
      GetStringUTF16(
          IDS_AUTOFILL_CARD_BNPL_SELECT_PROVIDER_PAYMENT_OPTION_AFFIRM_AND_AFTERPAY));
}

TEST_F(SelectBnplIssuerDialogControllerImplTest,
       GetSelectionOptionText_NotEligible) {
  SetIssuerContexts(
      {BnplIssuerContext(test::GetTestLinkedBnplIssuer(),
                         BnplIssuerEligibilityForPage::
                             kNotEligibleIssuerDoesNotSupportMerchant),
       BnplIssuerContext(
           test::GetTestLinkedBnplIssuer(IssuerId::kBnplZip),
           BnplIssuerEligibilityForPage::kNotEligibleCheckoutAmountTooLow),
       BnplIssuerContext(
           test::GetTestLinkedBnplIssuer(IssuerId::kBnplAfterpay),
           BnplIssuerEligibilityForPage::kNotEligibleCheckoutAmountTooHigh)});
  InitController();

  EXPECT_EQ(
      controller_->GetSelectionOptionText(IssuerId::kBnplAffirm),
      GetStringUTF16(
          IDS_AUTOFILL_CARD_BNPL_SELECT_PROVIDER_PAYMENT_OPTION_NOT_SUPPORTED_BY_MERCHANT));

  EXPECT_EQ(
      controller_->GetSelectionOptionText(IssuerId::kBnplZip),
      GetStringFUTF16(
          IDS_AUTOFILL_CARD_BNPL_SELECT_PROVIDER_PAYMENT_OPTION_CHECKOUT_AMOUNT_TOO_LOW,
          u"$50.00"));

  EXPECT_EQ(
      controller_->GetSelectionOptionText(IssuerId::kBnplAfterpay),
      GetStringFUTF16(
          IDS_AUTOFILL_CARD_BNPL_SELECT_PROVIDER_PAYMENT_OPTION_CHECKOUT_AMOUNT_TOO_HIGH,
          u"$200.00"));
}

TEST_F(SelectBnplIssuerDialogControllerImplTest,
       GetSelectionOptionText_NotEligible_LargeNumberFormatting) {
  BnplIssuer issuer = test::GetTestLinkedBnplIssuer();
  BnplIssuer::EligiblePriceRange price_range(
      /*currency=*/"USD", /*price_lower_bound=*/50'000'000,
      /*price_upper_bound=*/30'000'000'000);
  issuer.set_eligible_price_ranges({price_range});

  SetIssuerContexts({BnplIssuerContext(
      issuer,
      BnplIssuerEligibilityForPage::kNotEligibleCheckoutAmountTooHigh)});
  InitController();

  EXPECT_EQ(
      controller_->GetSelectionOptionText(IssuerId::kBnplAffirm),
      GetStringFUTF16(
          IDS_AUTOFILL_CARD_BNPL_SELECT_PROVIDER_PAYMENT_OPTION_CHECKOUT_AMOUNT_TOO_HIGH,
          u"$30,000.00"));
}

TEST_F(SelectBnplIssuerDialogControllerImplTest,
       GetSelectionOptionText_NotEligible_DecimalFormatting) {
  BnplIssuer issuer = test::GetTestLinkedBnplIssuer();
  BnplIssuer::EligiblePriceRange price_range(
      /*currency=*/"USD", /*price_lower_bound=*/49'491'234,
      /*price_upper_bound=*/30'000'000'000);
  issuer.set_eligible_price_ranges({price_range});

  SetIssuerContexts({BnplIssuerContext(
      issuer, BnplIssuerEligibilityForPage::kNotEligibleCheckoutAmountTooLow)});
  InitController();

  // Check that `$49.491234` truncates to `$49.49`.
  EXPECT_EQ(
      controller_->GetSelectionOptionText(IssuerId::kBnplAffirm),
      GetStringFUTF16(
          IDS_AUTOFILL_CARD_BNPL_SELECT_PROVIDER_PAYMENT_OPTION_CHECKOUT_AMOUNT_TOO_LOW,
          u"$49.49"));
}

TEST_F(SelectBnplIssuerDialogControllerImplTest,
       GetSelectionOptionText_NotEligible_DecimalRounding) {
  BnplIssuer issuer = test::GetTestLinkedBnplIssuer();
  BnplIssuer::EligiblePriceRange price_range(
      /*currency=*/"USD", /*price_lower_bound=*/99'999'999,
      /*price_upper_bound=*/30'000'000'000);
  issuer.set_eligible_price_ranges({price_range});

  SetIssuerContexts({BnplIssuerContext(
      issuer, BnplIssuerEligibilityForPage::kNotEligibleCheckoutAmountTooLow)});
  InitController();

  // Check that `$99.9999` rounds up to `$100.00`.
  EXPECT_EQ(
      controller_->GetSelectionOptionText(IssuerId::kBnplAffirm),
      GetStringFUTF16(
          IDS_AUTOFILL_CARD_BNPL_SELECT_PROVIDER_PAYMENT_OPTION_CHECKOUT_AMOUNT_TOO_LOW,
          u"$100.00"));
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
