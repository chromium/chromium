// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/bnpl_tos_view_desktop.h"

#include "base/json/json_reader.h"
#include "chrome/browser/ui/autofill/payments/payments_view_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/autofill/payments/bnpl_tos_dialog.h"
#include "components/autofill/core/browser/data_model/payments/bnpl_issuer.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/browser/ui/payments/bnpl_tos_controller_impl.h"
#include "content/public/test/browser_test.h"

namespace autofill {

class BnplTosViewDesktopBrowserTest : public DialogBrowserTest {
 public:
  BnplTosViewDesktopBrowserTest() = default;
  ~BnplTosViewDesktopBrowserTest() override = default;
  BnplTosViewDesktopBrowserTest(const BnplTosViewDesktopBrowserTest&) = delete;
  BnplTosViewDesktopBrowserTest& operator=(
      const BnplTosViewDesktopBrowserTest&) = delete;

  void ShowUi(const std::string& name) override {
    BnplTosModel model;
    model.issuer = BnplIssuer(
        /*instrument_id=*/std::nullopt, BnplIssuer::IssuerId::kBnplAffirm,
        std::vector<BnplIssuer::EligiblePriceRange>{});
    LegalMessageLine::Parse(
        base::JSONReader::Read(
            "{ \"line\" : [ { \"template\": \"This is a legal message "
            "with"
            "{0}.\", \"template_parameter\": [ { \"display_text\": "
            "\"a link\", \"url\": \"http://www.example.com/\" "
            "} ] }] }",
            base::JSON_PARSE_CHROMIUM_EXTENSIONS)
            ->GetDict(),
        &model.legal_message_lines, true);

    test_autofill_client_ = std::make_unique<TestAutofillClient>();
    static_cast<TestPaymentsDataManager&>(
        test_autofill_client_->GetPaymentsAutofillClient()
            ->GetPaymentsDataManager())
        .SetAccountInfoForPayments(
            test_autofill_client_->identity_test_environment()
                .MakePrimaryAccountAvailable("somebody@example.test",
                                             signin::ConsentLevel::kSignin));
    controller_ =
        std::make_unique<BnplTosControllerImpl>(test_autofill_client_.get());
    controller_->Show(
        base::BindOnce(&CreateAndShowBnplTos, controller_->GetWeakPtr(),
                       base::Unretained(web_contents())),
        std::move(model), base::DoNothing(), base::DoNothing());
  }

  void DismissUi() override {
    controller_.reset();
    test_autofill_client_.reset();
  }

  content::WebContents* web_contents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  std::unique_ptr<TestAutofillClient> test_autofill_client_;
  std::unique_ptr<BnplTosControllerImpl> controller_;
};

// Ensures that when the BNPL ToS UI is shown, it won't crash the browser.
IN_PROC_BROWSER_TEST_F(BnplTosViewDesktopBrowserTest, ShowAndVerifyUi) {
  ShowAndVerifyUi();
}

}  // namespace autofill
