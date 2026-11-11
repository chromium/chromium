// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/settings_utils.h"

#include "build/build_config.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_manager.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/test/scoped_feature_list.h"
#include "device/fido/public/features.h"
#include "device/fido/win/fake_webauthn_api.h"
#include "device/fido/win/webauthn_api.h"
#endif

namespace settings_utils {

#if BUILDFLAG(IS_CHROMEOS)
TEST(SettingsUtilsTest, IsGuest) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  EXPECT_FALSE(IsGuest(&profile));

  auto* user_manager = static_cast<ash::FakeChromeUserManager*>(
      user_manager::UserManager::Get());
  user_manager->LoginUser(user_manager->AddGuestUser()->GetAccountId());
  EXPECT_TRUE(IsGuest(&profile));
}

TEST(SettingsUtilsTest, IsGuestForManagedGuestSession) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  auto* user_manager = static_cast<ash::FakeChromeUserManager*>(
      user_manager::UserManager::Get());

  const AccountId account_id = AccountId::FromUserEmail("mgs@example.com");
  user_manager->AddPublicAccountUser(account_id);
  user_manager->LoginUser(account_id);
  EXPECT_TRUE(IsGuest(&profile));
}
#else
TEST(SettingsUtilsTest, IsGuest) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  EXPECT_FALSE(IsGuest(&profile));

  profile.SetGuestSession(true);
  EXPECT_TRUE(IsGuest(&profile));
}
#endif

#if BUILDFLAG(IS_WIN)
// The native Windows WebAuthn UI replaces the subpage when that API is
// available and enabled.
TEST(SettingsUtilsTest, IsSecurityKeysSubpageEnabled) {
  base::test::ScopedFeatureList feature_list{device::kWebAuthUseNativeWinApi};
  device::FakeWinWebAuthnApi fake_api;
  device::WinWebAuthnApi::ScopedOverride api_override(&fake_api);

  fake_api.set_available(/*available=*/false);
  EXPECT_TRUE(IsSecurityKeysSubpageEnabled());

  fake_api.set_available(/*available=*/true);
  EXPECT_FALSE(IsSecurityKeysSubpageEnabled());
}

TEST(SettingsUtilsTest, IsSecurityKeysSubpageEnabledWithoutNativeWinApi) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(device::kWebAuthUseNativeWinApi);
  device::FakeWinWebAuthnApi fake_api;
  device::WinWebAuthnApi::ScopedOverride api_override(&fake_api);

  fake_api.set_available(/*available=*/false);
  EXPECT_TRUE(IsSecurityKeysSubpageEnabled());

  fake_api.set_available(/*available=*/true);
  EXPECT_TRUE(IsSecurityKeysSubpageEnabled());
}
#else
TEST(SettingsUtilsTest, IsSecurityKeysSubpageEnabled) {
  EXPECT_TRUE(IsSecurityKeysSubpageEnabled());
}
#endif

TEST(SettingsUtilsTest, FixupAndValidateStartupPage) {
  EXPECT_FALSE(FixupAndValidateStartupPage(std::string(), nullptr));
  EXPECT_FALSE(FixupAndValidateStartupPage("   ", nullptr));
  EXPECT_FALSE(FixupAndValidateStartupPage("^&*@)^)", nullptr));
  EXPECT_FALSE(FixupAndValidateStartupPage("chrome://quit", nullptr));

  EXPECT_TRUE(FixupAndValidateStartupPage("facebook.com", nullptr));
  EXPECT_TRUE(FixupAndValidateStartupPage("http://reddit.com", nullptr));
  EXPECT_TRUE(FixupAndValidateStartupPage("https://google.com", nullptr));
  EXPECT_TRUE(FixupAndValidateStartupPage("chrome://apps", nullptr));

  GURL fixed_url;
  EXPECT_TRUE(FixupAndValidateStartupPage("about:settings", &fixed_url));
  EXPECT_EQ("chrome://settings/", fixed_url.spec());
}

}  // namespace settings_utils
