// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/webid/fedcm_modal_dialog_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"

class FedCmModalDialogViewBrowserTest : public DialogBrowserTest {
 public:
  FedCmModalDialogViewBrowserTest() = default;
  ~FedCmModalDialogViewBrowserTest() override = default;

  void ShowUi(const std::string& name) override {
    FedCmModalDialogView::ShowFedCmModalDialog(
        browser()->tab_strip_model()->GetActiveWebContents(),
        GURL(u"https://example.com"), /*observer=*/nullptr);
  }

 private:
  base::WeakPtrFactory<FedCmModalDialogViewBrowserTest> weak_factory_{this};
};

IN_PROC_BROWSER_TEST_F(FedCmModalDialogViewBrowserTest, InvokeUi_default) {
  ShowAndVerifyUi();
}
