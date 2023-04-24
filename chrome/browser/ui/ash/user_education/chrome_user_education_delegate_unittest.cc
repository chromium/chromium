// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/user_education/chrome_user_education_delegate.h"

#include <vector>

#include "ash/session/test_session_controller_client.h"
#include "ash/test/ash_test_helper.h"
#include "ash/user_education/user_education_types.h"
#include "ash/user_education/user_education_util.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ui/user_education/user_education_service.h"
#include "chrome/browser/ui/user_education/user_education_service_factory.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_education/common/help_bubble_params.h"
#include "components/user_education/common/tutorial_description.h"
#include "components/user_education/common/tutorial_registry.h"
#include "components/user_education/common/tutorial_service.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_test_util.h"
#include "ui/base/interaction/interaction_sequence.h"

namespace {

// Helpers ---------------------------------------------------------------------

std::vector<ash::TutorialId> GetTutorialIds() {
  std::vector<ash::TutorialId> tutorial_ids;
  for (size_t i = static_cast<size_t>(ash::TutorialId::kMinValue);
       i <= static_cast<size_t>(ash::TutorialId::kMaxValue); ++i) {
    tutorial_ids.emplace_back(static_cast<ash::TutorialId>(i));
  }
  return tutorial_ids;
}

}  // namespace

// ChromeUserEducationDelegateTest ---------------------------------------------

// Base class for tests of the `ChromeUserEducationDelegate` parameterized by
// user education tutorial ID.
class ChromeUserEducationDelegateTest
    : public BrowserWithTestWindowTest,
      public testing::WithParamInterface<ash::TutorialId> {
 public:
  ChromeUserEducationDelegateTest()
      : user_manager_(new ash::FakeChromeUserManager()),
        user_manager_enabler_(base::WrapUnique(user_manager_.get())) {}

  // Returns the `AccountId` for the primary `profile()`.
  const AccountId& account_id() const {
    return ash::BrowserContextHelper::Get()
        ->GetUserByBrowserContext(profile())
        ->GetAccountId();
  }

  // Returns a pointer to the `delegate_` instance under test.
  ash::UserEducationDelegate* delegate() { return delegate_.get(); }

  // Returns the tutorial ID associated with test parameterization.
  ash::TutorialId tutorial_id() const { return GetParam(); }

 private:
  // BrowserWithTestWindowTest:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    // Instantiate the `delegate_` after `BrowserWithTestWindowTest::SetUp()`
    // so that the browser process has fully initialized.
    delegate_ = std::make_unique<ChromeUserEducationDelegate>();
  }

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
  const raw_ptr<ash::FakeChromeUserManager, ExperimentalAsh> user_manager_;
  user_manager::ScopedUserManager user_manager_enabler_;

  // The delegate instance under test.
  std::unique_ptr<ChromeUserEducationDelegate> delegate_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         ChromeUserEducationDelegateTest,
                         testing::ValuesIn(GetTutorialIds()));

// Tests -----------------------------------------------------------------------

// Verifies `RegisterTutorial()` registers a tutorial with the browser registry.
TEST_P(ChromeUserEducationDelegateTest, RegisterTutorial) {
  const ash::TutorialId tutorial_id = this->tutorial_id();
  const auto tutorial_id_str = ash::user_education_util::ToString(tutorial_id);

  // Initially there should be no tutorial registered.
  user_education::TutorialRegistry& tutorial_registry =
      UserEducationServiceFactory::GetForProfile(profile())
          ->tutorial_registry();
  EXPECT_FALSE(tutorial_registry.IsTutorialRegistered(tutorial_id_str));

  // Attempt to register a tutorial.
  delegate()->RegisterTutorial(account_id(), tutorial_id,
                               user_education::TutorialDescription());

  // Confirm tutorial registration.
  EXPECT_TRUE(tutorial_registry.IsTutorialRegistered(tutorial_id_str));
}

// Verifies `StartTutorial()` starts a tutorial with the browser service.
TEST_P(ChromeUserEducationDelegateTest, StartTutorial) {
  const ash::TutorialId tutorial_id = this->tutorial_id();

  // Create a test element.
  const ui::ElementContext kElementContext(1);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kElementId);
  ui::test::TestElement test_element(kElementId, kElementContext);

  // Create a tutorial description.
  user_education::TutorialDescription tutorial_description;
  tutorial_description.steps.emplace_back(
      /*title_text_id_=*/0, /*body_text_id_=*/IDS_OK,
      ui::InteractionSequence::StepType::kShown, kElementId,
      /*element_name=*/std::string(), user_education::HelpBubbleArrow::kNone);

  // Register the tutorial.
  delegate()->RegisterTutorial(account_id(), tutorial_id,
                               std::move(tutorial_description));

  // Verify the tutorial is not running.
  user_education::TutorialService& tutorial_service =
      UserEducationServiceFactory::GetForProfile(profile())->tutorial_service();
  EXPECT_FALSE(tutorial_service.IsRunningTutorial());

  // Attempt to start the tutorial.
  delegate()->StartTutorial(account_id(), tutorial_id, kElementContext,
                            /*completed_callback=*/base::DoNothing(),
                            /*aborted_callback=*/base::DoNothing());

  // Confirm the tutorial is running.
  EXPECT_TRUE(tutorial_service.IsRunningTutorial());
}
