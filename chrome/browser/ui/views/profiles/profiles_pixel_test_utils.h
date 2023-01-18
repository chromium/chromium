// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILES_PIXEL_TEST_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILES_PIXEL_TEST_UTILS_H_

#include <memory>
#include <string>

namespace base {
class CommandLine;
class ScopedEnvironmentVariableOverride;

namespace test {
class FeatureRef;
class ScopedFeatureList;
}  // namespace test
}  // namespace base

struct AccountInfo;
class Profile;

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
};

enum class AccountManagementStatus {
  kManaged = 0,
  kNonManaged,
};

// Used to create a dummy account and sign it it as a primary account.
AccountInfo SignInWithPrimaryAccount(Profile* profile,
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
void InitPixelTestFeatures(
    const PixelTestParam& params,
    base::test::ScopedFeatureList& feature_list,
    std::vector<base::test::FeatureRef>& enabled_features,
    std::vector<base::test::FeatureRef>& disabled_features);

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILES_PIXEL_TEST_UTILS_H_
