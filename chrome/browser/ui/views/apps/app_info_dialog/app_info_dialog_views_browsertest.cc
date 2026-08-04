// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_dialog_views.h"

#include <memory>
#include <string>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/test_extension_environment.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_dialog_container.h"
#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_header_panel.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/unloaded_extension_reason.h"
#include "extensions/common/extension_urls.h"
#include "url/gurl.h"

// Placed in `namespace test` to match the friend class declaration in
// `app_info_header_panel.h` for accessing private `ShowAppInWebStore()`.
namespace test {

class AppInfoDialogTestApi {
 public:
  explicit AppInfoDialogTestApi(AppInfoDialog* dialog) : dialog_(dialog) {}

  AppInfoDialogTestApi(const AppInfoDialogTestApi&) = delete;
  AppInfoDialogTestApi& operator=(const AppInfoDialogTestApi&) = delete;

  void ShowAppInWebStore() {
    auto* header_panel =
        static_cast<AppInfoHeaderPanel*>(dialog_->children().front());
    return header_panel->ShowAppInWebStore();
  }

 private:
  raw_ptr<AppInfoDialog> dialog_;
};

}  // namespace test

class AppInfoDialogBrowserTest : public DialogBrowserTest {
 public:
  AppInfoDialogBrowserTest() = default;

  AppInfoDialogBrowserTest(const AppInfoDialogBrowserTest&) = delete;
  AppInfoDialogBrowserTest& operator=(const AppInfoDialogBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    extension_environment_ =
        std::make_unique<extensions::TestExtensionEnvironment>(
            extensions::TestExtensionEnvironment::Type::
                kInheritExistingTaskEnvironment,
            extensions::TestExtensionEnvironment::ProfileCreationType::kNoCreate
#if BUILDFLAG(IS_CHROMEOS)
            ,
            extensions::TestExtensionEnvironment::OSSetupType::kNoSetUp
#endif
        );
    constexpr char kTestExtensionId[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    extension_ =
        extension_environment_->MakePackagedApp(kTestExtensionId, false);
    extensions::ExtensionRegistrar::Get(browser()->GetProfile())
        ->AddExtension(extension_);
  }
  void TearDownOnMainThread() override { extension_environment_ = nullptr; }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
    ShowAppInfoInNativeDialog(web_contents, browser()->GetProfile(),
                              extension_.get(), base::DoNothing());
  }

 protected:
  std::unique_ptr<extensions::TestExtensionEnvironment> extension_environment_;
  scoped_refptr<const extensions::Extension> extension_;
};

// Invokes a dialog that shows details of an installed extension.
// Flaky on ChromeOS. See https://crbug.com/40933370
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_InvokeUi_default DISABLED_InvokeUi_default
#else
#define MAYBE_InvokeUi_default InvokeUi_default
#endif
IN_PROC_BROWSER_TEST_F(AppInfoDialogBrowserTest, MAYBE_InvokeUi_default) {
  ShowAndVerifyUi();
}

// Flaky on ChromeOS. See https://crbug.com/40932992
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

  // Unload the test extension.
  extensions::ExtensionRegistrar::Get(browser()->GetProfile())
      ->RemoveExtension(extension_->id(),
                        extensions::UnloadedExtensionReason::PROFILE_SHUTDOWN);

  // Dialog widgets and their root views are closed asynchronously so the dialog
  // is still alive.
  ASSERT_TRUE(AppInfoDialog::GetLastDialogForTesting());

  // Dialog is now closing.
  ASSERT_TRUE(
      AppInfoDialog::GetLastDialogForTesting()->GetWidget()->IsClosed());
}

// Tests that clicking the View in Store link opens a browser tab and closes the
// dialog cleanly.
IN_PROC_BROWSER_TEST_F(AppInfoDialogBrowserTest, ViewInStore) {
  ShowUi("");
  base::WeakPtr<AppInfoDialog> dialog =
      AppInfoDialog::GetLastDialogForTesting();
  ASSERT_TRUE(dialog);
  views::Widget* widget = dialog->GetWidget();
  ASSERT_TRUE(widget);
  EXPECT_FALSE(widget->IsClosed());

  TabStripModel* tabs = browser()->tab_strip_model();
  int initial_tab_count = tabs->count();

  test::AppInfoDialogTestApi(dialog.get()).ShowAppInWebStore();

  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsClosed());

  EXPECT_EQ(initial_tab_count + 1, tabs->count());
  content::WebContents* web_contents =
      tabs->GetWebContentsAt(initial_tab_count);

  std::string url = extension_urls::GetWebstoreItemDetailURLPrefix();
  url += extension_->id();
  url += "?utm_source=chrome-app-launcher-info-dialog";
  EXPECT_EQ(GURL(url), web_contents->GetURL());
}
