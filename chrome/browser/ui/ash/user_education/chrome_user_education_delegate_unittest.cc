// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/user_education/chrome_user_education_delegate.h"

#include "ash/session/test_session_controller_client.h"
#include "ash/test/ash_test_helper.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ui/user_education/user_education_service.h"
#include "chrome/browser/ui/user_education/user_education_service_factory.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/user_education/common/tutorial_description.h"
#include "components/user_education/common/tutorial_registry.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user.h"
#include "testing/gtest/include/gtest/gtest.h"

// ChromeUserEducationDelegateTest ---------------------------------------------

// Base class for tests of the `ChromeUserEducationDelegate`.
class ChromeUserEducationDelegateTest : public BrowserWithTestWindowTest {
 public:
  ChromeUserEducationDelegateTest()
      : user_manager_(new ash::FakeChromeUserManager()),
        user_manager_enabler_(base::WrapUnique(user_manager_)) {}

  // Returns a pointer to the `delegate_` instance under test.
  ash::UserEducationDelegate* delegate() { return &delegate_; }

 private:
  // BrowserWithTestWindowTest:
  TestingProfile* CreateProfile() override {
    constexpr char kUserEmail[] = "user@test";
    const AccountId kUserAccountId(AccountId::FromUserEmail(kUserEmail));

    // Register user.
    user_manager_->AddUser(kUserAccountId);
    user_manager_->LoginUser(kUserAccountId);

    // Activate session.
    auto* client = ash_test_helper()->test_session_controller_client();
    client->AddUserSession(kUserEmail);
    client->SwitchActiveUser(kUserAccountId);

    // Create profile.
    return profile_manager()->CreateTestingProfile(kUserEmail);
  }

  // User management.
  ash::FakeChromeUserManager* const user_manager_;
  user_manager::ScopedUserManager user_manager_enabler_;

  // The delegate instance under test.
  ChromeUserEducationDelegate delegate_;
};

// Tests -----------------------------------------------------------------------

// Verifies `RegisterTutorial()` registers a tutorial with the browser registry.
TEST_F(ChromeUserEducationDelegateTest, RegisterTutorial) {
  constexpr char kTutorialId[] = "Tutorial ID";

  // Initially there should be no tutorial registered.
  user_education::TutorialRegistry& tutorial_registry =
      UserEducationServiceFactory::GetForProfile(profile())
          ->tutorial_registry();
  EXPECT_FALSE(tutorial_registry.IsTutorialRegistered(kTutorialId));

  // Attempt to register a tutorial.
  delegate()->RegisterTutorial(ash::BrowserContextHelper::Get()
                                   ->GetUserByBrowserContext(profile())
                                   ->GetAccountId(),
                               kTutorialId,
                               user_education::TutorialDescription());

  // Confirm tutorial registration.
  EXPECT_TRUE(tutorial_registry.IsTutorialRegistered(kTutorialId));
}
