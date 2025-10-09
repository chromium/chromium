// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/collaboration_messaging_observer.h"

#include <optional>

#include "chrome/browser/collaboration/messaging/messaging_backend_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/collaboration_messaging_observer_factory.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/collaboration_messaging_tab_data.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_sync_service_initialized_observer.h"
#include "chrome/browser/ui/toasts/toast_features.h"
#include "chrome/browser/ui/toasts/toast_view.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_view_interface.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_group_header.h"
#include "chrome/browser/ui/views/tabs/tab_icon.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/collaboration/public/messaging/messaging_backend_service.h"
#include "components/data_sharing/public/features.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view.h"
#include "url/gurl.h"

using collaboration::messaging::CollaborationEvent;
using collaboration::messaging::InstantMessage;
using collaboration::messaging::InstantNotificationLevel;
using collaboration::messaging::InstantNotificationType;
using collaboration::messaging::MessageAttribution;
using collaboration::messaging::MessagingBackendService;
using collaboration::messaging::MessagingBackendServiceFactory;
using collaboration::messaging::PersistentMessage;
using collaboration::messaging::PersistentNotificationType;
using collaboration::messaging::TabGroupMessageMetadata;
using collaboration::messaging::TabMessageMetadata;
using data_sharing::GroupMember;
using testing::_;

using SuccessCallback = collaboration::messaging::MessagingBackendService::
    InstantMessageDelegate::SuccessCallback;

namespace tab_groups {
namespace {

PersistentMessage CreateMessage(std::string given_name,
                                std::string avatar_url,
                                CollaborationEvent event,
                                PersistentNotificationType type,
                                tab_groups::LocalTabID tab_id,
                                tab_groups::LocalTabGroupID tab_group_id) {
  GroupMember member;
  member.given_name = given_name;
  member.avatar_url = GURL(avatar_url);

  TabMessageMetadata tab_metadata;
  tab_metadata.local_tab_id = tab_id;

  TabGroupMessageMetadata tab_group_metadata;
  tab_group_metadata.local_tab_group_id = tab_group_id;

  MessageAttribution attribution;
  attribution.triggering_user = member;
  attribution.tab_metadata = tab_metadata;
  attribution.tab_group_metadata = tab_group_metadata;

  PersistentMessage message;
  message.type = type;
  message.attribution = attribution;
  message.collaboration_event = event;

  return message;
}

InstantMessage CreateInstantMessage(
    std::string given_name,
    CollaborationEvent event,
    std::string tab_url,
    std::string tab_title,
    std::optional<tab_groups::LocalTabGroupID> tab_group_id,
    std::string group_name) {
  GroupMember member;
  member.given_name = given_name;

  TabMessageMetadata tab_metadata;
  tab_metadata.last_known_url = tab_url;
  tab_metadata.last_known_title = tab_title;

  TabGroupMessageMetadata tab_group_metadata;
  tab_group_metadata.local_tab_group_id = tab_group_id;
  tab_group_metadata.last_known_title = group_name;

  MessageAttribution attribution;
  if (event == CollaborationEvent::COLLABORATION_MEMBER_ADDED ||
      event == CollaborationEvent::COLLABORATION_MEMBER_REMOVED) {
    attribution.affected_user = member;
  } else {
    attribution.triggering_user = member;
  }

  attribution.tab_metadata = tab_metadata;
  attribution.tab_group_metadata = tab_group_metadata;

  InstantMessage message;
  message.attributions.emplace_back(attribution);
  message.type = event == CollaborationEvent::TAB_REMOVED
                     ? InstantNotificationType::CONFLICT_TAB_REMOVED
                     : InstantNotificationType::UNDEFINED;
  message.level = InstantNotificationLevel::BROWSER;
  message.collaboration_event = event;
  message.localized_message = u"Sample instant message";

  return message;
}

}  // namespace

struct CollaborationMessagingInteractiveTestParams {
  bool page_actions_migration_enabled = false;
};

class CollaborationMessagingObserverBrowserTest
    : public InteractiveBrowserTest,
      public ::testing::WithParamInterface<
          CollaborationMessagingInteractiveTestParams> {
 public:
  CollaborationMessagingObserverBrowserTest() {
    std::vector<base::test::FeatureRefAndParams> enabled_features = {
        {data_sharing::features::kDataSharingFeature, {}},
    };
    std::vector<base::test::FeatureRef> disabled_features;

    if (GetParam().page_actions_migration_enabled) {
      enabled_features.push_back({
          features::kPageActionsMigration,
          {
              {
                  features::kPageActionsMigrationCollaborationMessaging.name,
                  "true",
              },
          },
      });
    } else {
      disabled_features.push_back(features::kPageActionsMigration);
    }
    features_.InitWithFeaturesAndParameters(enabled_features,
                                            disabled_features);
    CHECK_EQ(IsPageActionsMigrationEnabled(),
             GetParam().page_actions_migration_enabled);
  }
  ~CollaborationMessagingObserverBrowserTest() override = default;

 protected:
  CollaborationMessagingObserver* observer() {
    // All browsers in these tests share the same profile.
    return CollaborationMessagingObserverFactory::GetForProfile(
        browser()->profile());
  }

  TabStripViewInterface* GetTabStripView(Browser* target_browser) {
    return BrowserView::GetBrowserViewForBrowser(target_browser)
        ->tab_strip_view();
  }

  TabIcon* GetTabIcon(Browser* target_browser, int index) {
    return GetTabStripView(target_browser)
        ->GetTabAnchorViewAt(index)
        ->GetTabIconForTesting();
  }

  views::View* GetTabGroupHeader(Browser* target_browser,
                                 const tab_groups::TabGroupId index) {
    return GetTabStripView(target_browser)->GetTabGroupAnchorView(index);
  }

  tabs::TabInterface* GetTabInterface(Browser* target_browser, int index) {
    return target_browser->tab_strip_model()->GetTabAtIndex(index);
  }

  CollaborationMessagingTabData* GetTabDataAtIndex(Browser* target_browser,
                                                   int index) {
    return GetTabInterface(target_browser, index)
        ->GetTabFeatures()
        ->collaboration_messaging_tab_data();
  }

  bool AddTab(Browser* target_browser) {
    return AddTabAtIndexToBrowser(target_browser, -1, GURL("about:blank"),
                                  ui::PageTransition::PAGE_TRANSITION_FIRST);
  }

  void AddTabs(Browser* target_browser, int num) {
    for (int i = 0; i < num; i++) {
      AddTab(target_browser);
    }
  }

  bool IsPageActionsMigrationEnabled() const {
    return IsPageActionMigrated(PageActionIconType::kCollaborationMessaging);
  }

  void WaitForTabGroupSyncServiceInitialized() {
    auto observer =
        std::make_unique<tab_groups::TabGroupSyncServiceInitializedObserver>(
            tab_groups::TabGroupSyncServiceFactory::GetForProfile(
                browser()->profile()));
    observer->Wait();
  }

 private:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_P(CollaborationMessagingObserverBrowserTest,
                       HandlesMessages) {
  WaitForTabGroupSyncServiceInitialized();

  // Add 4 more tabs, for a total of 5.
  AddTabs(browser(), 4);
  EXPECT_EQ(5, browser()->tab_strip_model()->count());

  // Observer is initialized
  EXPECT_NE(observer(), nullptr);

  // Group 2 tabs in the middle of the tab strip to test group offsets.
  auto group_id = browser()->tab_strip_model()->AddToNewGroup({2, 3});
  // CHIP messages set the message in TabFeatures
  auto tab2_id =
      browser()->tab_strip_model()->GetTabAtIndex(2)->GetHandle().raw_value();

  // Prevent network request.
  GetTabDataAtIndex(browser(), 2)->set_mocked_avatar_for_testing(gfx::Image());

  auto chip_message =
      CreateMessage("User", "URL", CollaborationEvent::TAB_ADDED,
                    PersistentNotificationType::CHIP, tab2_id, group_id);
  EXPECT_FALSE(GetTabDataAtIndex(browser(), 2)->HasMessage());
  observer()->DisplayPersistentMessage(chip_message);
  EXPECT_TRUE(GetTabDataAtIndex(browser(), 2)->HasMessage());
  observer()->HidePersistentMessage(chip_message);
  EXPECT_FALSE(GetTabDataAtIndex(browser(), 2)->HasMessage());

  // DIRTY_TAB messages set attention on the tab icon
  auto tab3_id =
      browser()->tab_strip_model()->GetTabAtIndex(3)->GetHandle().raw_value();
  auto dirty_tab_message =
      CreateMessage("User", "URL", CollaborationEvent::TAB_UPDATED,
                    PersistentNotificationType::DIRTY_TAB, tab3_id, group_id);
  EXPECT_FALSE(GetTabIcon(browser(), 3)->GetShowingAttentionIndicator());
  observer()->DisplayPersistentMessage(dirty_tab_message);
  EXPECT_TRUE(GetTabIcon(browser(), 3)->GetShowingAttentionIndicator());
  observer()->HidePersistentMessage(dirty_tab_message);
  EXPECT_FALSE(GetTabIcon(browser(), 3)->GetShowingAttentionIndicator());

  // Collapse group so it can receive an attention indicator.
  views::View* tab_group_header =
      GetTabStripView(browser())->GetTabGroupAnchorView(group_id);
  const ui::MouseEvent event(ui::EventType::kMousePressed, gfx::Point(),
                             gfx::Point(), ui::EventTimeForNow(),
                             ui::EF_LEFT_MOUSE_BUTTON,
                             ui::EF_LEFT_MOUSE_BUTTON);
  tab_group_header->OnMousePressed(event);
  tab_group_header->OnMouseReleased(event);

  // DIRTY_TAB_GROUP messages set attention on tab group header
  auto dirty_tab_group_message = CreateMessage(
      "User", "URL", CollaborationEvent::TAB_ADDED,
      PersistentNotificationType::DIRTY_TAB_GROUP, tab2_id, group_id);

  views::View* attention_indicator_view =
      views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
          /*id=*/TabGroupHeader::kAttentionIndicatorViewElementId,
          /*context=*/views::ElementTrackerViews::GetContextForView(
              tab_group_header));

  // Verify the attention indicator updates correctly with persistent message
  // notifications.
  EXPECT_FALSE(attention_indicator_view->GetVisible());
  observer()->DisplayPersistentMessage(dirty_tab_group_message);
  EXPECT_TRUE(attention_indicator_view->GetVisible());
  observer()->HidePersistentMessage(dirty_tab_group_message);
  EXPECT_FALSE(attention_indicator_view->GetVisible());
}

IN_PROC_BROWSER_TEST_P(CollaborationMessagingObserverBrowserTest,
                       HandlesTabMessagesInCollapsedGroup) {
  WaitForTabGroupSyncServiceInitialized();

  // Add 2 more tabs, for a total of 3.
  AddTabs(browser(), 2);
  EXPECT_EQ(3, browser()->tab_strip_model()->count());

  // Observer is initialized
  EXPECT_NE(observer(), nullptr);

  // Group 2 tabs and collapse group.
  auto group_id = browser()->tab_strip_model()->AddToNewGroup({1, 2});

  // Collapse the TabGroupHeader.
  views::View* tab_group_header =
      GetTabStripView(browser())->GetTabGroupAnchorView(group_id);
  const ui::MouseEvent event(ui::EventType::kMousePressed, gfx::Point(),
                             gfx::Point(), ui::EventTimeForNow(),
                             ui::EF_LEFT_MOUSE_BUTTON,
                             ui::EF_LEFT_MOUSE_BUTTON);
  tab_group_header->OnMousePressed(event);
  tab_group_header->OnMouseReleased(event);

  // Tab 2 should be hidden. Do all tests on this tab.
  EXPECT_FALSE(GetTabIcon(browser(), 2)->IsDrawn());

  // CHIP messages set the message in TabFeatures
  auto tab2_id =
      browser()->tab_strip_model()->GetTabAtIndex(2)->GetHandle().raw_value();

  // Prevent network request.
  GetTabDataAtIndex(browser(), 2)->set_mocked_avatar_for_testing(gfx::Image());

  auto chip_message =
      CreateMessage("User", "URL", CollaborationEvent::TAB_ADDED,
                    PersistentNotificationType::CHIP, tab2_id, group_id);
  EXPECT_FALSE(GetTabDataAtIndex(browser(), 2)->HasMessage());
  observer()->DisplayPersistentMessage(chip_message);
  EXPECT_TRUE(GetTabDataAtIndex(browser(), 2)->HasMessage());
  observer()->HidePersistentMessage(chip_message);
  EXPECT_FALSE(GetTabDataAtIndex(browser(), 2)->HasMessage());

  // DIRTY_TAB messages set attention on the tab icon
  auto dirty_tab_message =
      CreateMessage("User", "URL", CollaborationEvent::TAB_UPDATED,
                    PersistentNotificationType::DIRTY_TAB, tab2_id, group_id);
  EXPECT_FALSE(GetTabIcon(browser(), 2)->GetShowingAttentionIndicator());
  observer()->DisplayPersistentMessage(dirty_tab_message);
  EXPECT_TRUE(GetTabIcon(browser(), 2)->GetShowingAttentionIndicator());
  observer()->HidePersistentMessage(dirty_tab_message);
  EXPECT_FALSE(GetTabIcon(browser(), 2)->GetShowingAttentionIndicator());
}

IN_PROC_BROWSER_TEST_P(CollaborationMessagingObserverBrowserTest,
                       IgnoresTabMessagesWithIncompleteData) {
  WaitForTabGroupSyncServiceInitialized();

  // Add 1 more tab, for a total of 2.
  AddTabs(browser(), 1);
  EXPECT_EQ(2, browser()->tab_strip_model()->count());

  // Observer is initialized
  EXPECT_NE(observer(), nullptr);

  // Group 2 tabs in the middle of the tab strip to test group offsets.
  auto group_id = browser()->tab_strip_model()->AddToNewGroup({0, 1});

  // CHIP messages set the message in TabFeatures
  auto tab0_id =
      browser()->tab_strip_model()->GetTabAtIndex(0)->GetHandle().raw_value();

  // Prevent network request.
  GetTabDataAtIndex(browser(), 0)->set_mocked_avatar_for_testing(gfx::Image());

  // Message has no tab group metadata.
  {
    auto message =
        CreateMessage("User", "URL", CollaborationEvent::TAB_ADDED,
                      PersistentNotificationType::CHIP, tab0_id, group_id);

    // Remove tab group metadata.
    message.attribution.tab_group_metadata = std::nullopt;

    // No messages are delivered.
    EXPECT_FALSE(GetTabDataAtIndex(browser(), 0)->HasMessage());
    observer()->DisplayPersistentMessage(message);
    EXPECT_FALSE(GetTabDataAtIndex(browser(), 0)->HasMessage());
  }

  // Message has no tab group id.
  {
    auto message = CreateMessage("User", "URL", CollaborationEvent::TAB_ADDED,
                                 PersistentNotificationType::DIRTY_TAB_GROUP,
                                 tab0_id, group_id);

    // Remove tab group id.
    message.attribution.tab_group_metadata->local_tab_group_id =
        tab_groups::LocalTabGroupID::CreateEmpty();

    // No messages are delivered.
    EXPECT_FALSE(GetTabDataAtIndex(browser(), 0)->HasMessage());
    observer()->DisplayPersistentMessage(message);
    EXPECT_FALSE(GetTabDataAtIndex(browser(), 0)->HasMessage());
  }

  // Message has no tab metadata.
  {
    auto message =
        CreateMessage("User", "URL", CollaborationEvent::TAB_UPDATED,
                      PersistentNotificationType::DIRTY_TAB, tab0_id, group_id);

    // Remove tab metadata.
    message.attribution.tab_metadata = std::nullopt;

    // No messages are delivered.
    EXPECT_FALSE(GetTabIcon(browser(), 0)->GetShowingAttentionIndicator());
    observer()->DisplayPersistentMessage(message);
    EXPECT_FALSE(GetTabIcon(browser(), 0)->GetShowingAttentionIndicator());
  }

  // Tab does not exists.
  {
    auto message =
        CreateMessage("User", "URL", CollaborationEvent::TAB_ADDED,
                      PersistentNotificationType::DIRTY_TAB, tab0_id, group_id);

    // Close the tab group so no message can be delivered.
    browser()->tab_strip_model()->CloseWebContentsAt(0, 0);
    EXPECT_EQ(1, browser()->tab_strip_model()->count());

    EXPECT_FALSE(GetTabIcon(browser(), 0)->GetShowingAttentionIndicator());
    observer()->DisplayPersistentMessage(message);
    EXPECT_FALSE(GetTabIcon(browser(), 0)->GetShowingAttentionIndicator());
  }
}

IN_PROC_BROWSER_TEST_P(CollaborationMessagingObserverBrowserTest,
                       IgnoresUnsupportedTabMessages) {
  WaitForTabGroupSyncServiceInitialized();

  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  // Observer is initialized
  EXPECT_NE(observer(), nullptr);

  auto group_id = browser()->tab_strip_model()->AddToNewGroup({0});

  // CHIP messages set the message in TabFeatures
  auto tab0_id =
      browser()->tab_strip_model()->GetTabAtIndex(0)->GetHandle().raw_value();

  // Prevent network request.
  GetTabDataAtIndex(browser(), 0)->set_mocked_avatar_for_testing(gfx::Image());

  auto message =
      CreateMessage("User", "URL", CollaborationEvent::TAB_ADDED,
                    PersistentNotificationType::TOMBSTONED, tab0_id, group_id);

  // No messages are delivered.
  EXPECT_FALSE(GetTabDataAtIndex(browser(), 0)->HasMessage());
  observer()->DisplayPersistentMessage(message);
  EXPECT_FALSE(GetTabDataAtIndex(browser(), 0)->HasMessage());
}

IN_PROC_BROWSER_TEST_P(CollaborationMessagingObserverBrowserTest,
                       InstantMessageReopensTab) {
  WaitForTabGroupSyncServiceInitialized();

  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  // Observer is initialized
  EXPECT_NE(observer(), nullptr);

  auto group_id = browser()->tab_strip_model()->AddToNewGroup({0});
  base::MockCallback<SuccessCallback> cb;
  std::string test_url = chrome::kChromeUISettingsURL;
  auto message =
      CreateInstantMessage("User", CollaborationEvent::TAB_REMOVED, test_url,
                           "Chrome Settings", group_id, "Vacation");

  EXPECT_CALL(cb, Run(true));
  observer()->DisplayInstantaneousMessage(message, cb.Get());

  auto* toast_controller =
      browser()->browser_window_features()->toast_controller();
  EXPECT_TRUE(toast_controller->IsShowingToast());

  toast_controller->GetToastViewForTesting()
      ->action_button_for_testing()
      ->button_controller()
      ->NotifyClick();

  EXPECT_EQ(browser()->tab_strip_model()->GetWebContentsAt(1)->GetURL(),
            test_url);
}

IN_PROC_BROWSER_TEST_P(CollaborationMessagingObserverBrowserTest,
                       InstantMessageManagesSharing) {
  WaitForTabGroupSyncServiceInitialized();

  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  // Observer is initialized
  EXPECT_NE(observer(), nullptr);

  auto group_id = browser()->tab_strip_model()->AddToNewGroup({0});
  tab_groups::TabGroupSyncService* tab_group_service =
      TabGroupSyncServiceFactory::GetForProfile(browser()->profile());
  tab_group_service->MakeTabGroupSharedForTesting(
      group_id, syncer::CollaborationId("fake_collaboration_id"));
  base::MockCallback<SuccessCallback> cb;
  std::string test_url = chrome::kChromeUISettingsURL;
  auto message = CreateInstantMessage(
      "User", CollaborationEvent::COLLABORATION_MEMBER_ADDED, test_url,
      "Chrome Settings", group_id, "Vacation");

  EXPECT_CALL(cb, Run(true));
  observer()->DisplayInstantaneousMessage(message, cb.Get());

  auto* toast_controller =
      browser()->browser_window_features()->toast_controller();
  EXPECT_TRUE(toast_controller->IsShowingToast());
}

IN_PROC_BROWSER_TEST_P(CollaborationMessagingObserverBrowserTest,
                       InstantMessageManagesSharingWithClosedGroup) {
  WaitForTabGroupSyncServiceInitialized();

  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  // Observer is initialized
  EXPECT_NE(observer(), nullptr);

  // Create a new group, get the sync tab group id, close it.
  AddTab(browser());
  auto group_id = browser()->tab_strip_model()->AddToNewGroup({0});
  tab_groups::TabGroupSyncService* tab_group_service =
      TabGroupSyncServiceFactory::GetForProfile(browser()->profile());
  auto sync_tab_group_id = tab_group_service->GetGroup(group_id)->saved_guid();
  tab_group_service->MakeTabGroupSharedForTesting(
      group_id, syncer::CollaborationId("fake_collaboration_id"));
  browser()->tab_strip_model()->CloseAllTabsInGroup(group_id);

  // Create an instant message with sync tab group id.
  base::MockCallback<SuccessCallback> cb;
  std::string test_url = chrome::kChromeUISettingsURL;
  auto message = CreateInstantMessage(
      "User", CollaborationEvent::COLLABORATION_MEMBER_ADDED, test_url,
      "Chrome Settings", std::nullopt, "Vacation");
  message.attributions[0].tab_group_metadata->sync_tab_group_id =
      sync_tab_group_id;

  EXPECT_CALL(cb, Run(true));
  observer()->DisplayInstantaneousMessage(message, cb.Get());

  auto* toast_controller =
      browser()->browser_window_features()->toast_controller();
  EXPECT_TRUE(toast_controller->IsShowingToast());

  // Ensure tab group is closed.
  EXPECT_FALSE(tab_group_service->GetGroup(sync_tab_group_id)
                   ->local_group_id()
                   .has_value());

  toast_controller->GetToastViewForTesting()
      ->action_button_for_testing()
      ->button_controller()
      ->NotifyClick();

  // Ensure tab group is open.
  EXPECT_TRUE(tab_group_service->GetGroup(sync_tab_group_id)
                  ->local_group_id()
                  .has_value());
}

IN_PROC_BROWSER_TEST_P(CollaborationMessagingObserverBrowserTest,
                       InstantMessageForTabGroupRemoved) {
  WaitForTabGroupSyncServiceInitialized();

  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  // Observer is initialized
  EXPECT_NE(observer(), nullptr);

  auto group_id = browser()->tab_strip_model()->AddToNewGroup({0});
  base::MockCallback<SuccessCallback> cb;
  std::string test_url = chrome::kChromeUISettingsURL;
  auto message =
      CreateInstantMessage("User", CollaborationEvent::TAB_GROUP_REMOVED,
                           test_url, "Chrome Settings", group_id, "Vacation");

  EXPECT_CALL(cb, Run(true));
  observer()->DisplayInstantaneousMessage(message, cb.Get());

  auto* toast_controller =
      browser()->browser_window_features()->toast_controller();
  EXPECT_TRUE(toast_controller->IsShowingToast());
}

INSTANTIATE_TEST_SUITE_P(
    ,
    CollaborationMessagingObserverBrowserTest,
    ::testing::Values(
        CollaborationMessagingInteractiveTestParams{
            .page_actions_migration_enabled = false,
        },
        CollaborationMessagingInteractiveTestParams{
            .page_actions_migration_enabled = true,
        }),
    [](const ::testing::TestParamInfo<
        CollaborationMessagingObserverBrowserTest::ParamType>& info) {
      return base::StrCat({
          info.param.page_actions_migration_enabled ? "NewPageAction"
                                                    : "OriginalPageAction",
      });
    });

}  // namespace tab_groups
