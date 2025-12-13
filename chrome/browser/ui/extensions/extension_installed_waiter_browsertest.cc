// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_installed_waiter.h"

#include "base/functional/callback_helpers.h"
#include "base/test/run_until.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/common/extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::Extension;

class ExtensionInstalledWaiterTest : public extensions::ExtensionBrowserTest {
 public:
  void TearDownOnMainThread() override {
    ExtensionInstalledWaiter::SetGivingUpCallbackForTesting(
        base::NullCallback());
    extensions::ExtensionBrowserTest::TearDownOnMainThread();
  }

  void WaitFor(scoped_refptr<const Extension> extension,
               Browser* test_browser = nullptr) {
    ExtensionInstalledWaiter::SetGivingUpCallbackForTesting(base::BindRepeating(
        &ExtensionInstalledWaiterTest::GivingUp, base::Unretained(this)));
    if (!test_browser) {
      test_browser = browser();
    }
    ExtensionInstalledWaiter::WaitForInstall(
        extension, test_browser,
        base::BindOnce(&ExtensionInstalledWaiterTest::Done,
                       base::Unretained(this)));
  }

  void Done() { done_called_++; }
  void GivingUp() { giving_up_called_++; }

 protected:
  int done_called_ = 0;
  int giving_up_called_ = 0;

  scoped_refptr<const Extension> MakeExtensionNamed(const std::string& name) {
    return extensions::ExtensionBuilder(name).Build();
  }

  extensions::ExtensionRegistrar* extension_registrar() {
    return extensions::ExtensionRegistrar::Get(profile());
  }
};

IN_PROC_BROWSER_TEST_F(ExtensionInstalledWaiterTest,
                       ExtensionIsAlreadyInstalled) {
  auto extension = MakeExtensionNamed("foo");
  extension_registrar()->AddExtension(extension);

  WaitFor(extension);
  EXPECT_EQ(1, done_called_);
}

IN_PROC_BROWSER_TEST_F(ExtensionInstalledWaiterTest, ExtensionInstall) {
  auto extension = MakeExtensionNamed("foo");

  WaitFor(extension);
  EXPECT_EQ(0, done_called_);

  extension_registrar()->AddExtension(extension);

  // ExtensionInstalledWaiter must *not* call the done callback on the same
  // runloop cycle as the extension installation, to allow all the other
  // observers to run.
  EXPECT_EQ(0, done_called_);

  ASSERT_TRUE(base::test::RunUntil([&] { return done_called_ >= 1; }));
  EXPECT_EQ(1, done_called_);
}

IN_PROC_BROWSER_TEST_F(ExtensionInstalledWaiterTest,
                       NotTheExtensionYouAreLookingFor) {
  auto foo = MakeExtensionNamed("foo");
  auto bar = MakeExtensionNamed("bar");

  WaitFor(foo);
  EXPECT_EQ(0, done_called_);

  extension_registrar()->AddExtension(bar);
  extension_registrar()->AddExtension(foo);

  ASSERT_TRUE(base::test::RunUntil([&] { return done_called_ >= 1; }));
  EXPECT_EQ(1, done_called_);
}

IN_PROC_BROWSER_TEST_F(ExtensionInstalledWaiterTest,
                       ExtensionUninstalledWhileWaiting) {
  auto extension = MakeExtensionNamed("foo");

  WaitFor(extension);
  EXPECT_EQ(0, done_called_);
  EXPECT_EQ(0, giving_up_called_);

  extension_registrar()->AddExtension(extension);
  extension_registrar()->RemoveExtension(
      extension->id(), extensions::UnloadedExtensionReason::UNINSTALL);

  ASSERT_TRUE(base::test::RunUntil([&] { return giving_up_called_ >= 1; }));
  EXPECT_EQ(1, giving_up_called_);
  EXPECT_EQ(0, done_called_);
}

IN_PROC_BROWSER_TEST_F(ExtensionInstalledWaiterTest,
                       BrowserShutdownWhileWaiting) {
  auto foo = MakeExtensionNamed("foo");
  WaitFor(foo, browser());

  CloseBrowserSynchronously(browser());
  EXPECT_EQ(1, giving_up_called_);
  EXPECT_EQ(0, done_called_);
}

// Regression test for https://crbug.com/1049190.
IN_PROC_BROWSER_TEST_F(ExtensionInstalledWaiterTest,
                       BrowserShutdownWhileWaitingDoesntCrash) {
  auto foo = MakeExtensionNamed("foo");
  WaitFor(foo, browser());

  // Null out the giving-up callback, which is how the class is actually used in
  // production.
  ExtensionInstalledWaiter::SetGivingUpCallbackForTesting({});

  // If the fix for https://crbug.com/1049190 regresses, this will crash:
  chrome::CloseWindow(browser());
  ui_test_utils::WaitForBrowserToClose(browser());

  EXPECT_EQ(0, giving_up_called_);
  EXPECT_EQ(0, done_called_);
}
