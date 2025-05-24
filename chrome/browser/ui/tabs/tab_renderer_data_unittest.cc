// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_renderer_data.h"

#include <memory>
#include <string>

#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_source.h"
#include "chrome/browser/resource_coordinator/utils.h"
#include "chrome/browser/ui/performance_controls/tab_resource_usage_tab_helper.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/tabs/alert/tab_alert.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/browser/ui/tabs/test_util.h"
#include "chrome/browser/ui/thumbnails/thumbnail_tab_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace tabs {

void SimulateWebContentsCrash(content::WebContents* contents) {
  content::MockRenderProcessHost* rph =
      static_cast<content::MockRenderProcessHost*>(
          contents->GetPrimaryMainFrame()->GetProcess());
  rph->SimulateCrash();
}

void UpdateTitleForEntry(content::WebContents* contents,
                         const std::u16string& title) {
  content::NavigationEntry* entry =
      contents->GetController().GetLastCommittedEntry();
  ASSERT_NE(nullptr, entry);
  contents->UpdateTitleForEntry(entry, title);
}

class TabRendererDataTest : public testing::Test {
 public:
  TabRendererDataTest() : tab_strip_model_(&delegate_, &profile_) {}
  TabRendererDataTest(const TabRendererDataTest&) = delete;
  TabRendererDataTest& operator=(const TabRendererDataTest&) = delete;
  ~TabRendererDataTest() override = default;

 protected:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  TestingProfile profile_;
  TestTabStripModelDelegate delegate_;
  TabStripModel tab_strip_model_;

  // These tests wont support parts of the rendererdata that need
  // BrowserWindowFeatures.
  tabs::PreventTabFeatureInitialization prevent_;

  int AddTab(bool foreground = true, bool pinned = false) {
    auto web_contents =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    tab_strip_model_.AppendWebContents(std::move(web_contents), foreground);
    int index = tab_strip_model_.count() - 1;
    if (pinned) {
      tab_strip_model_.SetTabPinned(index, true);
    }
    TabInterface* const tab_interface = tab_strip_model_.GetTabAtIndex(index);
    tab_interface->GetTabFeatures()->SetTabUIHelperForTesting(
        std::make_unique<TabUIHelper>(*tab_interface));
    return index;
  }
};

TEST_F(TabRendererDataTest, FromTabInModel) {
  const int index = AddTab();

  TabRendererData data =
      TabRendererData::FromTabInModel(&tab_strip_model_, index);

  EXPECT_FALSE(data.pinned);
  EXPECT_TRUE(data.show_icon);
  EXPECT_EQ(data.network_state, TabNetworkState::kNone);
  EXPECT_TRUE(data.alert_state.empty());
  EXPECT_EQ(data.visible_url, GURL());
  EXPECT_EQ(data.last_committed_url, GURL());
  EXPECT_EQ(data.title,
            data.tab_interface->GetTabFeatures()->tab_ui_helper()->GetTitle());
  EXPECT_FALSE(data.incognito);
  EXPECT_FALSE(data.blocked);
  EXPECT_FALSE(data.should_hide_throbber);
  EXPECT_FALSE(data.is_tab_discarded);
  EXPECT_FALSE(data.should_show_discard_status);
}

TEST_F(TabRendererDataTest, PinnedStateChange) {
  int index = AddTab();
  TabRendererData data_before =
      TabRendererData::FromTabInModel(&tab_strip_model_, index);
  EXPECT_FALSE(data_before.pinned);

  tab_strip_model_.SetTabPinned(index, true);
  TabRendererData data_after_pinning =
      TabRendererData::FromTabInModel(&tab_strip_model_, index);
  EXPECT_TRUE(data_after_pinning.pinned);

  tab_strip_model_.SetTabPinned(index, false);
  TabRendererData data_after_unpinning =
      TabRendererData::FromTabInModel(&tab_strip_model_, index);
  EXPECT_FALSE(data_after_unpinning.pinned);

  EXPECT_NE(data_before, data_after_pinning);
  EXPECT_NE(data_after_pinning, data_after_unpinning);
  EXPECT_EQ(data_before, data_after_unpinning);
}

TEST_F(TabRendererDataTest, TabInterfaceWeakPtr) {
  int index1 = AddTab();
  content::WebContents* wc1 = tab_strip_model_.GetWebContentsAt(index1);
  UpdateTitleForEntry(wc1, u"First Tab");

  TabRendererData data1 =
      TabRendererData::FromTabInModel(&tab_strip_model_, index1);

  EXPECT_EQ(data1.title, u"First Tab");
  EXPECT_EQ(data1.tab_interface->GetContents(), wc1);

  {
    int index2 = AddTab(false);
    content::WebContents* wc2 = tab_strip_model_.GetWebContentsAt(index2);
    TabRendererData data2 =
        TabRendererData::FromTabInModel(&tab_strip_model_, index2);
    EXPECT_EQ(data2.tab_interface->GetContents(), wc2);
    tab_strip_model_.CloseWebContentsAt(index2,
                                        TabCloseTypes::CLOSE_USER_GESTURE);
    EXPECT_FALSE(data2.tab_interface);
    EXPECT_FALSE(data2.pinned);
  }
}

TEST_F(TabRendererDataTest, TitleChange) {
  int index = AddTab();
  content::WebContents* wc = tab_strip_model_.GetWebContentsAt(index);

  UpdateTitleForEntry(wc, u"First Tab");
  TabRendererData data_initial =
      TabRendererData::FromTabInModel(&tab_strip_model_, index);
  EXPECT_EQ(data_initial.title, u"First Tab");

  UpdateTitleForEntry(wc, u"First Tab Updated");
  TabRendererData data_updated =
      TabRendererData::FromTabInModel(&tab_strip_model_, index);
  EXPECT_EQ(data_updated.title, u"First Tab Updated");
}

TEST_F(TabRendererDataTest, BlockedState) {
  int index = AddTab();
  // Initially not blocked
  TabRendererData data_initial =
      TabRendererData::FromTabInModel(&tab_strip_model_, index);
  EXPECT_FALSE(data_initial.blocked);

  // Block the tab and verify
  tab_strip_model_.SetTabBlocked(index, true);
  TabRendererData data_blocked =
      TabRendererData::FromTabInModel(&tab_strip_model_, index);
  EXPECT_TRUE(data_blocked.blocked);

  EXPECT_NE(data_initial, data_blocked);
}

TEST_F(TabRendererDataTest, FaviconAndIconFlags) {
  {  // Initial favicon data matches default
    const int default_index = AddTab();
    TabRendererData data =
        TabRendererData::FromTabInModel(&tab_strip_model_, default_index);
    EXPECT_EQ(
        data.favicon,
        data.tab_interface->GetTabFeatures()->tab_ui_helper()->GetFavicon());
    EXPECT_FALSE(data.should_themify_favicon);
    EXPECT_FALSE(data.is_monochrome_favicon);
    EXPECT_TRUE(data.show_icon);
  }

  {  // Themeable by virtual URL only.
    int virtual_url_index = AddTab();
    content::WebContents* wc_virtual =
        tab_strip_model_.GetWebContentsAt(virtual_url_index);
    auto* entry_virtual = wc_virtual->GetController().GetLastCommittedEntry();
    ASSERT_NE(nullptr, entry_virtual);
    const GURL themeable_virtual_url("chrome://feedback/");
    entry_virtual->SetVirtualURL(themeable_virtual_url);
    TabRendererData virtual_data =
        TabRendererData::FromTabInModel(&tab_strip_model_, virtual_url_index);
    EXPECT_TRUE(virtual_data.should_themify_favicon);
  }

  {  // Themeable by actual URL only.
    int actual_url_index = AddTab();
    content::WebContents* wc_actual =
        tab_strip_model_.GetWebContentsAt(actual_url_index);
    auto* entry_actual = wc_actual->GetController().GetLastCommittedEntry();
    ASSERT_NE(nullptr, entry_actual);
    const GURL themeable_url("chrome://new-tab-page/");
    entry_actual->SetURL(themeable_url);
    TabRendererData actual_data =
        TabRendererData::FromTabInModel(&tab_strip_model_, actual_url_index);
    EXPECT_TRUE(actual_data.should_themify_favicon);
  }
}

TEST_F(TabRendererDataTest, Urls) {
  int index = AddTab();
  content::WebContents* wc = tab_strip_model_.GetWebContentsAt(index);
  auto* entry = wc->GetController().GetLastCommittedEntry();
  ASSERT_NE(nullptr, entry);
  const GURL kUrl("http://example.com/");
  entry->SetURL(kUrl);
  TabRendererData data =
      TabRendererData::FromTabInModel(&tab_strip_model_, index);
  EXPECT_EQ(data.visible_url, kUrl);
  EXPECT_EQ(data.last_committed_url, kUrl);
  EXPECT_TRUE(data.should_display_url);
  EXPECT_FALSE(data.should_render_empty_title);
}

TEST_F(TabRendererDataTest, ShouldRenderEmptyTitle) {
  int index = AddTab();
  content::WebContents* wc = tab_strip_model_.GetWebContentsAt(index);
  UpdateTitleForEntry(wc, u"");

  auto* entry = wc->GetController().GetLastCommittedEntry();
  ASSERT_NE(nullptr, entry);
  const GURL kUntrustedUrl("chrome-untrusted://test/");
  entry->SetURL(kUntrustedUrl);

  TabRendererData data =
      TabRendererData::FromTabInModel(&tab_strip_model_, index);
#if BUILDFLAG(IS_MAC)
  // Mac requires "Untitled" to display for an empty title.
  EXPECT_FALSE(data.should_render_empty_title);
#else
  EXPECT_TRUE(data.should_render_empty_title);
#endif
}

TEST_F(TabRendererDataTest, DISABLED_CrashedStatus) {
  int index = AddTab();
  content::WebContents* wc = tab_strip_model_.GetWebContentsAt(index);
  TabRendererData data_initial =
      TabRendererData::FromTabInModel(&tab_strip_model_, index);
  EXPECT_EQ(data_initial.crashed_status,
            base::TERMINATION_STATUS_STILL_RUNNING);
  EXPECT_FALSE(data_initial.IsCrashed());

  SimulateWebContentsCrash(wc);

  TabRendererData data_crashed =
      TabRendererData::FromTabInModel(&tab_strip_model_, index);
  EXPECT_EQ(data_crashed.crashed_status,
            base::TERMINATION_STATUS_PROCESS_CRASHED);
  EXPECT_TRUE(data_crashed.IsCrashed());
}

TEST_F(TabRendererDataTest, NetworkState) {
  int index = AddTab();
  content::WebContents* wc = tab_strip_model_.GetWebContentsAt(index);
  const GURL kUrl("http://example.com/");
  auto navigation =
      content::NavigationSimulator::CreateBrowserInitiated(kUrl, wc);
  navigation->Start();
  TabRendererData data_loading =
      TabRendererData::FromTabInModel(&tab_strip_model_, index);
  EXPECT_NE(data_loading.network_state, TabNetworkState::kNone);
  navigation->Commit();
  TabRendererData data_committed =
      TabRendererData::FromTabInModel(&tab_strip_model_, index);
  EXPECT_EQ(data_committed.network_state, TabNetworkState::kNone);
}

TEST_F(TabRendererDataTest, AlertStateAudioPlaying) {
  int index = AddTab();
  content::WebContents* wc = tab_strip_model_.GetWebContentsAt(index);
  content::WebContentsTester::For(wc)->SetIsCurrentlyAudible(true);
  TabRendererData data =
      TabRendererData::FromTabInModel(&tab_strip_model_, index);
  EXPECT_NE(data.alert_state.end(),
            std::find(data.alert_state.begin(), data.alert_state.end(),
                      tabs::TabAlert::AUDIO_PLAYING));
}

TEST_F(TabRendererDataTest, ShouldHideThrobber) {
  const int index = AddTab();
  TabUIHelper* const helper =
      tab_strip_model_.GetTabAtIndex(index)->GetTabFeatures()->tab_ui_helper();
  ASSERT_NE(nullptr, helper);
  helper->set_created_by_session_restore(true);
  TabRendererData data =
      TabRendererData::FromTabInModel(&tab_strip_model_, index);
  EXPECT_TRUE(data.should_hide_throbber);
}

// This test must go into a BrowserTest since ThumbnailTabHelper doesnt exist.
TEST_F(TabRendererDataTest, DISABLED_Thumbnail) {
  int index = AddTab();
  content::WebContents* wc = tab_strip_model_.GetWebContentsAt(index);
  auto* thumbnail_tab_helper = ThumbnailTabHelper::FromWebContents(wc);
  ASSERT_NE(nullptr, thumbnail_tab_helper);

  // Initial data should reference the helper's thumbnail and have no data.
  TabRendererData data_initial =
      TabRendererData::FromTabInModel(&tab_strip_model_, index);
  EXPECT_EQ(data_initial.thumbnail.get(),
            thumbnail_tab_helper->thumbnail().get());
  EXPECT_FALSE(data_initial.thumbnail->has_data());

  // Assign a dummy bitmap to trigger thumbnail image change.
  SkBitmap bitmap;
  bitmap.allocN32Pixels(10, 10);
  thumbnail_tab_helper->thumbnail()->AssignSkBitmap(bitmap, /*frame_id=*/0);

  // After assignment, thumbnail has data and FromTabInModel reflects it.
  EXPECT_TRUE(thumbnail_tab_helper->thumbnail()->has_data());
  TabRendererData data_updated =
      TabRendererData::FromTabInModel(&tab_strip_model_, index);
  EXPECT_TRUE(data_updated.thumbnail->has_data());
  EXPECT_EQ(data_updated.thumbnail.get(),
            thumbnail_tab_helper->thumbnail().get());
  EXPECT_NE(data_initial, data_updated);
}

// TODO test for Deferred functionality.

TEST_F(TabRendererDataTest, TabLifecycleManagement) {
  int index = AddTab();

  auto* tab = tab_strip_model_.GetTabAtIndex(index);
  auto* features = tab->GetTabFeatures();
  auto* usage_helper = features->SetResourceUsageHelperForTesting(
      std::make_unique<TabResourceUsageTabHelper>(*tab));

  TabRendererData data_default =
      TabRendererData::FromTabInModel(&tab_strip_model_, index);
  EXPECT_FALSE(data_default.is_tab_discarded);
  EXPECT_FALSE(data_default.should_show_discard_status);
  EXPECT_EQ(data_default.discarded_memory_savings_in_bytes, 0);
  EXPECT_TRUE(data_default.tab_resource_usage);

  usage_helper->SetMemoryUsageInBytes(1234);
  TabRendererData data_usage =
      TabRendererData::FromTabInModel(&tab_strip_model_, index);
  ASSERT_TRUE(data_usage.tab_resource_usage);
  EXPECT_EQ(data_usage.tab_resource_usage->memory_usage_in_bytes(), 1234u);
}

// TODO: Add unit tests for TabRendererData::collaboration_messaging

}  // namespace tabs
