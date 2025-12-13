// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/select_bnpl_issuer_dialog.h"

#include "chrome/browser/ui/autofill/payments/payments_view_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/core/browser/data_model/payments/bnpl_issuer.h"
#include "components/autofill/core/browser/payments/bnpl_util.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/ui/payments/select_bnpl_issuer_dialog_controller_impl.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "content/public/test/browser_test.h"

namespace autofill::payments {

class SelectBnplIssuerDialogBrowserTest
    : public DialogBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  SelectBnplIssuerDialogBrowserTest() = default;
  SelectBnplIssuerDialogBrowserTest(const SelectBnplIssuerDialogBrowserTest&) =
      delete;
  SelectBnplIssuerDialogBrowserTest& operator=(
      const SelectBnplIssuerDialogBrowserTest&) = delete;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    select_bnpl_issuer_dialog_controller_ =
        std::make_unique<SelectBnplIssuerDialogControllerImpl>(
            autofill_client_injector_[web_contents]
                ->GetPaymentsAutofillClient());
    const bool has_seen_ai_terms = GetParam();
    select_bnpl_issuer_dialog_controller_->ShowDialog(
        base::BindOnce(&CreateAndShowBnplIssuerSelectionDialog,
                       select_bnpl_issuer_dialog_controller_->GetWeakPtr(),
                       base::Unretained(web_contents), has_seen_ai_terms),
        std::move(issuer_contexts_),
        /*app_locale=*/"en-US", base::DoNothing(), base::DoNothing());
  }

  bool VerifyUi() override {
    // TODO(crbug.com/363332740): Verify issuers list and other UI elements once
    // implemented.
    return true;
  }

  void DismissUi() override { select_bnpl_issuer_dialog_controller_.reset(); }

  void SetIssuerContexts(std::vector<BnplIssuerContext> issuer_contexts) {
    issuer_contexts_ = std::move(issuer_contexts);
  }

  SelectBnplIssuerDialogControllerImpl* controller() {
    return select_bnpl_issuer_dialog_controller_.get();
  }

 protected:
  base::test::ScopedFeatureList feature_list_{
      features::kAutofillEnableAiBasedAmountExtraction};
  std::vector<BnplIssuerContext> issuer_contexts_;
  std::unique_ptr<SelectBnplIssuerDialogControllerImpl>
      select_bnpl_issuer_dialog_controller_;
  TestAutofillClientInjector<TestContentAutofillClient>
      autofill_client_injector_;
};

IN_PROC_BROWSER_TEST_P(SelectBnplIssuerDialogBrowserTest,
                       UiShown_IssuersEligibile) {
  SetIssuerContexts(
      {BnplIssuerContext(test::GetTestLinkedBnplIssuer(),
                         BnplIssuerEligibilityForPage::kIsEligible),
       BnplIssuerContext(test::GetTestUnlinkedBnplIssuer(),
                         BnplIssuerEligibilityForPage::kIsEligible),
       BnplIssuerContext(
           test::GetTestLinkedBnplIssuer(BnplIssuer::IssuerId::kBnplAfterpay),
           BnplIssuerEligibilityForPage::kIsEligible)});
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(SelectBnplIssuerDialogBrowserTest,
                       UiShown_IssuersNotEligibile) {
  SetIssuerContexts(
      {BnplIssuerContext(test::GetTestLinkedBnplIssuer(),
                         BnplIssuerEligibilityForPage::
                             kNotEligibleIssuerDoesNotSupportMerchant),
       BnplIssuerContext(
           test::GetTestLinkedBnplIssuer(BnplIssuer::IssuerId::kBnplZip),
           BnplIssuerEligibilityForPage::kNotEligibleCheckoutAmountTooLow),
       BnplIssuerContext(
           test::GetTestLinkedBnplIssuer(BnplIssuer::IssuerId::kBnplAfterpay),
           BnplIssuerEligibilityForPage::kNotEligibleCheckoutAmountTooHigh)});
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(All,
                         SelectBnplIssuerDialogBrowserTest,
                         ::testing::Bool());

}  // namespace autofill::payments
