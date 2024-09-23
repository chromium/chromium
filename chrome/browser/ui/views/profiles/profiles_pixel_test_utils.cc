// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profiles_pixel_test_utils.h"

#include <memory>

#include "base/command_line.h"
#include "base/scoped_environment_variable_override.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/chrome_features.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"

AccountInfo FillAccountInfo(
    const CoreAccountInfo& core_info,
    AccountManagementStatus management_status,
    signin::Tribool
        can_show_history_sync_opt_ins_without_minor_mode_restrictions) {
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

  if (can_show_history_sync_opt_ins_without_minor_mode_restrictions !=
      signin::Tribool::kUnknown) {
    AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
    mutator.set_can_show_history_sync_opt_ins_without_minor_mode_restrictions(
        signin::TriboolToBoolOrDie(
            can_show_history_sync_opt_ins_without_minor_mode_restrictions));
  }

  return account_info;
}

AccountInfo SignInWithAccount(
    signin::IdentityTestEnvironment& identity_test_env,
    AccountManagementStatus management_status,
    std::optional<signin::ConsentLevel> consent_level,
    signin::Tribool
        can_show_history_sync_opt_ins_without_minor_mode_restrictions) {
  auto* identity_manager = identity_test_env.identity_manager();

  const std::string email =
      management_status == AccountManagementStatus::kManaged
          ? "joe.consumer@example.com"
          : "joe.consumer@gmail.com";

  AccountInfo base_account_info = identity_test_env.MakeAccountAvailable(
      email,
      {.primary_account_consent_level = consent_level, .set_cookie = true});

  identity_test_env.UpdateAccountInfoForAccount(FillAccountInfo(
      base_account_info, management_status,
      can_show_history_sync_opt_ins_without_minor_mode_restrictions));

  // Set account image
  SimulateAccountImageFetch(identity_manager, base_account_info.account_id,
                            "GAIA_IMAGE_URL_WITH_SIZE",
                            gfx::Image(gfx::test::CreatePlatformImage()));

  AccountInfo account_info =
      identity_manager->FindExtendedAccountInfoByEmailAddress(email);
  CHECK_EQ(account_info.account_id, base_account_info.account_id);
  CHECK(account_info.IsValid());

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

void InitPixelTestFeatures(const PixelTestParam& params,
                           base::test::ScopedFeatureList& feature_list) {
  std::vector<base::test::FeatureRef> enabled_features;
  std::vector<base::test::FeatureRef> disabled_features;

  feature_list.InitWithFeatures(enabled_features, disabled_features);
}
