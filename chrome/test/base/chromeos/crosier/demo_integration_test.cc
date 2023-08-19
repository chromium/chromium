// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/gtest_tags.h"
#include "base/test/test_switches.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/test/base/chromeos/crosier/chromeos_integration_test_mixin.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_switches.h"
#endif

// This is a demo to run a browser test on a ChromeOS device or VM.
//
// To run the test for lacros:
//
// Environment and device setup:
//   Follow //docs/lacros/build_dut_lacros.md so you can build Lacros
//   and deploy on a VM or ChromeOS device under test(DUT).
// 1 Use DUT gn args
//   Following is an example gn args for Lacros:
//   '''
//   import("//build/args/chromeos/amd64-generic-crostoolchain.gni")
//   chromeos_is_browser_only= true
//   dcheck_always_on = true
//   is_chromeos_device= true
//   is_debug= false
//   symbol_level= 0
//   target_os= "chromeos"
//   use_goma=true
//   '''
// 2 Build chromeos_integration_tests target
//   Example commandline:
//   $ autoninja -C out/lacrosdut chromeos_integration_tests
// 3 Run test on DUT/VM
//   For VM, follow
//   https://chromium.googlesource.com/chromiumos/docs/+/HEAD/cros_vm.md#run-a-chrome-gtest-binary-in-the-vm
//   For DUT, example commandline:
//   $ out/lacros_amd64/bin/run_chromeos_integration_tests --board=<DUT_board> \
//     --device=<device_ip> --gtest_filter=DemoIntegrationTest.NewTab
//   This test should pass.
// 4 Run test in interactive mode
//   This is helpful if you want to see the browser and interact with it.
//   First prepare the device to show the main screen, in either guest mode
//   or signed in.
//   Then run test in interactive mode:
//   $ out/lacros_amd64/bin/run_chromeos_integration_tests --board=<DUT_board> \
//     --device=<device_id> --enable-pixel-output-in-tests \
//     --test-launcher-interactive
//   You should see a browser open with tab direct to chrome://version.
//   You can now interact with the browser freely. There might be some
//   limitations, like the browser is only allowed to access certain websites.
//
// To run the test for ash:
//
// 1. See http://go/crosint-run for instructions.
// 2. You can optionally add --test-launcher-interactive to play with the
//    browser after the test finishes.
class DemoIntegrationTest : public MixinBasedInProcessBrowserTest {
 public:
#if BUILDFLAG(IS_CHROMEOS_ASH)
  void SetUpCommandLine(base::CommandLine* cmd_line) override {
    // On ash this test exercises the built-in browser, not lacros.
    cmd_line->AppendSwitch(ash::switches::kDisallowLacros);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  void SetUpInProcessBrowserTestFixture() override {
    // You don't need this in your test.
    // By default, we don't allow any network access in tests to
    // avoid flakiness. This is just a showcase so people can
    // interact with the browser try out a few websites.
    host_resolver()->AllowDirectLookup("*.google.com");
    host_resolver()->AllowDirectLookup("*.geotrust.com");
    host_resolver()->AllowDirectLookup("*.gstatic.com");
    host_resolver()->AllowDirectLookup("*.googleapis.com");
    host_resolver()->AllowDirectLookup("accounts.google.*");
    host_resolver()->AllowDirectLookup("*.chromium.org");

    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void TearDownOnMainThread() override {
    // Close any browsers we opened otherwise the test may hang on shutdown.
    // This happens because chromeos_integration_tests is not started by session
    // manager, but AttemptUserExit() uses session manager to kill the chrome
    // binary.
    // TODO(b/292067979): Find a better way to work around this issue.
    for (Browser* browser : *BrowserList::GetInstance()) {
      CloseBrowserSynchronously(browser);
    }

    InProcessBrowserTest::TearDownOnMainThread();
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

 private:
  ChromeOSIntegrationTestMixin chromeos_integration_test_mixin_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(DemoIntegrationTest, NewTab) {
  base::AddFeatureIdTagToTestResult(
      "screenplay-351d628b-e4a4-41c6-91e4-a4036ad12360");

  GURL version_url{"chrome://version"};

  ASSERT_TRUE(AddTabAtIndex(0, version_url, ui::PAGE_TRANSITION_TYPED));

  // You don't need these for your tests. This is just to showcase the browser
  // when you want to see the test browser and interact with it.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kTestLauncherInteractive)) {
    base::RunLoop loop;
    loop.Run();
  }
}
