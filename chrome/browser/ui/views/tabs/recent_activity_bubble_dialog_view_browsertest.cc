// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/recent_activity_bubble_dialog_view.h"

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "chrome/browser/data_sharing/data_sharing_service_factory.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/collaboration_messaging_tab_data.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/data_sharing/data_sharing_bubble_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/tabs/tab_group_editor_bubble_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/collaboration/public/features.h"
#include "components/collaboration/public/messaging/activity_log.h"
#include "components/data_sharing/public/features.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/signin/public/base/avatar_icon_util.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/widget/unique_widget_ptr.h"

using collaboration::messaging::ActivityLogItem;
using collaboration::messaging::CollaborationEvent;
using collaboration::messaging::MessageAttribution;
using collaboration::messaging::RecentActivityAction;
using collaboration::messaging::TabGroupMessageMetadata;
using collaboration::messaging::TabMessageMetadata;
using data_sharing::GroupMember;

namespace tab_groups {
namespace {

const int kAvatarSize = signin::kAccountInfoImageSize;

// Create mock gfx::Image and convert to a string.
std::string CreateSerializedAvatar() {
  auto avatar = gfx::ImageSkiaOperations::CreateImageWithRoundRectClip(
      kAvatarSize,
      gfx::test::CreateImage(kAvatarSize, SK_ColorBLUE).AsImageSkia());

  std::optional<std::vector<uint8_t>> compressed_avatar =
      gfx::PNGCodec::EncodeBGRASkBitmap(*avatar.bitmap(),
                                        /*discard_transparency=*/false);

  std::string avatar_string(base::as_string_view(compressed_avatar.value()));

  return avatar_string;
}

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

// On tab events will show the favicon.
bool GetShowFaviconFromCollaborationEvent(CollaborationEvent event) {
  switch (event) {
    case CollaborationEvent::TAB_ADDED:
    case CollaborationEvent::TAB_UPDATED:
    case CollaborationEvent::TAB_REMOVED:
      return true;
    default:
      return false;
  }
}

// Helper to create mock log items.
collaboration::messaging::ActivityLogItem CreateMockActivityItem(
    std::string_view name,
    std::string_view avatar_url,
    std::string_view last_url,
    std::string_view last_url_description,
    std::string_view time_delta,
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
  item.title_text = base::UTF8ToUTF16(name);
  item.description_text = base::UTF8ToUTF16(last_url_description);
  item.time_delta_text = base::UTF8ToUTF16(time_delta);
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
      "You changed a tab", "https://www.google.com/avatar/1",
      "https://www.google.com/1", "airbnb.com", "5h ago",
      tab_groups::CollaborationEvent::TAB_UPDATED, true));

  result.emplace_back(CreateMockActivityItem(
      "Shirley changed a tab", "https://www.google.com/avatar/2",
      "https://www.google.com/2", "hotels.com", "4h ago",
      tab_groups::CollaborationEvent::TAB_UPDATED, false));

  result.emplace_back(CreateMockActivityItem(
      "Elisa removed a tab", "https://www.google.com/avatar/3",
      "https://www.google.com/3", "expedia.com", "6h ago",
      tab_groups::CollaborationEvent::TAB_REMOVED, false));

  result.emplace_back(CreateMockActivityItem(
      "Shirley joined the group", "https://www.google.com/avatar/2",
      "https://www.google.com/2", "shirleys-email", "8h ago",
      tab_groups::CollaborationEvent::COLLABORATION_MEMBER_ADDED, false));

  result.emplace_back(CreateMockActivityItem(
      "Elisa added a tab", "https://www.google.com/avatar/3",
      "https://www.google.com/3", "expedia.com", "2d ago",
      tab_groups::CollaborationEvent::TAB_ADDED, false));

  return result;
}

// Helper to create a list of n items where the contents are not
// important for verification.
std::vector<ActivityLogItem> CreateMockActivityLog(int n) {
  std::vector<ActivityLogItem> result;
  for (int i = 0; i < n; i++) {
    // Choose random values to populate list.
    result.emplace_back(CreateMockActivityItem(
        "You changed a tab", "https://www.google.com/avatar/1",
        "https://www.google.com/1", "airbnb.com", "5m ago",
        tab_groups::CollaborationEvent::TAB_UPDATED, true));
  }
  return result;
}
}  // namespace

class RecentActivityBubbleDialogViewBrowserTest : public DialogBrowserTest {
 public:
  void ShowUi(const std::string& name) override {
    if (name == "Empty") {
      ShowLog({});
    } else if (name == "WithOneItem") {
      ShowLog(CreateMockActivityLog(1));
    } else if (name == "WithFullList") {
      ShowLog(CreateMockActivityLog(5));
    } else if (name == "WithTooManyItems") {
      ShowLog(CreateMockActivityLog(10));
    } else if (name == "ForCurrentTab") {
      ShowLogForCurrentTab(CreateMockActivityLog(5));
    }
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

  void ShowLogForCurrentTab(std::vector<ActivityLogItem> activity_log) {
    // Anchor to top container for tests.
    views::View* anchor_view =
        BrowserView::GetBrowserViewForBrowser(browser())->top_container();

    bubble_coordinator_ = std::make_unique<RecentActivityBubbleCoordinator>();
    EXPECT_EQ(nullptr, bubble_coordinator_->GetBubble());
    bubble_coordinator_->ShowForCurrentTab(
        anchor_view, browser()->tab_strip_model()->GetWebContentsAt(0), {},
        activity_log, browser()->profile());
  }

  bool VerifyUi() override {
    EXPECT_TRUE(bubble_coordinator_->IsShowing());
    EXPECT_NE(nullptr, bubble_coordinator_->GetBubble());
    auto* bubble = bubble_coordinator_->GetBubble();
    auto children = bubble->children();

    std::string test_name =
        testing::UnitTest::GetInstance()->current_test_info()->name();

    // Containers are always created
    EXPECT_TRUE(bubble->tab_activity_container());
    EXPECT_TRUE(bubble->group_activity_container());

    // All dialogs have 4 children, except for empty state dialog, which
    // also contains the label for the empty state.
    if (test_name == "InvokeUi_Empty") {
      EXPECT_EQ(6u, children.size());
    } else {
      EXPECT_EQ(5u, children.size());
    }

    // Tab container empty and hidden.
    EXPECT_FALSE(bubble->tab_activity_container()->GetVisible());
    EXPECT_EQ(0u, bubble->tab_activity_container()->children().size());

    if (test_name == "InvokeUi_Empty") {
      // Group container empty and hidden.
      EXPECT_FALSE(bubble->group_activity_container()->GetVisible());
      EXPECT_EQ(0u, bubble->group_activity_container()->children().size());
    } else if (test_name == "InvokeUi_WithOneItem") {
      // Group container is visible with one child.
      EXPECT_TRUE(bubble->group_activity_container()->GetVisible());
      EXPECT_EQ(1u, bubble->group_activity_container()->children().size());
    } else {
      // All other tests expect a complete list of 5 items in the group
      // container.
      EXPECT_TRUE(bubble->group_activity_container()->GetVisible());
      EXPECT_EQ(5u, bubble->group_activity_container()->children().size());
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

IN_PROC_BROWSER_TEST_F(RecentActivityBubbleDialogViewBrowserTest,
                       InvokeUi_Empty) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(RecentActivityBubbleDialogViewBrowserTest,
                       InvokeUi_WithOneItem) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(RecentActivityBubbleDialogViewBrowserTest,
                       InvokeUi_WithFullList) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(RecentActivityBubbleDialogViewBrowserTest,
                       InvokeUi_WithTooManyItems) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(RecentActivityBubbleDialogViewBrowserTest,
                       InvokeUi_ForCurrentTab) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(RecentActivityBubbleDialogViewBrowserTest,
                       ShowsAllTypes) {
  auto activity_log = CreateMockActivityLogWithAllTypes();
  ShowLog(activity_log);

  auto* bubble = BubbleCoordinator()->GetBubble();
  EXPECT_TRUE(bubble);

#if BUILDFLAG(IS_MAC)
  // Initial caps on Mac.
  EXPECT_EQ(bubble->GetTitleForTesting(), u"Recent Activity");
#else
  EXPECT_EQ(bubble->GetTitleForTesting(), u"Recent activity");
#endif

  EXPECT_EQ(bubble->GetRowForTesting(0)->GetAccessibleName(),
            u"You changed a tab");
  EXPECT_EQ(bubble->GetRowForTesting(0)->GetAccessibleDescription(),
            u"airbnb.com \u2022 5h ago");

  EXPECT_EQ(bubble->GetRowForTesting(1)->GetAccessibleName(),
            u"Shirley changed a tab");
  EXPECT_EQ(bubble->GetRowForTesting(1)->GetAccessibleDescription(),
            u"hotels.com \u2022 4h ago");

  EXPECT_EQ(bubble->GetRowForTesting(2)->GetAccessibleName(),
            u"Elisa removed a tab");
  EXPECT_EQ(bubble->GetRowForTesting(2)->GetAccessibleDescription(),
            u"expedia.com \u2022 6h ago");

  EXPECT_EQ(bubble->GetRowForTesting(3)->GetAccessibleName(),
            u"Shirley joined the group");
  EXPECT_EQ(bubble->GetRowForTesting(3)->GetAccessibleDescription(),
            u"shirleys-email \u2022 8h ago");

  EXPECT_EQ(bubble->GetRowForTesting(4)->GetAccessibleName(),
            u"Elisa added a tab");
  EXPECT_EQ(bubble->GetRowForTesting(4)->GetAccessibleDescription(),
            u"expedia.com \u2022 2d ago");
}

class RecentActivityBubbleDialogViewActionBrowserTest
    : public RecentActivityBubbleDialogViewBrowserTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {tab_groups::kTabGroupSyncServiceDesktopMigration,
         data_sharing::features::kDataSharingFeature,
         collaboration::features::kCollaborationMessaging},
        {});
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &RecentActivityBubbleDialogViewActionBrowserTest::HandleRequest,
        base::Unretained(this)));
    RecentActivityBubbleDialogViewBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    DialogBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->StartAcceptingConnections();
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    DialogBrowserTest::TearDownOnMainThread();
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    GURL absolute_url = embedded_test_server()->GetURL(request.relative_url);
    if (absolute_url.path() != avatar_url_) {
      return nullptr;
    }

    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HTTP_OK);
    http_response->set_content(CreateSerializedAvatar());
    return http_response;
  }

  void WaitForAvatar(int log_index) {
    CHECK(!run_loop_);
    run_loop_ = std::make_unique<base::RunLoop>();
    PollForAvatarLoaded(10, log_index);
    run_loop_->Run();
    run_loop_.reset();
  }

  void PollForAvatarLoaded(int tries_left, int log_index) {
    auto* bubble = BubbleCoordinator()->GetBubble();

    CHECK(tries_left > 0);
    CHECK(run_loop_);
    CHECK(bubble);

    // If activity row should show avatar, loading is complete.
    if (bubble->GetRowForTesting(log_index)->image_view()->ShouldShowAvatar()) {
      run_loop_->Quit();
    } else {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&RecentActivityBubbleDialogViewActionBrowserTest::
                             PollForAvatarLoaded,
                         base::Unretained(this), tries_left - 1, log_index),
          base::Milliseconds(200));
    }
  }

  tabs::TabInterface* CreateTab(
      GURL tab_url = GURL(chrome::kChromeUINewTabPageURL)) {
    auto index = browser()->tab_strip_model()->count();
    CHECK(AddTabAtIndex(index, tab_url, ui::PAGE_TRANSITION_TYPED));
    auto* tab = browser()->tab_strip_model()->GetTabAtIndex(index);
    CHECK(tab);
    return tab;
  }

  void CloseTab(tabs::TabInterface* tab) {
    auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
    auto* tabstrip_controller = browser_view->tabstrip()->controller();
    tabstrip_controller->CloseTab(TabIndex(tab));
  }

  LocalTabID TabId(tabs::TabInterface* tab) {
    return tab->GetHandle().raw_value();
  }

  int TabIndex(tabs::TabInterface* tab) {
    return browser()->tab_strip_model()->GetIndexOfTab(tab);
  }

  const TabGroupId CreateTabGroup(std::vector<tabs::TabInterface*> tabs) {
    std::vector<int> tab_indices = {};
    for (auto* tab : tabs) {
      tab_indices.emplace_back(
          browser()->tab_strip_model()->GetIndexOfTab(tab));
    }
    return browser()->tab_strip_model()->AddToNewGroup(tab_indices);
  }

  SavedTabGroup ShareTabGroup(TabGroupId group_id) {
    std::string collaboration_id = "fake_collaboration_id";
    TabGroupSyncService* tab_group_sync_service =
        TabGroupSyncServiceFactory::GetForProfile(browser()->profile());
    tab_group_sync_service->MakeTabGroupSharedForTesting(group_id,
                                                         collaboration_id);
    auto saved_tab_group = tab_group_sync_service->GetGroup(group_id);
    CHECK(saved_tab_group.has_value());
    return saved_tab_group.value();
  }

  GURL GetAvatarURL() { return embedded_test_server()->GetURL(avatar_url_); }

  // Create a mock message for the group and tab. This fills in just
  // enough information for RecentActivity to behave correctly.
  ActivityLogItem CreateActivityForTab(
      SavedTabGroup group,
      tabs::TabInterface* tab,
      CollaborationEvent collaboration_event = CollaborationEvent::TAB_ADDED,
      bool force_no_favicon = false) {
    GroupMember member;
    member.avatar_url = GetAvatarURL();

    TabMessageMetadata tab_metadata;
    tab_metadata.last_known_url = tab->GetContents()->GetURL().spec();
    tab_metadata.local_tab_id = TabId(tab);

    TabGroupMessageMetadata tab_group_metadata;
    tab_group_metadata.local_tab_group_id = group.local_group_id();

    MessageAttribution attribution;
    attribution.triggering_user = member;
    attribution.affected_user = member;
    attribution.tab_metadata = tab_metadata;
    attribution.tab_group_metadata = tab_group_metadata;
    attribution.collaboration_id =
        data_sharing::GroupId(group.collaboration_id().value().value());

    ActivityLogItem item;
    item.collaboration_event = collaboration_event;
    item.title_text = u"User added this tab";
    item.description_text = u"google.com";
    item.time_delta_text = u"2h ago";
    item.show_favicon =
        force_no_favicon
            ? false
            : GetShowFaviconFromCollaborationEvent(collaboration_event);
    item.action =
        GetRecentActivityActionFromCollaborationEvent(collaboration_event);
    item.activity_metadata = attribution;

    return item;
  }

 private:
  const std::string avatar_url_ =
      base::StringPrintf("/avatar=s%d-cc-rp-ns", kAvatarSize);
  std::unique_ptr<base::RunLoop> run_loop_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Trigger kFocusTab action from the recent activity dialog.
IN_PROC_BROWSER_TEST_F(RecentActivityBubbleDialogViewActionBrowserTest,
                       HandlesActionFocusTab) {
  GURL tab1_url = GURL("chrome://about");
  GURL tab2_url = GURL("chrome://settings");

  auto* tab1 = CreateTab(tab1_url);
  auto* tab2 = CreateTab(tab2_url);

  TabGroupId group_id = CreateTabGroup({tab1, tab2});
  auto saved_group = ShareTabGroup(group_id);

  std::vector<ActivityLogItem> activity_log = {
      // TAB_ADDED will contain the action to focus the specified tab.
      CreateActivityForTab(saved_group, tab1, CollaborationEvent::TAB_ADDED,
                           /*force_no_favicon=*/true),
  };
  ShowLog(activity_log);
  WaitForAvatar(/*log_index=*/0);

  auto* bubble = BubbleCoordinator()->GetBubble();
  EXPECT_TRUE(bubble);

  // Tab 2 starts active.
  EXPECT_EQ(tab2_url,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());

  bubble->GetRowForTesting(0)->FocusTab();

  // Now tab 1 is active.
  EXPECT_EQ(tab1_url,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

// Trigger kReopenTab action from the recent activity dialog.
IN_PROC_BROWSER_TEST_F(RecentActivityBubbleDialogViewActionBrowserTest,
                       HandlesActionReopenTab) {
  GURL tab1_url = GURL("chrome://about");
  GURL tab2_url = GURL("chrome://settings");

  auto* tab1 = CreateTab(tab1_url);
  auto* tab2 = CreateTab(tab2_url);
  CreateTab();  // Not used in group.

  TabGroupId group_id = CreateTabGroup({tab1, tab2});
  auto saved_group = ShareTabGroup(group_id);

  std::vector<ActivityLogItem> activity_log = {
      // TAB_REMOVED will contain the action to reopen the specified tab.
      CreateActivityForTab(saved_group, tab1, CollaborationEvent::TAB_REMOVED,
                           /*force_no_favicon=*/true),
  };
  ShowLog(activity_log);
  WaitForAvatar(/*log_index=*/0);

  auto* bubble = BubbleCoordinator()->GetBubble();
  EXPECT_TRUE(bubble);

  auto* group =
      browser()->tab_strip_model()->group_model()->GetTabGroup(group_id);

  // 4 tabs including 2 grouped.
  EXPECT_EQ(browser()->tab_strip_model()->count(), 4);
  EXPECT_EQ(group->tab_count(), 2);

  // Tab order is as it was created.
  EXPECT_EQ(browser()->tab_strip_model()->GetWebContentsAt(1)->GetURL(),
            tab1_url);
  EXPECT_EQ(browser()->tab_strip_model()->GetWebContentsAt(2)->GetURL(),
            tab2_url);

  // Close the first tab in the group.
  CloseTab(tab1);

  // 3 tabs including only 1 grouped.
  EXPECT_EQ(browser()->tab_strip_model()->count(), 3);
  EXPECT_EQ(group->tab_count(), 1);

  bubble->GetRowForTesting(0)->ReopenTab();

  // Tab order has switched because tab1 was opened at the end of the group.
  EXPECT_EQ(browser()->tab_strip_model()->GetWebContentsAt(1)->GetURL(),
            tab2_url);
  EXPECT_EQ(browser()->tab_strip_model()->GetWebContentsAt(2)->GetURL(),
            tab1_url);

  // 4 tabs including 2 grouped. Both tabs are grouped again.
  EXPECT_EQ(browser()->tab_strip_model()->count(), 4);
  EXPECT_EQ(group->tab_count(), 2);
}

// Trigger kOpenTabGroupEditDialog action from the recent activity dialog.
IN_PROC_BROWSER_TEST_F(RecentActivityBubbleDialogViewActionBrowserTest,
                       HandlesActionOpenTabGroupEditDialog) {
  auto* tab1 = CreateTab();
  TabGroupId group_id = CreateTabGroup({tab1});
  auto saved_group = ShareTabGroup(group_id);

  std::vector<ActivityLogItem> activity_log = {
      // TAB_GROUP_COLOR_UPDATED will contain the action to open the
      // group editor dialog.
      CreateActivityForTab(saved_group, tab1,
                           CollaborationEvent::TAB_GROUP_COLOR_UPDATED,
                           /*force_no_favicon=*/true),
  };
  ShowLog(activity_log);
  WaitForAvatar(/*log_index=*/0);

  auto* bubble = BubbleCoordinator()->GetBubble();
  EXPECT_TRUE(bubble);

  bubble->GetRowForTesting(0)->OpenTabGroupEditDialog();

  auto* editor_dialog =
      ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
          kTabGroupEditorBubbleId);

  EXPECT_TRUE(editor_dialog);
}

// Trigger kManageSharing action from the recent activity dialog.
IN_PROC_BROWSER_TEST_F(RecentActivityBubbleDialogViewActionBrowserTest,
                       HandlesActionManageSharing) {
  auto* tab1 = CreateTab();
  TabGroupId group_id = CreateTabGroup({tab1});
  auto saved_group = ShareTabGroup(group_id);

  std::vector<ActivityLogItem> activity_log = {
      // COLLABORATION_MEMBER_ADDED will contain the action to open the
      // manage sharing dialog.
      CreateActivityForTab(saved_group, tab1,
                           CollaborationEvent::COLLABORATION_MEMBER_ADDED,
                           /*force_no_favicon=*/true),
  };
  ShowLog(activity_log);
  WaitForAvatar(/*log_index=*/0);

  auto* bubble = BubbleCoordinator()->GetBubble();
  EXPECT_TRUE(bubble);

  bubble->GetRowForTesting(0)->ManageSharing();
}

}  // namespace tab_groups
