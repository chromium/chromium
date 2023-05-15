// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/user_education/chrome_user_education_delegate.h"

#include <memory>

#include "ash/session/test_session_controller_client.h"
#include "ash/test/ash_test_helper.h"
#include "ash/user_education/user_education_class_properties.h"
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
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace {

// Element identifiers.
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kElementId);

}  // namespace

// ChromeUserEducationDelegateTest ---------------------------------------------

// Base class for tests of the `ChromeUserEducationDelegate`.
class ChromeUserEducationDelegateTest : public BrowserWithTestWindowTest {
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

// Tests -----------------------------------------------------------------------

// Verifies `CreateHelpBubble()` is working as intended.
TEST_F(ChromeUserEducationDelegateTest, CreateHelpBubble) {
  // Create and show a `widget`.
  views::Widget widget;
  views::Widget::InitParams params;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  widget.Init(std::move(params));
  widget.SetContentsView(std::make_unique<views::View>());
  widget.CenterWindow(gfx::Size(100, 100));
  widget.Show();

  // Cache `element_context` for the widget.
  const ui::ElementContext element_context =
      views::ElementTrackerViews::GetContextForWidget(&widget);

  // Verify that a help bubble is *not* created for the specified `kElementId`
  // and `element_context` pair since no tracked element matching that pair has
  // been registered with the element tracker framework.
  EXPECT_FALSE(delegate()->CreateHelpBubble(
      account_id(), ash::HelpBubbleId::kTest,
      user_education::HelpBubbleParams(), kElementId, element_context));

  // Register the `widget`s contents `view` with the element tracker framework.
  views::View* const view = widget.GetContentsView();
  view->SetProperty(ash::kHelpBubbleContextKey, ash::HelpBubbleContext::kAsh);
  view->SetProperty(views::kElementIdentifierKey, kElementId);

  // Verify that a help bubble *is* created for the specified `kElementId` and
  // `element_context` pair.
  EXPECT_TRUE(delegate()->CreateHelpBubble(
      account_id(), ash::HelpBubbleId::kTest,
      user_education::HelpBubbleParams(), kElementId, element_context));
}

// Verifies `RegisterTutorial()` registers a tutorial with the browser registry.
TEST_F(ChromeUserEducationDelegateTest, RegisterTutorial) {
  const ash::TutorialId tutorial_id = ash::TutorialId::kTest;
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
TEST_F(ChromeUserEducationDelegateTest, StartTutorial) {
  // Create a test element.
  const ui::ElementContext element_context(1);
  ui::test::TestElement test_element(kElementId, element_context);

  // Create a tutorial description.
  user_education::TutorialDescription tutorial_description;
  tutorial_description.steps.emplace_back(
      /*title_text_id_=*/0, /*body_text_id_=*/IDS_OK,
      ui::InteractionSequence::StepType::kShown, kElementId,
      /*element_name=*/std::string(), user_education::HelpBubbleArrow::kNone);

  // Register the tutorial.
  delegate()->RegisterTutorial(account_id(), ash::TutorialId::kTest,
                               std::move(tutorial_description));

  // Verify the tutorial is not running.
  user_education::TutorialService& tutorial_service =
      UserEducationServiceFactory::GetForProfile(profile())->tutorial_service();
  EXPECT_FALSE(tutorial_service.IsRunningTutorial());

  // Attempt to start the tutorial.
  delegate()->StartTutorial(account_id(), ash::TutorialId::kTest,
                            element_context,
                            /*completed_callback=*/base::DoNothing(),
                            /*aborted_callback=*/base::DoNothing());

  // Confirm the tutorial is running.
  EXPECT_TRUE(tutorial_service.IsRunningTutorial());
}
