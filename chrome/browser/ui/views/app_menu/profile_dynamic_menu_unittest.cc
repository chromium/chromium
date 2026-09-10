// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/profile_dynamic_menu.h"

#include <memory>
#include <vector>

#include "build/build_config.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/views/app_menu/action_app_menu_manager.h"
#include "chrome/browser/ui/views/app_menu/action_app_menu_test_base.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/actions/actions.h"
#include "ui/base/test/mock_base_window.h"
#include "ui/color/color_id.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/views/widget/widget.h"

class ProfileDynamicMenuTest : public ActionAppMenuTestBase {
 public:
  ProfileDynamicMenuTest() = default;
  ~ProfileDynamicMenuTest() override = default;

  void SetUp() override {
    ActionAppMenuTestBase::SetUp();

    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    EXPECT_CALL(mock_base_window_, GetNativeWindow())
        .WillRepeatedly(testing::Return(widget_->GetNativeWindow()));
    EXPECT_CALL(mock_window_interface_, GetWindow())
        .WillRepeatedly(testing::Return(&mock_base_window_));

    auto add_action = [this](actions::ActionId action_id, std::u16string text) {
      root_action_->AddChild(
          actions::ActionItem::Builder(
              base::BindRepeating(&MockActionCallback::Call,
                                  base::Unretained(&mock_action_invoked_),
                                  action_id))
              .SetActionId(action_id)
              .SetText(text)
              .SetEnabled(true)
              .SetVisible(true)
              .Build());
    };

    add_action(kActionCustomizeChrome, u"Customize Chrome");
    add_action(kActionCloseProfile, u"Close profile");
    add_action(kActionShowSyncSettings, u"Sync settings");
    add_action(kActionShowSyncPassphraseDialog, u"Enter passphrase");
    add_action(kActionTurnOnSync, u"Turn on sync");
    add_action(kActionShowSigninWhenPaused, u"Sign in again");
    add_action(kActionOpenGuestProfile, u"Open Guest profile");
    add_action(kActionAddNewProfile, u"Add new profile");
    add_action(kActionManageChromeProfiles, u"Manage Chrome profiles");
    add_action(kActionShowSignin, u"Sign in to Chrome");
    add_action(kActionUpgradeDialog, u"Update Chrome");
  }

  void TearDown() override {
    widget_.reset();
    ActionAppMenuTestBase::TearDown();
  }

  actions::BaseAction* FindChildAction(actions::ActionItem* parent,
                                       actions::ActionId action_id) {
    if (!parent) {
      return nullptr;
    }
    for (const auto& child : parent->GetChildren().children()) {
      if (auto* action_item = child->GetActionItem()) {
        if (action_item->GetActionId() == action_id) {
          return child.get();
        }
      }
    }
    return nullptr;
  }

 protected:
  testing::NiceMock<ui::MockBaseWindow> mock_base_window_;
  std::unique_ptr<views::Widget> widget_;
};

TEST_F(ProfileDynamicMenuTest, BuildProfileActions_StandardProfile) {
  ProfileDynamicMenu menu(&mock_window_interface_);
  auto parent_item = actions::ActionItem::Builder().Build();

  menu.BuildProfileActions(parent_item.get());

  EXPECT_FALSE(parent_item->GetChildren().children().empty());

  actions::BaseAction* close_profile =
      FindChildAction(parent_item.get(), kActionCloseProfile);
  ASSERT_NE(close_profile, nullptr);
  EXPECT_EQ(close_profile->GetActionItem()->GetProperty(
                ActionAppMenuManager::kContainerColorKey),
            ui::kColorMenuBackground);

  actions::BaseAction* customize_chrome =
      FindChildAction(parent_item.get(), kActionCustomizeChrome);
  ASSERT_NE(customize_chrome, nullptr);
  EXPECT_EQ(customize_chrome->GetActionItem()->GetProperty(
                ActionAppMenuManager::kContainerColorKey),
            ui::kColorMenuBackground);

  actions::BaseAction* manage_chrome_profiles =
      FindChildAction(parent_item.get(), kActionManageChromeProfiles);
  ASSERT_NE(manage_chrome_profiles, nullptr);
  EXPECT_EQ(manage_chrome_profiles->GetActionItem()->GetProperty(
                ActionAppMenuManager::kContainerColorKey),
            ui::kColorMenuBackground);
}

TEST_F(ProfileDynamicMenuTest, BuildProfileActions_MultipleCallsResetList) {
  ProfileDynamicMenu menu(&mock_window_interface_);
  auto parent_item = actions::ActionItem::Builder().Build();

  menu.BuildProfileActions(parent_item.get());
  size_t initial_count = parent_item->GetChildren().children().size();
  EXPECT_GT(initial_count, 0u);

  // Calling it again should not append duplicates
  menu.BuildProfileActions(parent_item.get());
  EXPECT_EQ(parent_item->GetChildren().children().size(), initial_count);
}

TEST_F(ProfileDynamicMenuTest, BuildProfileActions_GuestProfile) {
  TestingProfile::Builder guest_builder;
  guest_builder.SetGuestSession();
  std::unique_ptr<TestingProfile> guest_profile = guest_builder.Build();

  ON_CALL(mock_window_interface_, GetProfile())
      .WillByDefault(testing::Return(guest_profile.get()));

  ProfileDynamicMenu menu(&mock_window_interface_);
  auto parent_item = actions::ActionItem::Builder().Build();

  menu.BuildProfileActions(parent_item.get());

  EXPECT_NE(FindChildAction(parent_item.get(), kActionCloseProfile), nullptr);
  EXPECT_EQ(FindChildAction(parent_item.get(), kActionCustomizeChrome),
            nullptr);
  EXPECT_EQ(FindChildAction(parent_item.get(), kActionManageChromeProfiles),
            nullptr);
}

TEST_F(ProfileDynamicMenuTest, BuildProfileActions_IncognitoProfile) {
  Profile* incognito_profile =
      profile_->GetPrimaryOTRProfile(/*create_if_needed=*/true);

  ON_CALL(mock_window_interface_, GetProfile())
      .WillByDefault(testing::Return(incognito_profile));

  ProfileDynamicMenu menu(&mock_window_interface_);
  auto parent_item = actions::ActionItem::Builder().Build();

  menu.BuildProfileActions(parent_item.get());

  EXPECT_NE(FindChildAction(parent_item.get(), kActionCloseProfile), nullptr);
  EXPECT_EQ(FindChildAction(parent_item.get(), kActionCustomizeChrome),
            nullptr);
  EXPECT_EQ(FindChildAction(parent_item.get(), kActionManageChromeProfiles),
            nullptr);
}

#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(ProfileDynamicMenuTest, BuildProfileActions_SignedInProfile) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_.get());
  signin::MakePrimaryAccountAvailable(identity_manager, "test@example.com",
                                      signin::ConsentLevel::kSignin);

  ProfileDynamicMenu menu(&mock_window_interface_);
  auto parent_item = actions::ActionItem::Builder().Build();

  menu.BuildProfileActions(parent_item.get());

  // When signed in, Manage Google Account row should be populated.
  EXPECT_NE(FindChildAction(parent_item.get(), kActionManageGoogleAccount),
            nullptr);
}

TEST_F(ProfileDynamicMenuTest, BuildProfileActions_SyncError) {
  SyncServiceFactory::GetInstance()->SetTestingFactory(
      profile_.get(),
      base::BindRepeating(
          [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
            auto service = std::make_unique<syncer::TestSyncService>();
            service->SetPassphraseRequired();
            return service;
          }));

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_.get());
  signin::MakePrimaryAccountAvailable(identity_manager, "test@example.com",
                                      signin::ConsentLevel::kSignin);

  ProfileDynamicMenu menu(&mock_window_interface_);
  auto parent_item = actions::ActionItem::Builder().Build();

  menu.BuildProfileActions(parent_item.get());

  // When Sync has a passphrase error, the passphrase dialog action row should
  // be populated.
  EXPECT_NE(FindChildAction(parent_item.get(), kActionShowSyncPassphraseDialog),
            nullptr);
  EXPECT_NE(FindChildAction(parent_item.get(), kActionManageGoogleAccount),
            nullptr);
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

TEST_F(ProfileDynamicMenuTest, BuildProfileActions_MultipleProfiles) {
  TestingProfileManager profile_manager(TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(profile_manager.SetUp());

  TestingProfile* profile1 = profile_manager.CreateTestingProfile("Profile 1");
  profile_manager.CreateTestingProfile("Profile 2");

  ON_CALL(mock_window_interface_, GetProfile())
      .WillByDefault(testing::Return(profile1));

  ProfileDynamicMenu menu(&mock_window_interface_);
  auto parent_item = actions::ActionItem::Builder().Build();

  menu.BuildProfileActions(parent_item.get());

  // With multiple profiles managed by ProfileManager, Other Profiles section
  // and profile management footer actions should be populated.
  EXPECT_NE(FindChildAction(parent_item.get(), kActionAddNewProfile), nullptr);
  EXPECT_NE(FindChildAction(parent_item.get(), kActionManageChromeProfiles),
            nullptr);
}
