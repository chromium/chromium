// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_renderer_data.h"

#include <string>

#include "base/byte_size.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/performance_controls/tab_resource_usage_tab_helper.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/tabs/alert/tab_alert.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/collaboration_messaging_tab_data.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/thumbnails/thumbnail_image.h"
#include "chrome/browser/ui/thumbnails/thumbnail_tab_helper.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/data_sharing/public/features.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_skia.h"
#include "url/url_constants.h"

namespace tabs {

constexpr char kGivenName[] = "User";

class TabRendererDataTest : public InProcessBrowserTest {
 public:
  TabRendererDataTest() {
    scoped_feature_list_.InitAndEnableFeature(
        data_sharing::features::kDataSharingFeature);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    ASSERT_TRUE(embedded_test_server()->Start());
  }

 protected:
  void UpdateTitleForEntry(content::WebContents* contents,
                           const std::u16string& title) {
    content::NavigationEntry* entry =
        contents->GetController().GetLastCommittedEntry();
    ASSERT_NE(nullptr, entry);
    contents->UpdateTitleForEntry(entry, title);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(TabRendererDataTest, FromTabInterface) {
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL), WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  tabs::TabInterface* const tab_interface =
      browser()->GetTabStripModel()->GetTabAtIndex(0);
  TabRendererData data = TabRendererData::FromTabInterface(tab_interface);

  EXPECT_FALSE(data.pinned);
  EXPECT_TRUE(data.show_icon);
  EXPECT_EQ(data.network_state, TabNetworkState::kNone);
  EXPECT_TRUE(data.alert_state.empty());
  EXPECT_EQ(data.visible_url, GURL(url::kAboutBlankURL));
  EXPECT_EQ(data.last_committed_url, GURL(url::kAboutBlankURL));
  EXPECT_EQ(data.title, TabUIHelper::From(tab_interface)->GetTitle());
  EXPECT_FALSE(data.blocked);
  EXPECT_FALSE(data.should_hide_throbber);
  EXPECT_FALSE(data.is_tab_discarded);
  EXPECT_FALSE(data.should_show_discard_status);
}

IN_PROC_BROWSER_TEST_F(TabRendererDataTest, PinnedStateChange) {
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL), WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  tabs::TabInterface* const tab_interface =
      browser()->GetTabStripModel()->GetTabAtIndex(0);
  TabRendererData data_before =
      TabRendererData::FromTabInterface(tab_interface);
  EXPECT_FALSE(data_before.pinned);

  tab_strip_model->SetTabPinned(0, true);
  TabRendererData data_after_pinning =
      TabRendererData::FromTabInterface(tab_interface);
  EXPECT_TRUE(data_after_pinning.pinned);

  tab_strip_model->SetTabPinned(0, false);
  TabRendererData data_after_unpinning =
      TabRendererData::FromTabInterface(tab_interface);
  EXPECT_FALSE(data_after_unpinning.pinned);

  EXPECT_NE(data_before, data_after_pinning);
  EXPECT_NE(data_after_pinning, data_after_unpinning);
  EXPECT_EQ(data_before, data_after_unpinning);
}

IN_PROC_BROWSER_TEST_F(TabRendererDataTest, TitleChange) {
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL), WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  content::WebContents* wc = tab_strip_model->GetWebContentsAt(0);

  UpdateTitleForEntry(wc, u"First Tab");
  tabs::TabInterface* const tab_interface =
      browser()->GetTabStripModel()->GetTabAtIndex(0);
  TabRendererData data_initial =
      TabRendererData::FromTabInterface(tab_interface);
  EXPECT_EQ(data_initial.title, u"First Tab");

  UpdateTitleForEntry(wc, u"First Tab Updated");
  TabRendererData data_updated =
      TabRendererData::FromTabInterface(tab_interface);
  EXPECT_EQ(data_updated.title, u"First Tab Updated");
}

IN_PROC_BROWSER_TEST_F(TabRendererDataTest, BlockedState) {
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL), WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  tabs::TabInterface* const tab_interface =
      browser()->GetTabStripModel()->GetTabAtIndex(0);
  // Initially not blocked
  TabRendererData data_initial =
      TabRendererData::FromTabInterface(tab_interface);
  EXPECT_FALSE(data_initial.blocked);

  // Block the tab and verify
  tab_strip_model->SetTabBlocked(0, true);
  TabRendererData data_blocked =
      TabRendererData::FromTabInterface(tab_interface);
  EXPECT_TRUE(data_blocked.blocked);

  EXPECT_NE(data_initial, data_blocked);
}

IN_PROC_BROWSER_TEST_F(TabRendererDataTest, FaviconAndIconFlags) {
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  {  // Initial favicon data matches default
    tabs::TabInterface* const tab_interface = tab_strip_model->GetTabAtIndex(0);
    ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL(url::kAboutBlankURL),
        WindowOpenDisposition::CURRENT_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
    TabRendererData data = TabRendererData::FromTabInterface(tab_interface);
    EXPECT_EQ(data.favicon, TabUIHelper::From(tab_interface)->GetFavicon());
    EXPECT_FALSE(data.should_themify_favicon);
    EXPECT_FALSE(data.is_monochrome_favicon);
    EXPECT_TRUE(data.show_icon);
  }

  {  // Themeable by virtual URL only.
    ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL(url::kAboutBlankURL),
        WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
    tabs::TabInterface* const tab_interface = tab_strip_model->GetTabAtIndex(1);
    content::WebContents* wc_virtual = tab_interface->GetContents();
    auto* entry_virtual = wc_virtual->GetController().GetLastCommittedEntry();
    ASSERT_NE(nullptr, entry_virtual);
    const GURL themeable_virtual_url("chrome://feedback/");
    entry_virtual->SetVirtualURL(themeable_virtual_url);
    TabRendererData virtual_data =
        TabRendererData::FromTabInterface(tab_interface);
    EXPECT_TRUE(virtual_data.should_themify_favicon);
  }

  {  // Themeable by actual URL only.
    ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL(url::kAboutBlankURL),
        WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
    tabs::TabInterface* const tab_interface = tab_strip_model->GetTabAtIndex(2);
    content::WebContents* wc_actual = tab_interface->GetContents();
    auto* entry_actual = wc_actual->GetController().GetLastCommittedEntry();
    ASSERT_NE(nullptr, entry_actual);
    const GURL themeable_url("chrome://new-tab-page/");
    entry_actual->SetURL(themeable_url);
    TabRendererData actual_data =
        TabRendererData::FromTabInterface(tab_interface);
    EXPECT_TRUE(actual_data.should_themify_favicon);
  }
}

IN_PROC_BROWSER_TEST_F(TabRendererDataTest, Urls) {
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL), WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  tabs::TabInterface* const tab_interface =
      browser()->GetTabStripModel()->GetTabAtIndex(0);
  content::WebContents* wc = tab_interface->GetContents();
  auto* entry = wc->GetController().GetLastCommittedEntry();
  ASSERT_NE(nullptr, entry);
  const GURL kUrl("http://example.com/");
  entry->SetURL(kUrl);
  TabRendererData data = TabRendererData::FromTabInterface(tab_interface);
  EXPECT_EQ(data.visible_url, kUrl);
  EXPECT_EQ(data.last_committed_url, kUrl);
  EXPECT_TRUE(data.should_display_url);
  EXPECT_FALSE(data.should_render_loading_title);
}

IN_PROC_BROWSER_TEST_F(TabRendererDataTest, DomainUrlHiddenForNtpAndTabSearch) {
  struct TestCase {
    GURL url;
    bool expected_domain_visible;
  };

  TestCase test_cases[] = {
      {GURL("https://example.com"), true},
      {GURL(chrome::kChromeUIVersionURL), true},
      {GURL(chrome::kChromeUINewTabURL), false},
      {GURL(chrome::kChromeUINewTabPageURL), false},
      {GURL(chrome::kChromeUITabSearchURL), false},
  };

  for (const auto& test_case : test_cases) {
    ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
        browser(), test_case.url, WindowOpenDisposition::CURRENT_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
    TabRendererData data = TabRendererData::FromTabInterface(
        browser()->GetTabStripModel()->GetTabAtIndex(0));
    EXPECT_EQ(data.should_display_url, test_case.expected_domain_visible)
        << test_case.url;
  }
}

IN_PROC_BROWSER_TEST_F(TabRendererDataTest, UncomittedNavigationUrl) {
  // The /nocontent navigation won't commit so it reverts back to the initial
  // navigation.
  GURL uncommitted_url = embedded_test_server()->GetURL("c.test", "/nocontent");
  ASSERT_FALSE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), uncommitted_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB));

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  content::WebContents* wc = tab_strip_model->GetWebContentsAt(1);
  auto* entry = wc->GetController().GetLastCommittedEntry();
  ASSERT_TRUE(entry->IsInitialEntry());
  TabRendererData data =
      TabRendererData::FromTabInterface(tab_strip_model->GetTabAtIndex(0));
  EXPECT_EQ(data.visible_url, GURL(url::kAboutBlankURL));
  EXPECT_EQ(data.last_committed_url, GURL(url::kAboutBlankURL));
  EXPECT_TRUE(data.should_display_url);
  EXPECT_FALSE(data.should_render_loading_title);
}

IN_PROC_BROWSER_TEST_F(TabRendererDataTest, ShouldRenderLoadingTitle) {
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL), WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  tabs::TabInterface* const tab_interface =
      browser()->GetTabStripModel()->GetTabAtIndex(0);
  content::WebContents* wc = tab_interface->GetContents();
  UpdateTitleForEntry(wc, u"");

  auto* entry = wc->GetController().GetLastCommittedEntry();
  ASSERT_NE(nullptr, entry);
  const GURL kUntrustedUrl("chrome-untrusted://test/");
  entry->SetURL(kUntrustedUrl);

  TabRendererData data = TabRendererData::FromTabInterface(tab_interface);
  EXPECT_FALSE(data.should_render_loading_title);
}

IN_PROC_BROWSER_TEST_F(TabRendererDataTest, CrashedStatus) {
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL), WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  tabs::TabInterface* const tab_interface =
      browser()->GetTabStripModel()->GetTabAtIndex(0);
  content::WebContents* wc = tab_interface->GetContents();
  TabRendererData data_initial =
      TabRendererData::FromTabInterface(tab_interface);
  EXPECT_FALSE(data_initial.is_crashed);
  content::CrashTab(wc);
  TabRendererData data_crashed =
      TabRendererData::FromTabInterface(tab_interface);
  EXPECT_TRUE(data_crashed.is_crashed);

  // The tab should not be crashed after navigating to another site.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL), WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  TabRendererData data_navigation =
      TabRendererData::FromTabInterface(tab_interface);
  EXPECT_FALSE(data_navigation.is_crashed);
}

IN_PROC_BROWSER_TEST_F(TabRendererDataTest, NetworkState) {
  const GURL kUrl("http://example.com/");
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), kUrl, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  tabs::TabInterface* const tab_interface =
      browser()->GetTabStripModel()->GetTabAtIndex(0);

  TabRendererData data_loading =
      TabRendererData::FromTabInterface(tab_interface);
  EXPECT_NE(data_loading.network_state, TabNetworkState::kNone);

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL), WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  TabRendererData data_committed =
      TabRendererData::FromTabInterface(tab_interface);
  EXPECT_EQ(data_committed.network_state, TabNetworkState::kNone);
}

IN_PROC_BROWSER_TEST_F(TabRendererDataTest, AlertStateAudioPlaying) {
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL), WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  tabs::TabInterface* const tab_interface =
      browser()->GetTabStripModel()->GetTabAtIndex(0);
  content::WebContents* wc = tab_interface->GetContents();
  base::ScopedClosureRunner scoped_closure_runner = wc->MarkAudible();
  TabRendererData data = TabRendererData::FromTabInterface(tab_interface);
  EXPECT_NE(data.alert_state.end(),
            std::find(data.alert_state.begin(), data.alert_state.end(),
                      tabs::TabAlert::kAudioPlaying));
}

IN_PROC_BROWSER_TEST_F(TabRendererDataTest, ShouldHideThrobber) {
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL),
      WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  tabs::TabInterface* const tab_interface =
      browser()->GetTabStripModel()->GetTabAtIndex(1);
  TabUIHelper* const helper = TabUIHelper::From(tab_interface);
  ASSERT_NE(nullptr, helper);
  helper->set_created_by_session_restore(true);
  TabRendererData data = TabRendererData::FromTabInterface(tab_interface);
  EXPECT_TRUE(helper->ShouldHideThrobber());
  EXPECT_TRUE(data.should_hide_throbber);
}

IN_PROC_BROWSER_TEST_F(TabRendererDataTest, Thumbnail) {
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL), WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  tabs::TabInterface* const tab_interface =
      browser()->GetTabStripModel()->GetTabAtIndex(0);
  content::WebContents* wc = tab_interface->GetContents();
  auto* thumbnail_tab_helper = ThumbnailTabHelper::FromWebContents(wc);
  ASSERT_NE(nullptr, thumbnail_tab_helper);

  // Initial data should reference the helper's thumbnail and have no data.
  TabRendererData data_initial =
      TabRendererData::FromTabInterface(tab_interface);
  EXPECT_EQ(data_initial.thumbnail.get(),
            thumbnail_tab_helper->thumbnail().get());
  EXPECT_FALSE(data_initial.thumbnail->has_data());

  base::RunLoop run_loop;
  std::unique_ptr<ThumbnailImage::Subscription> subscription =
      thumbnail_tab_helper->thumbnail()->Subscribe();
  subscription->SetUncompressedImageCallback(
      base::IgnoreArgs<gfx::ImageSkia>(run_loop.QuitClosure()));

  // Assign a dummy bitmap to trigger thumbnail image change.
  SkBitmap bitmap;
  bitmap.allocN32Pixels(10, 10);
  thumbnail_tab_helper->thumbnail()->AssignSkBitmap(bitmap, /*frame_id=*/0);
  run_loop.Run();

  // After assignment, thumbnail has data and FromTabInterface reflects it.
  EXPECT_TRUE(thumbnail_tab_helper->thumbnail()->has_data());
  TabRendererData data_updated =
      TabRendererData::FromTabInterface(tab_interface);
  EXPECT_TRUE(data_updated.thumbnail->has_data());
  EXPECT_EQ(data_updated.thumbnail.get(),
            thumbnail_tab_helper->thumbnail().get());
  EXPECT_EQ(data_initial, data_updated);
}

// TODO(crbug.com/443125652): Creating a test for deferred functionality
IN_PROC_BROWSER_TEST_F(TabRendererDataTest, TabLifecycleManagement) {
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL), WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  tabs::TabInterface* const tab_interface =
      browser()->GetTabStripModel()->GetTabAtIndex(0);

  TabRendererData data_default =
      TabRendererData::FromTabInterface(tab_interface);
  EXPECT_FALSE(data_default.is_tab_discarded);
  EXPECT_FALSE(data_default.should_show_discard_status);
  EXPECT_TRUE(data_default.discarded_memory_savings.is_zero());
  EXPECT_TRUE(data_default.tab_resource_usage);
  TabResourceUsageTabHelper::From(tab_interface)
      ->SetMemoryUsage(base::ByteSize(1234));
  TabRendererData data_usage = TabRendererData::FromTabInterface(tab_interface);
  ASSERT_TRUE(data_usage.tab_resource_usage);
  EXPECT_EQ(data_usage.tab_resource_usage->memory_usage(),
            base::ByteSize(1234));
}

IN_PROC_BROWSER_TEST_F(TabRendererDataTest,
                       CollaborationMessagingTabDataInvalidatedOnTabClosure) {
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL), WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  TabStripModel* const tab_strip_model = browser()->tab_strip_model();

  TabRendererData data1 =
      TabRendererData::FromTabInterface(tab_strip_model->GetTabAtIndex(0));

  EXPECT_TRUE(data1.collaboration_messaging);

  {
    ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL(url::kAboutBlankURL),
        WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

    ASSERT_EQ(2, tab_strip_model->count());

    TabRendererData data2 =
        TabRendererData::FromTabInterface(tab_strip_model->GetTabAtIndex(1));
    EXPECT_TRUE(data2.collaboration_messaging);

    // Before adding the message.
    EXPECT_FALSE(data2.collaboration_messaging->HasMessage());

    // Creating the message.
    tab_groups::PersistentMessage message;
    message.type = collaboration::messaging::PersistentNotificationType::CHIP;
    message.collaboration_event = tab_groups::CollaborationEvent::TAB_ADDED;
    message.attribution.triggering_user = data_sharing::GroupMember();
    message.attribution.triggering_user->given_name = kGivenName;

    // After adding the message.
    data2.collaboration_messaging->set_mocked_avatar_for_testing(
        favicon::GetDefaultFavicon());
    data2.collaboration_messaging->SetMessage(message);
    EXPECT_TRUE(data2.collaboration_messaging->HasMessage());

    tab_strip_model->CloseWebContentsAt(1, TabCloseTypes::CLOSE_NONE);
    ASSERT_EQ(1, tab_strip_model->count());

    EXPECT_FALSE(data2.collaboration_messaging);
  }

  EXPECT_TRUE(data1.collaboration_messaging);
}

}  // namespace tabs
