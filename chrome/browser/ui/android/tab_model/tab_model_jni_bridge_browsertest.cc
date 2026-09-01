// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/tab_model/tab_model_jni_bridge.h"

#include <memory>
#include <optional>
#include <set>
#include <vector>

#include "base/test/scoped_feature_list.h"
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
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/restore_type.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
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
    bridge()->SetMuteSetting(/*env=*/nullptr, tabs, mute);
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

  tab_list->ActivateTab(tab_a->GetHandle());
  ASSERT_EQ(tab_a, tab_list->GetActiveTab());
  ASSERT_THAT(tab_model->GetOrderedMultiSelectedTabs(),
              testing::ElementsAre(tab_a->GetHandle()));

  tab_list->HighlightTabs(tab_b->GetHandle(),
                          {tab_a->GetHandle(), tab_b->GetHandle()});
  ASSERT_EQ(tab_b, tab_list->GetActiveTab());
  ASSERT_THAT(tab_model->GetOrderedMultiSelectedTabs(),
              testing::ElementsAre(tab_a->GetHandle(), tab_b->GetHandle()));
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
  tabs::TabInterface* tab2 =
      GetTabListInterface()->OpenTab(GURL(), /*index=*/1, /*foreground=*/false);
  ASSERT_TRUE(tab2);

  std::vector<std::unique_ptr<content::NavigationEntry>> entries;
  entries.push_back(content::NavigationEntry::Create());
  entries.back()->SetURL(url2);
  tab2->GetContents()->GetController().Restore(
      0, content::RestoreType::kRestored, &entries);
  ASSERT_TRUE(tab2->GetContents()->GetController().NeedsReload());
  ASSERT_EQ(tab2->GetContents()->GetLastCommittedURL(), url2);

  content::TestNavigationObserver observer(tab2->GetContents());
  bridge()->BroadcastSessionRestoreComplete(/*env=*/nullptr);
  observer.WaitForNavigationFinished();

  EXPECT_FALSE(tab2->GetContents()->GetController().NeedsReload());
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
  tabs::TabInterface* tab2 =
      GetTabListInterface()->OpenTab(GURL(), /*index=*/1, /*foreground=*/false);
  ASSERT_TRUE(tab2);

  std::vector<std::unique_ptr<content::NavigationEntry>> entries;
  entries.push_back(content::NavigationEntry::Create());
  entries.back()->SetURL(url2);
  tab2->GetContents()->GetController().Restore(
      0, content::RestoreType::kRestored, &entries);
  ASSERT_TRUE(tab2->GetContents()->GetController().NeedsReload());

  EXPECT_FALSE(performance_manager::policies::CanScheduleLoadForRestoredTabs());
  bridge()->BroadcastSessionRestoreComplete(/*env=*/nullptr);

  // Should still need reload because background loading policy was not active.
  EXPECT_TRUE(tab2->GetContents()->GetController().NeedsReload());
}

}  // namespace
