// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/save_address_profile_bubble_controller_impl.h"

#include "base/callback_helpers.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class SaveAddressProfileBubbleControllerImplTest : public DialogBrowserTest {
 public:
  SaveAddressProfileBubbleControllerImplTest() {
    feature_list_.InitAndEnableFeature(
        features::kAutofillAddressProfileSavePrompt);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    DialogBrowserTest::SetUpCommandLine(command_line);
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    autofill::ChromeAutofillClient* autofill_client =
        autofill::ChromeAutofillClient::FromWebContents(web_contents);
    autofill_client->ConfirmSaveAddressProfile(test::GetFullProfile(),
                                               base::DoNothing());
    controller_ =
        SaveAddressProfileBubbleControllerImpl::FromWebContents(web_contents);
    DCHECK(controller_);
  }

  SaveAddressProfileBubbleControllerImpl* controller() { return controller_; }

 private:
  SaveAddressProfileBubbleControllerImpl* controller_ = nullptr;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SaveAddressProfileBubbleControllerImplTest,
                       InvokeUi_Save) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(SaveAddressProfileBubbleControllerImplTest,
                       InvokeUi_SaveCloseThenReopen) {
  ShowAndVerifyUi();
  controller()->OnBubbleClosed();
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(SaveAddressProfileBubbleControllerImplTest,
                       CloseTabWhileBubbleIsOpen) {
  ShowAndVerifyUi();
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  tab->Close();
}

}  // namespace autofill
