// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_PROFILE_TEST_HELPER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_PROFILE_TEST_HELPER_H_

#include <string>

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

// Profile type to test. Provided to subclasses of TestProfileTypeMixin via
// GetParam().
enum class TestProfileType {
  kRegular,
  kIncognito,
  kGuest,
};

// GTest string formatter for TestProfileType. Appends, e.g. "/Guest" to the end
// of test names.
std::string TestProfileTypeToString(
    const ::testing::TestParamInfo<TestProfileType>& param);

// Adds the necessary flags to |command_line| to start a browser test in guest
// mode. Should be invoked in SetUpCommandLine(). Any test can call this: it is
// not coupled to TestProfileTypeMixin. Should only be invoked on ChromeOS.
void ConfigureCommandLineForGuestMode(base::CommandLine* command_line);

// "Mixin" for configuring a test harness to parameterize on different profile
// types. To use it, inherit from
//     : public TestProfileTypeMixin<BaseBrowserTest>
// rather than BaseBrowserTest (e.g. a descendant of InProcessBrowserTest).
// Then choose the profile types to test against. E.g.,
//
// INSTANTIATE_TEST_SUITE_P(All,
//                          MySubclassOfTestProfileTypeMixin,
//                          ::testing::Values(TestProfileType::kRegular,
//                                            TestProfileType::kIncognito,
//                                            TestProfileType::kGuest),
//                          TestProfileTypeToString);
//
// Remember to use IN_PROC_BROWSER_TEST_P (not _F).
template <class T>
class TestProfileTypeMixin
    : public T,
      public ::testing::WithParamInterface<TestProfileType> {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    if (GetParam() == TestProfileType::kGuest) {
      ConfigureCommandLineForGuestMode(command_line);
    } else if (GetParam() == TestProfileType::kIncognito) {
      command_line->AppendSwitch(::switches::kIncognito);
    }
    T::SetUpCommandLine(command_line);
  }
};

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_PROFILE_TEST_HELPER_H_
