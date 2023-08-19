// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILES_PIXEL_TEST_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILES_PIXEL_TEST_UTILS_H_

#include <memory>
#include <string>

#include "base/scoped_environment_variable_override.h"
#include "chrome/browser/signin/signin_browser_test_base.h"

namespace base {
class CommandLine;

namespace test {
class ScopedFeatureList;
}  // namespace test
}  // namespace base

namespace signin {
class IdentityTestEnvironment;
}

struct AccountInfo;

// Parameters that are used for most of the pixel tests. These params
// will be used to create combinations with the test name as `test_suffix` and
// will be passed to the parametrised test as the second argument of
// `INSTANTIATE_TEST_SUITE_P`.
struct PixelTestParam {
  std::string test_suffix = "";
  bool use_dark_theme = false;
  bool use_right_to_left_language = false;
  bool use_small_window = false;
  bool use_fre_style = false;
  bool use_chrome_refresh_2023_style = false;
};

enum class AccountManagementStatus {
  kManaged = 0,
  kNonManaged,
};

// Used to create a dummy account and sign it it as a primary account.
AccountInfo SignInWithPrimaryAccount(
    signin::IdentityTestEnvironment& identity_test_env,
    AccountManagementStatus management_status =
        AccountManagementStatus::kNonManaged);

// Sets up the parameters that are passed to the command line. For example,
// to enable dark mode, we need to pass `kForceDarkMode` to the command line.
// This function should be called inside the `SetUpCommandLine` function.
void SetUpPixelTestCommandLine(
    const PixelTestParam& params,
    std::unique_ptr<base::ScopedEnvironmentVariableOverride>& env_variables,
    base::CommandLine* command_line);

// Enables and disables the features that we need for the test. This function
// will automatically add dark mode and the first run experience feature when
// used.
void InitPixelTestFeatures(const PixelTestParam& params,
                           base::test::ScopedFeatureList& feature_list);

// Base class for pixel tests for profiles-related features.
//
// Reduces test boilerplate by:
// - applying the configuration from `PixelTestParam` passed via the
//   constructor, removing the need to call `SetUpPixelTestCommandLine()` and
//   `InitPixelTestFeatures()`.
// - providing helpers to set up the primary account, see
//   `SignInWithPrimaryAccount()`.
template <typename T,
          typename =
              std::enable_if_t<std::is_base_of_v<InProcessBrowserTest, T>>>
class ProfilesPixelTestBaseT : public SigninBrowserTestBaseT<T> {
 public:
  template <typename... Args>
  explicit ProfilesPixelTestBaseT(const PixelTestParam& params, Args&&... args)
      : SigninBrowserTestBaseT<T>(std::forward<Args>(args)...),
        test_configuration_(params) {
    InitPixelTestFeatures(test_configuration_, scoped_feature_list_);
  }

  ~ProfilesPixelTestBaseT() override = default;

  // Used to create a dummy account and sign it it as a primary account.
  AccountInfo SignInWithPrimaryAccount(
      AccountManagementStatus management_status =
          AccountManagementStatus::kNonManaged) {
    return ::SignInWithPrimaryAccount(*this->identity_test_env(),
                                      management_status);
  }

  // SigninBrowserTestBaseT overrides:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    SigninBrowserTestBaseT<T>::SetUpCommandLine(command_line);
    SetUpPixelTestCommandLine(test_configuration_, scoped_env_override_,
                              command_line);
  }

 private:
  const PixelTestParam test_configuration_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<base::ScopedEnvironmentVariableOverride> scoped_env_override_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILES_PIXEL_TEST_UTILS_H_
