// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/tab_model/tab_model_jni_bridge.h"

#include <memory>
#include <optional>
#include <set>
#include <vector>

#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/android/flags/chrome_cached_flags.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/performance_manager/public/background_tab_loading_policy.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/tab_list/tab_list_interface_observer.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/split_tabs/split_tab_id.h"
#include "components/split_tabs/split_tab_visual_data.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/restore_type.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace {

class TabModelJniBridgeTest : public AndroidBrowserTest {
 public:
  void SetUpOnMainThread() override {
    AndroidBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  // Returns the TabModelJniBridge under test.
  TabModelJniBridge* bridge() {
    return static_cast<TabModelJniBridge*>(GetTabListInterface());
  }

  // Converts a TabInterface to TabAndroid.
  TabAndroid* ToTabAndroid(tabs::TabInterface* tab) {
    return TabAndroid::FromTabInterface(tab);
  }

  // Returns the HostContentSettingsMap for the current profile.
  HostContentSettingsMap* GetHostContentSettingsMap() {
    return HostContentSettingsMapFactory::GetForProfile(GetProfile());
  }

  // Returns the sound content setting for the given URL.
  ContentSetting GetSoundSetting(const GURL& url) {
    return GetHostContentSettingsMap()->GetContentSetting(
        url, url, ContentSettingsType::SOUND);
  }

  // Calls SetMuteSetting on the bridge.
  void SetMuteSetting(const std::vector<TabAndroid*>& tabs, bool mute) {
    bridge()->SetMuteSetting(tabs, mute);
  }

  // Navigates the given tab to the URL and waits for completion.
  tabs::TabInterface* NavigateTab(tabs::TabInterface* tab, const GURL& url) {
    EXPECT_TRUE(tab);
    if (tab) {
      EXPECT_TRUE(content::NavigateToURL(tab->GetContents(), url));
    }
    return tab;
  }

  // Opens a new tab and navigates it to the URL.
  tabs::TabInterface* OpenAndNavigateTab(const GURL& url) {
    tabs::TabInterface* tab =
        GetTabListInterface()->OpenTab(GURL("about:blank"), /*index=*/-1);
    return NavigateTab(tab, url);
  }
};

IN_PROC_BROWSER_TEST_F(TabModelJniBridgeTest, AddToGroupWithOutOfOrderHandles) {
  // Create some tabs. This is likely, though not guaranteed, to create
  // TabHandles with increasing internal values.
  TabListInterface* tab_list = GetTabListInterface();
  ASSERT_TRUE(tab_list);
  tabs::TabInterface* tab_a = tab_list->GetTab(0);
  tabs::TabInterface* tab_b =
      tab_list->OpenTab(GURL("about:blank"), /*index=*/-1);
  tabs::TabInterface* tab_c =
      tab_list->OpenTab(GURL("about:blank"), /*index=*/-1);

  // Move the last tab to the front, so its order in the tab strip doesn't
  // match the internal TabHandle order.
  tab_list->MoveTab(tab_c->GetHandle(), 0);

  // The tab strip order changed to C, A, B.
  ASSERT_EQ(tab_c->GetHandle(), tab_list->GetTab(0)->GetHandle());
  ASSERT_EQ(tab_a->GetHandle(), tab_list->GetTab(1)->GetHandle());
  ASSERT_EQ(tab_b->GetHandle(), tab_list->GetTab(2)->GetHandle());

  // Create a group from the tabs. Internally the set will likely be ordered
  // A, B, C based on the creation order of the handles (though this is not
  // guaranteed).
  std::set<tabs::TabHandle> handles{tab_a->GetHandle(), tab_b->GetHandle(),
                                    tab_c->GetHandle()};
  tab_list->AddTabsToGroup(std::nullopt, handles);

  // The tab strip order is still C, A, B (not the order of the handles).
  EXPECT_EQ(tab_c->GetHandle(), tab_list->GetTab(0)->GetHandle());
  EXPECT_EQ(tab_a->GetHandle(), tab_list->GetTab(1)->GetHandle());
  EXPECT_EQ(tab_b->GetHandle(), tab_list->GetTab(2)->GetHandle());
}

IN_PROC_BROWSER_TEST_F(TabModelJniBridgeTest, InsertWebContentsAt) {
  TabListInterface* tab_list = GetTabListInterface();
  ASSERT_TRUE(tab_list);
  int initial_count = tab_list->GetTabCount();

  // Insert WebContents to a new tab at index 1.
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(GetProfile()));
  auto* web_contents_ptr = web_contents.get();
  tabs::TabInterface* new_tab = tab_list->InsertWebContentsAt(
      /*index=*/1, std::move(web_contents), /*should_pin=*/false,
      /*group=*/std::nullopt);

  // Check the new tab.
  ASSERT_TRUE(new_tab);
  EXPECT_EQ(initial_count + 1, tab_list->GetTabCount());
  EXPECT_EQ(new_tab, tab_list->GetTab(1));
  EXPECT_EQ(web_contents_ptr, new_tab->GetContents());
}

IN_PROC_BROWSER_TEST_F(TabModelJniBridgeTest, DetachWebContents) {
  TabListInterface* tab_list = GetTabListInterface();
  ASSERT_TRUE(tab_list);

  // Open a new tab to detach.
  tabs::TabInterface* tab_to_detach =
      tab_list->OpenTab(GURL("about:blank"), /*index=*/-1);
  ASSERT_TRUE(tab_to_detach);
  content::WebContents* expected_contents = tab_to_detach->GetContents();
  tabs::TabHandle handle = tab_to_detach->GetHandle();

  int count_before = tab_list->GetTabCount();

  // Detach the WebContents.
  std::unique_ptr<content::WebContents> detached_contents =
      tab_list->DetachWebContents(handle);

  // Verify detachment.
  EXPECT_TRUE(detached_contents);
  EXPECT_EQ(expected_contents, detached_contents.get());
  EXPECT_EQ(count_before - 1, tab_list->GetTabCount());

  // Verify the tab is gone from the list.
  for (int i = 0; i < tab_list->GetTabCount(); ++i) {
    EXPECT_NE(tab_to_detach, tab_list->GetTab(i));
  }
}

IN_PROC_BROWSER_TEST_F(TabModelJniBridgeTest, ChangeActivationAndSelection) {
  TabListInterface* tab_list = GetTabListInterface();
  ASSERT_TRUE(tab_list);
  auto* tab_model = static_cast<TabModel*>(tab_list);

  auto* tab_a = tab_list->GetTab(0);
  auto* tab_b = tab_list->OpenTab(GURL("about:blank"), /*index=*/-1);
  auto* tab_c = tab_list->OpenTab(GURL("about:blank"), /*index=*/-1);
  ASSERT_TRUE(tab_b);
  ASSERT_TRUE(tab_c);

  tab_list->ActivateTab(tab_a->GetHandle());
  ASSERT_EQ(tab_a, tab_list->GetActiveTab());
  EXPECT_THAT(tab_model->GetOrderedMultiSelectedTabs(),
              testing::ElementsAre(tab_a->GetHandle()));
  EXPECT_THAT(tab_list->GetHighlightedTabs(), testing::ElementsAre(tab_a));

  tab_list->HighlightTabs(tab_c->GetHandle(),
                          {tab_a->GetHandle(), tab_c->GetHandle()});
  ASSERT_EQ(tab_c, tab_list->GetActiveTab());
  EXPECT_THAT(tab_model->GetOrderedMultiSelectedTabs(),
              testing::ElementsAre(tab_a->GetHandle(), tab_c->GetHandle()));
  EXPECT_THAT(tab_list->GetHighlightedTabs(),
              testing::ElementsAre(tab_a, tab_c));

  tab_list->MoveTab(tab_c->GetHandle(), 0);
  EXPECT_THAT(tab_model->GetOrderedMultiSelectedTabs(),
              testing::ElementsAre(tab_a->GetHandle(), tab_c->GetHandle()));
  EXPECT_THAT(tab_list->GetHighlightedTabs(),
              testing::ElementsAre(tab_c, tab_a));
}

class MockTabListInterfaceObserver : public TabListInterfaceObserver {
 public:
  MOCK_METHOD(void,
              OnTabAdded,
              (TabListInterface & tab_list, tabs::TabInterface* tab, int index),
              (override));
};

IN_PROC_BROWSER_TEST_F(TabModelJniBridgeTest, ObserverOnTabAddedIndex) {
  TabListInterface* tab_list = GetTabListInterface();
  ASSERT_TRUE(tab_list);

  MockTabListInterfaceObserver observer;
  tab_list->AddTabListInterfaceObserver(&observer);

  int expected_index = tab_list->GetTabCount();

  tabs::TabInterface* new_tab = nullptr;
  EXPECT_CALL(observer,
              OnTabAdded(testing::Ref(*tab_list), testing::_, expected_index))
      .WillOnce(testing::SaveArg<1>(&new_tab));

  tabs::TabInterface* opened_tab =
      tab_list->OpenTab(GURL("about:blank"), /*index=*/-1);
  ASSERT_TRUE(opened_tab);
  EXPECT_EQ(opened_tab, new_tab);

  tab_list->RemoveTabListInterfaceObserver(&observer);
}

IN_PROC_BROWSER_TEST_F(TabModelJniBridgeTest, SetMuteSettingSingleTab) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url = embedded_test_server()->GetURL("/title1.html");
  tabs::TabInterface* tab = NavigateTab(GetTabListInterface()->GetTab(0), url);
  ASSERT_TRUE(tab);
  TabAndroid* tab_android = ToTabAndroid(tab);
  ASSERT_TRUE(tab_android);

  // Sound content setting should initially be CONTENT_SETTING_ALLOW.
  EXPECT_EQ(CONTENT_SETTING_ALLOW, GetSoundSetting(url));

  // Muting the tab sets the sound content setting to CONTENT_SETTING_BLOCK and
  // mutes the WebContents audio.
  SetMuteSetting({tab_android}, /*mute=*/true);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, GetSoundSetting(url));
  EXPECT_TRUE(tab->GetContents()->IsAudioMuted());

  // Unmuting the tab restores the sound content setting to
  // CONTENT_SETTING_ALLOW and unmutes the WebContents audio.
  SetMuteSetting({tab_android}, /*mute=*/false);
  EXPECT_EQ(CONTENT_SETTING_ALLOW, GetSoundSetting(url));
  EXPECT_FALSE(tab->GetContents()->IsAudioMuted());
}

IN_PROC_BROWSER_TEST_F(TabModelJniBridgeTest, SetMuteSettingChromeScheme) {
  const GURL chrome_url("chrome://version");
  tabs::TabInterface* tab =
      NavigateTab(GetTabListInterface()->GetTab(0), chrome_url);
  ASSERT_TRUE(tab);
  TabAndroid* tab_android = ToTabAndroid(tab);
  ASSERT_TRUE(tab_android);

  EXPECT_FALSE(tab->GetContents()->IsAudioMuted());

  HostContentSettingsMap* settings_map = GetHostContentSettingsMap();
  const size_t initial_settings_count =
      settings_map->GetSettingsForOneType(ContentSettingsType::SOUND).size();

  // Muting a chrome:// tab toggles WebContents audio mute state without
  // creating a HostContentSettingsMap exception.
  SetMuteSetting({tab_android}, /*mute=*/true);
  EXPECT_TRUE(tab->GetContents()->IsAudioMuted());
  EXPECT_EQ(CONTENT_SETTING_ALLOW, GetSoundSetting(chrome_url));
  EXPECT_EQ(
      initial_settings_count,
      settings_map->GetSettingsForOneType(ContentSettingsType::SOUND).size());

  // Strengthen the verification: iterate through all sound settings to ensure
  // no host-specific pattern was created that matches the chrome:// URL,
  // confirming that internal scheme URLs bypass content settings map rules.
  for (const auto& setting :
       settings_map->GetSettingsForOneType(ContentSettingsType::SOUND)) {
    if (!setting.primary_pattern.MatchesAllHosts()) {
      EXPECT_FALSE(setting.primary_pattern.Matches(chrome_url));
    }
  }

  // Unmuting the chrome:// tab toggles WebContents audio mute back.
  SetMuteSetting({tab_android}, /*mute=*/false);
  EXPECT_FALSE(tab->GetContents()->IsAudioMuted());
  EXPECT_EQ(CONTENT_SETTING_ALLOW, GetSoundSetting(chrome_url));
  EXPECT_EQ(
      initial_settings_count,
      settings_map->GetSettingsForOneType(ContentSettingsType::SOUND).size());
}

IN_PROC_BROWSER_TEST_F(TabModelJniBridgeTest,
                       SetMuteSettingMultipleTabsSameOrigin) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url_a1 = embedded_test_server()->GetURL("a.com", "/title1.html");
  const GURL url_a2 = embedded_test_server()->GetURL("a.com", "/title2.html");
  const GURL url_b = embedded_test_server()->GetURL("b.com", "/title1.html");

  tabs::TabInterface* tab1_interface =
      NavigateTab(GetTabListInterface()->GetTab(0), url_a1);
  ASSERT_TRUE(tab1_interface);
  TabAndroid* tab1 = ToTabAndroid(tab1_interface);
  ASSERT_TRUE(tab1);

  tabs::TabInterface* tab2 = OpenAndNavigateTab(url_a2);
  ASSERT_TRUE(tab2);
  TabAndroid* tab2_android = ToTabAndroid(tab2);
  ASSERT_TRUE(tab2_android);

  tabs::TabInterface* tab3_interface = OpenAndNavigateTab(url_b);
  ASSERT_TRUE(tab3_interface);
  TabAndroid* tab3 = ToTabAndroid(tab3_interface);
  ASSERT_TRUE(tab3);

  EXPECT_EQ(CONTENT_SETTING_ALLOW, GetSoundSetting(url_a1));
  EXPECT_EQ(CONTENT_SETTING_ALLOW, GetSoundSetting(url_a2));
  EXPECT_EQ(CONTENT_SETTING_ALLOW, GetSoundSetting(url_b));

  // Muting tab1 (a.com) and tab3 (b.com) should also mute tab2 because tab2
  // shares the same origin with tab1, even though tab2 is not passed in the
  // list.
  SetMuteSetting({tab1, tab3}, /*mute=*/true);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, GetSoundSetting(url_a1));
  EXPECT_EQ(CONTENT_SETTING_BLOCK, GetSoundSetting(url_a2));
  EXPECT_EQ(CONTENT_SETTING_BLOCK, GetSoundSetting(url_b));
  EXPECT_TRUE(tab1->web_contents()->IsAudioMuted());
  EXPECT_TRUE(tab2->GetContents()->IsAudioMuted());
  EXPECT_TRUE(tab3->web_contents()->IsAudioMuted());

  // Unmuting with tab1, tab2, and tab3 in the same batch tests unmuting with
  // duplicate origins and restores the sound setting for all tabs.
  SetMuteSetting({tab1, tab2_android, tab3}, /*mute=*/false);
  EXPECT_EQ(CONTENT_SETTING_ALLOW, GetSoundSetting(url_a1));
  EXPECT_EQ(CONTENT_SETTING_ALLOW, GetSoundSetting(url_a2));
  EXPECT_EQ(CONTENT_SETTING_ALLOW, GetSoundSetting(url_b));
  EXPECT_FALSE(tab1->web_contents()->IsAudioMuted());
  EXPECT_FALSE(tab2->GetContents()->IsAudioMuted());
  EXPECT_FALSE(tab3->web_contents()->IsAudioMuted());
}

IN_PROC_BROWSER_TEST_F(TabModelJniBridgeTest, SetMuteSettingEmptyUrl) {
  // Insert a new tab with an un-navigated WebContents (empty URL).
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(GetProfile()));
  tabs::TabInterface* empty_tab = GetTabListInterface()->InsertWebContentsAt(
      /*index=*/1, std::move(web_contents), /*should_pin=*/false,
      /*group=*/std::nullopt);
  ASSERT_TRUE(empty_tab);
  EXPECT_TRUE(empty_tab->GetContents()->GetLastCommittedURL().is_empty());

  TabAndroid* empty_tab_android = ToTabAndroid(empty_tab);
  ASSERT_TRUE(empty_tab_android);

  // Passing empty URL tab should gracefully skip without crashing or modifying
  // settings.
  HostContentSettingsMap* settings_map = GetHostContentSettingsMap();
  const size_t initial_settings_count =
      settings_map->GetSettingsForOneType(ContentSettingsType::SOUND).size();
  SetMuteSetting({empty_tab_android}, /*mute=*/true);
  EXPECT_EQ(
      initial_settings_count,
      settings_map->GetSettingsForOneType(ContentSettingsType::SOUND).size());
  SetMuteSetting({empty_tab_android}, /*mute=*/false);
  EXPECT_EQ(
      initial_settings_count,
      settings_map->GetSettingsForOneType(ContentSettingsType::SOUND).size());
}

IN_PROC_BROWSER_TEST_F(TabModelJniBridgeTest,
                       SetMuteSettingWithWildcardPattern) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL subdomain_url =
      embedded_test_server()->GetURL("sub.example.com", "/title1.html");
  const GURL another_subdomain_url =
      embedded_test_server()->GetURL("another.example.com", "/title1.html");

  // Set a wildcard rule to block sound on [*.]example.com.
  GetHostContentSettingsMap()->SetContentSettingCustomScope(
      ContentSettingsPattern::FromString("[*.]example.com"),
      ContentSettingsPattern::Wildcard(), ContentSettingsType::SOUND,
      CONTENT_SETTING_BLOCK);

  tabs::TabInterface* tab =
      NavigateTab(GetTabListInterface()->GetTab(0), subdomain_url);
  ASSERT_TRUE(tab);
  TabAndroid* tab_android = ToTabAndroid(tab);
  ASSERT_TRUE(tab_android);

  // Both subdomains should initially have sound blocked by the wildcard rule.
  EXPECT_EQ(CONTENT_SETTING_BLOCK, GetSoundSetting(subdomain_url));
  EXPECT_EQ(CONTENT_SETTING_BLOCK, GetSoundSetting(another_subdomain_url));

  // Unmuting sub.example.com creates a more specific exception for it.
  SetMuteSetting({tab_android}, /*mute=*/false);
  EXPECT_EQ(CONTENT_SETTING_ALLOW, GetSoundSetting(subdomain_url));

  // Another subdomain should still have sound blocked by the wildcard rule.
  EXPECT_EQ(CONTENT_SETTING_BLOCK, GetSoundSetting(another_subdomain_url));

  // Muting sub.example.com clears the specific exception back to default.
  SetMuteSetting({tab_android}, /*mute=*/true);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, GetSoundSetting(subdomain_url));
}

class TabModelJniBridgeBackgroundTabLoadingTest : public TabModelJniBridgeTest {
 public:
  TabModelJniBridgeBackgroundTabLoadingTest() {
    scoped_feature_list_.InitWithFeatures(
        {chrome::android::kDesktopAndroidBackgroundTabLoading,
         chrome::android::kLoadAllTabsAtStartup},
        {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(TabModelJniBridgeBackgroundTabLoadingTest,
                       BroadcastSessionRestoreCompleteLoadsBackgroundTabs) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url1 = embedded_test_server()->GetURL("/title1.html");
  const GURL url2 = embedded_test_server()->GetURL("/title2.html");

  // Tab 0 is active.
  tabs::TabInterface* tab1 =
      NavigateTab(GetTabListInterface()->GetTab(0), url1);
  ASSERT_TRUE(tab1);

  // Tab 1 is opened in background with restored navigation entry.
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(GetProfile()));
  tabs::TabInterface* tab2 = bridge()->CreateTab(
      /*parent=*/nullptr, std::move(web_contents), /*index=*/1,
      TabModel::TabLaunchType::FROM_TAB_LIST_INTERFACE_BACKGROUND,
      /*should_pin=*/false);
  ASSERT_TRUE(tab2);

  std::vector<std::unique_ptr<content::NavigationEntry>> entries;
  entries.push_back(content::NavigationEntry::Create());
  entries.back()->SetURL(url2);
  tab2->GetContents()->GetController().Restore(
      0, content::RestoreType::kRestored, &entries);
  ASSERT_TRUE(tab2->GetContents()->GetController().NeedsReload());
  ASSERT_EQ(tab2->GetContents()->GetLastCommittedURL(), url2);

  content::TestNavigationObserver observer(tab2->GetContents());
  bridge()->BroadcastSessionRestoreComplete();
  observer.WaitForNavigationFinished();

  EXPECT_FALSE(tab2->GetContents()->GetController().NeedsReload());
  EXPECT_TRUE(observer.last_navigation_succeeded());
}

IN_PROC_BROWSER_TEST_F(
    TabModelJniBridgeBackgroundTabLoadingTest,
    BroadcastSessionRestoreCompleteSkipsAlreadyInitializedTabs) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url1 = embedded_test_server()->GetURL("/title1.html");
  const GURL url2 = embedded_test_server()->GetURL("/title2.html");
  const GURL url3 = embedded_test_server()->GetURL("/title3.html");
  const GURL url4 = embedded_test_server()->GetURL("/simple.html");

  // Tab 0 is active. Wait for any initial startup navigation on Tab 0 to settle
  // before navigating it to `url1`.
  tabs::TabInterface* tab1 = GetTabListInterface()->GetTab(0);
  ASSERT_TRUE(tab1);
  ASSERT_TRUE(content::WaitForLoadStop(tab1->GetContents()));
  ASSERT_TRUE(NavigateTab(tab1, url1));

  auto create_restored_background_tab = [&](int index, const GURL& url) {
    std::unique_ptr<content::WebContents> web_contents =
        content::WebContents::Create(
            content::WebContents::CreateParams(GetProfile()));
    tabs::TabInterface* tab = bridge()->CreateTab(
        /*parent=*/nullptr, std::move(web_contents), index,
        TabModel::TabLaunchType::FROM_TAB_LIST_INTERFACE_BACKGROUND,
        /*should_pin=*/false);
    EXPECT_TRUE(tab);
    std::vector<std::unique_ptr<content::NavigationEntry>> entries;
    entries.push_back(content::NavigationEntry::Create());
    entries.back()->SetURL(url);
    tab->GetContents()->GetController().Restore(
        0, content::RestoreType::kRestored, &entries);
    EXPECT_TRUE(tab->GetContents()->GetController().NeedsReload());
    EXPECT_EQ(tab->GetContents()->GetLastCommittedURL(), url);
    return tab;
  };

  // Tab 1: Background tab that already committed a navigation, then had
  // SetNeedsReload() called (simulating renderer kill / discard / load cancel
  // before deferred startup). Navigated before creating the restored tabs below
  // so any nested message loop during navigation cannot trigger a deferred
  // BroadcastSessionRestoreComplete() before all test tabs are set up.
  std::unique_ptr<content::WebContents> committed_web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(GetProfile()));
  tabs::TabInterface* already_committed_tab = bridge()->CreateTab(
      /*parent=*/nullptr, std::move(committed_web_contents), /*index=*/1,
      TabModel::TabLaunchType::FROM_TAB_LIST_INTERFACE_BACKGROUND,
      /*should_pin=*/false);
  ASSERT_TRUE(already_committed_tab);
  NavigateTab(already_committed_tab, url3);
  if (bridge()->GetActiveIndex() != 0) {
    GetTabListInterface()->ActivateTab(tab1->GetHandle());
  }
  ASSERT_EQ(bridge()->GetActiveIndex(), 0);
  ASSERT_EQ(already_committed_tab->GetContents()
                ->GetPrimaryMainFrame()
                ->GetLastCommittedURL(),
            url3);
  already_committed_tab->GetContents()->GetController().SetNeedsReload();
  ASSERT_TRUE(
      already_committed_tab->GetContents()->GetController().NeedsReload());

  // Tab 2: Restored background tab whose PageNode already had
  // SetMainFrameRestoredState() called by an earlier
  // ScheduleLoadForRestoredTabs call (simulating a reparented tab already
  // scheduled by a prior TabModelJniBridge, whose in-flight restored load was
  // stopped before commit), while NeedsReload() remains true and primary main
  // frame has no committed URL.
  tabs::TabInterface* already_scheduled_tab =
      create_restored_background_tab(/*index=*/2, url2);
  ASSERT_TRUE(already_scheduled_tab);
  content::TestNavigationManager nav_manager(
      already_scheduled_tab->GetContents(), url2);
  performance_manager::policies::SetMaxSimultaneousBackgroundTabLoadsForTesting(
      10);
  performance_manager::policies::ScheduleLoadForRestoredTabs(
      {already_scheduled_tab->GetContents()});
  ASSERT_TRUE(nav_manager.WaitForRequestStart());
  already_scheduled_tab->GetContents()->Stop();
  already_scheduled_tab->GetContents()->GetController().SetNeedsReload();
  ASSERT_TRUE(
      already_scheduled_tab->GetContents()->GetController().NeedsReload());
  ASSERT_TRUE(already_scheduled_tab->GetContents()
                  ->GetPrimaryMainFrame()
                  ->GetLastCommittedURL()
                  .is_empty());
  auto scheduled_page_node =
      performance_manager::PerformanceManager::GetPrimaryPageNodeForWebContents(
          already_scheduled_tab->GetContents());
  ASSERT_TRUE(scheduled_page_node);
  ASSERT_EQ(scheduled_page_node->GetMainFrameUrl(), url2);

  // Tab 3: Fresh restored background tab, never scheduled or navigated yet.
  tabs::TabInterface* fresh_restored_tab =
      create_restored_background_tab(/*index=*/3, url4);
  ASSERT_TRUE(fresh_restored_tab);

  content::TestNavigationObserver observer(fresh_restored_tab->GetContents());
  bridge()->BroadcastSessionRestoreComplete();
  observer.WaitForNavigationFinished();

  EXPECT_TRUE(
      already_scheduled_tab->GetContents()->GetController().NeedsReload());
  EXPECT_TRUE(
      already_committed_tab->GetContents()->GetController().NeedsReload());
  EXPECT_FALSE(
      fresh_restored_tab->GetContents()->GetController().NeedsReload());
  EXPECT_TRUE(observer.last_navigation_succeeded());
}

class TabModelJniBridgeDisabledBackgroundTabLoadingTest
    : public TabModelJniBridgeTest {
 public:
  TabModelJniBridgeDisabledBackgroundTabLoadingTest() {
    scoped_feature_list_.InitAndDisableFeature(
        chrome::android::kDesktopAndroidBackgroundTabLoading);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(TabModelJniBridgeDisabledBackgroundTabLoadingTest,
                       BroadcastSessionRestoreCompleteDoesNotLoadWhenDisabled) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url1 = embedded_test_server()->GetURL("/title1.html");
  const GURL url2 = embedded_test_server()->GetURL("/title2.html");

  // Tab 0 is active.
  tabs::TabInterface* tab1 =
      NavigateTab(GetTabListInterface()->GetTab(0), url1);
  ASSERT_TRUE(tab1);

  // Tab 1 is in background.
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(GetProfile()));
  tabs::TabInterface* tab2 = bridge()->CreateTab(
      /*parent=*/nullptr, std::move(web_contents), /*index=*/1,
      TabModel::TabLaunchType::FROM_TAB_LIST_INTERFACE_BACKGROUND,
      /*should_pin=*/false);
  ASSERT_TRUE(tab2);

  std::vector<std::unique_ptr<content::NavigationEntry>> entries;
  entries.push_back(content::NavigationEntry::Create());
  entries.back()->SetURL(url2);
  tab2->GetContents()->GetController().Restore(
      0, content::RestoreType::kRestored, &entries);
  ASSERT_TRUE(tab2->GetContents()->GetController().NeedsReload());

  EXPECT_FALSE(performance_manager::policies::CanScheduleLoadForRestoredTabs());
  bridge()->BroadcastSessionRestoreComplete();

  // Should still need reload because background loading policy was not active.
  EXPECT_TRUE(tab2->GetContents()->GetController().NeedsReload());
}

IN_PROC_BROWSER_TEST_F(TabModelJniBridgeTest,
                       HandlePopupNavigationPreservesInitiatorPolicyContainer) {
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      [](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        if (request.relative_url != "/csp.html") {
          return nullptr;
        }
        auto response = std::make_unique<net::test_server::BasicHttpResponse>();
        response->set_code(net::HTTP_OK);
        response->set_content_type("text/html");
        response->AddCustomHeader("Content-Security-Policy",
                                  "script-src 'self'");
        response->set_content("<!doctype html><html><body></body></html>");
        return response;
      }));
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL url = embedded_test_server()->GetURL("a.test", "/csp.html");
  TabListInterface* tab_list = GetTabListInterface();
  ASSERT_TRUE(tab_list);
  tabs::TabInterface* initial_tab = NavigateTab(tab_list->GetTab(0), url);
  ASSERT_TRUE(initial_tab);

  content::WebContentsAddedObserver new_tab_observer;
  ASSERT_TRUE(content::ExecJs(initial_tab->GetContents(),
                              R"(
        const html = "<title>INITIAL</title>" +
                     "<script>document.title='EXECUTED';</script>";
        const blobUrl = URL.createObjectURL(
            new Blob([html], {type: "text/html"}));
        const a = document.createElement("a");
        a.href = blobUrl;
        document.body.appendChild(a);
        a.dispatchEvent(new MouseEvent("click", {
          ctrlKey: true,
          shiftKey: true,
          bubbles: true,
          cancelable: true
        }));
      )"));

  content::WebContents* new_contents = new_tab_observer.GetWebContents();
  ASSERT_TRUE(new_contents);
  ASSERT_TRUE(content::WaitForLoadStop(new_contents));

  EXPECT_TRUE(new_contents->GetLastCommittedURL().SchemeIsBlob());
  EXPECT_EQ(u"INITIAL", new_contents->GetTitle());
}

IN_PROC_BROWSER_TEST_F(TabModelJniBridgeTest, CreateSplit_SplitViewEnabled) {
  // CachedFlag ignores `defaultValueInTests` on `is_chrome_branded = true`
  // builds (see CachedFlag.java), so regardless of `defaultValueInTests`, we
  // need to explicitly enable the feature flag here.
  chrome::android::ScopedJavaDrivenFeatureForTesting enable_split_view(
      chrome::android::kEnableAndroidSplitView, true);

  // Arrange:
  TabListInterface* tab_list = GetTabListInterface();
  ASSERT_TRUE(tab_list);

  tabs::TabInterface* tab_a = tab_list->GetTab(0);
  tabs::TabInterface* tab_b =
      tab_list->OpenTab(GURL("about:blank"), /*index=*/-1);
  ASSERT_TRUE(tab_a);
  ASSERT_TRUE(tab_b);

  const split_tabs::SplitTabVisualData expected_visual_data(
      split_tabs::SplitTabLayout::kSideBySide, /*split_ratio=*/0.4);

  // Act:
  std::optional<split_tabs::SplitTabId> split_id = tab_list->CreateSplit(
      {tab_a->GetHandle(), tab_b->GetHandle()}, expected_visual_data);

  // Assert:
  ASSERT_TRUE(split_id.has_value());
  EXPECT_EQ(tab_a->GetSplit(), split_id);
  EXPECT_EQ(tab_b->GetSplit(), split_id);
  EXPECT_THAT(tab_list->ListSplits(), testing::ElementsAre(*split_id));
  EXPECT_EQ(tab_list->GetSplitTabVisualData(*split_id), expected_visual_data);
}

IN_PROC_BROWSER_TEST_F(TabModelJniBridgeTest, CreateSplit_SplitViewDisabled) {
  chrome::android::ScopedJavaDrivenFeatureForTesting disable_split_view(
      chrome::android::kEnableAndroidSplitView, false);

  // Arrange:
  TabListInterface* tab_list = GetTabListInterface();
  ASSERT_TRUE(tab_list);

  tabs::TabInterface* tab_a = tab_list->GetTab(0);
  tabs::TabInterface* tab_b =
      tab_list->OpenTab(GURL("about:blank"), /*index=*/-1);
  ASSERT_TRUE(tab_a);
  ASSERT_TRUE(tab_b);

  const split_tabs::SplitTabVisualData visual_data(
      split_tabs::SplitTabLayout::kSideBySide, /*split_ratio=*/0.4);

  // Act:
  std::optional<split_tabs::SplitTabId> split_id = tab_list->CreateSplit(
      {tab_a->GetHandle(), tab_b->GetHandle()}, visual_data);

  // Assert:
  EXPECT_FALSE(split_id.has_value());
  EXPECT_FALSE(tab_a->GetSplit().has_value());
  EXPECT_FALSE(tab_b->GetSplit().has_value());
  EXPECT_TRUE(tab_list->ListSplits().empty());
}

IN_PROC_BROWSER_TEST_F(TabModelJniBridgeTest, Unsplit) {
  chrome::android::ScopedJavaDrivenFeatureForTesting enable_split_view(
      chrome::android::kEnableAndroidSplitView, true);

  // Arrange:
  TabListInterface* tab_list = GetTabListInterface();
  ASSERT_TRUE(tab_list);

  tabs::TabInterface* tab_a = tab_list->GetTab(/*index=*/0);
  tabs::TabInterface* tab_b =
      tab_list->OpenTab(GURL("about:blank"), /*index=*/-1);
  ASSERT_TRUE(tab_a);
  ASSERT_TRUE(tab_b);

  const split_tabs::SplitTabVisualData visual_data(
      split_tabs::SplitTabLayout::kSideBySide, /*split_ratio=*/0.4);
  std::optional<split_tabs::SplitTabId> split_id = tab_list->CreateSplit(
      {tab_a->GetHandle(), tab_b->GetHandle()}, visual_data);
  ASSERT_TRUE(split_id.has_value());
  EXPECT_EQ(tab_a->GetSplit(), split_id);
  EXPECT_EQ(tab_b->GetSplit(), split_id);
  EXPECT_THAT(tab_list->ListSplits(), testing::ElementsAre(*split_id));
  EXPECT_EQ(tab_list->GetSplitTabVisualData(*split_id), visual_data);
  ASSERT_EQ(tab_list->GetTabCount(), 2);
  EXPECT_EQ(tab_list->GetTab(/*index=*/0), tab_a);
  EXPECT_EQ(tab_list->GetTab(/*index=*/1), tab_b);

  // Act:
  tab_list->Unsplit(*split_id);

  // Assert:
  EXPECT_FALSE(tab_a->GetSplit().has_value());
  EXPECT_FALSE(tab_b->GetSplit().has_value());
  EXPECT_TRUE(tab_list->ListSplits().empty());
  EXPECT_EQ(tab_list->GetSplitTabVisualData(*split_id), std::nullopt);
  ASSERT_EQ(tab_list->GetTabCount(), 2);
  EXPECT_EQ(tab_list->GetTab(/*index=*/0), tab_a);
  EXPECT_EQ(tab_list->GetTab(/*index=*/1), tab_b);
}

}  // namespace
