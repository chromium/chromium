// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/glanceables/glanceables_keyed_service.h"

#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/glanceables/glanceables_v2_controller.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

const char kPrimaryProfileName[] = "primary_profile@example.com";
const char kSecondaryProfileName[] = "secondary_profile@example.com";

}  // namespace

class GlanceablesKeyedServiceTest : public BrowserWithTestWindowTest {
 public:
  GlanceablesKeyedServiceTest()
      : scoped_user_manager_(std::make_unique<FakeChromeUserManager>()) {}

  TestingProfile* CreateProfile() override {
    const auto account_id = AccountId::FromUserEmail(kPrimaryProfileName);
    fake_chrome_user_manager()->AddUser(account_id);
    fake_chrome_user_manager()->LoginUser(account_id);
    session_controller_client()->AddUserSession(kPrimaryProfileName);
    session_controller_client()->SwitchActiveUser(account_id);
    return profile_manager()->CreateTestingProfile(kPrimaryProfileName,
                                                   /*is_main_profile=*/true);
  }

  FakeChromeUserManager* fake_chrome_user_manager() {
    return static_cast<FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }

  TestSessionControllerClient* session_controller_client() {
    return ash_test_helper()->test_session_controller_client();
  }

 protected:
  base::test::ScopedFeatureList feature_list_{features::kGlanceablesV2};
  user_manager::ScopedUserManager scoped_user_manager_;
};

TEST_F(GlanceablesKeyedServiceTest, RegistersClientsInAsh) {
  auto* const controller = Shell::Get()->glanceables_v2_controller();
  EXPECT_FALSE(controller->GetTasksClient());

  auto service = std::make_unique<GlanceablesKeyedService>(profile());
  EXPECT_TRUE(controller->GetTasksClient());

  service->Shutdown();
  EXPECT_FALSE(controller->GetTasksClient());
}

TEST_F(GlanceablesKeyedServiceTest,
       DoesNotRegisterClientsInAshForNonPrimaryUser) {
  auto* const controller = Shell::Get()->glanceables_v2_controller();
  auto service = std::make_unique<GlanceablesKeyedService>(profile());
  EXPECT_TRUE(controller->GetTasksClient());

  const auto first_account_id = AccountId::FromUserEmail(kPrimaryProfileName);
  const auto second_account_id =
      AccountId::FromUserEmail(kSecondaryProfileName);
  fake_chrome_user_manager()->AddUser(second_account_id);
  fake_chrome_user_manager()->LoginUser(second_account_id);
  profile_manager()->CreateTestingProfile(kSecondaryProfileName,
                                          /*is_main_profile=*/false);
  session_controller_client()->AddUserSession(kSecondaryProfileName);

  session_controller_client()->SwitchActiveUser(second_account_id);
  EXPECT_FALSE(controller->GetTasksClient());

  session_controller_client()->SwitchActiveUser(first_account_id);
  EXPECT_TRUE(controller->GetTasksClient());
}

}  // namespace ash
