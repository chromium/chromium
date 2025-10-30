// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tab_search/tab_search_page_handler.h"

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/timer/mock_timer.h"
#include "build/build_config.h"
#include "chrome/browser/sessions/chrome_tab_restore_service_client.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/recently_audible_helper.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/tabs/alert/tab_alert.h"
#include "chrome/browser/ui/tabs/alert/tab_alert_controller.h"
#include "chrome/browser/ui/tabs/organization/tab_declutter_controller.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_sync_service_initialized_observer.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/metrics_reporter/metrics_reporter.h"
#include "chrome/browser/ui/webui/metrics_reporter/mock_metrics_reporter.h"
#include "chrome/browser/ui/webui/tab_search/tab_search.mojom-forward.h"
#include "chrome/browser/ui/webui/tab_search/tab_search_ui.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/browser/vr/vr_tab_helper.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/sessions/core/tab_restore_service_impl.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/split_tab_data.h"
#include "components/tabs/public/split_tab_id.h"
#include "components/tabs/public/split_tab_visual_data.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_web_ui.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"
#include "ui/base/unowned_user_data/user_data_factory.h"
#include "ui/gfx/color_utils.h"

using testing::_;
using testing::Truly;

namespace {

class TabSearchTabStripModelDelegate : public TestTabStripModelDelegate {
 public:
  TabSearchTabStripModelDelegate() = default;

  void WillAddWebContents(content::WebContents* contents) override {
    TestTabStripModelDelegate::WillAddWebContents(contents);
    // VrTabHelper and audible helper are needed for tab alerts.
    vr::VrTabHelper::CreateForWebContents(contents);
    RecentlyAudibleHelper::CreateForWebContents(contents);
  }
};

constexpr char kTabUrl1[] = "http://foo/1";
constexpr char kTabUrl2[] = "http://foo/2";
constexpr char kTabUrl3[] = "http://foo/3";
constexpr char kTabUrl4[] = "http://foo/4";
constexpr char kTabUrl5[] = "http://foo/5";
constexpr char kTabUrl6[] = "http://foo/6";

constexpr char kTabName1[] = "Tab 1";
constexpr char kTabName2[] = "Tab 2";
constexpr char kTabName3[] = "Tab 3";
constexpr char kTabName4[] = "Tab 4";
constexpr char kTabName5[] = "Tab 5";
constexpr char kTabName6[] = "Tab 6";

class MockTabDeclutterController : public tabs::TabDeclutterController {
 public:
  explicit MockTabDeclutterController(BrowserWindowInterface* browser)
      : TabDeclutterController(browser) {}

  MOCK_METHOD(std::vector<tabs::TabInterface*>, GetStaleTabs, (), (override));
  MOCK_METHOD((std::map<GURL, std::vector<tabs::TabInterface*>>),
              GetDuplicateTabs,
              (),
              (override));
};

class MockPage : public tab_search::mojom::Page {
 public:
  MockPage() = default;
  ~MockPage() override = default;

  mojo::PendingRemote<tab_search::mojom::Page> BindAndGetRemote() {
    DCHECK(!receiver_.is_bound());
    return receiver_.BindNewPipeAndPassRemote();
  }
  mojo::Receiver<tab_search::mojom::Page> receiver_{this};

  MOCK_METHOD(void,
              TabOrganizationSessionUpdated,
              (tab_search::mojom::TabOrganizationSessionPtr));
  MOCK_METHOD(void,
              TabOrganizationModelStrategyUpdated,
              (tab_search::mojom::TabOrganizationModelStrategy));
  MOCK_METHOD(void, HostWindowChanged, ());
  MOCK_METHOD(void, TabsChanged, (tab_search::mojom::ProfileDataPtr));
  MOCK_METHOD(void, TabUpdated, (tab_search::mojom::TabUpdateInfoPtr));
  MOCK_METHOD(void, TabsRemoved, (tab_search::mojom::TabsRemovedInfoPtr));
  MOCK_METHOD(void,
              TabSearchSectionChanged,
              (tab_search::mojom::TabSearchSection));
  MOCK_METHOD(void,
              TabOrganizationFeatureChanged,
              (tab_search::mojom::TabOrganizationFeature));
  MOCK_METHOD(void, ShowFREChanged, (bool));
  MOCK_METHOD(void, TabOrganizationEnabledChanged, (bool));
  MOCK_METHOD(void, UnusedTabsChanged, (tab_search::mojom::UnusedTabInfoPtr));
  MOCK_METHOD(void, TabUnsplit, ());
};

void ExpectNewTab(const tab_search::mojom::Tab* tab,
                  const std::string url,
                  const std::string title,
                  int index) {
  EXPECT_EQ(index, tab->index);
  EXPECT_LT(0, tab->tab_id);
  EXPECT_FALSE(tab->group_id.has_value());
  EXPECT_FALSE(tab->pinned);
  EXPECT_EQ(title, tab->title);
  EXPECT_EQ(url, tab->url.spec());
  EXPECT_TRUE(tab->favicon_url.has_value());
  EXPECT_TRUE(tab->is_default_favicon);
  EXPECT_TRUE(tab->show_icon);
  EXPECT_GT(tab->last_active_time_ticks, base::TimeTicks());
}

void ExpectRecentlyClosedTab(const tab_search::mojom::RecentlyClosedTab* tab,
                             const std::string url,
                             const std::string title) {
  EXPECT_EQ(url, tab->url);
  EXPECT_EQ(title, tab->title);
}

void ExpectProfileTabs(tab_search::mojom::ProfileData* profile_tabs) {
  ASSERT_EQ(2u, profile_tabs->windows.size());
  auto* window1 = profile_tabs->windows[0].get();
  ASSERT_EQ(2u, window1->tabs.size());
  ASSERT_FALSE(window1->tabs[0]->active);
  ASSERT_TRUE(window1->tabs[1]->active);
  auto* window2 = profile_tabs->windows[1].get();
  ASSERT_EQ(1u, window2->tabs.size());
  ASSERT_TRUE(window2->tabs[0]->active);
}

class TestTabSearchPageHandler : public TabSearchPageHandler {
 public:
  TestTabSearchPageHandler(mojo::PendingRemote<tab_search::mojom::Page> page,
                           content::WebUI* web_ui,
                           TabSearchUI* webui_controller)
      : TabSearchPageHandler(
            mojo::PendingReceiver<tab_search::mojom::PageHandler>(),
            std::move(page),
            web_ui,
            webui_controller,
            &metrics_reporter_) {
    mock_debounce_timer_ = new base::MockRetainingOneShotTimer();
    SetTimerForTesting(base::WrapUnique(mock_debounce_timer_.get()));
  }
  base::MockRetainingOneShotTimer* mock_debounce_timer() {
    return mock_debounce_timer_;
  }

 private:
  raw_ptr<base::MockRetainingOneShotTimer> mock_debounce_timer_;
  testing::NiceMock<MockMetricsReporter> metrics_reporter_;
};

class TabSearchPageHandlerTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile()));
    web_ui_.set_web_contents(web_contents_.get());
    webui::SetBrowserWindowInterface(web_contents_.get(), browser());
    profile2_ = profile_manager()->CreateTestingProfile(
        "testing_profile2", nullptr, std::u16string(), 0,
        GetTestingFactories());
    browser2_ = CreateTestBrowser(profile1(), false);
    browser3_ = CreateTestBrowser(
        browser()->profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true),
        false);
    browser4_ = CreateTestBrowser(profile2(), false);
    browser5_ = CreateTestBrowser(profile1(), true);
    browser1()->DidBecomeActive();
    webui_controller_ = std::make_unique<TabSearchUI>(web_ui());

    // Handle any mock calls that might occur during the instantiation of the
    // TabSearchPageHandler. Tests are able to override these calls with more
    // specific EXPECT_CALLS.
    EXPECT_CALL(page_, UnusedTabsChanged(testing::_))
        .WillRepeatedly(testing::Return());

    handler_ = std::make_unique<TestTabSearchPageHandler>(
        page_.BindAndGetRemote(), web_ui(), webui_controller_.get());
    EXPECT_CALL(page_, HostWindowChanged()).Times(1);
    feature_list_.InitWithFeatures(
        {features::kTabstripDeclutter, features::kTabstripDedupe,
         features::kSideBySide},
        {});

    // Wait for the TabGroupSyncService to properly initialize before making any
    // changes to tab groups.
    WaitForTabGroupSyncServiceInitialized();
  }

  void TearDown() override {
    browser1()->tab_strip_model()->CloseAllTabs();
    browser2()->tab_strip_model()->CloseAllTabs();
    browser3()->tab_strip_model()->CloseAllTabs();
    browser4()->tab_strip_model()->CloseAllTabs();
    browser5()->tab_strip_model()->CloseAllTabs();
    handler_.reset();
    webui_controller_.reset();
    browser5_.reset();
    browser4_.reset();
    browser3_.reset();
    browser2_.reset();
    web_contents_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

  content::TestWebUI* web_ui() { return &web_ui_; }
  Profile* profile1() { return browser()->profile(); }
  Profile* profile2() { return profile2_; }

  // The default browser.
  Browser* browser1() { return browser(); }

  // Browser with the same profile of the default browser.
  Browser* browser2() { return browser2_.get(); }

  // Browser with incognito profile.
  Browser* browser3() { return browser3_.get(); }

  // Browser with a different profile of the default browser.
  Browser* browser4() { return browser4_.get(); }

  // Browser with the same profile but not normal type.
  Browser* browser5() { return browser5_.get(); }

  TestTabSearchPageHandler* handler() { return handler_.get(); }
  void FireTimer() { handler_->mock_debounce_timer()->Fire(); }
  bool IsTimerRunning() { return handler_->mock_debounce_timer()->IsRunning(); }

  static std::unique_ptr<KeyedService> GetTabRestoreService(
      content::BrowserContext* browser_context) {
    return std::make_unique<sessions::TabRestoreServiceImpl>(
        std::make_unique<ChromeTabRestoreServiceClient>(
            Profile::FromBrowserContext(browser_context)),
        nullptr, nullptr);
  }

  void WaitForTabGroupSyncServiceInitialized() {
    tab_groups::TabGroupSyncService* tab_group_service_1 =
        tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile1());
    tab_groups::TabGroupSyncService* tab_group_service_2 =
        tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile2());
    auto observer_1 =
        std::make_unique<tab_groups::TabGroupSyncServiceInitializedObserver>(
            tab_group_service_1);
    auto observer_2 =
        std::make_unique<tab_groups::TabGroupSyncServiceInitializedObserver>(
            tab_group_service_2);
    observer_1->Wait();
    observer_2->Wait();
  }

 protected:
  void AddTabWithTitle(Browser* browser,
                       const GURL url,
                       const std::string title) {
    AddTab(browser, url);
    NavigateAndCommitActiveTabWithTitle(browser, url,
                                        base::ASCIIToUTF16(title));
  }

  TabSearchUI* webui_controller() { return webui_controller_.get(); }

  void HideWebContents() {
    web_contents_->WasHidden();
    ASSERT_FALSE(handler_->IsWebContentsVisible());
  }

  testing::StrictMock<MockPage> page_;

 private:
  std::unique_ptr<Browser> CreateTestBrowser(Profile* profile, bool popup) {
    Browser::Type type = popup ? Browser::TYPE_POPUP : Browser::TYPE_NORMAL;
    return CreateBrowser(profile, type, false);
  }

  std::unique_ptr<content::WebContents> web_contents_;
  content::TestWebUI web_ui_;
  base::test::ScopedFeatureList feature_list_;
  raw_ptr<Profile, DanglingUntriaged> profile2_;
  std::unique_ptr<Browser> browser2_;
  std::unique_ptr<Browser> browser3_;
  std::unique_ptr<Browser> browser4_;
  std::unique_ptr<Browser> browser5_;
  std::unique_ptr<TestTabSearchPageHandler> handler_;
  std::unique_ptr<TabSearchUI> webui_controller_;
};

TEST_F(TabSearchPageHandlerTest, GetTabs) {
  // Browser3 and browser4 are using different profiles, browser5 is not a
  // normal type browser, thus their tabs should not be accessible.
  AddTabWithTitle(browser5(), GURL(kTabUrl6), kTabName6);
  AddTabWithTitle(browser4(), GURL(kTabUrl5), kTabName5);
  AddTabWithTitle(browser3(), GURL(kTabUrl4), kTabName4);
  AddTabWithTitle(browser2(), GURL(kTabUrl3), kTabName3);
  AddTabWithTitle(browser1(), GURL(kTabUrl2), kTabName2);
  AddTabWithTitle(browser1(), GURL(kTabUrl1), kTabName1);

  EXPECT_CALL(page_, TabsChanged(_)).Times(1);
  EXPECT_CALL(page_, TabUpdated(_)).Times(2);
  EXPECT_CALL(page_, TabsRemoved(_)).Times(2);
  handler()->mock_debounce_timer()->Fire();

  int32_t tab_id2 = 0;
  int32_t tab_id3 = 0;

  // Get Tabs.
  tab_search::mojom::PageHandler::GetProfileDataCallback callback1 =
      base::BindLambdaForTesting(
          [&](tab_search::mojom::ProfileDataPtr profile_tabs) {
            ASSERT_EQ(2u, profile_tabs->windows.size());
            auto* window1 = profile_tabs->windows[0].get();
            ASSERT_TRUE(window1->active);
            ASSERT_EQ(2u, window1->tabs.size());

            auto* tab1 = window1->tabs[0].get();
            ExpectNewTab(tab1, kTabUrl1, kTabName1, 0);
            ASSERT_TRUE(tab1->active);

            auto* tab2 = window1->tabs[1].get();
            ExpectNewTab(tab2, kTabUrl2, kTabName2, 1);
            ASSERT_FALSE(tab2->active);

            auto* window2 = profile_tabs->windows[1].get();
            ASSERT_FALSE(window2->active);
            ASSERT_EQ(1u, window2->tabs.size());

            auto* tab3 = window2->tabs[0].get();
            ExpectNewTab(tab3, kTabUrl3, kTabName3, 0);
            ASSERT_TRUE(tab3->active);

            tab_id2 = tab2->tab_id;
            tab_id3 = tab3->tab_id;
          });
  handler()->GetProfileData(std::move(callback1));

  // Switch to 2nd tab.
  auto switch_to_tab_info = tab_search::mojom::SwitchToTabInfo::New();
  switch_to_tab_info->tab_id = tab_id2;
  handler()->SwitchToTab(std::move(switch_to_tab_info));

  // Get Tabs again to verify tab switch.
  tab_search::mojom::PageHandler::GetProfileDataCallback callback2 =
      base::BindLambdaForTesting(
          [&](tab_search::mojom::ProfileDataPtr profile_tabs) {
            ExpectProfileTabs(profile_tabs.get());
          });
  handler()->GetProfileData(std::move(callback2));

  // Switch to 3rd tab.
  switch_to_tab_info = tab_search::mojom::SwitchToTabInfo::New();
  switch_to_tab_info->tab_id = tab_id3;
  handler()->SwitchToTab(std::move(switch_to_tab_info));

  // Get Tabs again to verify tab switch.
  tab_search::mojom::PageHandler::GetProfileDataCallback callback3 =
      base::BindLambdaForTesting(
          [&](tab_search::mojom::ProfileDataPtr profile_tabs) {
            ExpectProfileTabs(profile_tabs.get());
          });
  handler()->GetProfileData(std::move(callback3));
}

TEST_F(TabSearchPageHandlerTest, TabActivationChangedByInteraction) {
  EXPECT_CALL(page_, TabUpdated(_)).Times(1);
  EXPECT_CALL(page_, TabsRemoved(_)).Times(1);

  AddTabWithTitle(browser1(), GURL(kTabUrl1), kTabName1);
  AddTabWithTitle(browser1(), GURL(kTabUrl2), kTabName2);

  base::TimeTicks tab1_ticks;
  base::TimeTicks tab2_ticks;

  // Get initial last active time ticks.
  tab_search::mojom::PageHandler::GetProfileDataCallback callback1 =
      base::BindLambdaForTesting(
          [&](tab_search::mojom::ProfileDataPtr profile_tabs) {
            ASSERT_EQ(2u, profile_tabs->windows.size());
            auto* window1 = profile_tabs->windows[0].get();
            ASSERT_EQ(2u, window1->tabs.size());
            // Tabs are in index order.
            tab1_ticks = window1->tabs[0]->last_active_time_ticks;
            tab2_ticks = window1->tabs[1]->last_active_time_ticks;
          });
  handler()->GetProfileData(std::move(callback1));

  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  task_runner->FastForwardBy(base::Seconds(1));

  // Simulate interaction with the first tab.
  browser1()->tab_strip_model()->GetWebContentsAt(0)->Copy();

  // Get last active time ticks again and verify.
  tab_search::mojom::PageHandler::GetProfileDataCallback callback2 =
      base::BindLambdaForTesting(
          [&](tab_search::mojom::ProfileDataPtr profile_tabs) {
            ASSERT_EQ(2u, profile_tabs->windows.size());
            auto* window1 = profile_tabs->windows[0].get();
            ASSERT_EQ(2u, window1->tabs.size());
            base::TimeTicks new_tab1_ticks =
                window1->tabs[0]->last_active_time_ticks;
            base::TimeTicks new_tab2_ticks =
                window1->tabs[1]->last_active_time_ticks;
            EXPECT_GT(new_tab1_ticks, tab1_ticks);
            EXPECT_EQ(new_tab2_ticks, tab2_ticks);
          });
  handler()->GetProfileData(std::move(callback2));
}

TEST_F(TabSearchPageHandlerTest, TabsAndGroups) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

  TabRestoreServiceFactory::GetInstance()->SetTestingFactory(
      profile(),
      base::BindRepeating(&TabSearchPageHandlerTest::GetTabRestoreService));

  // Add tabs to a browser.
  AddTabWithTitle(browser1(), GURL(kTabUrl1), kTabName1);
  AddTabWithTitle(browser1(), GURL(kTabUrl2), kTabName2);

  TabStripModel* tab_strip_model = browser1()->tab_strip_model();

  // Associate a tab to a given tab group.
  tab_groups::TabGroupId group1 = tab_strip_model->AddToNewGroup({0});

  std::u16string sample_title = u"Sample title";
  const tab_groups::TabGroupColorId sample_color =
      tab_groups::TabGroupColorId::kGrey;
  tab_groups::TabGroupVisualData visual_data1(sample_title, sample_color);
  tab_strip_model->ChangeTabGroupVisuals(group1, visual_data1);

  // Get Tabs and Tab Group details.
  tab_search::mojom::PageHandler::GetProfileDataCallback callback1 =
      base::BindLambdaForTesting(
          [&](tab_search::mojom::ProfileDataPtr profile_tabs) {
            ASSERT_EQ(2u, profile_tabs->windows.size());
            auto* window1 = profile_tabs->windows[0].get();
            ASSERT_TRUE(window1->active);
            ASSERT_EQ(2u, window1->tabs.size());

            ASSERT_EQ(1u, profile_tabs->tab_groups.size());
            auto* tab_group = profile_tabs->tab_groups[0].get();
            ASSERT_EQ(sample_color, tab_group->color);
            ASSERT_EQ(base::UTF16ToUTF8(sample_title), tab_group->title);
          });
  handler()->GetProfileData(std::move(callback1));

  // Close a group's tab.
  const int tab_id =
      browser1()->tab_strip_model()->GetTabAtIndex(0)->GetHandle().raw_value();
  handler()->CloseTab(tab_id);

  // Assert the closed tab's data is correct in ProfileData.
  tab_search::mojom::PageHandler::GetProfileDataCallback callback2 =
      base::BindLambdaForTesting(
          [&](tab_search::mojom::ProfileDataPtr profile_tabs) {
            ASSERT_EQ(2u, profile_tabs->windows.size());
            auto* window1 = profile_tabs->windows[0].get();
            ASSERT_TRUE(window1->active);
            ASSERT_EQ(1u, window1->tabs.size());

            auto& tab_groups = profile_tabs->tab_groups;
            ASSERT_EQ(1u, tab_groups.size());
            tab_search::mojom::TabGroup* tab_group = tab_groups[0].get();
            ASSERT_EQ(sample_color, tab_group->color);
            ASSERT_EQ(base::UTF16ToUTF8(sample_title), tab_group->title);

            auto& recently_closed_tabs = profile_tabs->recently_closed_tabs;
            ASSERT_EQ(1u, recently_closed_tabs.size());
            tab_search::mojom::RecentlyClosedTab* tab =
                recently_closed_tabs[0].get();
            ExpectRecentlyClosedTab(tab, kTabUrl2, kTabName2);
            ASSERT_TRUE(tab->group_id);
            ASSERT_EQ(tab_group->id, tab->group_id);
          });
  handler()->GetProfileData(std::move(callback2));

  EXPECT_CALL(page_, TabUpdated(_)).Times(1);
  EXPECT_CALL(page_, TabsRemoved(_)).Times(2);
}

TEST_F(TabSearchPageHandlerTest, MediaTabsTest) {
  std::unique_ptr<content::WebContents> test_web_contents(
      content::WebContentsTester::CreateTestWebContents(
          content::WebContents::CreateParams(profile())));
  content::WebContentsTester* const raw_test_web_contents =
      content::WebContentsTester::For(test_web_contents.get());
  AddTab(browser(), GURL(kTabUrl1));
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  tab_strip_model->DiscardWebContentsAt(0, std::move(test_web_contents));
  raw_test_web_contents->SetIsCurrentlyAudible(true);
  NavigateAndCommitActiveTab(GURL(kTabUrl1));
  tab_search::mojom::PageHandler::GetProfileDataCallback callback =
      base::BindLambdaForTesting(
          [&](tab_search::mojom::ProfileDataPtr profile_tabs) {
            auto* window1 = profile_tabs->windows[0].get();
            auto* tab1 = window1->tabs[0].get();
            EXPECT_EQ(tabs::TabAlert::kAudioPlaying, tab1->alert_states[0]);
          });
  handler()->GetProfileData(std::move(callback));

  // Tab will be removed on tear down.
  EXPECT_CALL(page_, TabsRemoved(_)).Times(1);
}

TEST_F(TabSearchPageHandlerTest, RecentlyClosedTabGroup) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

  TabRestoreServiceFactory::GetInstance()->SetTestingFactory(
      profile(),
      base::BindRepeating(&TabSearchPageHandlerTest::GetTabRestoreService));

  // Add tabs to a browser.
  AddTabWithTitle(browser1(), GURL(kTabUrl1), kTabName1);
  AddTabWithTitle(browser1(), GURL(kTabUrl2), kTabName2);

  TabStripModel* tab_strip_model = browser1()->tab_strip_model();

  // Associate a tab to a given tab group.
  tab_groups::TabGroupId group1 = tab_strip_model->AddToNewGroup({0});

  std::u16string sample_title = u"Sample title";
  const tab_groups::TabGroupColorId sample_color =
      tab_groups::TabGroupColorId::kGrey;
  tab_groups::TabGroupVisualData visual_data1(sample_title, sample_color);
  tab_strip_model->ChangeTabGroupVisuals(group1, visual_data1);

  // Close a group and its tabs.
  tab_strip_model->CloseAllTabsInGroup(group1);

  // Assert the closed tab group and tab data is correct in ProfileData.
  tab_search::mojom::PageHandler::GetProfileDataCallback callback =
      base::BindLambdaForTesting(
          [&](tab_search::mojom::ProfileDataPtr profile_tabs) {
            ASSERT_EQ(2u, profile_tabs->windows.size());
            auto* window1 = profile_tabs->windows[0].get();
            ASSERT_TRUE(window1->active);
            ASSERT_EQ(1u, window1->tabs.size());

            ASSERT_EQ(1u, profile_tabs->tab_groups.size());

            auto& recently_closed_tab_groups =
                profile_tabs->recently_closed_tab_groups;
            ASSERT_EQ(1u, recently_closed_tab_groups.size());
            tab_search::mojom::RecentlyClosedTabGroup* tab_group =
                recently_closed_tab_groups[0].get();
            ASSERT_EQ(sample_color, tab_group->color);
            ASSERT_EQ(base::UTF16ToUTF8(sample_title), tab_group->title);

            auto& recently_closed_tabs = profile_tabs->recently_closed_tabs;
            ASSERT_EQ(1u, recently_closed_tabs.size());
            tab_search::mojom::RecentlyClosedTab* tab =
                recently_closed_tabs[0].get();
            ExpectRecentlyClosedTab(tab, kTabUrl2, kTabName2);
            ASSERT_TRUE(tab->group_id);
            ASSERT_EQ(tab_group->id, tab->group_id);
          });
  handler()->GetProfileData(std::move(callback));

  EXPECT_CALL(page_, TabUpdated(_)).Times(1);
  EXPECT_CALL(page_, TabsRemoved(_)).Times(2);
}

TEST_F(TabSearchPageHandlerTest, RecentlyClosedWindowWithGroupTabs) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

  TabRestoreServiceFactory::GetInstance()->SetTestingFactory(
      profile(),
      base::BindRepeating(&TabSearchPageHandlerTest::GetTabRestoreService));

  // Add tabs to browser windows.
  AddTabWithTitle(browser1(), GURL(kTabUrl1), kTabName1);
  AddTabWithTitle(browser1(), GURL(kTabUrl2), kTabName2);
  AddTabWithTitle(browser2(), GURL(kTabUrl3), kTabName3);
  AddTabWithTitle(browser2(), GURL(kTabUrl4), kTabName4);

  // Associate a tab to a given tab group.
  TabStripModel* tab_strip_model = browser1()->tab_strip_model();
  tab_groups::TabGroupId group1 = tab_strip_model->AddToNewGroup({0});

  std::u16string sample_title = u"Sample title";
  const tab_groups::TabGroupColorId sample_color =
      tab_groups::TabGroupColorId::kGrey;
  tab_groups::TabGroupVisualData visual_data1(sample_title, sample_color);
  tab_strip_model->ChangeTabGroupVisuals(group1, visual_data1);

  // Close the tabs associated with a browser.
  browser1()->tab_strip_model()->CloseAllTabs();

  // Assert that the tabs that were in groups in the closed window contain the
  // associated group data necessary to render properly.
  tab_search::mojom::PageHandler::GetProfileDataCallback callback =
      base::BindLambdaForTesting(
          [&](tab_search::mojom::ProfileDataPtr profile_tabs) {
            ASSERT_EQ(2u, profile_tabs->windows.size());
            auto* window2 = profile_tabs->windows[1].get();
            ASSERT_EQ(2u, window2->tabs.size());

            auto& tab_groups = profile_tabs->tab_groups;
            ASSERT_EQ(1u, tab_groups.size());
            tab_search::mojom::TabGroup* tab_group = tab_groups[0].get();
            ASSERT_EQ(sample_color, tab_group->color);
            ASSERT_EQ(base::UTF16ToUTF8(sample_title), tab_group->title);

            auto& recently_closed_tabs = profile_tabs->recently_closed_tabs;
            ASSERT_EQ(2u, recently_closed_tabs.size());
            tab_search::mojom::RecentlyClosedTab* tab =
                recently_closed_tabs[0].get();
            ExpectRecentlyClosedTab(tab, kTabUrl2, kTabName2);
            ASSERT_TRUE(tab->group_id);
          });
  handler()->GetProfileData(std::move(callback));

  EXPECT_CALL(page_, TabUpdated(_)).Times(2);
  EXPECT_CALL(page_, TabsRemoved(_)).Times(2);
}

// Ensure that repeated tab model changes do not result in repeated calls to
// TabsChanged() and TabsChanged() is only called when the page handler's
// timer fires.
TEST_F(TabSearchPageHandlerTest, TabsChanged) {
  EXPECT_CALL(page_, TabsChanged(_)).Times(3);
  EXPECT_CALL(page_, TabUpdated(_)).Times(1);
  EXPECT_CALL(page_, TabsRemoved(_)).Times(3);
  FireTimer();  // Will call TabsChanged().

  // Add 2 tabs in browser1.
  ASSERT_FALSE(IsTimerRunning());
  AddTabWithTitle(browser1(), GURL(kTabUrl1),
                  kTabName1);  // Will kick off timer.
  ASSERT_TRUE(IsTimerRunning());
  AddTabWithTitle(browser1(), GURL(kTabUrl2), kTabName2);
  // Subsequent tabs change will not change the state of the timer.
  ASSERT_TRUE(IsTimerRunning());
  FireTimer();  // Will call TabsChanged().

  // Add 1 tab in browser2.
  ASSERT_FALSE(IsTimerRunning());
  AddTabWithTitle(browser2(), GURL(kTabUrl3), kTabName3);
  ASSERT_TRUE(IsTimerRunning());
  FireTimer();  // Will call TabsChanged().

  // Close a tab in browser 1.
  ASSERT_FALSE(IsTimerRunning());
  browser1()->tab_strip_model()->CloseWebContentsAt(
      0, TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
  ASSERT_FALSE(IsTimerRunning());
}

// Assert that no browser -> renderer messages are sent when the WebUI is not
// visible.
TEST_F(TabSearchPageHandlerTest, EventsDoNotPropagatedWhenWebUIIsHidden) {
  HideWebContents();
  EXPECT_CALL(page_, TabsChanged(_)).Times(0);
  EXPECT_CALL(page_, TabUpdated(_)).Times(0);
  EXPECT_CALL(page_, TabsRemoved(_)).Times(0);
  FireTimer();

  // Inserting tabs should not cause the debounce timer to start running.
  ASSERT_FALSE(IsTimerRunning());
  AddTabWithTitle(browser1(), GURL(kTabUrl1), kTabName1);
  ASSERT_FALSE(IsTimerRunning());

  // Adding the following tab would usually trigger TabUpdated() for the first
  // tab since the tab index will change from 0 to 1
  AddTabWithTitle(browser1(), GURL(kTabUrl2), kTabName2);

  // Closing a tab would usually result in a call to TabsRemoved().
  browser1()->tab_strip_model()->CloseWebContentsAt(
      0, TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
}

// Ensure that tab model changes in a browser with a different profile
// will not call TabsChanged().
TEST_F(TabSearchPageHandlerTest, TabsNotChanged) {
  EXPECT_CALL(page_, TabsChanged(_)).Times(1);
  EXPECT_CALL(page_, TabUpdated(_)).Times(0);
  FireTimer();  // Will call TabsChanged().
  ASSERT_FALSE(IsTimerRunning());
  AddTabWithTitle(browser3(), GURL(kTabUrl1),
                  kTabName1);  // Will not kick off timer.
  ASSERT_FALSE(IsTimerRunning());
  AddTabWithTitle(browser4(), GURL(kTabUrl2),
                  kTabName2);  // Will not kick off timer.
  ASSERT_FALSE(IsTimerRunning());
}

bool VerifyTabUpdated(
    const tab_search::mojom::TabUpdateInfoPtr& tab_update_info) {
  const tab_search::mojom::TabPtr& tab = tab_update_info->tab;
  ExpectNewTab(tab.get(), kTabUrl1, kTabName1, 1);
  return true;
}

// Verify tab update event is called correctly with data
TEST_F(TabSearchPageHandlerTest, TabUpdated) {
  EXPECT_CALL(page_, TabsChanged(_)).Times(1);
  EXPECT_CALL(page_, TabUpdated(Truly(VerifyTabUpdated))).Times(1);
  EXPECT_CALL(page_, TabsRemoved(_)).Times(1);
  AddTabWithTitle(browser1(), GURL(kTabUrl1), kTabName1);
  // Adding the following tab will trigger TabUpdated() to the first tab
  // since the tab index will change from 0 to 1
  AddTabWithTitle(browser1(), GURL(kTabUrl2), kTabName2);
  FireTimer();
}

TEST_F(TabSearchPageHandlerTest, CloseTab) {
  AddTabWithTitle(browser1(), GURL(kTabUrl1), kTabName1);
  AddTabWithTitle(browser2(), GURL(kTabUrl2), kTabName2);
  AddTabWithTitle(browser2(), GURL(kTabUrl2), kTabName2);
  ASSERT_EQ(1, browser1()->tab_strip_model()->count());
  ASSERT_EQ(2, browser2()->tab_strip_model()->count());

  const int tab_id =
      browser2()->tab_strip_model()->GetTabAtIndex(0)->GetHandle().raw_value();
  EXPECT_CALL(page_, TabUpdated(_)).Times(1);
  EXPECT_CALL(page_, TabsRemoved(_)).Times(3);
  handler()->CloseTab(tab_id);
  ASSERT_EQ(1, browser1()->tab_strip_model()->count());
  ASSERT_EQ(1, browser2()->tab_strip_model()->count());
}

TEST_F(TabSearchPageHandlerTest, RecentlyClosedTab) {
  TabRestoreServiceFactory::GetInstance()->SetTestingFactory(
      profile(),
      base::BindRepeating(&TabSearchPageHandlerTest::GetTabRestoreService));
  AddTabWithTitle(browser1(), GURL(kTabUrl1), kTabName1);
  AddTabWithTitle(browser1(), GURL(kTabUrl2), kTabName2);
  AddTabWithTitle(browser2(), GURL(kTabUrl3), kTabName3);
  AddTabWithTitle(browser2(), GURL(kTabUrl4), kTabName4);
  AddTabWithTitle(browser3(), GURL(kTabUrl5), kTabName5);

  const int tab_id =
      browser1()->tab_strip_model()->GetTabAtIndex(0)->GetHandle().raw_value();
  handler()->CloseTab(tab_id);
  browser2()->tab_strip_model()->CloseAllTabs();
  browser3()->tab_strip_model()->CloseAllTabs();
  tab_search::mojom::PageHandler::GetProfileDataCallback callback =
      base::BindLambdaForTesting(
          [&](tab_search::mojom::ProfileDataPtr profile_tabs) {
            auto& tabs = profile_tabs->recently_closed_tabs;
            ASSERT_EQ(3u, tabs.size());
            ExpectRecentlyClosedTab(tabs[0].get(), kTabUrl4, kTabName4);
            ExpectRecentlyClosedTab(tabs[1].get(), kTabUrl3, kTabName3);
            ExpectRecentlyClosedTab(tabs[2].get(), kTabUrl2, kTabName2);
          });
  handler()->GetProfileData(std::move(callback));
  EXPECT_CALL(page_, TabUpdated(_)).Times(2);
  EXPECT_CALL(page_, TabsRemoved(_)).Times(3);
}

TEST_F(TabSearchPageHandlerTest, OpenRecentlyClosedTab) {
  TabRestoreServiceFactory::GetInstance()->SetTestingFactory(
      profile(),
      base::BindRepeating(&TabSearchPageHandlerTest::GetTabRestoreService));
  AddTabWithTitle(browser1(), GURL(kTabUrl1), kTabName1);
  AddTabWithTitle(browser1(), GURL(kTabUrl2), kTabName2);

  int tab_id =
      browser1()->tab_strip_model()->GetTabAtIndex(0)->GetHandle().raw_value();
  handler()->CloseTab(tab_id);
  tab_search::mojom::PageHandler::GetProfileDataCallback callback1 =
      base::BindLambdaForTesting(
          [&](tab_search::mojom::ProfileDataPtr profile_tabs) {
            auto& tabs = profile_tabs->windows[0]->tabs;
            ASSERT_EQ(1u, tabs.size());
            ExpectNewTab(tabs[0].get(), kTabUrl1, kTabName1, 0);
            auto& recently_closed_tabs = profile_tabs->recently_closed_tabs;
            ASSERT_EQ(1u, recently_closed_tabs.size());
            ExpectRecentlyClosedTab(recently_closed_tabs[0].get(), kTabUrl2,
                                    kTabName2);
            tab_id = recently_closed_tabs[0]->tab_id;
          });
  handler()->GetProfileData(std::move(callback1));
  handler()->OpenRecentlyClosedEntry(tab_id);
  tab_search::mojom::PageHandler::GetProfileDataCallback callback2 =
      base::BindLambdaForTesting(
          [&](tab_search::mojom::ProfileDataPtr profile_tabs) {
            auto& tabs = profile_tabs->windows[0]->tabs;
            ASSERT_EQ(2u, tabs.size());
            ExpectNewTab(tabs[0].get(), kTabUrl1, kTabName1, 0);
            ExpectNewTab(tabs[1].get(), kTabUrl2, kTabName2, 1);
            auto& recently_closed_tabs = profile_tabs->recently_closed_tabs;
            ASSERT_EQ(0u, recently_closed_tabs.size());
          });
  handler()->GetProfileData(std::move(callback2));
  EXPECT_CALL(page_, TabUpdated(_)).Times(1);
  EXPECT_CALL(page_, TabsRemoved(_)).Times(2);
}

TEST_F(TabSearchPageHandlerTest, RecentlyClosedTabsHaveNoRepeatedURLEntry) {
  TabRestoreServiceFactory::GetInstance()->SetTestingFactory(
      profile(),
      base::BindRepeating(&TabSearchPageHandlerTest::GetTabRestoreService));

  AddTabWithTitle(browser1(), GURL(kTabUrl1), kTabName1);
  AddTabWithTitle(browser1(), GURL(kTabUrl1), kTabName1);
  browser1()->tab_strip_model()->CloseAllTabs();
  EXPECT_CALL(page_, TabsRemoved(_)).Times(1);
  EXPECT_CALL(page_, TabUpdated(_)).Times(1);

  tab_search::mojom::PageHandler::GetProfileDataCallback callback1 =
      base::BindLambdaForTesting(
          [&](tab_search::mojom::ProfileDataPtr profile_tabs) {
            auto& recently_closed_tabs = profile_tabs->recently_closed_tabs;
            ASSERT_EQ(1u, recently_closed_tabs.size());
            ExpectRecentlyClosedTab(recently_closed_tabs[0].get(), kTabUrl1,
                                    kTabName1);
          });
  handler()->GetProfileData(std::move(callback1));
}

TEST_F(TabSearchPageHandlerTest,
       RecentlyClosedTabGroupsHaveNoRepeatedURLEntries) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

  TabRestoreServiceFactory::GetInstance()->SetTestingFactory(
      profile(),
      base::BindRepeating(&TabSearchPageHandlerTest::GetTabRestoreService));

  // Add tabs to a browser.
  AddTabWithTitle(browser1(), GURL(kTabUrl1), kTabName1);
  AddTabWithTitle(browser1(), GURL(kTabUrl1), kTabName1);
  AddTabWithTitle(browser2(), GURL(kTabUrl1), kTabName1);
  AddTabWithTitle(browser2(), GURL(kTabUrl1), kTabName1);

  // Associate tabs to a given tab group.
  TabStripModel* tab_strip_model = browser1()->tab_strip_model();
  tab_groups::TabGroupId group1 = tab_strip_model->AddToNewGroup({0, 1});

  std::u16string sample_title = u"Sample title";
  const tab_groups::TabGroupColorId sample_color =
      tab_groups::TabGroupColorId::kGrey;
  tab_groups::TabGroupVisualData visual_data1(sample_title, sample_color);
  tab_strip_model->ChangeTabGroupVisuals(group1, visual_data1);

  browser1()->tab_strip_model()->CloseAllTabs();
  browser2()->tab_strip_model()->CloseAllTabs();

  tab_search::mojom::PageHandler::GetProfileDataCallback callback1 =
      base::BindLambdaForTesting(
          [&](tab_search::mojom::ProfileDataPtr profile_tabs) {
            auto& recently_closed_tabs = profile_tabs->recently_closed_tabs;
            ASSERT_EQ(2u, recently_closed_tabs.size());
            ExpectRecentlyClosedTab(recently_closed_tabs[0].get(), kTabUrl1,
                                    kTabName1);
            ExpectRecentlyClosedTab(recently_closed_tabs[1].get(), kTabUrl1,
                                    kTabName1);
          });
  handler()->GetProfileData(std::move(callback1));

  EXPECT_CALL(page_, TabsRemoved(_)).Times(2);
  EXPECT_CALL(page_, TabUpdated(_)).Times(2);
}

TEST_F(TabSearchPageHandlerTest, RecentlyClosedTabEntriesFilterOpenTabUrls) {
  TabRestoreServiceFactory::GetInstance()->SetTestingFactory(
      profile(),
      base::BindRepeating(&TabSearchPageHandlerTest::GetTabRestoreService));

  AddTabWithTitle(browser1(), GURL(kTabUrl1), kTabName1);
  AddTabWithTitle(browser1(), GURL(kTabUrl1), kTabName1);

  const int tab_id =
      browser1()->tab_strip_model()->GetTabAtIndex(0)->GetHandle().raw_value();
  handler()->CloseTab(tab_id);

  EXPECT_CALL(page_, TabsRemoved(_)).Times(2);
  EXPECT_CALL(page_, TabUpdated(_)).Times(1);

  tab_search::mojom::PageHandler::GetProfileDataCallback callback1 =
      base::BindLambdaForTesting(
          [&](tab_search::mojom::ProfileDataPtr profile_tabs) {
            auto& tabs = profile_tabs->windows[0]->tabs;
            ASSERT_EQ(1u, tabs.size());
            ExpectNewTab(tabs[0].get(), kTabUrl1, kTabName1, 0);
            auto& recently_closed_tabs = profile_tabs->recently_closed_tabs;
            ASSERT_EQ(0u, recently_closed_tabs.size());
          });
  handler()->GetProfileData(std::move(callback1));
}

TEST_F(TabSearchPageHandlerTest, RecentlyClosedSectionExpandedUserPref) {
  TabRestoreServiceFactory::GetInstance()->SetTestingFactory(
      profile(),
      base::BindRepeating(&TabSearchPageHandlerTest::GetTabRestoreService));

  AddTabWithTitle(browser1(), GURL(kTabUrl1), kTabName1);
  AddTabWithTitle(browser1(), GURL(kTabUrl2), kTabName2);

  const int tab_id =
      browser1()->tab_strip_model()->GetTabAtIndex(0)->GetHandle().raw_value();
  handler()->CloseTab(tab_id);

  EXPECT_CALL(page_, TabsRemoved(_)).Times(2);
  EXPECT_CALL(page_, TabUpdated(_)).Times(1);

  tab_search::mojom::PageHandler::GetProfileDataCallback callback1 =
      base::BindLambdaForTesting(
          [&](tab_search::mojom::ProfileDataPtr profile_tabs) {
            auto& tabs = profile_tabs->windows[0]->tabs;
            ASSERT_EQ(1u, tabs.size());
            ExpectNewTab(tabs[0].get(), kTabUrl1, kTabName1, 0);
            auto& recently_closed_tabs = profile_tabs->recently_closed_tabs;
            ASSERT_EQ(1u, recently_closed_tabs.size());
            ASSERT_TRUE(profile_tabs->recently_closed_section_expanded);
          });
  handler()->GetProfileData(std::move(callback1));

  handler()->SaveRecentlyClosedExpandedPref(false);
  tab_search::mojom::PageHandler::GetProfileDataCallback callback2 =
      base::BindLambdaForTesting(
          [&](tab_search::mojom::ProfileDataPtr profile_tabs) {
            ASSERT_FALSE(profile_tabs->recently_closed_section_expanded);
          });
  handler()->GetProfileData(std::move(callback2));
}

TEST_F(TabSearchPageHandlerTest, TabDataToMojo) {
  AddTabWithTitle(browser1(), GURL(kTabUrl1), kTabName1);
  std::unique_ptr<TabData> tab_data = std::make_unique<TabData>(
      browser1()->tab_strip_model()->GetTabAtIndex(0));
  tab_search::mojom::TabPtr mojo_tab_ptr =
      handler()->GetMojoForTabData(tab_data.get());

  EXPECT_EQ(mojo_tab_ptr->url,
            tab_data->tab()->GetContents()->GetLastCommittedURL());
  const int tab_id =
      browser1()->tab_strip_model()->GetTabAtIndex(0)->GetHandle().raw_value();
  handler()->CloseTab(tab_id);
  EXPECT_CALL(page_, TabsRemoved(_)).Times(1);
}

TEST_F(TabSearchPageHandlerTest, TabOrganizationToMojo) {
  std::unique_ptr<TabOrganization> organization =
      std::make_unique<TabOrganization>(
          std::vector<std::unique_ptr<TabData>>{},
          std::vector<std::u16string>{u"default_name"});
  tab_search::mojom::TabOrganizationPtr mojo_tab_org_ptr =
      handler()->GetMojoForTabOrganization(organization.get());

  EXPECT_EQ(mojo_tab_org_ptr->name, organization->GetDisplayName());
  EXPECT_EQ(mojo_tab_org_ptr->organization_id, organization->organization_id());
}

TEST_F(TabSearchPageHandlerTest, TabOrganizationSessionToMojo) {
  std::unique_ptr<TabOrganizationSession> session =
      std::make_unique<TabOrganizationSession>();
  tab_search::mojom::TabOrganizationSessionPtr mojo_session_ptr =
      handler()->GetMojoForTabOrganizationSession(session.get());

  EXPECT_EQ(mojo_session_ptr->session_id, session->session_id());
}

TEST_F(TabSearchPageHandlerTest, TabOrganizationSessionObservation) {
  std::unique_ptr<TabOrganizationSession> session =
      std::make_unique<TabOrganizationSession>();
  EXPECT_CALL(page_, TabOrganizationSessionUpdated(_)).Times(3);
  // Register it with the page handler under the same profile
  handler()->OnSessionCreated(browser1(), session.get());

  // Updating should notify the page.
  handler()->OnTabOrganizationSessionUpdated(session.get());

  // Destroying should notify the page.
  session.reset();
}

TEST_F(TabSearchPageHandlerTest,
       TabOrganizationSessionObservationWrongProfile) {
  std::unique_ptr<TabOrganizationSession> session =
      std::make_unique<TabOrganizationSession>();
  EXPECT_CALL(page_, TabOrganizationSessionUpdated(_)).Times(0);
  // Registering with the page handler in the wrong profile should not notify.
  handler()->OnSessionCreated(browser4(), session.get());

  // Updating should not notify the page.
  handler()->OnTabOrganizationSessionUpdated(session.get());

  // Destroying should not notify the page.
  session.reset();
}

class TabSearchPageHandlerDeclutterTest : public TabSearchPageHandlerTest {
 public:
  TabSearchPageHandlerDeclutterTest() = default;
  ~TabSearchPageHandlerDeclutterTest() override = default;

  void SetUp() override {
    TabSearchPageHandlerTest::SetUp();

    testing_profile_ = std::make_unique<TestingProfile>();
    tab_strip_model_delegate_ =
        std::make_unique<TabSearchTabStripModelDelegate>();
    tab_strip_model_ = std::make_unique<TabStripModel>(
        tab_strip_model_delegate_.get(), testing_profile_.get());

    browser_window_interface_ = std::make_unique<MockBrowserWindowInterface>();
    ON_CALL(*browser_window_interface_, GetUnownedUserDataHost())
        .WillByDefault(::testing::ReturnRef(user_data_host_));
    ON_CALL(*browser_window_interface_, GetTabStripModel())
        .WillByDefault(::testing::Return(tab_strip_model_.get()));

    tab_declutter_controller_ = std::make_unique<MockTabDeclutterController>(
        browser_window_interface_.get());
    tab_declutter_controller_->DidBecomeActive(browser_window_interface_.get());
    handler()->SetTabDeclutterControllerForTesting(
        tab_declutter_controller_.get());
  }

  void TearDown() override {
    tab_interface_to_alert_controller_.clear();
    // Remove the tab declutter observation first.
    handler()->SetTabDeclutterControllerForTesting(nullptr);

    tab_declutter_controller_.reset();
    tab_strip_model_.reset();
    tab_strip_model_delegate_.reset();
    testing_profile_.reset();
    TabSearchPageHandlerTest::TearDown();
  }

  MockTabDeclutterController* tab_declutter_controller() {
    return tab_declutter_controller_.get();
  }
  TabStripModel* fake_tab_strip_model() { return tab_strip_model_.get(); }
  Profile* testing_profile() { return testing_profile_.get(); }

  void CloseTab(int index) {
    tabs::TabInterface* const tab_interface =
        fake_tab_strip_model()->GetTabAtIndex(index);
    tab_interface_to_alert_controller_.erase(tab_interface);
    fake_tab_strip_model()->CloseWebContentsAt(index,
                                               TabCloseTypes::CLOSE_NONE);
  }

  tabs::TabInterface* AppendBackgroundTab() {
    std::unique_ptr<tabs::TabModel> tab_model =
        std::make_unique<tabs::TabModel>(
            content::WebContents::Create(
                content::WebContents::CreateParams(testing_profile())),
            fake_tab_strip_model());
    tabs::TabFeatures* const tab_features = tab_model->GetTabFeatures();
    tabs::TabInterface* const tab_interface = tab_model.get();
    tab_features->SetTabUIHelperForTesting(
        std::make_unique<TabUIHelper>(*tab_interface));
    std::unique_ptr<tabs::TabAlertController> tab_alert_controller =
        tabs::TabFeatures::GetUserDataFactoryForTesting()
            .CreateInstance<tabs::TabAlertController>(*tab_interface,
                                                      *tab_interface);
    tab_interface_to_alert_controller_.insert(
        {tab_interface, std::move(tab_alert_controller)});
    fake_tab_strip_model()->AppendTab(std::move(tab_model), false);
    return tab_interface;
  }

 private:
  ui::UnownedUserDataHost user_data_host_;
  std::unique_ptr<TestingProfile> testing_profile_;
  std::unique_ptr<TabSearchTabStripModelDelegate> tab_strip_model_delegate_;
  std::unique_ptr<TabStripModel> tab_strip_model_;
  std::unique_ptr<MockTabDeclutterController> tab_declutter_controller_;
  std::unique_ptr<MockBrowserWindowInterface> browser_window_interface_;
  const tabs::TabModel::PreventFeatureInitializationForTesting prevent_;
  ui::UserDataFactory::ScopedOverride tab_alert_controller_override_;
  std::map<tabs::TabInterface* const, std::unique_ptr<tabs::TabAlertController>>
      tab_interface_to_alert_controller_;
};

TEST_F(TabSearchPageHandlerDeclutterTest, TabDeclutterFindUnusedTabs) {
  EXPECT_CALL(page_, UnusedTabsChanged(_)).Times(1);

  // Create stale tabs.
  std::vector<tabs::TabInterface*> stale_tabs_raw_ptr;
  for (int i = 0; i < 4; ++i) {
    stale_tabs_raw_ptr.push_back(AppendBackgroundTab());
  }

  // Create duplicate tabs.
  std::map<GURL, std::vector<tabs::TabInterface*>> duplicate_tabs;
  GURL duplicate_tabs_url("https://duplicate_url.com");
  for (int i = 0; i < 2; ++i) {
    duplicate_tabs[duplicate_tabs_url].push_back(AppendBackgroundTab());
  }

  EXPECT_CALL(*tab_declutter_controller(), GetStaleTabs())
      .WillOnce(testing::Return(stale_tabs_raw_ptr));

  EXPECT_CALL(*tab_declutter_controller(), GetDuplicateTabs())
      .WillOnce(testing::Return(duplicate_tabs));

  tab_search::mojom::PageHandler::GetUnusedTabsCallback callback =
      base::BindLambdaForTesting(
          [&](tab_search::mojom::UnusedTabInfoPtr unused_tabs) {
            // Verify stale tabs.
            EXPECT_EQ(4u, unused_tabs->stale_tabs.size());

            // Verify duplicate tabs.
            auto it =
                unused_tabs->duplicate_tabs.find(duplicate_tabs_url.spec());
            ASSERT_NE(it, unused_tabs->duplicate_tabs.end());
            EXPECT_EQ(2u, it->second.size());
          });

  handler()->GetUnusedTabs(std::move(callback));
}

TEST_F(TabSearchPageHandlerDeclutterTest, TabDeclutterExcludeTabs) {
  EXPECT_CALL(page_, UnusedTabsChanged(_)).Times(2);

  // Create stale tabs.
  std::vector<tabs::TabInterface*> stale_tabs_raw_ptr;
  for (int i = 0; i < 4; ++i) {
    stale_tabs_raw_ptr.push_back(AppendBackgroundTab());
  }

  // Create duplicate tabs.
  std::map<GURL, std::vector<tabs::TabInterface*>> duplicate_tabs;
  GURL duplicate_tabs_url("https://duplicate_url.com");
  for (int i = 0; i < 2; ++i) {
    tabs::TabInterface* const tab_interface = AppendBackgroundTab();
    auto navigation_simulator =
        content::NavigationSimulator::CreateBrowserInitiated(
            duplicate_tabs_url, tab_interface->GetContents());
    navigation_simulator->Commit();
    duplicate_tabs[duplicate_tabs_url].push_back(tab_interface);
  }

  EXPECT_CALL(*tab_declutter_controller(), GetStaleTabs())
      .WillOnce(testing::Return(stale_tabs_raw_ptr));

  EXPECT_CALL(*tab_declutter_controller(), GetDuplicateTabs())
      .WillOnce(testing::Return(duplicate_tabs));

  tab_search::mojom::PageHandler::GetUnusedTabsCallback callback =
      base::BindLambdaForTesting(
          [&](tab_search::mojom::UnusedTabInfoPtr unused_tabs) {
            // Verify stale tabs.
            EXPECT_EQ(4u, unused_tabs->stale_tabs.size());

            // Verify duplicate tabs.
            auto it =
                unused_tabs->duplicate_tabs.find(duplicate_tabs_url.spec());
            ASSERT_NE(it, unused_tabs->duplicate_tabs.end());
            EXPECT_EQ(2u, it->second.size());
          });

  handler()->GetUnusedTabs(std::move(callback));

  handler()->ExcludeFromDuplicateTabs(duplicate_tabs_url.GetWithoutRef());
  EXPECT_TRUE(handler()->duplicate_tabs_for_testing().empty());
}

TEST_F(TabSearchPageHandlerDeclutterTest, TabDeclutterObserverTest) {
  EXPECT_CALL(page_, UnusedTabsChanged(_)).Times(2);
  std::vector<tabs::TabInterface*> stale_tabs_raw_ptr;

  for (int i = 0; i < 4; ++i) {
    stale_tabs_raw_ptr.push_back(AppendBackgroundTab());
  }

  std::map<GURL, std::vector<tabs::TabInterface*>> duplicate_tabs;
  GURL duplicate_tabs_url("https://duplicate_url.com");
  for (int i = 0; i < 2; ++i) {
    duplicate_tabs[duplicate_tabs_url].push_back(AppendBackgroundTab());
  }

  EXPECT_CALL(*tab_declutter_controller(), GetStaleTabs())
      .WillRepeatedly(testing::Return(stale_tabs_raw_ptr));

  EXPECT_CALL(*tab_declutter_controller(), GetDuplicateTabs())
      .WillRepeatedly(testing::Return(duplicate_tabs));

  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  base::TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner);
  tab_declutter_controller()->SetTimerForTesting(
      task_runner->GetMockTickClock(), task_runner);

  task_runner->FastForwardBy(
      tab_declutter_controller()->declutter_timer_interval());
}

TEST_F(TabSearchPageHandlerDeclutterTest, TabDeclutterUnusedTabChanges) {
  EXPECT_CALL(page_, UnusedTabsChanged(_)).Times(::testing::AtLeast(1));
  std::vector<tabs::TabInterface*> stale_tabs_raw_ptr;

  // Create 10 stale tabs.
  for (int i = 0; i < 10; ++i) {
    stale_tabs_raw_ptr.push_back(AppendBackgroundTab());
  }

  // Create duplicate tabs.
  std::map<GURL, std::vector<tabs::TabInterface*>> duplicate_tabs;
  GURL duplicate_tabs_url("https://duplicate_url.com");
  for (int i = 0; i < 5; ++i) {
    tabs::TabInterface* const tab_interface = AppendBackgroundTab();
    auto navigation_simulator =
        content::NavigationSimulator::CreateBrowserInitiated(
            duplicate_tabs_url, tab_interface->GetContents());
    navigation_simulator->Commit();
    duplicate_tabs[duplicate_tabs_url].push_back(tab_interface);
  }

  EXPECT_CALL(*tab_declutter_controller(), GetStaleTabs())
      .WillRepeatedly(testing::Return(stale_tabs_raw_ptr));

  EXPECT_CALL(*tab_declutter_controller(), GetDuplicateTabs())
      .WillOnce(testing::Return(duplicate_tabs));

  tab_search::mojom::PageHandler::GetUnusedTabsCallback callback =
      base::BindLambdaForTesting(
          [&](tab_search::mojom::UnusedTabInfoPtr unused_tabs) {
            // Verify stale tabs.
            EXPECT_EQ(10u, unused_tabs->stale_tabs.size());

            // Verify duplicate tabs.
            auto it =
                unused_tabs->duplicate_tabs.find(duplicate_tabs_url.spec());
            ASSERT_NE(it, unused_tabs->duplicate_tabs.end());
            EXPECT_EQ(5u, it->second.size());
          });

  handler()->GetUnusedTabs(std::move(callback));

  // Make a stale tab a part of a group. It should remove it from the internal
  // stale tab list.
  fake_tab_strip_model()->AddToNewGroup({1});
  EXPECT_EQ(handler()->stale_tabs_for_testing().size(), 9u);

  // Make a stale tab pinned. It should remove it from the internal stale tab
  // list.
  fake_tab_strip_model()->SetTabPinned(2, true);
  EXPECT_EQ(handler()->stale_tabs_for_testing().size(), 8u);

  // Activate a stale tab. It should remove it from the internal stale tab list.
  fake_tab_strip_model()->ActivateTabAt(3);
  EXPECT_EQ(handler()->stale_tabs_for_testing().size(), 7u);

  // Detach a stale tab. It should remove it from the internal stale tab list.
  CloseTab(4);
  EXPECT_EQ(handler()->stale_tabs_for_testing().size(), 6u);

  fake_tab_strip_model()->AddToNewGroup(
      {fake_tab_strip_model()->GetTabCount() - 1});
  EXPECT_EQ(handler()->duplicate_tabs_for_testing()[duplicate_tabs_url].size(),
            4u);

  fake_tab_strip_model()->SetTabPinned(
      fake_tab_strip_model()->GetTabCount() - 2, true);
  EXPECT_EQ(handler()->duplicate_tabs_for_testing()[duplicate_tabs_url].size(),
            3u);

  CloseTab(fake_tab_strip_model()->GetTabCount() - 2);
  EXPECT_EQ(handler()->duplicate_tabs_for_testing()[duplicate_tabs_url].size(),
            2u);
}

TEST_F(TabSearchPageHandlerDeclutterTest, TabDuplicateURLChanges) {
  EXPECT_CALL(page_, UnusedTabsChanged(_)).Times(::testing::AtLeast(1));
  std::vector<tabs::TabInterface*> stale_tabs_raw_ptr;

  // Create 10 stale tabs.
  for (int i = 0; i < 10; ++i) {
    stale_tabs_raw_ptr.push_back(AppendBackgroundTab());
  }

  // Create duplicate tabs.
  std::map<GURL, std::vector<tabs::TabInterface*>> duplicate_tabs;
  GURL duplicate_tabs_url("https://duplicate_url.com");
  for (int i = 0; i < 5; ++i) {
    tabs::TabInterface* const tab_interface = AppendBackgroundTab();

    auto navigation_simulator =
        content::NavigationSimulator::CreateBrowserInitiated(
            duplicate_tabs_url, tab_interface->GetContents());
    navigation_simulator->Commit();
    duplicate_tabs[duplicate_tabs_url].push_back(tab_interface);
  }

  EXPECT_CALL(*tab_declutter_controller(), GetStaleTabs())
      .WillRepeatedly(testing::Return(stale_tabs_raw_ptr));

  EXPECT_CALL(*tab_declutter_controller(), GetDuplicateTabs())
      .WillOnce(testing::Return(duplicate_tabs));

  tab_search::mojom::PageHandler::GetUnusedTabsCallback callback =
      base::BindLambdaForTesting(
          [&](tab_search::mojom::UnusedTabInfoPtr unused_tabs) {
            // Verify stale tabs.
            EXPECT_EQ(10u, unused_tabs->stale_tabs.size());

            // Verify duplicate tabs.
            auto it =
                unused_tabs->duplicate_tabs.find(duplicate_tabs_url.spec());
            ASSERT_NE(it, unused_tabs->duplicate_tabs.end());
            EXPECT_EQ(5u, it->second.size());
          });

  handler()->GetUnusedTabs(std::move(callback));

  GURL new_url("https://duplicate_url_two.com");

  auto navigation_simulator =
      content::NavigationSimulator::CreateBrowserInitiated(
          new_url,
          fake_tab_strip_model()
              ->GetTabAtIndex(fake_tab_strip_model()->GetTabCount() - 1)
              ->GetContents());
  navigation_simulator->Commit();

  EXPECT_EQ(handler()->duplicate_tabs_for_testing()[duplicate_tabs_url].size(),
            4u);

  fake_tab_strip_model()->DiscardWebContentsAt(
      fake_tab_strip_model()->GetTabCount() - 2,
      content::WebContents::Create(
          content::WebContents::CreateParams(testing_profile())));
  EXPECT_EQ(handler()->duplicate_tabs_for_testing()[duplicate_tabs_url].size(),
            3u);
}

TEST_F(TabSearchPageHandlerTest, ReplaceActiveSplitTab) {
  std::unique_ptr<content::WebContents> test_web_contents(
      content::WebContentsTester::CreateTestWebContents(
          content::WebContents::CreateParams(profile())));
  AddTab(browser(), GURL(kTabUrl1));
  AddTab(browser(), GURL(kTabUrl2));
  AddTab(browser(), GURL(kTabUrl3));
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  const split_tabs::SplitTabId split_id = tab_strip_model->AddToNewSplit(
      {1}, split_tabs::SplitTabVisualData(),
      split_tabs::SplitTabCreatedSource::kToolbarButton);

  const split_tabs::SplitTabData* split_data =
      tab_strip_model->GetSplitData(split_id);
  const std::vector<tabs::TabInterface*> tabs_in_split = split_data->ListTabs();
  EXPECT_EQ(tabs_in_split.size(), 2u);
  EXPECT_EQ(kTabUrl3, tabs_in_split[0]->GetContents()->GetURL().spec());
  EXPECT_EQ(kTabUrl2, tabs_in_split[1]->GetContents()->GetURL().spec());

  EXPECT_FALSE(tab_strip_model->GetTabAtIndex(2)->IsSplit());
  const int32_t replacement_tab_id =
      tab_strip_model->GetTabAtIndex(2)->GetHandle().raw_value();
  handler()->ReplaceActiveSplitTab(replacement_tab_id);

  const split_tabs::SplitTabData* split_data_after_replacement =
      tab_strip_model->GetSplitData(split_id);
  const std::vector<tabs::TabInterface*> tabs_in_split_after_replacement =
      split_data_after_replacement->ListTabs();
  EXPECT_EQ(tabs_in_split_after_replacement.size(), 2u);
  EXPECT_EQ(kTabUrl1,
            tabs_in_split_after_replacement[0]->GetContents()->GetURL().spec());
  EXPECT_EQ(kTabUrl2,
            tabs_in_split_after_replacement[1]->GetContents()->GetURL().spec());

  EXPECT_CALL(page_, TabUpdated(_)).Times(3);
  EXPECT_CALL(page_, TabsRemoved(_)).Times(2);
}

}  // namespace
