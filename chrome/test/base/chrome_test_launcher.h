// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_CHROME_TEST_LAUNCHER_H_
#define CHROME_TEST_BASE_CHROME_TEST_LAUNCHER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/app/startup_timestamps.h"
#include "content/public/test/test_launcher.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/app/android/chrome_main_delegate_android.h"
#else
#include "chrome/app/chrome_main_delegate.h"
#endif

class ChromeTestSuite;

// Allows a test suite to override the TestSuite class used. By default it is an
// instance of ChromeTestSuite.
class ChromeTestSuiteRunner {
 public:
  ChromeTestSuiteRunner() = default;
  ChromeTestSuiteRunner(const ChromeTestSuiteRunner&) = delete;
  ChromeTestSuiteRunner& operator=(const ChromeTestSuiteRunner&) = delete;
  virtual ~ChromeTestSuiteRunner() = default;

  virtual int RunTestSuite(int argc, char** argv);

 protected:
  static int RunTestSuiteInternal(ChromeTestSuite* test_suite);
};

// Acts like normal ChromeMainDelegate but injects behaviour for browser tests.
class ChromeTestChromeMainDelegate
#if BUILDFLAG(IS_ANDROID)
    : public ChromeMainDelegateAndroid {
#else
    : public ChromeMainDelegate {
#endif
 public:
#if BUILDFLAG(IS_ANDROID)
  ChromeTestChromeMainDelegate() : ChromeMainDelegateAndroid() {}
#else
  ChromeTestChromeMainDelegate()
      : ChromeMainDelegate({.exe_entry_point_ticks = base::TimeTicks::Now()}) {}
#endif

  // ChromeMainDelegateOverrides.
  content::ContentUtilityClient* CreateContentUtilityClient() override;
#if !BUILDFLAG(IS_ANDROID)
  std::optional<int> PostEarlyInitialization(InvokedIn invoked_in) override;
#endif  // !BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(IS_WIN)
  bool ShouldHandleConsoleControlEvents() override;
#endif
  void CreateThreadPool(std::string_view name) override;
};

// Delegate used for setting up and running chrome browser tests.
class ChromeTestLauncherDelegate : public content::TestLauncherDelegate {
 public:
  // Does not take ownership of ChromeTestSuiteRunner.
  explicit ChromeTestLauncherDelegate(ChromeTestSuiteRunner* runner);
  ChromeTestLauncherDelegate(const ChromeTestLauncherDelegate&) = delete;
  ChromeTestLauncherDelegate& operator=(const ChromeTestLauncherDelegate&) =
      delete;
  ~ChromeTestLauncherDelegate() override;

 protected:
  // content::TestLauncherDelegate:
  int RunTestSuite(int argc, char** argv) override;
  std::string GetUserDataDirectoryCommandLineSwitch() override;
#if !BUILDFLAG(IS_ANDROID)
  content::ContentMainDelegate* CreateContentMainDelegate() override;
#endif
  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override;
  void PreSharding() override;
  void OnDoneRunningTests() override;

 private:
#if BUILDFLAG(IS_WIN)
  class ScopedFirewallRules;

  std::unique_ptr<ScopedFirewallRules> firewall_rules_;
#endif

  raw_ptr<ChromeTestSuiteRunner> runner_;
};

// Launches Chrome browser tests. |parallel_jobs| is number of test jobs to be
// run in parallel. Returns exit code.
// Does not take ownership of ChromeTestLauncherDelegate.
int LaunchChromeTests(size_t parallel_jobs,
                      content::TestLauncherDelegate* delegate,
                      int argc,
                      char** argv);

#endif  // CHROME_TEST_BASE_CHROME_TEST_LAUNCHER_H_
