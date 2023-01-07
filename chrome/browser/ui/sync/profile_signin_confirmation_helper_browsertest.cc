// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sync/profile_signin_confirmation_helper.h"

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"
#include "testing/gtest/include/gtest/gtest.h"

class ProfileSigninConfirmationHelperBrowserTest : public InProcessBrowserTest {
 public:
  ProfileSigninConfirmationHelperBrowserTest() {}

  ProfileSigninConfirmationHelperBrowserTest(
      const ProfileSigninConfirmationHelperBrowserTest&) = delete;
  ProfileSigninConfirmationHelperBrowserTest& operator=(
      const ProfileSigninConfirmationHelperBrowserTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Force the first-run flow to trigger autoimport.
    command_line->AppendSwitch(switches::kForceFirstRun);
  }
};

// http://crbug.com/321302
#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && \
    (BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS))
#define MAYBE_HasNotBeenShutdown DISABLED_HasNotBeenShutdown
#else
#define MAYBE_HasNotBeenShutdown HasNotBeenShutdown
#endif
IN_PROC_BROWSER_TEST_F(ProfileSigninConfirmationHelperBrowserTest,
                       MAYBE_HasNotBeenShutdown) {
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_TRUE(first_run::auto_import_state() & first_run::AUTO_IMPORT_CALLED);
#endif
  EXPECT_FALSE(ui::HasBeenShutdown(browser()->profile()));
}

// http://crbug.com/321302
#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && \
    (BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS))
#define MAYBE_HasNoSyncedExtensions DISABLED_HasNoSyncedExtensions
#else
#define MAYBE_HasNoSyncedExtensions HasNoSyncedExtensions
#endif
IN_PROC_BROWSER_TEST_F(ProfileSigninConfirmationHelperBrowserTest,
                       MAYBE_HasNoSyncedExtensions) {
  EXPECT_FALSE(ui::HasSyncedExtensions(browser()->profile()));
}
