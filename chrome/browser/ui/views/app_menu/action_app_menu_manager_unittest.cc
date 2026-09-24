// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/action_app_menu_manager.h"

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/profiles/profile_view_utils.h"
#include "chrome/browser/ui/safety_hub/menu_notification_service.h"
#include "chrome/browser/ui/safety_hub/menu_notification_service_factory.h"
#include "chrome/browser/ui/safety_hub/safe_browsing_result.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/mock_vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/views/app_menu/action_app_menu_test_base.h"
#include "chrome/browser/ui/views/app_menu/app_menu_action_item.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/bookmarks/common/bookmark_bar_visibility_state.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/enterprise/isolated_mode/isolated_mode_features.h"
#include "components/enterprise/isolated_mode/prefs.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/test/test_sync_service.h"
#include "components/user_education/common/tutorial/tutorial_description.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

class ActionAppMenuManagerTest : public ActionAppMenuTestBase {
 protected:
  actions::ActionItem* GetBlockSection(actions::ActionItem* root) {
    for (const auto& child : root->GetChildren().children()) {
      if (child->GetActionItem()->GetProperty(
              AppMenuActionItem::kDisplayTypeKey) ==
          AppMenuActionItem::DisplayType::kBlock) {
        return child->GetActionItem();
      }
    }
    return nullptr;
  }

  actions::ActionItem* GetYourChromeSection(actions::ActionItem* root) {
    for (const auto& child : root->GetChildren().children()) {
      if (child->GetActionItem()->GetProperty(
              AppMenuActionItem::kContainerColorKey) ==
          kColorAppMenuYourChromeBackground) {
        return child->GetActionItem();
      }
    }
    return nullptr;
  }
};

TEST_F(ActionAppMenuManagerTest, ProxySyncsWithDelegateAndInvokes) {
  ActionAppMenuManager menu_manager(&mock_window_interface_);
  menu_manager.CreateMenuHierarchy();

  actions::ActionItem* delegate =
      actions::ActionManager::Get().FindAction(kActionShowPasswordManager);
  ASSERT_NE(delegate, nullptr);

  actions::ActionItem* root = menu_manager.GetAppMenuRoot();
  ASSERT_NE(root, nullptr);
  actions::ActionItem* your_chrome_section = GetYourChromeSection(root);
  ASSERT_NE(your_chrome_section, nullptr);
  actions::BaseAction* passwords_submenu = nullptr;
  for (const auto& child : your_chrome_section->GetChildren().children()) {
    if (child->GetActionItem()->GetActionId() ==
        kActionPasswordsAndAutofillSubmenu) {
      passwords_submenu = child.get();
      break;
    }
  }
  ASSERT_NE(passwords_submenu, nullptr);
  actions::ActionItem* passwords_proxy =
      passwords_submenu->GetChildren().children()[0]->GetActionItem();

  // Test dynamic synchronization.
  EXPECT_EQ(passwords_proxy->GetText(), u"Password Manager");
  delegate->SetText(u"Passwords Title");
  EXPECT_EQ(passwords_proxy->GetText(), u"Passwords Title");

  EXPECT_TRUE(passwords_proxy->GetEnabled());
  delegate->SetEnabled(false);
  EXPECT_FALSE(passwords_proxy->GetEnabled());

  // Re-enable the delegate so that InvokeAction() can execute the callback.
  delegate->SetEnabled(true);
  EXPECT_TRUE(passwords_proxy->GetEnabled());

  EXPECT_CALL(mock_action_invoked_,
              Call(kActionShowPasswordManager, testing::_, testing::_))
      .Times(1);
  passwords_proxy->InvokeAction();
  testing::Mock::VerifyAndClearExpectations(&mock_action_invoked_);
}

TEST_F(ActionAppMenuManagerTest, BlockActionsGuestSessionExcludesIncognito) {
  profile_->SetGuestSession(true);

  ActionAppMenuManager menu_manager(&mock_window_interface_);
  menu_manager.CreateMenuHierarchy();

  actions::ActionItem* root = menu_manager.GetAppMenuRoot();
  ASSERT_NE(root, nullptr);

  actions::ActionItem* block_section = GetBlockSection(root);
  ASSERT_NE(block_section, nullptr);

  // Only New Tab and New Window should be present for a guest session.
  EXPECT_EQ(block_section->GetChildren().children().size(), 2u);
}

// Profile submenu is disabled for ChromeOS
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_ProfileSubmenu DISABLED_ProfileSubmenu
#define MAYBE_ProfileSubmenuSingleProfile DISABLED_ProfileSubmenuSingleProfile
#else
#define MAYBE_ProfileSubmenu ProfileSubmenu
#define MAYBE_ProfileSubmenuSingleProfile ProfileSubmenuSingleProfile
#endif
TEST_F(ActionAppMenuManagerTest, MAYBE_ProfileSubmenu) {
  TestingProfileManager profile_manager(TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(profile_manager.SetUp());

  TestingProfile* profile1 = profile_manager.CreateTestingProfile("Profile 1");
  profile_manager.CreateTestingProfile("Profile 2");

  SafetyHubMenuNotificationServiceFactory::GetInstance()->SetTestingFactory(
      profile1, BrowserContextKeyedServiceFactory::TestingFactory());
  SyncServiceFactory::GetInstance()->SetTestingFactory(
      profile1,
      base::BindRepeating(
          [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
            return std::make_unique<syncer::TestSyncService>();
          }));

  ON_CALL(mock_window_interface_, GetProfile())
      .WillByDefault(testing::Return(profile1));

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile1);
  signin::MakePrimaryAccountAvailable(identity_manager, "test@example.com",
                                      signin::ConsentLevel::kSignin);

  ActionAppMenuManager menu_manager(&mock_window_interface_);
  menu_manager.CreateMenuHierarchy();

  actions::ActionItem* root = menu_manager.GetAppMenuRoot();
  ASSERT_NE(root, nullptr);

  actions::ActionItem* your_chrome_section = GetYourChromeSection(root);
  ASSERT_NE(your_chrome_section, nullptr);

  const auto& section_children = your_chrome_section->GetChildren().children();
  ASSERT_GE(section_children.size(), 3u);
  actions::BaseAction* profile_submenu = section_children[1].get();
  actions::BaseAction* item_after_profile = section_children[2].get();

  EXPECT_EQ(profile_submenu->GetActionItem()->GetActionId(),
            kActionProfileSubmenu);
  EXPECT_EQ(profile_submenu->GetActionItem()->GetProperty(
                AppMenuActionItem::kDisplayTypeKey),
            AppMenuActionItem::DisplayType::kRow);
  EXPECT_EQ(profile_submenu->GetActionItem()->GetProperty(
                AppMenuActionItem::kItemHeightKey),
            AppMenuActionItem::ItemHeight::kExpanded);
  EXPECT_NE(profile_submenu->GetProperty(AppMenuActionItem::kIconOverrideKey),
            nullptr);
  ASSERT_NE(profile_submenu->GetProperty(AppMenuActionItem::kChipTextKey),
            nullptr);
  EXPECT_EQ(*profile_submenu->GetProperty(AppMenuActionItem::kChipTextKey),
            l10n_util::GetStringUTF16(IDS_PROFILE_ROW_SIGNED_IN_MESSAGE));

  EXPECT_EQ(item_after_profile->GetActionItem()->GetProperty(
                AppMenuActionItem::kDisplayTypeKey),
            AppMenuActionItem::DisplayType::kDivider);
  EXPECT_EQ(item_after_profile->GetActionItem()->GetProperty(
                AppMenuActionItem::kSeparatorKey),
            ui::MenuSeparatorType::MENU_ITEM_SEPARATOR);

  // It should contain sync header, divider, primary actions, divider, header,
  // other profiles, divider, and footer actions.
  const auto& children = profile_submenu->GetChildren().children();
  ASSERT_EQ(children.size(), 12u);
  EXPECT_EQ(children[0]->GetActionItem()->GetProperty(
                AppMenuActionItem::kDisplayTypeKey),
            AppMenuActionItem::DisplayType::kHeader);
  EXPECT_EQ(
      children[0]->GetActionItem()->GetText(),
      l10n_util::GetStringFUTF16(IDS_PROFILE_ROW_SIGNED_IN_MESSAGE_WITH_EMAIL,
                                 {u"test@example.com"}));
  EXPECT_EQ(children[1]->GetActionItem()->GetProperty(
                AppMenuActionItem::kDisplayTypeKey),
            AppMenuActionItem::DisplayType::kDivider);
  EXPECT_EQ(children[2]->GetActionItem()->GetActionId(),
            kActionManageGoogleAccount);
  EXPECT_EQ(children[2]->GetActionItem()->GetProperty(
                AppMenuActionItem::kDisplayTypeKey),
            AppMenuActionItem::DisplayType::kRow);
  EXPECT_EQ(children[3]->GetActionItem()->GetActionId(),
            kActionCustomizeChrome);
  EXPECT_EQ(children[3]->GetActionItem()->GetProperty(
                AppMenuActionItem::kDisplayTypeKey),
            AppMenuActionItem::DisplayType::kRow);
  EXPECT_EQ(children[4]->GetActionItem()->GetActionId(), kActionCloseProfile);
  EXPECT_EQ(children[4]->GetActionItem()->GetProperty(
                AppMenuActionItem::kDisplayTypeKey),
            AppMenuActionItem::DisplayType::kRow);
  EXPECT_EQ(*children[4]->GetProperty(AppMenuActionItem::kTextOverrideKey),
            l10n_util::GetPluralStringFUTF16(IDS_CLOSE_PROFILE,
                                             CountBrowsersFor(profile_.get())));
  EXPECT_EQ(children[5]->GetActionItem()->GetProperty(
                AppMenuActionItem::kDisplayTypeKey),
            AppMenuActionItem::DisplayType::kDivider);
  EXPECT_EQ(children[6]->GetActionItem()->GetProperty(
                AppMenuActionItem::kDisplayTypeKey),
            AppMenuActionItem::DisplayType::kHeader);
  EXPECT_EQ(children[6]->GetActionItem()->GetText(),
            l10n_util::GetStringUTF16(IDS_OTHER_CHROME_PROFILES_TITLE));
  EXPECT_EQ(children[7]->GetActionItem()->GetProperty(
                AppMenuActionItem::kDisplayTypeKey),
            AppMenuActionItem::DisplayType::kRow);
  EXPECT_EQ(children[7]->GetActionItem()->GetText(), u"Profile 2");
  EXPECT_EQ(children[8]->GetActionItem()->GetProperty(
                AppMenuActionItem::kDisplayTypeKey),
            AppMenuActionItem::DisplayType::kDivider);
  EXPECT_EQ(children[9]->GetActionItem()->GetActionId(), kActionAddNewProfile);
  EXPECT_EQ(children[9]->GetActionItem()->GetProperty(
                AppMenuActionItem::kDisplayTypeKey),
            AppMenuActionItem::DisplayType::kRow);
  EXPECT_EQ(children[10]->GetActionItem()->GetActionId(),
            kActionOpenGuestProfile);
  EXPECT_EQ(children[10]->GetActionItem()->GetProperty(
                AppMenuActionItem::kDisplayTypeKey),
            AppMenuActionItem::DisplayType::kRow);
  EXPECT_EQ(children[11]->GetActionItem()->GetActionId(),
            kActionManageChromeProfiles);
  EXPECT_EQ(children[11]->GetActionItem()->GetProperty(
                AppMenuActionItem::kDisplayTypeKey),
            AppMenuActionItem::DisplayType::kRow);
}

TEST_F(ActionAppMenuManagerTest, MAYBE_ProfileSubmenuSingleProfile) {
  TestingProfileManager profile_manager(TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(profile_manager.SetUp());

  TestingProfile* profile1 = profile_manager.CreateTestingProfile("Profile 1");

  SafetyHubMenuNotificationServiceFactory::GetInstance()->SetTestingFactory(
      profile1, BrowserContextKeyedServiceFactory::TestingFactory());
  SyncServiceFactory::GetInstance()->SetTestingFactory(
      profile1,
      base::BindRepeating(
          [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
            return std::make_unique<syncer::TestSyncService>();
          }));

  ON_CALL(mock_window_interface_, GetProfile())
      .WillByDefault(testing::Return(profile1));

  ActionAppMenuManager menu_manager(&mock_window_interface_);
  menu_manager.CreateMenuHierarchy();

  actions::ActionItem* root = menu_manager.GetAppMenuRoot();
  ASSERT_NE(root, nullptr);

  actions::ActionItem* your_chrome_section = GetYourChromeSection(root);
  ASSERT_NE(your_chrome_section, nullptr);

  actions::BaseAction* profile_submenu = nullptr;
  for (const auto& child : your_chrome_section->GetChildren().children()) {
    if (child->GetActionItem()->GetActionId() == kActionProfileSubmenu) {
      profile_submenu = child.get();
      break;
    }
  }
  ASSERT_NE(profile_submenu, nullptr);

  // With a single profile and signed out, sync header and signin button are
  // present, followed by primary actions, divider, header, but no other
  // profiles and no trailing divider.
  const auto& children = profile_submenu->GetChildren().children();
  ASSERT_EQ(children.size(), 11u);
  EXPECT_EQ(children[0]->GetActionItem()->GetProperty(
                AppMenuActionItem::kDisplayTypeKey),
            AppMenuActionItem::DisplayType::kHeader);
  EXPECT_EQ(children[0]->GetActionItem()->GetText(),
            l10n_util::GetStringUTF16(IDS_PROFILES_LOCAL_PROFILE_STATE));
  EXPECT_EQ(children[1]->GetActionItem()->GetActionId(), kActionShowSignin);
  EXPECT_EQ(children[2]->GetActionItem()->GetProperty(
                AppMenuActionItem::kDisplayTypeKey),
            AppMenuActionItem::DisplayType::kDivider);
  EXPECT_EQ(children[3]->GetActionItem()->GetActionId(),
            kActionManageGoogleAccount);
  EXPECT_EQ(children[4]->GetActionItem()->GetActionId(),
            kActionCustomizeChrome);
  EXPECT_EQ(children[5]->GetActionItem()->GetActionId(), kActionCloseProfile);
  EXPECT_EQ(children[6]->GetActionItem()->GetProperty(
                AppMenuActionItem::kDisplayTypeKey),
            AppMenuActionItem::DisplayType::kDivider);
  EXPECT_EQ(children[7]->GetActionItem()->GetProperty(
                AppMenuActionItem::kDisplayTypeKey),
            AppMenuActionItem::DisplayType::kHeader);
  EXPECT_EQ(children[7]->GetActionItem()->GetText(),
            l10n_util::GetStringUTF16(IDS_OTHER_CHROME_PROFILES_TITLE));
  EXPECT_EQ(children[8]->GetActionItem()->GetActionId(), kActionAddNewProfile);
  EXPECT_EQ(children[9]->GetActionItem()->GetActionId(),
            kActionOpenGuestProfile);
  EXPECT_EQ(children[10]->GetActionItem()->GetActionId(),
            kActionManageChromeProfiles);
}

TEST_F(ActionAppMenuManagerTest,
       BlockActionsEnterpriseIsolatedModeReplacesIncognito) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      enterprise_isolated_mode::kEnableEnterpriseIsolatedMode);
  profile_->GetPrefs()->SetInteger(
      enterprise_isolated_mode::kEnterpriseIsolatedModeSettings,
      static_cast<int>(
          enterprise_isolated_mode::IsolatedModeSetting::kEnabled));

  ActionAppMenuManager menu_manager(&mock_window_interface_);
  menu_manager.CreateMenuHierarchy();

  actions::ActionItem* root = menu_manager.GetAppMenuRoot();
  ASSERT_NE(root, nullptr);

  actions::ActionItem* block_section = GetBlockSection(root);
  ASSERT_NE(block_section, nullptr);

  // When isolated mode replaces incognito, it should replace the New Incognito
  // Window action rather than adding a fourth item.
  ASSERT_EQ(block_section->GetChildren().children().size(), 3u);
  EXPECT_EQ(block_section->GetChildren()
                .children()[0]
                ->GetActionItem()
                ->GetActionId(),
            kActionNewTab);
  EXPECT_EQ(block_section->GetChildren()
                .children()[1]
                ->GetActionItem()
                ->GetActionId(),
            kActionNewWindow);
  EXPECT_EQ(block_section->GetChildren()
                .children()[2]
                ->GetActionItem()
                ->GetActionId(),
            kActionNewIsolatedWindow);
}

TEST_F(ActionAppMenuManagerTest, BlockActionsIncognitoNewTabTextOverride) {
  Profile* otr_profile =
      profile_->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  ASSERT_TRUE(otr_profile->IsIncognitoProfile());
  ON_CALL(mock_window_interface_, GetProfile())
      .WillByDefault(testing::Return(otr_profile));

  ActionAppMenuManager menu_manager(&mock_window_interface_);
  menu_manager.CreateMenuHierarchy();

  actions::ActionItem* root = menu_manager.GetAppMenuRoot();
  ASSERT_NE(root, nullptr);

  actions::ActionItem* block_section =
      root->GetChildren().children()[1]->GetActionItem();
  ASSERT_NE(block_section, nullptr);
  ASSERT_FALSE(block_section->GetChildren().children().empty());

  actions::BaseAction* new_tab_action =
      block_section->GetChildren().children()[0].get();
  ASSERT_NE(new_tab_action, nullptr);
  EXPECT_EQ(new_tab_action->GetActionItem()->GetActionId(), kActionNewTab);

  std::u16string* text_override =
      new_tab_action->GetProperty(AppMenuActionItem::kTextOverrideKey);
  ASSERT_NE(text_override, nullptr);
  EXPECT_EQ(*text_override, u"New Incognito tab");
}

TEST_F(ActionAppMenuManagerTest,
       BlockActionsEnterpriseIsolatedModeNewTabTextOverride) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      enterprise_isolated_mode::kEnableEnterpriseIsolatedMode);
  profile_->GetPrefs()->SetInteger(
      enterprise_isolated_mode::kEnterpriseIsolatedModeSettings,
      static_cast<int>(
          enterprise_isolated_mode::IsolatedModeSetting::kEnabled));

  Profile* isolated_profile =
      profile_->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  ASSERT_TRUE(isolated_profile->IsEnterpriseIsolatedModeProfile());
  ON_CALL(mock_window_interface_, GetProfile())
      .WillByDefault(testing::Return(isolated_profile));

  ActionAppMenuManager menu_manager(&mock_window_interface_);
  menu_manager.CreateMenuHierarchy();

  actions::ActionItem* root = menu_manager.GetAppMenuRoot();
  ASSERT_NE(root, nullptr);

  actions::ActionItem* block_section =
      root->GetChildren().children()[1]->GetActionItem();
  ASSERT_NE(block_section, nullptr);
  ASSERT_FALSE(block_section->GetChildren().children().empty());

  actions::BaseAction* new_tab_action =
      block_section->GetChildren().children()[0].get();
  ASSERT_NE(new_tab_action, nullptr);
  EXPECT_EQ(new_tab_action->GetActionItem()->GetActionId(), kActionNewTab);

  std::u16string* text_override =
      new_tab_action->GetProperty(AppMenuActionItem::kTextOverrideKey);
  ASSERT_NE(text_override, nullptr);
  EXPECT_EQ(*text_override, u"New Isolated tab");
}

TEST_F(ActionAppMenuManagerTest, BookmarkBarSubmenuCheckItems) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      ntp_features::kNtpSimplificationBookmarkBar);

  profile_->GetPrefs()->SetInteger(
      bookmarks::prefs::kBookmarkBarVisibilityState,
      static_cast<int>(bookmarks::BookmarkBarVisibilityState::kAlwaysShow));

  ActionAppMenuManager menu_manager(&mock_window_interface_);
  menu_manager.CreateMenuHierarchy();

  actions::ActionItem* root = menu_manager.GetAppMenuRoot();
  ASSERT_NE(root, nullptr);

  actions::ActionItem* your_chrome_section =
      root->GetChildren().children()[2]->GetActionItem();
  ASSERT_NE(your_chrome_section, nullptr);

  actions::BaseAction* bookmarks_submenu = nullptr;
  for (const auto& child : your_chrome_section->GetChildren().children()) {
    if (child->GetActionItem()->GetActionId() == kActionBookmarksSubmenu) {
      bookmarks_submenu = child.get();
      break;
    }
  }
  ASSERT_NE(bookmarks_submenu, nullptr);

  actions::BaseAction* bookmark_bar_submenu = nullptr;
  for (const auto& child : bookmarks_submenu->GetChildren().children()) {
    if (child->GetActionItem()->GetActionId() == kActionBookmarkBarSubmenu) {
      bookmark_bar_submenu = child.get();
      break;
    }
  }
  ASSERT_NE(bookmark_bar_submenu, nullptr);

  const auto& bookmark_bar_children =
      bookmark_bar_submenu->GetChildren().children();
  ASSERT_EQ(bookmark_bar_children.size(), 3u);

  EXPECT_EQ(bookmark_bar_children[0]->GetActionItem()->GetActionId(),
            kActionBookmarkBarSubmenuAlwaysHide);
  EXPECT_TRUE(bookmark_bar_children[0]->GetActionItem()->GetProperty(
      AppMenuActionItem::kIsCheckableKey));

  EXPECT_EQ(bookmark_bar_children[1]->GetActionItem()->GetActionId(),
            kActionBookmarkBarSubmenuAlwaysShow);
  EXPECT_TRUE(bookmark_bar_children[1]->GetActionItem()->GetProperty(
      AppMenuActionItem::kIsCheckableKey));

  EXPECT_EQ(bookmark_bar_children[2]->GetActionItem()->GetActionId(),
            kActionBookmarkBarSubmenuOnlyOnNtp);
  EXPECT_TRUE(bookmark_bar_children[2]->GetActionItem()->GetProperty(
      AppMenuActionItem::kIsCheckableKey));
}

TEST_F(ActionAppMenuManagerTest, NotificationHeaderNoNotification) {
  ActionAppMenuManager menu_manager(&mock_window_interface_);
  menu_manager.CreateMenuHierarchy();

  actions::ActionItem* root = menu_manager.GetAppMenuRoot();
  ASSERT_NE(root, nullptr);

  ASSERT_FALSE(root->GetChildren().children().empty());
  actions::ActionItem* notification_section =
      root->GetChildren().children()[0]->GetActionItem();
  ASSERT_NE(notification_section, nullptr);
  EXPECT_TRUE(notification_section->GetChildren().children().empty());
}

#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(ActionAppMenuManagerTest, NotificationHeaderUpgradeNotification) {
  if (!browser_defaults::kShowUpgradeMenuItem) {
    GTEST_SKIP() << "Upgrade menu item is not supported on this platform.";
  }

  actions::ActionItem* upgrade_action =
      actions::ActionManager::Get().FindAction(kActionUpgradeDialog);
  ASSERT_NE(upgrade_action, nullptr);
  upgrade_action->SetVisible(true);

  ActionAppMenuManager menu_manager(&mock_window_interface_);
  menu_manager.CreateMenuHierarchy();

  actions::ActionItem* root = menu_manager.GetAppMenuRoot();
  ASSERT_NE(root, nullptr);

  const auto& children = root->GetChildren().children();
  ASSERT_GE(children.size(), 2u);

  // The first item should be the notification section.
  actions::ActionItem* notification_section = children[0]->GetActionItem();
  ASSERT_NE(notification_section, nullptr);
  EXPECT_EQ(
      notification_section->GetProperty(AppMenuActionItem::kDisplayTypeKey),
      AppMenuActionItem::DisplayType::kSection);
  EXPECT_EQ(
      notification_section->GetProperty(AppMenuActionItem::kContainerColorKey),
      ui::kColorAppMenuUpgradeRowBackground);

  const auto& section_children = notification_section->GetChildren().children();
  ASSERT_EQ(section_children.size(), 1u);
  EXPECT_EQ(section_children[0]->GetActionItem()->GetActionId(),
            kActionUpgradeDialog);
  EXPECT_TRUE(section_children[0]->GetActionItem()->GetVisible());
  EXPECT_EQ(section_children[0]->GetActionItem()->GetProperty(
                AppMenuActionItem::kDisplayTypeKey),
            AppMenuActionItem::DisplayType::kNotification);
  EXPECT_EQ(section_children[0]->GetActionItem()->GetProperty(
                AppMenuActionItem::kContainerColorKey),
            ui::kColorAppMenuUpgradeRowBackground);
}
#endif

TEST_F(ActionAppMenuManagerTest, NotificationHeaderSafetyHubNotification) {
  SafetyHubMenuNotificationServiceFactory::GetInstance()->SetTestingFactory(
      profile_.get(), base::BindRepeating([](content::BrowserContext* context)
                                              -> std::unique_ptr<KeyedService> {
        Profile* profile = Profile::FromBrowserContext(context);
        auto service = std::make_unique<SafetyHubMenuNotificationService>(
            profile->GetPrefs(), nullptr, nullptr,
#if !BUILDFLAG(IS_ANDROID)
            nullptr,
#endif
            profile);
        auto getter = base::BindRepeating(
            []() -> std::optional<std::unique_ptr<SafetyHubResult>> {
              return std::make_unique<SafetyHubSafeBrowsingResult>(
                  SafeBrowsingState::kDisabledByUser);
            });
        service->UpdateResultGetterForTesting(
            safety_hub::SafetyHubModuleType::UNUSED_SITE_PERMISSIONS, getter);
        return service;
      }));

  ActionAppMenuManager menu_manager(&mock_window_interface_);
  menu_manager.CreateMenuHierarchy();

  actions::ActionItem* root = menu_manager.GetAppMenuRoot();
  ASSERT_NE(root, nullptr);

  const auto& children = root->GetChildren().children();
  ASSERT_GE(children.size(), 2u);

  actions::ActionItem* notification_section = children[0]->GetActionItem();
  ASSERT_NE(notification_section, nullptr);
  const auto& section_children = notification_section->GetChildren().children();
  ASSERT_EQ(section_children.size(), 1u);

  EXPECT_EQ(section_children[0]->GetActionItem()->GetActionId(),
            kActionOpenSafetyHub);
  EXPECT_TRUE(section_children[0]->GetActionItem()->GetVisible());
  ASSERT_NE(
      section_children[0]->GetProperty(AppMenuActionItem::kTextOverrideKey),
      nullptr);
  EXPECT_EQ(
      *section_children[0]->GetProperty(AppMenuActionItem::kTextOverrideKey),
      l10n_util::GetStringUTF16(
          IDS_SETTINGS_SAFETY_HUB_SAFE_BROWSING_MENU_NOTIFICATION));
  EXPECT_EQ(section_children[0]->GetActionItem()->GetProperty(
                AppMenuActionItem::kDisplayTypeKey),
            AppMenuActionItem::DisplayType::kNotification);
  EXPECT_EQ(section_children[0]->GetActionItem()->GetProperty(
                AppMenuActionItem::kContainerColorKey),
            ui::kColorAppMenuUpgradeRowBackground);
}

TEST_F(ActionAppMenuManagerTest, NotificationHeaderGlobalError) {
  actions::ActionItem* global_error_action =
      actions::ActionManager::Get().FindAction(kActionGlobalError);
  ASSERT_NE(global_error_action, nullptr);
  global_error_action->SetVisible(true);

  ActionAppMenuManager menu_manager(&mock_window_interface_);
  menu_manager.CreateMenuHierarchy();

  actions::ActionItem* root = menu_manager.GetAppMenuRoot();
  ASSERT_NE(root, nullptr);

  const auto& children = root->GetChildren().children();
  ASSERT_GE(children.size(), 2u);

  actions::ActionItem* notification_section = children[0]->GetActionItem();
  ASSERT_NE(notification_section, nullptr);
  EXPECT_EQ(
      notification_section->GetProperty(AppMenuActionItem::kDisplayTypeKey),
      AppMenuActionItem::DisplayType::kSection);
  EXPECT_EQ(
      notification_section->GetProperty(AppMenuActionItem::kContainerColorKey),
      ui::kColorAppMenuUpgradeRowBackground);

  const auto& section_children = notification_section->GetChildren().children();
  ASSERT_EQ(section_children.size(), 1u);

  EXPECT_EQ(section_children[0]->GetActionItem()->GetActionId(),
            kActionGlobalError);
  EXPECT_TRUE(section_children[0]->GetActionItem()->GetVisible());
  EXPECT_EQ(section_children[0]->GetActionItem()->GetProperty(
                AppMenuActionItem::kDisplayTypeKey),
            AppMenuActionItem::DisplayType::kNotification);
  EXPECT_EQ(section_children[0]->GetActionItem()->GetProperty(
                AppMenuActionItem::kContainerColorKey),
            ui::kColorAppMenuUpgradeRowBackground);
}

TEST_F(ActionAppMenuManagerTest, ZoomSubmenuHasExpandedHeightProperty) {
  ActionAppMenuManager menu_manager(&mock_window_interface_);
  menu_manager.CreateMenuHierarchy();

  actions::ActionItem* root = menu_manager.GetAppMenuRoot();
  ASSERT_NE(root, nullptr);
  actions::ActionItem* zoom_action =
      actions::ActionManager::Get().FindAction(kActionZoomSubmenu, root);
  ASSERT_NE(zoom_action, nullptr);
  EXPECT_EQ(zoom_action->GetProperty(AppMenuActionItem::kItemHeightKey),
            AppMenuActionItem::ItemHeight::kExpanded);
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
TEST_F(ActionAppMenuManagerTest, NotificationHeaderDefaultBrowserPrompt) {
  actions::ActionItem* default_browser_action =
      actions::ActionManager::Get().FindAction(kActionSetBrowserAsDefault);
  ASSERT_NE(default_browser_action, nullptr);
  default_browser_action->SetVisible(true);

  ActionAppMenuManager menu_manager(&mock_window_interface_);
  menu_manager.CreateMenuHierarchy();

  actions::ActionItem* root = menu_manager.GetAppMenuRoot();
  ASSERT_NE(root, nullptr);

  const auto& children = root->GetChildren().children();
  ASSERT_GE(children.size(), 2u);

  actions::ActionItem* notification_section = children[0]->GetActionItem();
  ASSERT_NE(notification_section, nullptr);
  EXPECT_EQ(
      notification_section->GetProperty(AppMenuActionItem::kDisplayTypeKey),
      AppMenuActionItem::DisplayType::kSection);
  EXPECT_EQ(
      notification_section->GetProperty(AppMenuActionItem::kContainerColorKey),
      ui::kColorAppMenuUpgradeRowBackground);

  const auto& section_children = notification_section->GetChildren().children();
  ASSERT_EQ(section_children.size(), 1u);

  EXPECT_EQ(section_children[0]->GetActionItem()->GetActionId(),
            kActionSetBrowserAsDefault);
  EXPECT_TRUE(section_children[0]->GetActionItem()->GetVisible());
  EXPECT_EQ(section_children[0]->GetActionItem()->GetProperty(
                AppMenuActionItem::kDisplayTypeKey),
            AppMenuActionItem::DisplayType::kNotification);
  EXPECT_EQ(section_children[0]->GetActionItem()->GetProperty(
                AppMenuActionItem::kContainerColorKey),
            ui::kColorAppMenuUpgradeRowBackground);
}
#endif

TEST_F(ActionAppMenuManagerTest, VerticalTabsNewBadgeProperty) {
  tabs::test::MockVerticalTabStripStateController vertical_tabs_controller(
      mock_window_interface_);

  EXPECT_CALL(vertical_tabs_controller, ShouldDisplayVerticalTabs())
      .WillOnce(testing::Return(false));

  ActionAppMenuManager menu_manager(&mock_window_interface_);
  menu_manager.CreateMenuHierarchy();

  actions::ActionItem* root = menu_manager.GetAppMenuRoot();
  ASSERT_NE(root, nullptr);

  actions::ActionItem* toggle_vertical_tabs =
      actions::ActionManager::Get().FindAction(kActionToggleVerticalTabs, root);
  ASSERT_NE(toggle_vertical_tabs, nullptr);
  EXPECT_EQ(
      toggle_vertical_tabs->GetProperty(AppMenuActionItem::kNewBadgeFeatureKey),
      &tabs::kVerticalTabsNewBadge);

  // When vertical tabs are already displayed, the new badge feature should not
  // be set.
  root->ResetActionList();
  EXPECT_CALL(vertical_tabs_controller, ShouldDisplayVerticalTabs())
      .WillOnce(testing::Return(true));
  menu_manager.CreateMenuHierarchy();

  toggle_vertical_tabs =
      actions::ActionManager::Get().FindAction(kActionToggleVerticalTabs, root);
  ASSERT_NE(toggle_vertical_tabs, nullptr);
  EXPECT_EQ(
      toggle_vertical_tabs->GetProperty(AppMenuActionItem::kNewBadgeFeatureKey),
      nullptr);
}

TEST_F(ActionAppMenuManagerTest, AlertedElementPropagatesToSubmenu) {
  UserEducationServiceFactory::GetInstance()->SetTestingFactory(
      profile_.get(), base::BindRepeating([](content::BrowserContext* context)
                                              -> std::unique_ptr<KeyedService> {
        return std::make_unique<UserEducationService>(
            Profile::FromBrowserContext(context), /*allows_promos=*/true);
      }));
  auto* const user_ed_service =
      UserEducationServiceFactory::GetForBrowserContext(profile_.get());
  user_education::TutorialDescription desc;
  desc.steps.push_back(user_education::TutorialDescription::BubbleStep(
                           AppMenuModel::kPasswordManagerMenuItem)
                           .SetBubbleBodyText(IDS_OK));
  desc.steps.push_back(
      user_education::TutorialDescription::BubbleStep("NamedElement")
          .SetBubbleBodyText(IDS_OK));
  user_ed_service->tutorial_registry().AddTutorial("TestTutorial",
                                                   std::move(desc));
  user_ed_service->tutorial_service()->StartTutorial(
      "TestTutorial", ui::ElementContext::CreateFakeContextForTesting(1));

  ActionAppMenuManager menu_manager(&mock_window_interface_);
  menu_manager.CreateMenuHierarchy();

  actions::ActionItem* root = menu_manager.GetAppMenuRoot();
  ASSERT_NE(root, nullptr);
  actions::ActionItem* your_chrome_section = GetYourChromeSection(root);
  ASSERT_NE(your_chrome_section, nullptr);

  actions::BaseAction* passwords_submenu = nullptr;
  actions::BaseAction* show_downloads = nullptr;
  for (const auto& child : your_chrome_section->GetChildren().children()) {
    if (child->GetActionItem()->GetActionId() ==
        kActionPasswordsAndAutofillSubmenu) {
      passwords_submenu = child.get();
    } else if (child->GetActionItem()->GetActionId() ==
               kActionShowDownloadsPage) {
      show_downloads = child.get();
    }
  }
  ASSERT_NE(passwords_submenu, nullptr);
  EXPECT_TRUE(passwords_submenu->GetProperty(AppMenuActionItem::kIsAlertedKey));

  actions::BaseAction* show_passwords =
      passwords_submenu->GetChildren().children()[0].get();
  ASSERT_NE(show_passwords, nullptr);
  EXPECT_TRUE(show_passwords->GetProperty(AppMenuActionItem::kIsAlertedKey));

  // Items without an element_id (like Payment methods) must not match tutorial
  // steps that also have an empty element_id.
  actions::BaseAction* show_payment_methods =
      passwords_submenu->GetChildren().children()[1].get();
  ASSERT_NE(show_payment_methods, nullptr);
  EXPECT_FALSE(
      show_payment_methods->GetProperty(AppMenuActionItem::kIsAlertedKey));

  ASSERT_NE(show_downloads, nullptr);
  EXPECT_FALSE(show_downloads->GetProperty(AppMenuActionItem::kIsAlertedKey));
}

}  // namespace
