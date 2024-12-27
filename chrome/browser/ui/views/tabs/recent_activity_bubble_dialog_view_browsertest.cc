// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/recent_activity_bubble_dialog_view.h"

#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/collaboration_messaging_tab_data.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/collaboration/public/messaging/activity_log.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/widget/unique_widget_ptr.h"

using collaboration::messaging::ActivityLogItem;
using collaboration::messaging::CollaborationEvent;
using collaboration::messaging::MessageAttribution;
using collaboration::messaging::RecentActivityAction;
using collaboration::messaging::TabGroupMessageMetadata;
using collaboration::messaging::TabMessageMetadata;
using data_sharing::GroupMember;

namespace {
// Copied from
// components/collaboration/internal/messaging/messaging_backend_service_impl.cc
RecentActivityAction GetRecentActivityActionFromCollaborationEvent(
    CollaborationEvent event) {
  switch (event) {
    case CollaborationEvent::TAB_ADDED:
    case CollaborationEvent::TAB_UPDATED:
      return RecentActivityAction::kFocusTab;
    case CollaborationEvent::TAB_REMOVED:
      return RecentActivityAction::kReopenTab;
    case CollaborationEvent::TAB_GROUP_ADDED:
    case CollaborationEvent::TAB_GROUP_REMOVED:
      return RecentActivityAction::kNone;
    case CollaborationEvent::TAB_GROUP_NAME_UPDATED:
    case CollaborationEvent::TAB_GROUP_COLOR_UPDATED:
      return RecentActivityAction::kOpenTabGroupEditDialog;
    case CollaborationEvent::COLLABORATION_ADDED:
    case CollaborationEvent::COLLABORATION_REMOVED:
      return RecentActivityAction::kNone;
    case CollaborationEvent::COLLABORATION_MEMBER_ADDED:
    case CollaborationEvent::COLLABORATION_MEMBER_REMOVED:
      return RecentActivityAction::kManageSharing;
    case CollaborationEvent::UNDEFINED:
      return RecentActivityAction::kNone;
  }
}

// Helper to create mock log items.
collaboration::messaging::ActivityLogItem CreateMockActivityItem(
    std::string_view name,
    std::string_view avatar_url,
    std::string_view last_url,
    std::string_view last_url_description,
    base::TimeDelta time,
    tab_groups::CollaborationEvent event,
    bool is_self) {
  GroupMember trig_member;
  trig_member.given_name = name;
  trig_member.avatar_url = GURL(avatar_url);

  GroupMember aff_member;
  aff_member.given_name = name;
  aff_member.avatar_url = GURL(avatar_url);

  TabMessageMetadata tab_metadata;
  tab_metadata.last_known_url = last_url;

  MessageAttribution attribution;
  attribution.triggering_user = trig_member;
  attribution.affected_user = aff_member;
  attribution.tab_metadata = tab_metadata;

  ActivityLogItem item;
  item.collaboration_event = event;
  item.user_display_name = name;
  item.user_is_self = is_self;
  item.description = base::UTF8ToUTF16(last_url_description);
  item.time_delta = base::TimeDelta(time);
  item.show_favicon = true;
  item.action =
      GetRecentActivityActionFromCollaborationEvent(item.collaboration_event);
  item.activity_metadata = attribution;
  return item;
}

// Helper to create mock log with all types of events.
std::vector<collaboration::messaging::ActivityLogItem>
CreateMockActivityLogWithAllTypes() {
  std::vector<ActivityLogItem> result;

  result.emplace_back(CreateMockActivityItem(
      "Me", "https://www.google.com/avatar/1", "https://www.google.com/1",
      "airbnb.com", base::Minutes(5),
      tab_groups::CollaborationEvent::TAB_UPDATED, true));

  result.emplace_back(CreateMockActivityItem(
      "Shirley", "https://www.google.com/avatar/2", "https://www.google.com/2",
      "hotels.com", base::Hours(4), tab_groups::CollaborationEvent::TAB_UPDATED,
      false));

  result.emplace_back(CreateMockActivityItem(
      "Elisa", "https://www.google.com/avatar/3", "https://www.google.com/3",
      "expedia.com", base::Hours(6),
      tab_groups::CollaborationEvent::TAB_REMOVED, false));

  result.emplace_back(CreateMockActivityItem(
      "Shirley", "https://www.google.com/avatar/2", "https://www.google.com/2",
      "shirleys-email", base::Hours(8),
      tab_groups::CollaborationEvent::COLLABORATION_MEMBER_ADDED, false));

  result.emplace_back(CreateMockActivityItem(
      "Elisa", "https://www.google.com/avatar/3", "https://www.google.com/3",
      "expedia.com", base::Days(2), tab_groups::CollaborationEvent::TAB_ADDED,
      false));

  return result;
}

// Helper to create a list of n items where the contents are not
// important for verification.
std::vector<ActivityLogItem> CreateMockActivityLog(int n) {
  std::vector<ActivityLogItem> result;
  for (int i = 0; i < n; i++) {
    // Choose random values to populate list.
    result.emplace_back(CreateMockActivityItem(
        "Me", "https://www.google.com/avatar/1", "https://www.google.com/1",
        "airbnb.com", base::Minutes(5),
        tab_groups::CollaborationEvent::TAB_UPDATED, true));
  }
  return result;
}
}  // namespace

class RecentActivityBubbleDialogViewUnitTest : public DialogBrowserTest {
 public:
  void ShowUi(const std::string& name) override {
    std::vector<ActivityLogItem> activity_log;
    if (name == "WithOneItem") {
      activity_log = CreateMockActivityLog(1);
    } else if (name == "WithFullList") {
      activity_log = CreateMockActivityLog(5);
    } else if (name == "WithTooManyItems") {
      activity_log = CreateMockActivityLog(10);
    }
    ShowLog(activity_log);
  }

  void ShowLog(std::vector<ActivityLogItem> activity_log) {
    // Anchor to top container for tests.
    views::View* anchor_view =
        BrowserView::GetBrowserViewForBrowser(browser())->top_container();

    bubble_coordinator_ = std::make_unique<RecentActivityBubbleCoordinator>();
    EXPECT_EQ(nullptr, bubble_coordinator_->GetBubble());
    bubble_coordinator_->Show(anchor_view,
                              browser()->tab_strip_model()->GetWebContentsAt(0),
                              activity_log, browser()->profile());
  }

  bool VerifyUi() override {
    EXPECT_TRUE(bubble_coordinator_->IsShowing());
    EXPECT_NE(nullptr, bubble_coordinator_->GetBubble());
    auto children = bubble_coordinator_->GetBubble()->children();

    std::string test_name =
        testing::UnitTest::GetInstance()->current_test_info()->name();

    if (test_name == "InvokeUi_WithOneItem") {
      EXPECT_EQ(1u, children.size());
    } else {
      // All other tests expect a complete list of 5 items.
      EXPECT_EQ(5u, children.size());
    }

    return true;
  }

  void DismissUi() override { bubble_coordinator_->Hide(); }

  RecentActivityBubbleCoordinator* BubbleCoordinator() {
    return bubble_coordinator_.get();
  }

 private:
  views::UniqueWidgetPtr anchor_widget_;
  std::unique_ptr<RecentActivityBubbleCoordinator> bubble_coordinator_;
};

IN_PROC_BROWSER_TEST_F(RecentActivityBubbleDialogViewUnitTest,
                       InvokeUi_WithOneItem) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(RecentActivityBubbleDialogViewUnitTest,
                       InvokeUi_WithFullList) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(RecentActivityBubbleDialogViewUnitTest,
                       InvokeUi_WithTooManyItems) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(RecentActivityBubbleDialogViewUnitTest, ShowsAllTypes) {
  auto activity_log = CreateMockActivityLogWithAllTypes();
  ShowLog(activity_log);

  auto* bubble = BubbleCoordinator()->GetBubble();
  EXPECT_TRUE(bubble);

#if BUILDFLAG(IS_MAC)
  // Initial caps on Mac.
  EXPECT_EQ(bubble->GetWindowTitle(), u"Recent Activity");
#else
  EXPECT_EQ(bubble->GetWindowTitle(), u"Recent activity");
#endif

  EXPECT_EQ(bubble->GetRowForTesting(0)->activity_text(), u"You changed a tab");
  EXPECT_EQ(bubble->GetRowForTesting(0)->metadata_text(),
            u"airbnb.com \u2022 5m ago");

  EXPECT_EQ(bubble->GetRowForTesting(1)->activity_text(),
            u"Shirley changed a tab");
  EXPECT_EQ(bubble->GetRowForTesting(1)->metadata_text(),
            u"hotels.com \u2022 4h ago");

  EXPECT_EQ(bubble->GetRowForTesting(2)->activity_text(),
            u"Elisa removed a tab");
  EXPECT_EQ(bubble->GetRowForTesting(2)->metadata_text(),
            u"expedia.com \u2022 6h ago");

  EXPECT_EQ(bubble->GetRowForTesting(3)->activity_text(),
            u"Shirley joined the group");
  EXPECT_EQ(bubble->GetRowForTesting(3)->metadata_text(),
            u"shirleys-email \u2022 8h ago");

  EXPECT_EQ(bubble->GetRowForTesting(4)->activity_text(), u"Elisa added a tab");
  EXPECT_EQ(bubble->GetRowForTesting(4)->metadata_text(),
            u"expedia.com \u2022 2d ago");
}
