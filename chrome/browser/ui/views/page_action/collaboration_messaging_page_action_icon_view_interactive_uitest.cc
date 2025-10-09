// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/collaboration_messaging_observer.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/collaboration_messaging_observer_factory.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/collaboration_messaging_tab_data.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/page_action/collaboration_messaging_page_action_icon_view.h"
#include "chrome/browser/ui/views/page_action/test_support/page_action_interactive_test_mixin.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/collaboration/public/messaging/message.h"
#include "components/data_sharing/public/features.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/tabs/public/tab_group.h"
#include "content/public/test/browser_test.h"
#include "ui/views/interaction/interactive_views_test.h"

using collaboration::messaging::CollaborationEvent;
using collaboration::messaging::PersistentMessage;

namespace {

constexpr char kSkipPixelTestsReason[] = "Should only run in pixel_tests.";

PersistentMessage CreateChipMessage(std::string given_name,
                                    CollaborationEvent event,
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

  PersistentMessage message;
  message.type = PersistentNotificationType::CHIP;
  message.attribution = attribution;
  message.collaboration_event = event;

  return message;
}

}  // namespace

struct CollaborationMessagingPageActionIconInteractiveTestParams {
  bool page_actions_migration_enabled = false;
};

class CollaborationMessagingPageActionIconViewInteractiveTest
    : public PageActionInteractiveTestMixin<InteractiveBrowserTest>,
      public ::testing::WithParamInterface<
          CollaborationMessagingPageActionIconInteractiveTestParams> {
 public:
  CollaborationMessagingPageActionIconViewInteractiveTest() {
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

 protected:
  bool IsPageActionsMigrationEnabled() {
    return IsPageActionMigrated(PageActionIconType::kCollaborationMessaging);
  }

  using PageActionInteractiveTestMixin::WaitForPageActionChipVisible;

  auto WaitForPageActionToShow() {
    MultiStep steps;
    if (IsPageActionsMigrationEnabled()) {
      steps +=
          WaitForPageActionChipVisible(kActionShowCollaborationRecentActivity);
    } else {
      steps += WaitForShow(kCollaborationMessagingPageActionIconElementId);
    }
    return steps;
  }

  auto CheckLabelText(const std::u16string expected_string) {
    MultiStep steps;
    if (IsPageActionsMigrationEnabled()) {
      steps +=
          WaitForPageActionChipVisible(kActionShowCollaborationRecentActivity);
    }
    steps += CheckView(
        kCollaborationMessagingPageActionIconElementId,
        [](IconLabelBubbleView* icon) { return icon->GetText(); },
        expected_string);
    return steps;
  }

 private:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_P(CollaborationMessagingPageActionIconViewInteractiveTest,
                       ShowPageActionWithAvatarFallback) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

  TabStripModel* model = browser()->tab_strip_model();
  tab_groups::TabGroupId group = model->AddToNewGroup({0});

  EXPECT_EQ(1, model->count());
  EXPECT_EQ(1u, model->group_model()->GetTabGroup(group)->ListTabs().length());

  auto* collaboration_message_observer =
      tab_groups::CollaborationMessagingObserverFactory::GetForProfile(
          browser()->profile());

  auto* tab = browser()->tab_strip_model()->GetActiveTab();
  auto message = CreateChipMessage("User", CollaborationEvent::TAB_ADDED, tab);

  RunTestSequence(
      Do([&]() {
        // Dispatch "added" message.
        collaboration_message_observer->DispatchMessageForTests(
            message, /*display=*/true);
      }),
      WaitForPageActionToShow(),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              kSkipPixelTestsReason),
      Screenshot(kCollaborationMessagingPageActionIconElementId,
                 "page_action_with_avatar_fallback", "6313918"),
      Do([&]() {
        // Hide message.
        collaboration_message_observer->DispatchMessageForTests(
            message, /*display=*/false);
      }),
      WaitForHide(kCollaborationMessagingPageActionIconElementId));
}

IN_PROC_BROWSER_TEST_P(CollaborationMessagingPageActionIconViewInteractiveTest,
                       ReactsToChangesInTabData) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

  const std::u16string expected_added_string =
      base::UTF8ToUTF16(std::string("Added this tab"));
  const std::u16string expected_updated_string =
      base::UTF8ToUTF16(std::string("Changed this tab"));

  TabStripModel* model = browser()->tab_strip_model();
  tab_groups::TabGroupId group = model->AddToNewGroup({0});

  EXPECT_EQ(1, model->count());
  EXPECT_EQ(1u, model->group_model()->GetTabGroup(group)->ListTabs().length());

  auto* collaboration_message_observer =
      tab_groups::CollaborationMessagingObserverFactory::GetForProfile(
          browser()->profile());

  auto* tab = browser()->tab_strip_model()->GetActiveTab();
  auto message = CreateChipMessage("User", CollaborationEvent::TAB_ADDED, tab);

  RunTestSequence(Do([&]() {
                    // Dispatch "added" message.
                    collaboration_message_observer->DispatchMessageForTests(
                        message, /*display=*/true);
                  }),
                  WaitForPageActionToShow(),
                  // Text shows the "added" string.
                  CheckLabelText(expected_added_string), Do([&]() {
                    // Change to an "update" message and dispatch.
                    message.collaboration_event =
                        CollaborationEvent::TAB_UPDATED;
                    collaboration_message_observer->DispatchMessageForTests(
                        message, /*display=*/true);
                  }),
                  // Text changes to the "updated" string.
                  CheckLabelText(expected_updated_string), Do([&]() {
                    // Hide message.
                    collaboration_message_observer->DispatchMessageForTests(
                        message, /*display=*/false);
                  }),
                  WaitForHide(kCollaborationMessagingPageActionIconElementId));
}

INSTANTIATE_TEST_SUITE_P(
    ,
    CollaborationMessagingPageActionIconViewInteractiveTest,
    ::testing::Values(
        CollaborationMessagingPageActionIconInteractiveTestParams{
            .page_actions_migration_enabled = false,
        },
        CollaborationMessagingPageActionIconInteractiveTestParams{
            .page_actions_migration_enabled = true,
        }),
    [](const ::testing::TestParamInfo<
        CollaborationMessagingPageActionIconViewInteractiveTest::ParamType>&
           info) {
      return base::StrCat({
          info.param.page_actions_migration_enabled ? "NewPageAction"
                                                    : "OriginalPageAction",
      });
    });
