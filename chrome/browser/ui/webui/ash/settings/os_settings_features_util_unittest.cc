// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/os_settings_features_util.h"

#include <memory>

#include "base/check_deref.h"
#include "chrome/browser/ash/login/test/chrome_user_session_test_environment_delegate.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "components/account_id/account_id_literal.h"
#include "components/session_manager/test/user_session_test_environment.h"
#include "components/user_manager/user.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::settings {

namespace {

constexpr auto kAccountId =
    AccountId::Literal::FromUserEmailGaiaId("test-user@testdomain.com",
                                            GaiaId::Literal("1234567890"));
constexpr auto kSecondaryAccountId =
    AccountId::Literal::FromUserEmailGaiaId("secondary-user@testdomain.com",
                                            GaiaId::Literal("2345678901"));

}  // namespace

class OsSettingsFeaturesUtilTest : public testing::Test {
 public:
  OsSettingsFeaturesUtilTest() = default;
  ~OsSettingsFeaturesUtilTest() override = default;

  void SetUp() override {
    auto* browser_process = TestingBrowserProcess::GetGlobal();
    user_session_test_environment_ =
        std::make_unique<test::UserSessionTestEnvironment>(
            browser_process->local_state(),
            std::make_unique<test::ChromeUserSessionTestEnvironmentDelegate>(
                browser_process));
  }

  void TearDown() override { user_session_test_environment_.reset(); }

  test::UserSessionTestEnvironment& user_session_test_environment() {
    return *user_session_test_environment_;
  }

  StubInstallAttributes& stub_install_attributes() {
    return CHECK_DEREF(cros_settings_test_helper_.InstallAttributes());
  }

  Profile* GetProfile(const AccountId& account_id) {
    return Profile::FromBrowserContext(
        BrowserContextHelper::Get()->GetBrowserContextByAccountId(account_id));
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  ScopedCrosSettingsTestHelper cros_settings_test_helper_;
  std::unique_ptr<test::UserSessionTestEnvironment>
      user_session_test_environment_;
};

TEST_F(OsSettingsFeaturesUtilTest, PowerwashAllowedForRegularUser) {
  const user_manager::User* user =
      user_session_test_environment().AddRegularUser(kAccountId);
  ASSERT_TRUE(user);
  user_session_test_environment().LogIn(kAccountId);

  EXPECT_TRUE(IsPowerwashAllowed(*user));
}

TEST_F(OsSettingsFeaturesUtilTest, PowerwashDisallowedForGuestUser) {
  const user_manager::User* user =
      user_session_test_environment().AddGuestUser();
  ASSERT_TRUE(user);
  user_session_test_environment().LogIn(user->GetAccountId());

  EXPECT_FALSE(IsPowerwashAllowed(*user));
}

TEST_F(OsSettingsFeaturesUtilTest, PowerwashDisallowedForChildUser) {
  const user_manager::User* user =
      user_session_test_environment().AddChildUser(kAccountId);
  ASSERT_TRUE(user);
  user_session_test_environment().LogIn(kAccountId);

  EXPECT_FALSE(IsPowerwashAllowed(*user));
}

TEST_F(OsSettingsFeaturesUtilTest, PowerwashDisallowedForManagedUser) {
  stub_install_attributes().SetCloudManaged("fake-managed.com", "device-id");

  const user_manager::User* user =
      user_session_test_environment().AddRegularUser(kAccountId);
  ASSERT_TRUE(user);
  user_session_test_environment().LogIn(kAccountId);

  EXPECT_FALSE(IsPowerwashAllowed(*user));
}

TEST_F(OsSettingsFeaturesUtilTest, ParentalControlSettingsShownForPrimaryUser) {
  ASSERT_TRUE(user_session_test_environment().AddRegularUser(kAccountId));
  user_session_test_environment().LogIn(kAccountId);

  EXPECT_TRUE(ShouldShowParentalControlSettings(GetProfile(kAccountId)));
}

TEST_F(OsSettingsFeaturesUtilTest,
       ParentalControlSettingsHiddenForSecondaryUser) {
  ASSERT_TRUE(user_session_test_environment().AddRegularUser(kAccountId));
  ASSERT_TRUE(
      user_session_test_environment().AddRegularUser(kSecondaryAccountId));
  user_session_test_environment().LogIn(kAccountId);
  user_session_test_environment().LogIn(kSecondaryAccountId);

  EXPECT_FALSE(
      ShouldShowParentalControlSettings(GetProfile(kSecondaryAccountId)));
}

}  // namespace ash::settings
