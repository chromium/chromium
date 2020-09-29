// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tab_search/tab_search_page_handler.h"

#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/timer/mock_timer.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/test/test_web_ui.h"
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

constexpr char kTabName1[] = "Tab 1";
constexpr char kTabName2[] = "Tab 2";
constexpr char kTabName3[] = "Tab 3";
constexpr char kTabName4[] = "Tab 4";
constexpr char kTabName5[] = "Tab 5";

class MockPage : public tab_search::mojom::Page {
 public:
  MockPage() = default;
  ~MockPage() override = default;

  mojo::PendingRemote<tab_search::mojom::Page> BindAndGetRemote() {
    DCHECK(!receiver_.is_bound());
    return receiver_.BindNewPipeAndPassRemote();
  }
  mojo::Receiver<tab_search::mojom::Page> receiver_{this};

  MOCK_METHOD0(TabsChanged, void());
  MOCK_METHOD1(TabUpdated, void(tab_search::mojom::TabPtr));
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
  EXPECT_EQ(url, tab->url);
  EXPECT_TRUE(tab->favicon_url.has_value());
  EXPECT_TRUE(tab->is_default_favicon);
  EXPECT_TRUE(tab->show_icon);
  EXPECT_GT(tab->last_active_time_ticks, base::TimeTicks());
}

void ExpectProfileTabs(tab_search::mojom::ProfileTabs* profile_tabs) {
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
                           TabSearchPageHandler::Delegate* delegate)
      : TabSearchPageHandler(
            mojo::PendingReceiver<tab_search::mojom::PageHandler>(),
            std::move(page),
            web_ui,
            delegate) {
    mock_debounce_timer_ = new base::MockRetainingOneShotTimer();
    SetTimerForTesting(base::WrapUnique(mock_debounce_timer_));
  }
  base::MockRetainingOneShotTimer* mock_debounce_timer() {
    return mock_debounce_timer_;
  }

 private:
  base::MockRetainingOneShotTimer* mock_debounce_timer_;
};

class MockTabSearchPageHandlerDelegate : public TabSearchPageHandler::Delegate {
 public:
  MockTabSearchPageHandlerDelegate() = default;
  virtual ~MockTabSearchPageHandlerDelegate() = default;

  MOCK_METHOD(void, ShowUI, (), (override));
  MOCK_METHOD(void, CloseUI, (), (override));
};

class TabSearchPageHandlerTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    profile2_ = profile_manager()->CreateTestingProfile(
        "testing_profile2", nullptr, base::string16(), 0, std::string(),
        GetTestingFactories());
    browser2_ = CreateTestBrowser(profile1(), false);
    browser3_ =
        CreateTestBrowser(browser()->profile()->GetPrimaryOTRProfile(), false);
    browser4_ = CreateTestBrowser(profile2(), false);
    BrowserList::SetLastActive(browser1());
    handler_delegate_ = std::make_unique<MockTabSearchPageHandlerDelegate>();
    handler_ = std::make_unique<TestTabSearchPageHandler>(
        page_.BindAndGetRemote(), web_ui(), handler_delegate_.get());
  }

  void TearDown() override {
    browser1()->tab_strip_model()->CloseAllTabs();
    browser2()->tab_strip_model()->CloseAllTabs();
    browser3()->tab_strip_model()->CloseAllTabs();
    browser4()->tab_strip_model()->CloseAllTabs();
    browser2_.reset();
    browser3_.reset();
    browser4_.reset();
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

  TestTabSearchPageHandler* handler() { return handler_.get(); }
  MockTabSearchPageHandlerDelegate* handler_delegate() {
    return handler_delegate_.get();
  }
  void FireTimer() { handler_->mock_debounce_timer()->Fire(); }
  bool IsTimerRunning() { return handler_->mock_debounce_timer()->IsRunning(); }

 protected:
  void AddTabWithTitle(Browser* browser,
                       const GURL url,
                       const std::string title) {
    AddTab(browser, url);
    NavigateAndCommitActiveTabWithTitle(browser, url,
                                        base::ASCIIToUTF16(title));
  }

  testing::StrictMock<MockPage> page_;

 private:
  std::unique_ptr<Browser> CreateTestBrowser(Profile* profile, bool popup) {
    auto window = std::make_unique<TestBrowserWindow>();
    Browser::Type type = popup ? Browser::TYPE_POPUP : Browser::TYPE_NORMAL;

    std::unique_ptr<Browser> browser =
        CreateBrowser(profile, type, false, window.get());
    BrowserList::SetLastActive(browser.get());
    new TestBrowserWindowOwner(window.release());
    return browser;
  }

  content::TestWebUI web_ui_;
  Profile* profile2_;
  std::unique_ptr<Browser> browser2_;
  std::unique_ptr<Browser> browser3_;
  std::unique_ptr<Browser> browser4_;
  std::unique_ptr<TestTabSearchPageHandler> handler_;
  std::unique_ptr<MockTabSearchPageHandlerDelegate> handler_delegate_;
};

TEST_F(TabSearchPageHandlerTest, GetTabs) {
  // Browser3 and browser4 are using different profiles, thus their tabs should
  // not be accessible.
  AddTabWithTitle(browser4(), GURL(kTabUrl5), kTabName5);
  AddTabWithTitle(browser3(), GURL(kTabUrl4), kTabName4);
  AddTabWithTitle(browser2(), GURL(kTabUrl3), kTabName3);
  AddTabWithTitle(browser1(), GURL(kTabUrl2), kTabName2);
  AddTabWithTitle(browser1(), GURL(kTabUrl1), kTabName1);

  EXPECT_CALL(page_, TabsChanged()).Times(1);
  EXPECT_CALL(page_, TabUpdated(_)).Times(2);
  handler()->mock_debounce_timer()->Fire();

  int32_t tab_id2 = 0;
  int32_t tab_id3 = 0;

  // Get Tabs.
  tab_search::mojom::PageHandler::GetProfileTabsCallback callback1 =
      base::BindLambdaForTesting(
          [&](tab_search::mojom::ProfileTabsPtr profile_tabs) {
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
  handler()->GetProfileTabs(std::move(callback1));

  // Switch to 2nd tab.
  auto switch_to_tab_info = tab_search::mojom::SwitchToTabInfo::New();
  switch_to_tab_info->tab_id = tab_id2;
  handler()->SwitchToTab(std::move(switch_to_tab_info));

  // Get Tabs again to verify tab switch.
  tab_search::mojom::PageHandler::GetProfileTabsCallback callback2 =
      base::BindLambdaForTesting(
          [&](tab_search::mojom::ProfileTabsPtr profile_tabs) {
            ExpectProfileTabs(profile_tabs.get());
          });
  handler()->GetProfileTabs(std::move(callback2));

  // Switch to 3rd tab.
  switch_to_tab_info = tab_search::mojom::SwitchToTabInfo::New();
  switch_to_tab_info->tab_id = tab_id3;
  handler()->SwitchToTab(std::move(switch_to_tab_info));

  // Get Tabs again to verify tab switch.
  tab_search::mojom::PageHandler::GetProfileTabsCallback callback3 =
      base::BindLambdaForTesting(
          [&](tab_search::mojom::ProfileTabsPtr profile_tabs) {
            ExpectProfileTabs(profile_tabs.get());
          });
  handler()->GetProfileTabs(std::move(callback3));
}

// Ensure that repeated tab model changes do not result in repeated calls to
// TabsChanged() and TabsChanged() is only called when the page handler's
// timer fires.
TEST_F(TabSearchPageHandlerTest, TabsChanged) {
  EXPECT_CALL(page_, TabsChanged()).Times(4);
  EXPECT_CALL(page_, TabUpdated(_)).Times(1);
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
      0, TabStripModel::CLOSE_CREATE_HISTORICAL_TAB);
  ASSERT_TRUE(IsTimerRunning());
  FireTimer();  // Will call TabsChanged().
}

// Ensure that tab model changes in a browser with a different profile
// will not call TabsChanged().
TEST_F(TabSearchPageHandlerTest, TabsNotChanged) {
  EXPECT_CALL(page_, TabsChanged()).Times(1);
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

bool VerifyTabUpdated(const tab_search::mojom::TabPtr& tab) {
  ExpectNewTab(tab.get(), kTabUrl1, kTabName1, 1);
  return true;
}

// Verify tab update event is called correctly with data
TEST_F(TabSearchPageHandlerTest, TabUpdated) {
  EXPECT_CALL(page_, TabsChanged()).Times(1);
  EXPECT_CALL(page_, TabUpdated(Truly(VerifyTabUpdated))).Times(1);
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
  EXPECT_CALL(page_, TabsChanged()).Times(1);
  EXPECT_CALL(page_, TabUpdated(_)).Times(1);
  handler()->CloseTab(tab_id);
  ASSERT_EQ(1, browser1()->tab_strip_model()->count());
  ASSERT_EQ(1, browser2()->tab_strip_model()->count());
}

// TODO(crbug.com/1128855): Fix the test for Lacros build.
#if BUILDFLAG(IS_LACROS)
#define MAYBE_ShowFeedbackPage DISABLED_ShowFeedbackPage
#else
#define MAYBE_ShowFeedbackPage ShowFeedbackPage
#endif
TEST_F(TabSearchPageHandlerTest, MAYBE_ShowFeedbackPage) {
  base::HistogramTester histogram_tester;
  handler()->ShowFeedbackPage();
  histogram_tester.ExpectTotalCount("Feedback.RequestSource", 1);
}

// Make sure the delegate receives the ShowUI() call.
TEST_F(TabSearchPageHandlerTest, ShowUITest) {
  EXPECT_CALL(*handler_delegate(), ShowUI()).Times(1);
  handler()->ShowUI();
}

// Make sure the delegate receives the closeUI() call.
TEST_F(TabSearchPageHandlerTest, CloseUITest) {
  EXPECT_CALL(*handler_delegate(), CloseUI()).Times(1);
  handler()->CloseUI();
}

}  // namespace
