// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/edit_address_profile_dialog_controller_impl.h"

#include "base/functional/callback_helpers.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class EditAddressProfileDialogControllerImplTest : public DialogBrowserTest {
 public:
  EditAddressProfileDialogControllerImplTest() = default;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    EditAddressProfileDialogControllerImpl::CreateForWebContents(web_contents);
    EditAddressProfileDialogControllerImpl* controller =
        EditAddressProfileDialogControllerImpl::FromWebContents(web_contents);
    DCHECK(controller);
    controller->OfferEdit(
        test::GetFullProfile(), /*original_profile=*/nullptr,
        /*footer_message=*/u"",
        /*address_profile_save_prompt_callback=*/base::DoNothing(),
        /*is_migration=*/false);
  }
};

IN_PROC_BROWSER_TEST_F(EditAddressProfileDialogControllerImplTest,
                       InvokeUi_Edit) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(EditAddressProfileDialogControllerImplTest,
                       CloseTabWhileDialogIsOpenShouldNotCrash) {
  ShowUi("CloseBrowserWhileDialogIsOpenShouldNotCrash2");
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  tab->Close();
}

}  // namespace autofill
