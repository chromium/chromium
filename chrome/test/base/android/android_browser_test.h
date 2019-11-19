// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_ANDROID_ANDROID_BROWSER_TEST_H_
#define CHROME_TEST_BASE_ANDROID_ANDROID_BROWSER_TEST_H_

#include "base/macros.h"
#include "base/files/scoped_temp_dir.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"

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
  ~AndroidBrowserTest() override;

  // Sets up default command line that will be visible to the code under test.
  // Called by SetUp() after SetUpCommandLine() to add default command line
  // switches. A default implementation is provided in this class. If a test
  // does not want to use the default implementation, it should override this
  // method.
  virtual void SetUpDefaultCommandLine(base::CommandLine* command_line);

  // content::BrowserTestBase implementation.
  void SetUp() override;
  void PreRunTestOnMainThread() override;
  void PostRunTestOnMainThread() override;

 private:
  // Temporary user data directory. Used only when a user data directory is not
  // specified in the command line.
  base::ScopedTempDir temp_user_data_dir_;

  DISALLOW_COPY_AND_ASSIGN(AndroidBrowserTest);
};

// When including either android_browser_test.h or in_process_browser_test.h
// depending on the platform, use this type alias as the test base class.
using PlatformBrowserTest = AndroidBrowserTest;

#endif  // CHROME_TEST_BASE_ANDROID_ANDROID_BROWSER_TEST_H_
