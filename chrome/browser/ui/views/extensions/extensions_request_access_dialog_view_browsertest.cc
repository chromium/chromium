// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_request_access_dialog_view.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/toolbar/test_toolbar_action_view_controller.h"
#include "chrome/browser/ui/views/extensions/extensions_dialogs_browsertest.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension_features.h"

class ExtensionsRequestAccessDialogViewBrowserTest
    : public ExtensionsDialogBrowserTest {
 public:
  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    // Install extension so the extensions menu button is visible and can serve
    // as the dialog's anchor point.
    InstallExtension("Extension");
    views::View* const anchor_view =
        extensions_container()->GetExtensionsButton();
    EXPECT_TRUE(anchor_view->GetVisible());

    auto controller_A = std::make_unique<TestToolbarActionViewController>("A");
    std::vector<ToolbarActionViewController*> extensions_requesting_access;
    extensions_requesting_access.push_back(controller_A.get());

    ShowExtensionsRequestAccessDialogView(
        browser()->tab_strip_model()->GetActiveWebContents(), anchor_view,
        extensions_requesting_access);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      extensions_features::kExtensionsMenuAccessControl};
};

// TODO(crbug.com/1339738): Flaky on win-clang and win/win64 trunk builds. 
// ExtensionsRequestAccessDialog may not longer be used, wait to see if the class is
// deleted before fixing this.
IN_PROC_BROWSER_TEST_F(ExtensionsRequestAccessDialogViewBrowserTest, DISABLED_InvokeUi) {
  ShowAndVerifyUi();
}
