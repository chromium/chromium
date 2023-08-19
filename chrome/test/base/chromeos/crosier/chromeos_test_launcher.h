// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_CHROMEOS_CROSIER_CHROMEOS_TEST_LAUNCHER_H_
#define CHROME_TEST_BASE_CHROMEOS_CROSIER_CHROMEOS_TEST_LAUNCHER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/app/chrome_main_delegate.h"
#include "content/public/test/test_launcher.h"

class ChromeOSTestSuite;

// Allows a test suite to override the TestSuite class used. By default it is an
// instance of ChromeTestSuite.
class ChromeOSTestSuiteRunner {
 public:
  ChromeOSTestSuiteRunner() = default;
  ChromeOSTestSuiteRunner(const ChromeOSTestSuiteRunner&) = delete;
  ChromeOSTestSuiteRunner& operator=(const ChromeOSTestSuiteRunner&) = delete;
  virtual ~ChromeOSTestSuiteRunner() = default;

  virtual int RunTestSuite(int argc, char** argv);

 protected:
  static int RunTestSuiteInternal(ChromeOSTestSuite* test_suite);
};

// Acts like normal ChromeMainDelegate but injects behaviour for crosint tests.
class ChromeOSTestChromeMainDelegate : public ChromeMainDelegate {
 public:
  // |time| is the time at which the main function of the
  // executable was entered, or null if not available.
  explicit ChromeOSTestChromeMainDelegate(base::TimeTicks time)
      : ChromeMainDelegate(time) {}

  // ChromeMainDelegateOverrides.
  content::ContentBrowserClient* CreateContentBrowserClient() override;
  content::ContentUtilityClient* CreateContentUtilityClient() override;
};

// Delegate used for setting up and running chrome browser tests.
class ChromeOSTestLauncherDelegate : public content::TestLauncherDelegate {
 public:
  explicit ChromeOSTestLauncherDelegate(ChromeOSTestSuiteRunner* runner);
  ChromeOSTestLauncherDelegate(const ChromeOSTestLauncherDelegate&) = delete;
  ChromeOSTestLauncherDelegate& operator=(const ChromeOSTestLauncherDelegate&) =
      delete;
  ~ChromeOSTestLauncherDelegate() override;

 protected:
  // content::TestLauncherDelegate:
  content::ContentMainDelegate* CreateContentMainDelegate() override;
  int RunTestSuite(int argc, char** argv) override;
  void PreSharding() override;
  void OnDoneRunningTests() override;

 private:
  raw_ptr<ChromeOSTestSuiteRunner> runner_;
};

// Launches crosint tests.
// Does not take ownership of ChromeTestLauncherDelegate.
int LaunchChromeOSTests(content::TestLauncherDelegate* delegate,
                        int argc,
                        char** argv);

#endif  // CHROME_TEST_BASE_CHROMEOS_CROSIER_CHROMEOS_TEST_LAUNCHER_H_
