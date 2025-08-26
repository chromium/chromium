// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/collaboration_messaging_page_action_controller.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/collaboration_messaging_observer.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/page_action/test_support/page_action_interactive_test_mixin.h"
#include "chrome/browser/ui/views/tabs/recent_activity_bubble_dialog_view.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/data_sharing/public/features.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/tabs/public/tab_group.h"
#include "content/public/test/browser_test.h"

namespace {
tab_groups::PersistentMessage CreateChipMessage(
    std::string given_name,
    tab_groups::CollaborationEvent event,
    tabs::TabInterface* tab) {
  using collaboration::messaging::MessageAttribution;
  using collaboration::messaging::PersistentNotificationType;
  using collaboration::messaging::TabGroupMessageMetadata;
  using collaboration::messaging::TabMessageMetadata;
  using data_sharing::GroupMember;

  auto tab_id = tab->GetHandle().raw_value();
  auto tab_group_id = tab->GetGroup();

  GroupMember member;
  member.given_name = given_name;
  member.avatar_url = GURL("");

  TabMessageMetadata tab_metadata;
  tab_metadata.local_tab_id = tab_id;

  TabGroupMessageMetadata tab_group_metadata;
  tab_group_metadata.local_tab_group_id = tab_group_id;

  MessageAttribution attribution;
  attribution.triggering_user = member;
  attribution.tab_metadata = tab_metadata;
  attribution.tab_group_metadata = tab_group_metadata;

  tab_groups::PersistentMessage message;
  message.type = PersistentNotificationType::CHIP;
  message.attribution = attribution;
  message.collaboration_event = event;

  return message;
}
}  // namespace

class CollaborationMessagingPageActionControllerBrowserTest
    : public PageActionInteractiveTestMixin<InteractiveBrowserTest> {
 public:
  CollaborationMessagingPageActionControllerBrowserTest() {
    std::vector<base::test::FeatureRefAndParams> enabled_features = {
        {data_sharing::features::kDataSharingFeature, {}},
        {
            features::kPageActionsMigration,
            {
                {
                    features::kPageActionsMigrationCollaborationMessaging.name,
                    "true",
                },
            },
        }};

    features_.InitWithFeaturesAndParameters(enabled_features, {});
  }
  ~CollaborationMessagingPageActionControllerBrowserTest() override = default;

 protected:
  tabs::TabInterface* GetTabInterface(Browser* target_browser, int index) {
    return target_browser->tab_strip_model()->GetTabAtIndex(index);
  }

  CollaborationMessagingPageActionController* GetControllerAtIndex(
      Browser* target_browser,
      int index) {
    return CollaborationMessagingPageActionController::From(
        GetTabInterface(target_browser, index));
  }

  tab_groups::CollaborationMessagingTabData* GetTabDataAtIndex(
      Browser* target_browser,
      int index) {
    return GetTabInterface(target_browser, index)
        ->GetTabFeatures()
        ->collaboration_messaging_tab_data();
  }

  RecentActivityBubbleCoordinator* GetBubbleCoordinator(
      Browser* target_browser) {
    return RecentActivityBubbleCoordinator::From(target_browser);
  }

  using PageActionInteractiveTestMixin::WaitForPageActionChipVisible;

  auto WaitForPageActionChipVisible() {
    MultiStep steps;
    steps +=
        WaitForPageActionChipVisible(kActionShowCollaborationRecentActivity);
    return steps;
  }

 private:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(CollaborationMessagingPageActionControllerBrowserTest,
                       ShowsBubbleView) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

  TabStripModel* model = browser()->tab_strip_model();
  tab_groups::TabGroupId group = model->AddToNewGroup({0});

  EXPECT_EQ(1, model->count());
  EXPECT_EQ(1u, model->group_model()->GetTabGroup(group)->ListTabs().length());

  auto* tab = browser()->tab_strip_model()->GetActiveTab();
  auto message =
      CreateChipMessage("User", tab_groups::CollaborationEvent::TAB_ADDED, tab);

  tab_groups::CollaborationMessagingTabData* tab_data =
      GetTabDataAtIndex(browser(), 0);

  EXPECT_FALSE(GetBubbleCoordinator(browser())->IsShowing());

  RunTestSequence(
      Do([&]() {
        // Dispatch "added" message.
        tab_data->set_mocked_avatar_for_testing(favicon::GetDefaultFavicon());
        tab_data->SetMessage(message);
      }),
      WaitForPageActionChipVisible(),
      PressButton(kCollaborationMessagingPageActionIconElementId),
      Check([this]() { return GetBubbleCoordinator(browser())->IsShowing(); },
            "Ensure the coordinator believes the bubble is showing."));
}
