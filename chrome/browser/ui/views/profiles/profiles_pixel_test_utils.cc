// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profiles_pixel_test_utils.h"
#include <memory>
#include "base/command_line.h"
#include "base/scoped_environment_variable_override.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_features.h"
#include "chrome/common/chrome_features.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches.h"

namespace {
AccountInfo FillAccountInfo(const CoreAccountInfo& core_info,
                            AccountManagementStatus management_status) {
  const char kHostedDomain[] = "example.com";
  AccountInfo account_info;

  account_info.email = core_info.email;
  account_info.gaia = core_info.gaia;
  account_info.account_id = core_info.account_id;
  account_info.is_under_advanced_protection =
      core_info.is_under_advanced_protection;
  account_info.full_name = "Test Full Name";
  account_info.given_name = "Joe";
  account_info.hosted_domain =
      management_status == AccountManagementStatus::kManaged
          ? kHostedDomain
          : kNoHostedDomainFound;
  account_info.locale = "en";
  account_info.picture_url = "https://example.com";
  return account_info;
}
}  // namespace

AccountInfo SignInWithPrimaryAccount(
    Profile* profile,
    AccountManagementStatus management_status) {
  DCHECK(profile);

  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  auto core_account_info = signin::MakePrimaryAccountAvailable(
      identity_manager,
      management_status == AccountManagementStatus::kManaged
          ? "joe.consumer@example.com"
          : "joe.consumer@gmail.com",
      signin::ConsentLevel::kSignin);
  auto account_info = FillAccountInfo(core_account_info, management_status);
  signin::UpdateAccountInfoForAccount(identity_manager, account_info);

  return account_info;
}

void SetUpPixelTestCommandLine(
    const PixelTestParam& params,
    std::unique_ptr<base::ScopedEnvironmentVariableOverride>& env_variables,
    base::CommandLine* command_line) {
  if (params.use_dark_theme) {
    command_line->AppendSwitch(switches::kForceDarkMode);
  }
  if (params.use_right_to_left_language) {
    const std::string language = "ar-XB";
    command_line->AppendSwitchASCII(switches::kLang, language);

    // On Linux & Lacros the command line switch has no effect, we need to use
    // environment variables to change the language.
    env_variables = std::make_unique<base::ScopedEnvironmentVariableOverride>(
        "LANGUAGE", language);
  }
}

void InitPixelTestFeatures(
    const PixelTestParam& params,
    base::test::ScopedFeatureList& feature_list,
    std::vector<base::test::FeatureRef>& enabled_features,
    std::vector<base::test::FeatureRef>& disabled_features) {
  if (params.use_dark_theme) {
    enabled_features.push_back(features::kWebUIDarkMode);
  }
  if (params.use_chrome_refresh_2023_style) {
    enabled_features.push_back(features::kChromeRefresh2023);
    enabled_features.push_back(features::kChromeWebuiRefresh2023);
  }
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  if (params.use_fre_style) {
    enabled_features.push_back(kForYouFre);
  }
#endif
  feature_list.InitWithFeatures(enabled_features, disabled_features);
}
