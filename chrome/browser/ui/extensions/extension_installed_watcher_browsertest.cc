// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_installed_watcher.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/test/run_until.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/extensions/extension_install_ui.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/common/extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::Extension;

class ExtensionInstalledWatcherBrowserTest
    : public extensions::ExtensionBrowserTest {
 public:
  void WaitFor(scoped_refptr<const Extension> extension,
               BrowserWindowInterface* test_browser = nullptr) {
    if (!test_browser) {
      test_browser = browser();
    }
    ExtensionInstalledWatcher::From(test_browser)
        ->WaitForInstall(
            extension->id(),
            base::BindOnce(&ExtensionInstalledWatcherBrowserTest::Done,
                           base::Unretained(this)));
  }

  void Done(bool success) {
    done_called_++;
    install_success_ = success;
  }

 protected:
  int done_called_ = 0;
  std::optional<bool> install_success_;

  scoped_refptr<const Extension> MakeExtensionNamed(const std::string& name) {
    return extensions::ExtensionBuilder(name).Build();
  }

  extensions::ExtensionRegistrar* extension_registrar() {
    return extensions::ExtensionRegistrar::Get(profile());
  }
};

IN_PROC_BROWSER_TEST_F(ExtensionInstalledWatcherBrowserTest,
                       ExtensionIsAlreadyInstalled) {
  auto extension = MakeExtensionNamed("foo");
  extension_registrar()->AddExtension(extension);

  WaitFor(extension);
  EXPECT_EQ(0, done_called_);
  ASSERT_TRUE(base::test::RunUntil([&] { return done_called_ >= 1; }));
  EXPECT_EQ(1, done_called_);
  EXPECT_TRUE(install_success_.has_value() && install_success_.value());
}

IN_PROC_BROWSER_TEST_F(ExtensionInstalledWatcherBrowserTest, ExtensionInstall) {
  auto extension = MakeExtensionNamed("foo");

  WaitFor(extension);
  EXPECT_EQ(0, done_called_);

  extension_registrar()->AddExtension(extension);

  // ExtensionInstalledWatcher must *not* call the done callback on the same
  // runloop cycle as the extension installation, to allow all the other
  // observers to run.
  EXPECT_EQ(0, done_called_);

  ASSERT_TRUE(base::test::RunUntil([&] { return done_called_ >= 1; }));
  EXPECT_EQ(1, done_called_);
  EXPECT_TRUE(install_success_.has_value() && install_success_.value());
}

IN_PROC_BROWSER_TEST_F(ExtensionInstalledWatcherBrowserTest,
                       NotTheExtensionYouAreLookingFor) {
  auto foo = MakeExtensionNamed("foo");
  auto bar = MakeExtensionNamed("bar");

  WaitFor(foo);
  EXPECT_EQ(0, done_called_);

  extension_registrar()->AddExtension(bar);
  extension_registrar()->AddExtension(foo);

  ASSERT_TRUE(base::test::RunUntil([&] { return done_called_ >= 1; }));
  EXPECT_EQ(1, done_called_);
  EXPECT_TRUE(install_success_.has_value() && install_success_.value());
}

IN_PROC_BROWSER_TEST_F(ExtensionInstalledWatcherBrowserTest,
                       BrowserShutdownWhileWaiting) {
  auto foo = MakeExtensionNamed("foo");

  WaitFor(foo);

  CloseBrowserSynchronously(browser());
  EXPECT_EQ(0, done_called_);
  EXPECT_FALSE(install_success_.has_value());  // Callback was never run
}

// Regression test for https://crbug.com/40672408.
IN_PROC_BROWSER_TEST_F(ExtensionInstalledWatcherBrowserTest,
                       BrowserShutdownWhileWaitingDoesntCrash) {
  auto foo = MakeExtensionNamed("foo");
  WaitFor(foo);

  // If the fix for https://crbug.com/40672408 regresses, this will crash:
  ui_test_utils::BrowserDestroyedObserver observer(browser());
  chrome::CloseWindow(browser());
  observer.Wait();
  EXPECT_EQ(0, done_called_);
}

// Regression test for https://crbug.com/573465330: closing the target browser
// window before the extension finishes loading must not dereference a dangling
// `BrowserWindowInterface` when the post-install watcher callback runs.
IN_PROC_BROWSER_TEST_F(ExtensionInstalledWatcherBrowserTest,
                       OnInstallSuccessBrowserClosedBeforeExtensionLoads) {
  auto foo = MakeExtensionNamed("foo");

  std::unique_ptr<ExtensionInstallUI> install_ui =
      ExtensionInstallUI::Create(profile());
  install_ui->OnInstallSuccess(foo, /*icon=*/nullptr);

  // Keep `profile()` alive with a second browser window so closing the
  // original `browser()` bound by `OnInstallSuccess` does not tear down the
  // profile before the extension loads.
  BrowserWindowInterface* second_browser = CreateBrowser(profile());
  WaitFor(foo, second_browser);

  CloseBrowserSynchronously(browser());

  extension_registrar()->AddExtension(foo);
  ASSERT_TRUE(base::test::RunUntil([&] { return done_called_ >= 1; }));
  EXPECT_EQ(1, done_called_);
  EXPECT_TRUE(install_success_.value_or(false));
}

// Regression test for https://crbug.com/573465330: closing all tabs on the
// target browser window while the post-install watcher callback is queued must
// handle a null active tab during window teardown.
IN_PROC_BROWSER_TEST_F(ExtensionInstalledWatcherBrowserTest,
                       OnInstallSuccessTabsClosedWhileCallbackPending) {
  auto foo = MakeExtensionNamed("foo");
  extension_registrar()->AddExtension(foo);

  // Keep `profile()` alive with a non-tabbed popup window created *before*
  // `OnInstallSuccess` so `FindTabbedBrowser()` still targets `browser()` and
  // no message loop runs after the post-install callback is queued.
  BrowserWindowInterface* popup_browser = CreateBrowserForPopup(profile());

  std::unique_ptr<ExtensionInstallUI> install_ui =
      ExtensionInstallUI::Create(profile());
  install_ui->OnInstallSuccess(foo, /*icon=*/nullptr);
  WaitFor(foo, popup_browser);

  // Closing the sole tab empties the tab strip synchronously while `browser()`
  // is still alive and the already-queued post-install callback is pending.
  ui_test_utils::BrowserDestroyedObserver observer(browser());
  chrome::CloseTab(browser());
  EXPECT_EQ(nullptr, browser()->GetActiveTabInterface());

  ASSERT_TRUE(base::test::RunUntil([&] { return done_called_ >= 1; }));
  observer.Wait();
  EXPECT_EQ(1, done_called_);
  EXPECT_TRUE(install_success_.value_or(false));
}
