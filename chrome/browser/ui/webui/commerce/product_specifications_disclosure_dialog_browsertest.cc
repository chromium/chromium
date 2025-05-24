// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/commerce/product_specifications_disclosure_dialog.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

namespace commerce {
// Tests ProductSpecificationsDisclosureDialogBrowser.
class ProductSpecificationsDisclosureDialogBrowserTest
    : public DialogBrowserTest {
 public:
  void SetUpOnMainThread() override {
    DialogBrowserTest::SetUpOnMainThread();
    profile_ = Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    DialogArgs args({}, "", "", true);
    ProductSpecificationsDisclosureDialog::ShowDialog(profile_, web_contents(),
                                                      std::move(args));
  }

 protected:
  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  raw_ptr<Profile, DanglingUntriaged> profile_;
};

IN_PROC_BROWSER_TEST_F(ProductSpecificationsDisclosureDialogBrowserTest,
                       InvokeUi_default) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ProductSpecificationsDisclosureDialogBrowserTest,
                       CheckDialogAttribute) {
  ASSERT_EQ(nullptr, commerce::ProductSpecificationsDisclosureDialog::
                         current_instance_for_testing());
  DialogArgs args({}, "", "", true);
  commerce::ProductSpecificationsDisclosureDialog::ShowDialog(
      profile_, web_contents(), std::move(args));
  auto* dialog = commerce::ProductSpecificationsDisclosureDialog::
      current_instance_for_testing();
  ASSERT_TRUE(dialog);
  ASSERT_EQ(ui::WebDialogDelegate::FrameKind::kDialog,
            dialog->GetWebDialogFrameKind());
  ASSERT_FALSE(dialog->ShouldShowCloseButton());
  ASSERT_FALSE(dialog->ShouldShowDialogTitle());
}
}  // namespace commerce
