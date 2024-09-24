// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_PROFILE_TEST_HELPER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_PROFILE_TEST_HELPER_H_

#include <string>

#include "base/command_line.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_commands.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Profile type to test. Provided to subclasses of TestProfileTypeMixin via
// get_profile().
enum class TestProfileType {
  kRegular,
  kIncognito,
  kGuest,
};

// Profile type to test. Provided to subclasses of TestProfileTypeMixin via
// GetParam().
struct TestProfileParam {
  TestProfileType profile_type;
};

// GTest string formatter for TestProfileType. Appends, e.g. "/Guest" to the end
// of test names.
std::string TestProfileTypeToString(
    const ::testing::TestParamInfo<TestProfileParam>& param);

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
      public ::testing::WithParamInterface<TestProfileParam> {
 public:
  template <class... Args>
  explicit TestProfileTypeMixin(Args&&... args)
      : T(std::forward<Args>(args)...) {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    if (profile_type() == TestProfileType::kGuest) {
      ConfigureCommandLineForGuestMode(command_line);
    } else if (profile_type() == TestProfileType::kIncognito) {
      command_line->AppendSwitch(::switches::kIncognito);
    }
    T::SetUpCommandLine(command_line);
  }

  TestProfileType profile_type() const { return GetParam().profile_type; }
};

#define INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_P(SUITE, PARAMS) \
  INSTANTIATE_TEST_SUITE_P(All, SUITE, PARAMS, TestProfileTypeToString)

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Instantiates 3 versions of each test in |SUITE| to ensure coverage of
// Guest and Incognito profiles, as well as regular profiles. This is currently
// only used on ChromeOS. Other platforms will likely need a differently defined
// macro because there is no such thing as Guest mode.
#define INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_ALL_PROFILE_TYPES_P( \
    SUITE)                                                                 \
  INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_P(                         \
      SUITE,                                                               \
      ::testing::Values(TestProfileParam({TestProfileType::kRegular}),     \
                        TestProfileParam({TestProfileType::kIncognito}),   \
                        TestProfileParam({TestProfileType::kGuest})))

#define INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(SUITE) \
  INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_P(                             \
      SUITE, ::testing::Values(TestProfileParam({TestProfileType::kRegular})))

#define INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_GUEST_SESSION_P(SUITE) \
  INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_P(                           \
      SUITE, ::testing::Values(TestProfileParam({TestProfileType::kGuest})))
#else
// Instantiates 3 versions of each test in |SUITE| to ensure coverage of
// Guest and Incognito profiles, as well as regular profiles. This is currently
// only used on ChromeOS. Other platforms will likely need a differently defined
// macro because there is no such thing as Guest mode.
#define INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_ALL_PROFILE_TYPES_P( \
    SUITE)                                                                 \
  INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_P(                         \
      SUITE,                                                               \
      ::testing::Values(TestProfileParam({TestProfileType::kRegular}),     \
                        TestProfileParam({TestProfileType::kIncognito}),   \
                        TestProfileParam({TestProfileType::kGuest})))

#define INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(SUITE) \
  INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_P(                             \
      SUITE, ::testing::Values(TestProfileParam({TestProfileType::kRegular})))

#define INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_GUEST_SESSION_P(SUITE) \
  INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_P(                           \
      SUITE, ::testing::Values(TestProfileParam({TestProfileType::kGuest})))
#endif

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_PROFILE_TEST_HELPER_H_
