// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/save_update_address_profile_bubble_controller_impl.h"

#include "base/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class SaveUpdateAddressProfileBubbleControllerImplTest
    : public DialogBrowserTest {
 public:
  SaveUpdateAddressProfileBubbleControllerImplTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    DialogBrowserTest::SetUpCommandLine(command_line);
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    autofill::ChromeAutofillClient* autofill_client =
        autofill::ChromeAutofillClient::FromWebContents(web_contents);
    AutofillProfile profile = test::GetFullProfile();
    AutofillProfile* original_profile = (name == "Update") ? &profile : nullptr;
    autofill_client->ConfirmSaveAddressProfile(
        profile, original_profile,
        AutofillClient::SaveAddressProfilePromptOptions{.show_prompt = true},
        base::DoNothing());
    controller_ = SaveUpdateAddressProfileBubbleControllerImpl::FromWebContents(
        web_contents);
    DCHECK(controller_);
  }

  SaveUpdateAddressProfileBubbleControllerImpl* controller() {
    return controller_;
  }

 private:
  raw_ptr<SaveUpdateAddressProfileBubbleControllerImpl, DanglingUntriaged>
      controller_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(SaveUpdateAddressProfileBubbleControllerImplTest,
                       InvokeUi_Save) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(SaveUpdateAddressProfileBubbleControllerImplTest,
                       InvokeUi_Update) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(SaveUpdateAddressProfileBubbleControllerImplTest,
                       InvokeUi_SaveCloseThenReopen) {
  ShowAndVerifyUi();
  controller()->OnBubbleClosed();
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(SaveUpdateAddressProfileBubbleControllerImplTest,
                       CloseTabWhileBubbleIsOpen) {
  ShowAndVerifyUi();
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  tab->Close();
}

}  // namespace autofill
