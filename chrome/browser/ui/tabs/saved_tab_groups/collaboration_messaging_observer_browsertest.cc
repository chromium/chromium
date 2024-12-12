// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/collaboration_messaging_observer.h"

#include <optional>

#include "chrome/browser/collaboration/messaging/messaging_backend_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/collaboration_messaging_observer_factory.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/collaboration_messaging_tab_data.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_icon.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/collaboration/public/messaging/messaging_backend_service.h"
#include "components/data_sharing/public/features.h"
#include "components/saved_tab_groups/public/features.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using collaboration::messaging::CollaborationEvent;
using collaboration::messaging::MessagingBackendService;
using collaboration::messaging::MessagingBackendServiceFactory;
using collaboration::messaging::PersistentMessage;
using collaboration::messaging::PersistentNotificationType;
using testing::_;

namespace tab_groups {
namespace {

PersistentMessage CreateMessage(std::string given_name,
                                std::string avatar_url,
                                CollaborationEvent event,
                                PersistentNotificationType type,
                                tab_groups::LocalTabID tab_id,
                                tab_groups::LocalTabGroupID tab_group_id) {
  using collaboration::messaging::MessageAttribution;
  using collaboration::messaging::TabGroupMessageMetadata;
  using collaboration::messaging::TabMessageMetadata;
  using data_sharing::GroupMember;

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

}  // namespace

class CollaborationMessagingObserverBrowserTest
    : public InteractiveBrowserTest {
 public:
  CollaborationMessagingObserverBrowserTest() {
    features_.InitWithFeatures(
        {
            tab_groups::kTabGroupsSaveV2,
            tab_groups::kTabGroupSyncServiceDesktopMigration,
            data_sharing::features::kDataSharingFeature,
        },
        {});
  }
  ~CollaborationMessagingObserverBrowserTest() override = default;

 protected:
  CollaborationMessagingObserver* observer() {
    // All browsers in these tests share the same profile.
    return CollaborationMessagingObserverFactory::GetForProfile(
        browser()->profile());
  }

  TabStrip* GetTabStrip(Browser* target_browser) {
    return BrowserView::GetBrowserViewForBrowser(target_browser)->tabstrip();
  }

  TabIcon* GetTabIcon(Browser* target_browser, int index) {
    return GetTabStrip(target_browser)->tab_at(index)->GetTabIconForTesting();
  }

  TabGroupHeader* GetTabGroupHeader(Browser* target_browser,
                                    const tab_groups::TabGroupId index) {
    return GetTabStrip(target_browser)->group_header(index);
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

 private:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(CollaborationMessagingObserverBrowserTest,
                       HandlesMessages) {
  // Add 4 more tabs, for a total of 5.
  AddTabs(browser(), 4);
  EXPECT_EQ(5, browser()->tab_strip_model()->count());
  EXPECT_TRUE(browser()->IsActive());

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
  GetTabStrip(browser())->ToggleTabGroupCollapsedState(
      group_id, ToggleTabGroupCollapsedStateOrigin::kMenuAction);

  // DIRTY_TAB_GROUP messages set attention on tab group header
  auto dirty_tab_group_message = CreateMessage(
      "User", "URL", CollaborationEvent::TAB_ADDED,
      PersistentNotificationType::DIRTY_TAB_GROUP, tab2_id, group_id);
  EXPECT_FALSE(
      GetTabGroupHeader(browser(), group_id)->GetShowingAttentionIndicator());
  observer()->DisplayPersistentMessage(dirty_tab_group_message);
  EXPECT_TRUE(
      GetTabGroupHeader(browser(), group_id)->GetShowingAttentionIndicator());
  observer()->HidePersistentMessage(dirty_tab_group_message);
  EXPECT_FALSE(
      GetTabGroupHeader(browser(), group_id)->GetShowingAttentionIndicator());
}

IN_PROC_BROWSER_TEST_F(CollaborationMessagingObserverBrowserTest,
                       HandlesTabMessagesInCollapsedGroup) {
  // Add 2 more tabs, for a total of 3.
  AddTabs(browser(), 2);
  EXPECT_EQ(3, browser()->tab_strip_model()->count());

  // Observer is initialized
  EXPECT_NE(observer(), nullptr);

  // Group 2 tabs and collapse group.
  auto group_id = browser()->tab_strip_model()->AddToNewGroup({1, 2});
  GetTabStrip(browser())->ToggleTabGroupCollapsedState(
      group_id, ToggleTabGroupCollapsedStateOrigin::kMenuAction);

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

}  // namespace tab_groups
