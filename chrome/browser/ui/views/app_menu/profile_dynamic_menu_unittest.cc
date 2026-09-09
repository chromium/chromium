// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/profile_dynamic_menu.h"

#include <memory>
#include <vector>

#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/views/app_menu/action_app_menu_manager.h"
#include "chrome/browser/ui/views/app_menu/action_app_menu_test_base.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/actions/actions.h"
#include "ui/base/test/mock_base_window.h"
#include "ui/color/color_id.h"
#include "ui/views/widget/widget.h"

class ProfileDynamicMenuTest : public ActionAppMenuTestBase {
 public:
  ProfileDynamicMenuTest() = default;
  ~ProfileDynamicMenuTest() override = default;

  void SetUp() override {
    ActionAppMenuTestBase::SetUp();

    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    ON_CALL(mock_base_window_, GetNativeWindow()).WillByDefault([this]() {
      return widget_->GetNativeWindow();
    });
    ON_CALL(mock_window_interface_, GetWindow())
        .WillByDefault(testing::Return(&mock_base_window_));

    auto register_action = [this](actions::ActionId action_id,
                                  std::u16string text) {
      root_action_->AddChild(actions::ActionItem::Builder()
                                 .SetActionId(action_id)
                                 .SetText(text)
                                 .SetEnabled(true)
                                 .SetVisible(true)
                                 .Build());
    };

    // Submenu entries
    register_action(kActionManageGoogleAccount, u"Manage Google Account");
    register_action(kActionCustomizeChrome, u"Customize Chrome");
    register_action(kActionCloseProfile, u"Close Profile");
    register_action(kActionShowSyncSettings, u"Sync Settings");
    register_action(kActionShowSyncPassphraseDialog, u"Passphrase Dialog");
    register_action(kActionTurnOnSync, u"Turn On Sync");
    register_action(kActionShowSigninWhenPaused, u"Sign in when paused");
    register_action(kActionOpenGuestProfile, u"Open Guest Profile");
    register_action(kActionAddNewProfile, u"Add New Profile");
    register_action(kActionManageChromeProfiles, u"Manage Profiles");
    register_action(kActionShowSignin, u"Sign in");
    register_action(kActionUpgradeDialog, u"Upgrade Dialog");
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
  std::unique_ptr<views::Widget> widget_;
  testing::NiceMock<ui::MockBaseWindow> mock_base_window_;
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
