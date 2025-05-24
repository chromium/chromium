// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "chrome/browser/data_sharing/data_sharing_service_factory.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test/tab_strip_interactive_test_mixin.h"
#include "chrome/browser/ui/views/tabs/recent_activity_bubble_dialog_view.h"
#include "chrome/browser/ui/views/tabs/tab_group_header.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/collaboration/public/features.h"
#include "components/data_sharing/public/features.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/signin/public/base/avatar_icon_util.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/tab_group.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_connection.h"
#include "net/test/embedded_test_server/http_request.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_unittest_util.h"

using collaboration::messaging::ActivityLogItem;
using collaboration::messaging::CollaborationEvent;
using collaboration::messaging::MessageAttribution;
using collaboration::messaging::RecentActivityAction;
using collaboration::messaging::TabGroupMessageMetadata;
using collaboration::messaging::TabMessageMetadata;
using data_sharing::GroupData;
using data_sharing::GroupMember;
using data_sharing::MemberRole;

namespace {

const int kAvatarSize = signin::kAccountInfoImageSize;
constexpr char kSkipPixelTestsReason[] = "Should only run in pixel_tests.";

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

}  // namespace

namespace tab_groups {

class RecentActivityBubbleDialogViewInteractiveUiTest
    : public TabStripInteractiveTestMixin<InteractiveBrowserTest> {
 public:
  RecentActivityBubbleDialogViewInteractiveUiTest() = default;
  ~RecentActivityBubbleDialogViewInteractiveUiTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {tab_groups::kTabGroupSyncServiceDesktopMigration,
         data_sharing::features::kDataSharingFeature,
         collaboration::features::kCollaborationMessaging},
        {});
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &RecentActivityBubbleDialogViewInteractiveUiTest::HandleRequest,
        base::Unretained(this)));
    InteractiveBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->StartAcceptingConnections();
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    InteractiveBrowserTest::TearDownOnMainThread();
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

  MultiStep WaitForImages(int activity_log_index) {
    DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<bool>,
                                        kImagesLoaded);

    return Steps(
        PollState(
            kImagesLoaded,
            [&, activity_log_index]() {
              auto* image_view =
                  bubble()->GetRowForTesting(activity_log_index)->image_view();
              return image_view->ShouldShowAvatar() &&
                     image_view->ShouldShowFavicon();
            }),
        WaitForState(kImagesLoaded, true), StopObservingState(kImagesLoaded));
  }

  GURL GetAvatarURL() { return embedded_test_server()->GetURL(avatar_url_); }

  tabs::TabInterface* CreateTab() {
    auto index = browser()->tab_strip_model()->count();
    CHECK(AddTabAtIndex(index, GURL(chrome::kChromeUINewTabPageURL),
                        ui::PAGE_TRANSITION_TYPED));
    auto* tab = browser()->tab_strip_model()->GetTabAtIndex(index);
    CHECK(tab);
    return tab;
  }

  LocalTabID GetTabId(tabs::TabInterface* tab) {
    return tab->GetHandle().raw_value();
  }

  const TabGroupId CreateTabGroup(std::vector<tabs::TabInterface*> tabs) {
    std::vector<int> tab_indices = {};
    for (auto* tab : tabs) {
      tab_indices.emplace_back(
          browser()->tab_strip_model()->GetIndexOfTab(tab));
    }
    return browser()->tab_strip_model()->AddToNewGroup(tab_indices);
  }

  SavedTabGroup ShareTabGroup(TabGroupId group_id,
                              std::string collaboration_id) {
    TabGroupSyncService* tab_group_sync_service =
        TabGroupSyncServiceFactory::GetForProfile(browser()->profile());
    tab_group_sync_service->MakeTabGroupSharedForTesting(group_id,
                                                         collaboration_id);
    auto saved_tab_group = tab_group_sync_service->GetGroup(group_id);
    CHECK(saved_tab_group.has_value());
    return saved_tab_group.value();
  }

  // Sharing activities cannot be mocked at this point, so this test
  // triggers the dialog anchored to the tab_strip instead of using
  // the page action, which is driven by live data.
  auto TriggerDialog(std::vector<ActivityLogItem> activity_log) {
    return WithView(kTabStripElementId, [&, activity_log](TabStrip* tab_strip) {
      bubble_coordinator_.Show(
          tab_strip, browser()->tab_strip_model()->GetWebContentsAt(0),
          activity_log, browser()->profile());
    });
  }

  // Same as above, but for current tab version of the dialog.
  auto TriggerCurrentTabDialog(std::vector<ActivityLogItem> activity_log) {
    return WithView(kTabStripElementId, [&, activity_log](TabStrip* tab_strip) {
      bubble_coordinator_.ShowForCurrentTab(
          tab_strip, browser()->tab_strip_model()->GetWebContentsAt(0), {},
          activity_log, browser()->profile());
    });
  }

  ActivityLogItem CreateActivityForTab(
      LocalTabGroupID group_id,
      tabs::TabInterface* tab,
      CollaborationEvent collaboration_event = CollaborationEvent::TAB_ADDED) {
    GroupMember member;
    member.avatar_url = GetAvatarURL();

    TabMessageMetadata tab_metadata;
    tab_metadata.last_known_url = tab->GetContents()->GetURL().spec();
    tab_metadata.local_tab_id = GetTabId(tab);

    TabGroupMessageMetadata tab_group_metadata;
    tab_group_metadata.local_tab_group_id = group_id;

    MessageAttribution attribution;
    attribution.triggering_user = member;
    attribution.tab_metadata = tab_metadata;
    attribution.tab_group_metadata = tab_group_metadata;

    ActivityLogItem item;
    item.collaboration_event = collaboration_event;
    item.title_text = u"User added this tab";
    item.description_text = u"google.com";
    item.time_delta_text = u"2h ago";
    item.show_favicon = true;
    item.action =
        GetRecentActivityActionFromCollaborationEvent(collaboration_event);
    item.activity_metadata = attribution;

    return item;
  }

  RecentActivityBubbleDialogView* bubble() {
    return bubble_coordinator_.GetBubble();
  }

 private:
  const std::string avatar_url_ =
      base::StringPrintf("/avatar=s%d-cc-rp-ns", kAvatarSize);
  base::test::ScopedFeatureList scoped_feature_list_;
  RecentActivityBubbleCoordinator bubble_coordinator_;
};

// Take a screenshot of the recent activity dialog.
IN_PROC_BROWSER_TEST_F(RecentActivityBubbleDialogViewInteractiveUiTest,
                       ShowDialog) {
  // Set up tab group.
  tabs::TabInterface* tab = CreateTab();
  TabGroupId group_id = CreateTabGroup({tab});
  std::string collaboration_id = "fake_collaboration_id";
  ShareTabGroup(group_id, collaboration_id);

  // Create mock activity log.
  std::vector<ActivityLogItem> activity_log;
  activity_log.emplace_back(CreateActivityForTab(group_id, tab));
  const int activity_log_index = 0;

  RunTestSequence(
      WaitForShow(kTabGroupHeaderElementId), FinishTabstripAnimations(),
      TriggerDialog(activity_log), WaitForShow(kRecentActivityBubbleDialogId),
      WaitForImages(activity_log_index),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              kSkipPixelTestsReason),
      Screenshot(kRecentActivityBubbleDialogId, "", "6267139"), HoverTabAt(0),
      ClickMouse(), WaitForHide(kRecentActivityBubbleDialogId));
}

IN_PROC_BROWSER_TEST_F(RecentActivityBubbleDialogViewInteractiveUiTest,
                       ShowDialogForCurrentTab) {
  // Set up tab group.
  tabs::TabInterface* tab = CreateTab();
  tabs::TabInterface* tab2 = CreateTab();
  TabGroupId group_id = CreateTabGroup({tab, tab2});
  std::string collaboration_id = "fake_collaboration_id";
  ShareTabGroup(group_id, collaboration_id);

  // Create mock activity log.
  std::vector<ActivityLogItem> activity_log;
  activity_log.emplace_back(CreateActivityForTab(group_id, tab));
  activity_log.emplace_back(CreateActivityForTab(group_id, tab));

  auto activity_without_avatar = CreateActivityForTab(group_id, tab2);
  activity_without_avatar.activity_metadata.triggering_user->avatar_url =
      GURL("");
  activity_log.emplace_back(activity_without_avatar);

  auto activity_with_long_description = CreateActivityForTab(group_id, tab2);
  activity_with_long_description.activity_metadata.triggering_user->avatar_url =
      GURL("");
  activity_with_long_description.description_text =
      u"long long long long long long long long long long long long long long ";
  activity_log.emplace_back(activity_with_long_description);

  RunTestSequence(
      WaitForShow(kTabGroupHeaderElementId), FinishTabstripAnimations(),
      TriggerCurrentTabDialog(activity_log),
      WaitForShow(kRecentActivityBubbleDialogId), WaitForImages(0),
      WaitForImages(1), WaitForImages(2),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              kSkipPixelTestsReason),
      Screenshot(kRecentActivityBubbleDialogId, "", "6267139"), HoverTabAt(0),
      ClickMouse(), WaitForHide(kRecentActivityBubbleDialogId));
}

}  // namespace tab_groups
