// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_dialog_views.h"

#include <memory>
#include <string>

#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_environment.h"
#include "chrome/browser/ui/apps/app_info_dialog.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_test.h"

class AppInfoDialogBrowserTest : public DialogBrowserTest {
 public:
  AppInfoDialogBrowserTest() {}

  AppInfoDialogBrowserTest(const AppInfoDialogBrowserTest&) = delete;
  AppInfoDialogBrowserTest& operator=(const AppInfoDialogBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    extension_environment_ =
        std::make_unique<extensions::TestExtensionEnvironment>(
            extensions::TestExtensionEnvironment::Type::
                kInheritExistingTaskEnvironment);
    constexpr char kTestExtensionId[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    extension_ =
        extension_environment_->MakePackagedApp(kTestExtensionId, true);
  }
  void TearDownOnMainThread() override { extension_environment_ = nullptr; }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
    ShowAppInfoInNativeDialog(web_contents, extension_environment_->profile(),
                              extension_.get(), base::DoNothing());
  }

 protected:
  std::unique_ptr<extensions::TestExtensionEnvironment> extension_environment_;
  scoped_refptr<const extensions::Extension> extension_;
};

// Invokes a dialog that shows details of an installed extension.
// Flaky on ChromeOS. See https://crbug.com/1485666
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_InvokeUi_default DISABLED_InvokeUi_default
#else
#define MAYBE_InvokeUi_default InvokeUi_default
#endif
IN_PROC_BROWSER_TEST_F(AppInfoDialogBrowserTest, MAYBE_InvokeUi_default) {
  ShowAndVerifyUi();
}

// Flaky on ChromeOS. See https://crbug.com/1484928
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_CreateShortcutsAfterExtensionUnloaded \
  DISABLED_CreateShortcutsAfterExtensionUnloaded
#else
#define MAYBE_CreateShortcutsAfterExtensionUnloaded \
  CreateShortcutsAfterExtensionUnloaded
#endif
IN_PROC_BROWSER_TEST_F(AppInfoDialogBrowserTest,
                       MAYBE_CreateShortcutsAfterExtensionUnloaded) {
  ShowUi("");
  ASSERT_TRUE(AppInfoDialog::GetLastDialogForTesting());

  // Unload all extensions.
  extension_environment_->GetExtensionService()
      ->ProfileMarkedForPermanentDeletionForTest();

  // Dialog widgets and their root views are closed asynchronously so the dialog
  // is still alive.
  ASSERT_TRUE(AppInfoDialog::GetLastDialogForTesting());

  // Dialog is now closing.
  ASSERT_TRUE(
      AppInfoDialog::GetLastDialogForTesting()->GetWidget()->IsClosed());
}
