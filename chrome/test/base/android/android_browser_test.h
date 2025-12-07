// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_ANDROID_ANDROID_BROWSER_TEST_H_
#define CHROME_TEST_BASE_ANDROID_ANDROID_BROWSER_TEST_H_

#include "base/callback_list.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/test/browser_test_base.h"

class PrefService;

// A base class for browser tests run on Android. It exposes very little API
// since the majority of the Android UI is accessed through static methods,
// such as TabModelList.
//
// Do *not* extend this class to attempt to mirror APIs on InProcessBrowserTest
// which is the base class for desktop platforms.
// Shared abstractions around Desktop-vs-Android should be implemented as
// topical helper classes or functions independent of the BrowserTestBase class
// hierarchy. Helpers may take a PlatformBrowserTest* in their APIs in order to
// support both types of browser tests, such as in the chrome_test_utils
// namespace.
//
// Further details and methodology can be found in the design doc:
// https://docs.google.com/document/d/1jT3W6VnVI4b0FuiNbYzgGZPxIOUZmppUZZwi3OebvVE/preview
class AndroidBrowserTest : public content::BrowserTestBase {
 public:
  AndroidBrowserTest();

  AndroidBrowserTest(const AndroidBrowserTest&) = delete;
  AndroidBrowserTest& operator=(const AndroidBrowserTest&) = delete;

  ~AndroidBrowserTest() override;

  // Returns the currently running AndroidBrowserTest.
  static AndroidBrowserTest* GetCurrent();

  // Sets up default command line that will be visible to the code under test.
  // Called by SetUp() after SetUpCommandLine() to add default command line
  // switches. A default implementation is provided in this class. If a test
  // does not want to use the default implementation, it should override this
  // method.
  virtual void SetUpDefaultCommandLine(base::CommandLine* command_line);

  // Initializes the contents of the user data directory. Called by SetUp()
  // after creating the user data directory, but before any browser is launched.
  // If a test wishes to set up some initial non-empty state in the user data
  // directory before the browser starts up, it can do so here. Returns true if
  // successful. To set initial prefs, see SetUpLocalStatePrefService.
  [[nodiscard]] virtual bool SetUpUserDataDirectory();

  // Called just before BrowserContextKeyedService creation is started
  // for each Profile creation.
  // Test fixtures inheriting AndroidBrowserTest can inject some fake/test
  // BrowserContextKeyedService as necessary for testing.
  virtual void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) {}

  // Tests can override this to customize the initial local_state.
  virtual void SetUpLocalStatePrefService(PrefService* local_state) {}

  // content::BrowserTestBase implementation.
  void SetUp() override;
  void PreRunTestOnMainThread() override;
  void PostRunTestOnMainThread() override;

  // Counts the number of "PRE_" prefixes in the test name. This is used to
  // differentiate between different PRE tests in browser test constructors
  // and setup functions.
  static size_t GetTestPreCount();

  // Returns the test data path used by the embedded test server.
  base::FilePath GetChromeTestDataDir() const;

  // Returns the profile. If there are multiple profiles, it's not determined
  // what profile is returned.
  Profile* GetProfile() const;

 private:
  // Temporary user data directory. Used only when a user data directory is not
  // specified in the command line.
  base::ScopedTempDir temp_user_data_dir_;

  base::test::ScopedFeatureList feature_list_;

  // Used to set up test factories for each browser context.
  base::CallbackListSubscription create_services_subscription_;
};

#endif  // CHROME_TEST_BASE_ANDROID_ANDROID_BROWSER_TEST_H_
