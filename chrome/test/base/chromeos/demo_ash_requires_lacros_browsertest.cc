// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/chromeos/ash_browser_test_starter.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"

// This is a demo for ash browser test, which requires Lacros.
// To run this test, you need to build test with Lacros. That means
// your gn args should contain "also_build_lacros_chrome = true"
// Also, some gn args can cause trouble, strongly suggest you use
// the linux-chromeos-rel bot(CI or CQ) gn args locally.
// $ autoninja -C out/ashdesktop browser_tests
// $ out/ashdesktop/browser_tests \
//   --gtest_filter=DemoAshRequiresLacrosTest* \
//   --lacros-chrome-path=out/ashdesktop/lacros_clang_x64/test_lacros_chrome \
//   --enable-pixel-output-in-tests
// You should see there are 2 browser instances, one is ash browser,
// another one is Lacros browser and Lacros is in the front.
// IMPORTANT: If you're adding new tests in browser_tests target following this
// example, make sure to add your tests in
// //testing/buildbot/filters/linux-chromeos.browser_tests.require_lacros.filter
// so that it can run with the correct argument on linux-chromeos-rel builder
// with the step name "browser_tests_require_lacros".
class DemoAshRequiresLacrosTest : public InProcessBrowserTest {
  void SetUpInProcessBrowserTestFixture() override {
    if (ash_starter_.HasLacrosArgument()) {
      ASSERT_TRUE(ash_starter_.PrepareEnvironmentForLacros());
    }
  }

  void SetUpOnMainThread() override {
    if (ash_starter_.HasLacrosArgument()) {
      ash_starter_.StartLacros(this);
    }
  }

 protected:
  test::AshBrowserTestStarter ash_starter_;
};

IN_PROC_BROWSER_TEST_F(DemoAshRequiresLacrosTest, NewTab) {
  if (ash_starter_.HasLacrosArgument()) {
    crosapi::BrowserManager::Get()->NewTab();
    // Assert Lacros is running.
    ASSERT_TRUE(crosapi::BrowserManager::Get()->IsRunning());
  }
}
