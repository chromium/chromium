// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_PROFILE_TEST_HELPER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_PROFILE_TEST_HELPER_H_

#include <string>

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/test/with_crosapi_param.h"
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
  typedef web_app::test::CrosapiParam CrosapiParam;

  TestProfileType profile_type;
  CrosapiParam crosapi_state = CrosapiParam::kDisabled;
};

// GTest string formatter for TestProfileType. Appends, e.g. "/Guest" to the end
// of test names.
std::string TestProfileTypeToString(
    const ::testing::TestParamInfo<TestProfileParam>& param);

// Adds the necessary flags to |command_line| to start a browser test in guest
// mode. Should be invoked in SetUpCommandLine(). Any test can call this: it is
// not coupled to TestProfileTypeMixin. Should only be invoked on ChromeOS.
void ConfigureCommandLineForGuestMode(base::CommandLine* command_line);

void InitCrosapiFeaturesForParam(
    web_app::test::CrosapiParam crosapi_state,
    base::test::ScopedFeatureList* scoped_feature_list);

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
      : T(std::forward<Args>(args)...) {
    InitCrosapiFeaturesForParam(GetParam().crosapi_state,
                                &scoped_feature_list_);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    if (profile_type() == TestProfileType::kGuest) {
      ConfigureCommandLineForGuestMode(command_line);
    } else if (profile_type() == TestProfileType::kIncognito) {
      command_line->AppendSwitch(::switches::kIncognito);
    }
    T::SetUpCommandLine(command_line);
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void SetUpOnMainThread() override {
    T::SetUpOnMainThread();
    if (T::browser() == nullptr) {
      // Create a new Ash browser window so test code using browser() can work
      // even when Lacros is the only browser.
      // TODO(crbug.com/1450158): Remove uses of browser() from such tests.
      chrome::NewEmptyWindow(ProfileManager::GetActiveUserProfile());
      T::SelectFirstBrowser();
    }
    ASSERT_EQ(GetParam().crosapi_state == web_app::test::CrosapiParam::kEnabled,
              crosapi::browser_util::IsLacrosEnabled());
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  TestProfileType profile_type() const { return GetParam().profile_type; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

#define INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_P(SUITE, PARAMS) \
  INSTANTIATE_TEST_SUITE_P(All, SUITE, PARAMS, TestProfileTypeToString)

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Instantiates 6 versions of each test in |SUITE| to ensure coverage of
// Guest and Incognito profiles, as well as regular profiles. This is currently
// only used on ChromeOS. Other platforms will likely need a differently defined
// macro because there is no such thing as Guest mode.
#define INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_ALL_PROFILE_TYPES_P(   \
    SUITE)                                                                   \
  INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_P(                           \
      SUITE, ::testing::Values(                                              \
                 TestProfileParam({TestProfileType::kRegular,                \
                                   web_app::test::CrosapiParam::kDisabled}), \
                 TestProfileParam({TestProfileType::kRegular,                \
                                   web_app::test::CrosapiParam::kEnabled}),  \
                 TestProfileParam({TestProfileType::kIncognito,              \
                                   web_app::test::CrosapiParam::kDisabled}), \
                 TestProfileParam({TestProfileType::kIncognito,              \
                                   web_app::test::CrosapiParam::kEnabled}),  \
                 TestProfileParam({TestProfileType::kGuest,                  \
                                   web_app::test::CrosapiParam::kDisabled}), \
                 TestProfileParam({TestProfileType::kGuest,                  \
                                   web_app::test::CrosapiParam::kEnabled})))

#define INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(SUITE) \
  INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_P(                             \
      SUITE, ::testing::Values(                                                \
                 TestProfileParam({TestProfileType::kRegular,                  \
                                   web_app::test::CrosapiParam::kDisabled}),   \
                 TestProfileParam({TestProfileType::kRegular,                  \
                                   web_app::test::CrosapiParam::kEnabled})))

#define INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_GUEST_SESSION_P(SUITE) \
  INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_P(                           \
      SUITE, ::testing::Values(                                              \
                 TestProfileParam({TestProfileType::kGuest,                  \
                                   web_app::test::CrosapiParam::kDisabled}), \
                 TestProfileParam({TestProfileType::kGuest,                  \
                                   web_app::test::CrosapiParam::kEnabled})))
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
