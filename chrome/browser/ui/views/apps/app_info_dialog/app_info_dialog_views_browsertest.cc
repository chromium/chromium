// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/apps/app_info_dialog.h"

#include <memory>
#include <string>

#include "base/bind_helpers.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/test_extension_environment.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/test/base/testing_profile.h"

class AppInfoDialogBrowserTest : public DialogBrowserTest {
 public:
  AppInfoDialogBrowserTest() {}

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    extension_environment_ =
        std::make_unique<extensions::TestExtensionEnvironment>(
            extensions::TestExtensionEnvironment::Type::
                kInheritExistingTaskEnvironment);
    constexpr char kTestExtensionId[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    extension_ =
        extension_environment_->MakePackagedApp(kTestExtensionId, true);
    auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
    ShowAppInfoInNativeDialog(web_contents, extension_environment_->profile(),
                              extension_.get(), base::DoNothing());
  }

  void TearDownOnMainThread() override { extension_environment_ = nullptr; }

 private:
  std::unique_ptr<extensions::TestExtensionEnvironment> extension_environment_;
  scoped_refptr<const extensions::Extension> extension_;

  DISALLOW_COPY_AND_ASSIGN(AppInfoDialogBrowserTest);
};

// Invokes a dialog that shows details of an installed extension.
IN_PROC_BROWSER_TEST_F(AppInfoDialogBrowserTest, InvokeUi_default) {
  ShowAndVerifyUi();
}
