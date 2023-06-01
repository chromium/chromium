// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_installed_waiter.h"

#include "base/command_line.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/load_error_reporter.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::Extension;

class ExtensionInstalledWaiterTest : public BrowserWithTestWindowTest {
 public:
  ExtensionInstalledWaiterTest()
      : BrowserWithTestWindowTest(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~ExtensionInstalledWaiterTest() override = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    extensions::LoadErrorReporter::Init(false);
    extensions::TestExtensionSystem* extension_system =
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(profile()));
    extension_system->CreateExtensionService(
        base::CommandLine::ForCurrentProcess(), base::FilePath(), false);
    extension_service_ =
        extensions::ExtensionSystem::Get(profile())->extension_service();
  }

  void TearDown() override {
    ExtensionInstalledWaiter::SetGivingUpCallbackForTesting(
        base::NullCallback());
    BrowserWithTestWindowTest::TearDown();
  }

  void WaitFor(scoped_refptr<const Extension> extension,
               Browser* test_browser = nullptr) {
    ExtensionInstalledWaiter::SetGivingUpCallbackForTesting(base::BindRepeating(
        &ExtensionInstalledWaiterTest::GivingUp, base::Unretained(this)));
    if (!test_browser)
      test_browser = browser();
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

  extensions::ExtensionService* extension_service() {
    return extension_service_;
  }

 private:
  raw_ptr<extensions::ExtensionService, DanglingUntriaged> extension_service_ =
      nullptr;
};

TEST_F(ExtensionInstalledWaiterTest, ExtensionIsAlreadyInstalled) {
  auto extension = MakeExtensionNamed("foo");
  extension_service()->AddExtension(extension.get());

  WaitFor(extension);
  EXPECT_EQ(1, done_called_);
}

TEST_F(ExtensionInstalledWaiterTest, ExtensionInstall) {
  auto extension = MakeExtensionNamed("foo");

  WaitFor(extension);
  EXPECT_EQ(0, done_called_);

  extension_service()->AddExtension(extension.get());

  // ExtensionInstalledWaiter must *not* call the done callback on the same
  // runloop cycle as the extension installation, to allow all the other
  // observers to run.
  EXPECT_FALSE(task_environment()->MainThreadIsIdle());
  EXPECT_EQ(0, done_called_);

  task_environment()->RunUntilIdle();
  EXPECT_EQ(1, done_called_);
}

TEST_F(ExtensionInstalledWaiterTest, NotTheExtensionYouAreLookingFor) {
  auto foo = MakeExtensionNamed("foo");
  auto bar = MakeExtensionNamed("bar");

  WaitFor(foo);
  EXPECT_EQ(0, done_called_);

  extension_service()->AddExtension(bar.get());
  task_environment()->RunUntilIdle();
  EXPECT_EQ(0, done_called_);

  extension_service()->AddExtension(foo.get());
  task_environment()->RunUntilIdle();
  EXPECT_EQ(1, done_called_);
}

TEST_F(ExtensionInstalledWaiterTest, ExtensionUninstalledWhileWaiting) {
  auto extension = MakeExtensionNamed("foo");

  WaitFor(extension);
  EXPECT_EQ(0, done_called_);

  extension_service()->AddExtension(extension.get());
  extension_service()->UnloadExtension(
      extension->id(), extensions::UnloadedExtensionReason::UNINSTALL);
  EXPECT_EQ(1, giving_up_called_);

  task_environment()->RunUntilIdle();
  EXPECT_EQ(0, done_called_);
}

TEST_F(ExtensionInstalledWaiterTest, BrowserShutdownWhileWaiting) {
  std::unique_ptr<BrowserWindow> window = CreateBrowserWindow();
  std::unique_ptr<Browser> browser =
      CreateBrowser(profile(), Browser::TYPE_NORMAL, false, window.get());

  auto foo = MakeExtensionNamed("foo");
  WaitFor(foo, browser.get());

  browser.reset();
  EXPECT_EQ(1, giving_up_called_);
  EXPECT_EQ(0, done_called_);
}

// Regression test for https://crbug.com/1049190.
TEST_F(ExtensionInstalledWaiterTest, BrowserShutdownWhileWaitingDoesntCrash) {
  std::unique_ptr<BrowserWindow> window = CreateBrowserWindow();
  std::unique_ptr<Browser> browser =
      CreateBrowser(profile(), Browser::TYPE_NORMAL, false, window.get());

  auto foo = MakeExtensionNamed("foo");
  WaitFor(foo, browser.get());

  // Null out the giving-up callback, which is how the class is actually used in
  // production.
  ExtensionInstalledWaiter::SetGivingUpCallbackForTesting({});

  // If the fix for https://crbug.com/1049190 regresses, this will crash:
  browser->OnWindowClosing();

  EXPECT_EQ(0, giving_up_called_);
  EXPECT_EQ(0, done_called_);
}
