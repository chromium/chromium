// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tab_search/tab_search_page_handler.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/timer/mock_timer.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/sessions/chrome_tab_restore_service_client.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/tabs/organization/tab_declutter_controller.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/browser/ui/tabs/test_util.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/metrics_reporter/metrics_reporter.h"
#include "chrome/browser/ui/webui/metrics_reporter/mock_metrics_reporter.h"
#include "chrome/browser/ui/webui/tab_search/tab_search.mojom-forward.h"
#include "chrome/browser/ui/webui/tab_search/tab_search_ui.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/sessions/core/tab_restore_service_impl.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "content/public/test/test_web_ui.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/color_utils.h"

using testing::_;
using testing::Truly;

namespace {

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

  MOCK_METHOD(std::vector<tabs::TabModel*>, GetStaleTabs, (), (override));
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
  MOCK_METHOD(void, TabsChanged, (tab_search::mojom::ProfileDataPtr));
  MOCK_METHOD(void, TabUpdated, (tab_search::mojom::TabUpdateInfoPtr));
  MOCK_METHOD(void, TabsRemoved, (tab_search::mojom::TabsRemovedInfoPtr));
  MOCK_METHOD(void, TabSearchTabIndexChanged, (int32_t));
  MOCK_METHOD(void,
              TabOrganizationFeatureChanged,
              (tab_search::mojom::TabOrganizationFeature));
  MOCK_METHOD(void, ShowFREChanged, (bool));
  MOCK_METHOD(void, TabOrganizationEnabledChanged, (bool));
  MOCK_METHOD(void, StaleTabsChanged, (std::vector<tab_search::mojom::TabPtr>));
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
    profile2_ = profile_manager()->CreateTestingProfile(
        "testing_profile2", nullptr, std::u16string(), 0,
        GetTestingFactories());
    browser2_ = CreateTestBrowser(profile1(), false);
    browser3_ = CreateTestBrowser(
        browser()->profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true),
        false);
    browser4_ = CreateTestBrowser(profile2(), false);
    browser5_ = CreateTestBrowser(profile1(), true);
    BrowserList::SetLastActive(browser1());
    webui_controller_ = std::make_unique<TabSearchUI>(web_ui());
    handler_ = std::make_unique<TestTabSearchPageHandler>(
        page_.BindAndGetRemote(), web_ui(), webui_controller_.get());
  }

  void TearDown() override {
    browser1()->tab_strip_model()->CloseAllTabs();
    browser2()->tab_strip_model()->CloseAllTabs();
    browser3()->tab_strip_model()->CloseAllTabs();
    browser4()->tab_strip_model()->CloseAllTabs();
    browser5()->tab_strip_model()->CloseAllTabs();
    browser2_.reset();
    browser3_.reset();
    browser4_.reset();
    browser5_.reset();
    web_contents_.reset();
    // Ensure destructor is called
    handler_ = nullptr;
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
    auto window = std::make_unique<TestBrowserWindow>();
    Browser::Type type = popup ? Browser::TYPE_POPUP : Browser::TYPE_NORMAL;

    std::unique_ptr<Browser> browser =
        CreateBrowser(profile, type, false, window.get());
    BrowserList::SetLastActive(browser.get());
    // Self deleting.
    new TestBrowserWindowOwner(std::move(window));
    return browser;
  }

  std::unique_ptr<content::WebContents> web_contents_;
  content::TestWebUI web_ui_;
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

TEST_F(TabSearchPageHandlerTest, TabsAndGroups) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

  TabRestoreServiceFactory::GetInstance()->SetTestingFactory(
      profile(),
      base::BindRepeating(&TabSearchPageHandlerTest::GetTabRestoreService));

  // Add tabs to a browser.
  AddTabWithTitle(browser1(), GURL(kTabUrl1), kTabName1);
  AddTabWithTitle(browser1(), GURL(kTabUrl2), kTabName2);

  TabStripModel* tab_strip_model = browser1()->tab_strip_model();
  TabGroupModel* tab_group_model = tab_strip_model->group_model();

  // Associate a tab to a given tab group.
  tab_groups::TabGroupId group1 = tab_strip_model->AddToNewGroup({0});

  std::u16string sample_title = u"Sample title";
  const tab_groups::TabGroupColorId sample_color =
      tab_groups::TabGroupColorId::kGrey;
  tab_groups::TabGroupVisualData visual_data1(sample_title, sample_color);
  tab_group_model->GetTabGroup(group1)->SetVisualData(visual_data1);

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
  int tab_id = extensions::ExtensionTabUtil::GetTabId(
      browser1()->tab_strip_model()->GetWebContentsAt(0));
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
  content::WebContentsTester::For(test_web_contents.get())
      ->SetIsCurrentlyAudible(true);
  AddTab(browser(), GURL(kTabUrl1));
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  tab_strip_model->DiscardWebContentsAt(0, std::move(test_web_contents));
  NavigateAndCommitActiveTab(GURL(kTabUrl1));
  tab_search::mojom::PageHandler::GetProfileDataCallback callback =
      base::BindLambdaForTesting(
          [&](tab_search::mojom::ProfileDataPtr profile_tabs) {
            auto* window1 = profile_tabs->windows[0].get();
            auto* tab1 = window1->tabs[0].get();
            EXPECT_EQ(TabAlertState::AUDIO_PLAYING, tab1->alert_states[0]);
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
  TabGroupModel* tab_group_model = tab_strip_model->group_model();

  // Associate a tab to a given tab group.
  tab_groups::TabGroupId group1 = tab_strip_model->AddToNewGroup({0});

  std::u16string sample_title = u"Sample title";
  const tab_groups::TabGroupColorId sample_color =
      tab_groups::TabGroupColorId::kGrey;
  tab_groups::TabGroupVisualData visual_data1(sample_title, sample_color);
  tab_group_model->GetTabGroup(group1)->SetVisualData(visual_data1);

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
  TabGroupModel* tab_group_model = tab_strip_model->group_model();
  tab_group_model->GetTabGroup(group1)->SetVisualData(visual_data1);

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

  int tab_id = extensions::ExtensionTabUtil::GetTabId(
      browser2()->tab_strip_model()->GetWebContentsAt(0));
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

  int tab_id = extensions::ExtensionTabUtil::GetTabId(
      browser1()->tab_strip_model()->GetWebContentsAt(0));
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

  int tab_id = extensions::ExtensionTabUtil::GetTabId(
      browser1()->tab_strip_model()->GetWebContentsAt(0));
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
  TabGroupModel* tab_group_model = tab_strip_model->group_model();
  tab_group_model->GetTabGroup(group1)->SetVisualData(visual_data1);

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

  int tab_id = extensions::ExtensionTabUtil::GetTabId(
      browser1()->tab_strip_model()->GetWebContentsAt(0));
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

  int tab_id = extensions::ExtensionTabUtil::GetTabId(
      browser1()->tab_strip_model()->GetWebContentsAt(0));
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
            tab_data->tab()->contents()->GetLastCommittedURL());
  int tab_id = extensions::ExtensionTabUtil::GetTabId(
      browser1()->tab_strip_model()->GetWebContentsAt(0));
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
  void SetUp() override {
    TabSearchPageHandlerTest::SetUp();
    feature_list_.InitWithFeatures({features::kTabstripDeclutter}, {});
    testing_profile_ = std::make_unique<TestingProfile>();
    tab_strip_model_delegate_ = std::make_unique<TestTabStripModelDelegate>();
    tab_strip_model_ = std::make_unique<TabStripModel>(
        tab_strip_model_delegate_.get(), testing_profile_.get());

    browser_window_interface_ = std::make_unique<MockBrowserWindowInterface>();
    ON_CALL(*browser_window_interface_, GetTabStripModel)
        .WillByDefault(::testing::Return(tab_strip_model_.get()));

    tab_declutter_controller_ = std::make_unique<MockTabDeclutterController>(
        browser_window_interface_.get());
    tab_declutter_controller_->DidBecomeActive(browser_window_interface_.get());

    webui_controller()->InstallTabDeclutterController(
        tab_declutter_controller());
  }

  void TearDown() override {
    // Remove the tab declutter observation first.
    webui_controller()->InstallTabDeclutterController(nullptr);
    handler()->RemoveDeclutterObserverForTesting();

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

 private:
  std::unique_ptr<TestingProfile> testing_profile_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestTabStripModelDelegate> tab_strip_model_delegate_;
  std::unique_ptr<TabStripModel> tab_strip_model_;
  std::unique_ptr<MockTabDeclutterController> tab_declutter_controller_;
  std::unique_ptr<MockBrowserWindowInterface> browser_window_interface_;
  tabs::PreventTabFeatureInitialization prevent_;
};

TEST_F(TabSearchPageHandlerDeclutterTest, TabDeclutterFindStaleTabs) {
  std::vector<tabs::TabModel*> stale_tabs_raw_ptr;

  for (int i = 0; i < 4; ++i) {
    std::unique_ptr<tabs::TabModel> tab_model =
        std::make_unique<tabs::TabModel>(
            content::WebContents::Create(
                content::WebContents::CreateParams(testing_profile())),
            fake_tab_strip_model());
    stale_tabs_raw_ptr.push_back(tab_model.get());
    fake_tab_strip_model()->AppendTab(std::move(tab_model), false);
  }

  EXPECT_CALL(*tab_declutter_controller(), GetStaleTabs())
      .WillOnce(testing::Return(stale_tabs_raw_ptr));

  tab_search::mojom::PageHandler::GetStaleTabsCallback callback =
      base::BindLambdaForTesting(
          [&](std::vector<tab_search::mojom::TabPtr> stale_tabs) {
            EXPECT_EQ(4u, stale_tabs.size());
          });

  // Installing a declutter controller will trigger `GetStaleTabs()`.
  handler()->GetStaleTabs(std::move(callback));
}

TEST_F(TabSearchPageHandlerDeclutterTest, TabDeclutterObserverTest) {
  handler()->TabDeclutterControllerInstalled();

  std::vector<tabs::TabModel*> stale_tabs_raw_ptr;

  for (int i = 0; i < 4; ++i) {
    std::unique_ptr<tabs::TabModel> tab_model =
        std::make_unique<tabs::TabModel>(
            content::WebContents::Create(
                content::WebContents::CreateParams(testing_profile())),
            fake_tab_strip_model());
    stale_tabs_raw_ptr.push_back(tab_model.get());
    fake_tab_strip_model()->AppendTab(std::move(tab_model), false);
  }

  EXPECT_CALL(*tab_declutter_controller(), GetStaleTabs())
      .WillRepeatedly(testing::Return(stale_tabs_raw_ptr));

  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  base::TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner);
  tab_declutter_controller()->SetTimerForTesting(
      task_runner->GetMockTickClock(), task_runner);

  EXPECT_CALL(page_, StaleTabsChanged(_)).Times(2);

  task_runner->FastForwardBy(
      tab_declutter_controller()->declutter_timer_interval());
}

}  // namespace
