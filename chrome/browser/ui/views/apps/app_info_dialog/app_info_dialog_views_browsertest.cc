// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/apps/app_info_dialog.h"

#include <memory>
#include <string>

#include "base/callback_helpers.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/test_extension_environment.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_footer_panel.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_test.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/test/button_test_api.h"

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
IN_PROC_BROWSER_TEST_F(AppInfoDialogBrowserTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

// Ash platform can't create shortcuts.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
class DummyEvent : public ui::Event {
 public:
  DummyEvent() : Event(ui::ET_UNKNOWN, base::TimeTicks(), 0) {}
  ~DummyEvent() override = default;
};

IN_PROC_BROWSER_TEST_F(AppInfoDialogBrowserTest,
                       CreateShortcutsAfterProfileDeletion) {
  // Open app info and grab handle to footer panel.
  AppInfoFooterPanel* panel = nullptr;
  AppInfoFooterPanel::InstanceCallbackForTesting() = base::BindLambdaForTesting(
      [&panel](AppInfoFooterPanel* new_panel) { panel = new_panel; });
  ShowUi("");
  ASSERT_TRUE(panel);
  ASSERT_TRUE(panel->get_create_shortcuts_button_for_testing());
  base::WeakPtr<AppInfoFooterPanel> panel_weak_ptr = panel->AsWeakPtr();

  extension_environment_->DeleteProfile();

  // Panel is still alive despite the widget closing.
  ASSERT_TRUE(panel_weak_ptr);
  ASSERT_TRUE(panel->GetWidget()->IsClosed());

  // Clicking create shortcuts button should not crash.
  DummyEvent dummy_event;
  views::test::ButtonTestApi(panel->get_create_shortcuts_button_for_testing())
      .NotifyClick(dummy_event);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
