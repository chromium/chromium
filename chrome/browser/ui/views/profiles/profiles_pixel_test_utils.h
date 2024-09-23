// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILES_PIXEL_TEST_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILES_PIXEL_TEST_UTILS_H_

#include <memory>
#include <string>

#include "base/scoped_environment_variable_override.h"
#include "chrome/browser/signin/signin_browser_test_base.h"
#include "components/signin/public/identity_manager/tribool.h"

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
};

enum class AccountManagementStatus {
  kManaged = 0,
  kNonManaged,
};

// Returns an AccountInfo with all fields filled in, such that
// AccountInfo::IsValid() is true.
AccountInfo FillAccountInfo(
    const CoreAccountInfo& core_info,
    AccountManagementStatus management_status,
    signin::Tribool
        can_show_history_sync_opt_ins_without_minor_mode_restrictions);

// Used to create a dummy account and sign it in, by default as a primary
// account.
AccountInfo SignInWithAccount(
    signin::IdentityTestEnvironment& identity_test_env,
    AccountManagementStatus management_status =
        AccountManagementStatus::kNonManaged,
    std::optional<signin::ConsentLevel> consent_level =
        signin::ConsentLevel::kSignin,
    signin::Tribool
        can_show_history_sync_opt_ins_without_minor_mode_restrictions =
            signin::Tribool::kTrue);

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
// - providing helpers to set up the account, see `SignInWithAccount()`.
template <typename T>
class ProfilesPixelTestBaseT : public SigninBrowserTestBaseT<T> {
 public:
  template <typename... Args>
  explicit ProfilesPixelTestBaseT(const PixelTestParam& params, Args&&... args)
      : SigninBrowserTestBaseT<T>(std::forward<Args>(args)...),
        test_configuration_(params) {
    InitPixelTestFeatures(test_configuration_, scoped_feature_list_);
  }

  ~ProfilesPixelTestBaseT() override = default;

  // Used to create a dummy account and sign it in, by default as a primary
  // account.
  AccountInfo SignInWithAccount(
      AccountManagementStatus management_status =
          AccountManagementStatus::kNonManaged,
      std::optional<signin::ConsentLevel> consent_level =
          signin::ConsentLevel::kSignin,
      signin::Tribool
          can_show_history_sync_opt_ins_without_minor_mode_restrictions =
              signin::Tribool::kTrue) {
    return ::SignInWithAccount(
        *this->identity_test_env(), management_status, consent_level,
        can_show_history_sync_opt_ins_without_minor_mode_restrictions);
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
