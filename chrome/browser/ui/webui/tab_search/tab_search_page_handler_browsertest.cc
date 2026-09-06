// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tab_search/tab_search_page_handler.h"

#include <stdint.h>

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/timer/mock_timer.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/sessions/chrome_tab_restore_service_client.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_ui_controller/browser_ui_controller.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/recently_audible_helper.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_sync_service_initialized_observer.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/metrics_reporter/metrics_reporter.h"
#include "chrome/browser/ui/webui/metrics_reporter/mock_metrics_reporter.h"
#include "chrome/browser/ui/webui/tab_search/tab_search.mojom-forward.h"
#include "chrome/browser/ui/webui/tab_search/tab_search_prefs.h"
#include "chrome/browser/ui/webui/tab_search/tab_search_ui.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/browser/vr/vr_tab_helper.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/core/tab_restore_service_impl.h"
#include "components/split_tabs/split_tab_id.h"
#include "components/split_tabs/split_tab_visual_data.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/split_tab_data.h"
#include "components/tabs/public/tab_alert.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/base_window.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"
#include "ui/base/unowned_user_data/user_data_factory.h"
#include "ui/gfx/color_utils.h"

using testing::_;
using testing::Truly;

namespace {

constexpr char kTabName1[] = "Tab 1";
constexpr char kTabName2[] = "Tab 2";
constexpr char kTabName3[] = "Tab 3";
constexpr char kTabName4[] = "Tab 4";
constexpr char kTabName5[] = "Tab 5";
constexpr char kTabName6[] = "Tab 6";

class MockPage : public tab_search::mojom::Page {
 public:
  MockPage() = default;
  ~MockPage() override = default;

  mojo::PendingRemote<tab_search::mojom::Page> BindAndGetRemote() {
    DCHECK(!receiver_.is_bound());
    return receiver_.BindNewPipeAndPassRemote();
  }
  mojo::Receiver<tab_search::mojom::Page> receiver_{this};

  MOCK_METHOD(void, HostWindowChanged, ());
  MOCK_METHOD(void, TabsChanged, (tab_search::mojom::ProfileDataPtr));
  MOCK_METHOD(void, TabUpdated, (tab_search::mojom::TabUpdateInfoPtr));
  MOCK_METHOD(void, TabsRemoved, (tab_search::mojom::TabsRemovedInfoPtr));
  MOCK_METHOD(void, TabUnsplit, ());
};

void ExpectNewTab(const tab_search::mojom::Tab* tab,
                  const std::string url,
                  const std::string title) {
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

[[nodiscard]] bool WaitForActiveTab(BrowserWindowInterface* browser,
                                    const GURL& url) {
  return base::test::RunUntil([&]() {
    return browser->GetTabStripModel()->GetActiveWebContents() &&
           browser->GetTabStripModel()
                   ->GetActiveWebContents()
                   ->GetLastCommittedURL() == url;
  });
}

void ExpectProfileTabs(tab_search::mojom::ProfileData* profile_tabs) {
  ASSERT_EQ(2u, profile_tabs->windows.size());
  const tab_search::mojom::Window* host_window = nullptr;
  const tab_search::mojom::Window* other_window = nullptr;
  for (const auto& window : profile_tabs->windows) {
    if (window->is_host_window) {
      host_window = window.get();
    } else {
      other_window = window.get();
    }
  }
  ASSERT_TRUE(host_window);
  ASSERT_TRUE(other_window);

  ASSERT_EQ(3u, host_window->tabs.size());
  EXPECT_FALSE(host_window->tabs[0]->active);
  EXPECT_TRUE(host_window->tabs[1]->active);
  EXPECT_FALSE(host_window->tabs[2]->active);

  ASSERT_EQ(1u, other_window->tabs.size());
  EXPECT_TRUE(other_window->tabs[0]->active);
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
    auto timer = std::make_unique<base::MockRetainingOneShotTimer>();
    mock_debounce_timer_ = timer.get();
    SetTimerForTesting(std::move(timer));
  }

  base::MockRetainingOneShotTimer* mock_debounce_timer() {
    return mock_debounce_timer_;
  }

 private:
  raw_ptr<base::MockRetainingOneShotTimer> mock_debounce_timer_ = nullptr;
  testing::NiceMock<MockMetricsReporter> metrics_reporter_;
};

class TabSearchPageHandlerTest : public InProcessBrowserTest {
 public:
  TabSearchPageHandlerTest() {
    webui_omnibox_feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/
        // TODO(crbug.com/452061489): Fix tests that fail when the WebUI Omnibox
        // is enabled and then remove these two Features.
        {omnibox::internal::kWebUIOmniboxPopup,
         omnibox::internal::kWebUIOmniboxAimPopup});
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    ASSERT_TRUE(embedded_test_server()->Start());

    tab_url1_ = embedded_test_server()->GetURL("/title1.html?1");
    tab_url2_ = embedded_test_server()->GetURL("/title1.html?2");
    tab_url3_ = embedded_test_server()->GetURL("/title1.html?3");
    tab_url4_ = embedded_test_server()->GetURL("/title1.html?4");
    tab_url5_ = embedded_test_server()->GetURL("/title1.html?5");
    tab_url6_ = embedded_test_server()->GetURL("/title1.html?6");

#if !BUILDFLAG(IS_CHROMEOS)
    base::FilePath path =
        g_browser_process->profile_manager()->user_data_dir().AppendASCII(
            "testing_profile2");
    profile2_ = &profiles::testing::CreateProfileSync(
        g_browser_process->profile_manager(), path);
#endif

    browser2_ =
        CreateBrowserForTest(profile1(), BrowserWindowInterface::TYPE_NORMAL);
    browser3_ =
        CreateBrowserForTest(browser()->GetProfile()->GetPrimaryOTRProfile(
                                 /*create_if_needed=*/true),
                             BrowserWindowInterface::TYPE_NORMAL);
#if !BUILDFLAG(IS_CHROMEOS)
    browser4_ =
        CreateBrowserForTest(profile2_, BrowserWindowInterface::TYPE_NORMAL);
#endif
    browser5_ =
        CreateBrowserForTest(profile1(), BrowserWindowInterface::TYPE_POPUP);

    browser1()->GetWindow()->Activate();
    BrowserUiController::From(browser1())
        ->set_update_ui_immediately_for_testing();

    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile1()));
    web_ui_.set_web_contents(web_contents_.get());
    webui::SetBrowserWindowInterface(web_contents_.get(), browser1());

    webui_controller_ = std::make_unique<TabSearchUI>(web_ui());

    handler_ = std::make_unique<TestTabSearchPageHandler>(
        page_.BindAndGetRemote(), web_ui(), webui_controller_.get());
    EXPECT_CALL(page_, HostWindowChanged()).Times(testing::AnyNumber());
    EXPECT_CALL(page_, TabUpdated(_)).Times(testing::AnyNumber());
    EXPECT_CALL(page_, TabsChanged(_)).Times(testing::AnyNumber());
    EXPECT_CALL(page_, TabsRemoved(_)).Times(testing::AnyNumber());
    EXPECT_CALL(page_, TabUnsplit()).Times(testing::AnyNumber());

    WaitForTabGroupSyncServiceInitialized();
  }

  void TearDownOnMainThread() override {
    handler_.reset();
    webui_controller_.reset();
    profile2_ = nullptr;

    if (browser5_) {
      BrowserWindowInterface* browser = browser5_;
      browser5_ = nullptr;
      CloseBrowserSynchronously(browser);
    }
    if (browser4_) {
      BrowserWindowInterface* browser = browser4_;
      browser4_ = nullptr;
      CloseBrowserSynchronously(browser);
    }
    if (browser3_) {
      BrowserWindowInterface* browser = browser3_;
      browser3_ = nullptr;
      CloseBrowserSynchronously(browser);
    }
    if (browser2_) {
      BrowserWindowInterface* browser = browser2_;
      browser2_ = nullptr;
      CloseBrowserSynchronously(browser);
    }

    web_contents_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void ClearSetupExpectations() {
    testing::Mock::VerifyAndClearExpectations(&page_);
  }

  content::TestWebUI* web_ui() { return &web_ui_; }
  Profile* profile1() { return browser()->GetProfile(); }
  Profile* profile2() { return profile2_; }

  // The default browser.
  BrowserWindowInterface* browser1() { return browser(); }

  BrowserWindowInterface* browser2() { return browser2_; }
  BrowserWindowInterface* browser3() { return browser3_; }
  BrowserWindowInterface* browser4() { return browser4_; }
  BrowserWindowInterface* browser5() { return browser5_; }

  TestTabSearchPageHandler* handler() { return handler_.get(); }
  void reset_handler() { handler_.reset(); }
  void FireTimer() { handler_->mock_debounce_timer()->Fire(); }
  bool IsTimerRunning() { return handler_->mock_debounce_timer()->IsRunning(); }

  void WaitForTabGroupSyncServiceInitialized() {
    tab_groups::TabGroupSyncService* tab_group_service_1 =
        tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile1());
    auto observer_1 =
        std::make_unique<tab_groups::TabGroupSyncServiceInitializedObserver>(
            tab_group_service_1);
    observer_1->Wait();

#if !BUILDFLAG(IS_CHROMEOS)
    tab_groups::TabGroupSyncService* tab_group_service_2 =
        tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile2());
    auto observer_2 =
        std::make_unique<tab_groups::TabGroupSyncServiceInitializedObserver>(
            tab_group_service_2);
    observer_2->Wait();
#endif
  }

 protected:
  BrowserWindowInterface* CreateBrowserForTest(
      Profile* profile,
      BrowserWindowInterface::Type type) {
    BrowserWindowCreateParams params(type, profile, /*from_user_gesture=*/true);
    BrowserWindowInterface* browser = CreateBrowserWindow(std::move(params));
    browser->GetWindow()->Show();
    BrowserUiController::From(browser)->set_update_ui_immediately_for_testing();
    return browser;
  }

  void AddTabWithTitle(BrowserWindowInterface* browser,
                       const GURL& url,
                       const std::string& title) {
    chrome::AddTabAt(browser, url, 0, true);
    content::WebContents* web_contents =
        browser->GetTabStripModel()->GetActiveWebContents();
    content::WaitForLoadStop(web_contents);
    content::TitleWatcher title_watcher(web_contents, base::UTF8ToUTF16(title));
    ASSERT_TRUE(content::ExecJs(
        web_contents,
        base::StringPrintf("document.title = '%s';", title.c_str())));
    ASSERT_EQ(base::UTF8ToUTF16(title), title_watcher.WaitAndGetTitle());
    BrowserUiController::From(browser)->ProcessPendingUIUpdates();
    page_.receiver_.FlushForTesting();
  }

  TabSearchUI* webui_controller() { return webui_controller_.get(); }

  void HideWebContents() {
    web_contents_->WasHidden();
    ASSERT_FALSE(handler_->IsWebContentsVisible());
  }

 protected:
  testing::StrictMock<MockPage> page_;

  std::unique_ptr<content::WebContents> web_contents_;
  GURL tab_url1_;
  GURL tab_url2_;
  GURL tab_url3_;
  GURL tab_url4_;
  GURL tab_url5_;
  GURL tab_url6_;
  raw_ptr<Profile> profile2_ = nullptr;
  raw_ptr<BrowserWindowInterface> browser2_ = nullptr;
  raw_ptr<BrowserWindowInterface> browser3_ = nullptr;
  raw_ptr<BrowserWindowInterface> browser4_ = nullptr;
  raw_ptr<BrowserWindowInterface> browser5_ = nullptr;

 private:
  content::TestWebUI web_ui_;
  base::test::ScopedFeatureList feature_list_;
  base::test::ScopedFeatureList webui_omnibox_feature_list_;
  std::unique_ptr<TestTabSearchPageHandler> handler_;
  std::unique_ptr<TabSearchUI> webui_controller_;
};

// TODO(crbug.com/537538766): Flaky on Linux and ChromeOS.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_GetTabs DISABLED_GetTabs
#else
#define MAYBE_GetTabs GetTabs
#endif
IN_PROC_BROWSER_TEST_F(TabSearchPageHandlerTest, MAYBE_GetTabs) {
  // Browser3 and browser4 are using different profiles, browser5 is not a
  // normal type browser, thus their tabs should not be accessible.
  AddTabWithTitle(browser5(), tab_url6_, kTabName6);
#if !BUILDFLAG(IS_CHROMEOS)
  AddTabWithTitle(browser4(), tab_url5_, kTabName5);
#endif
  AddTabWithTitle(browser3(), tab_url4_, kTabName4);
  AddTabWithTitle(browser2(), tab_url3_, kTabName3);
  AddTabWithTitle(browser1(), tab_url2_, kTabName2);
  AddTabWithTitle(browser1(), tab_url1_, kTabName1);

  ClearSetupExpectations();

  EXPECT_CALL(page_, TabsChanged(_)).Times(1);
  EXPECT_CALL(page_, TabUpdated(_)).Times(0);
  EXPECT_CALL(page_, TabsRemoved(_)).Times(0);
  handler()->mock_debounce_timer()->Fire();

  int32_t tab_id2 = 0;
  int32_t tab_id3 = 0;

  // Get Tabs.
  tab_search::mojom::PageHandler::GetProfileDataCallback callback1 =
      base::BindLambdaForTesting(
          [&](tab_search::mojom::ProfileDataPtr profile_tabs) {
            ASSERT_EQ(2u, profile_tabs->windows.size());
            const tab_search::mojom::Window* host_window = nullptr;
            const tab_search::mojom::Window* other_window = nullptr;
            for (const auto& window : profile_tabs->windows) {
              if (window->is_host_window) {
                host_window = window.get();
              } else {
                other_window = window.get();
              }
            }
            ASSERT_TRUE(host_window);
            ASSERT_TRUE(other_window);

            ASSERT_EQ(3u, host_window->tabs.size());
            auto* tab1 = host_window->tabs[0].get();
            ExpectNewTab(tab1, tab_url1_.spec(), kTabName1);
            ASSERT_TRUE(tab1->active);

            auto* tab2 = host_window->tabs[1].get();
            ExpectNewTab(tab2, tab_url2_.spec(), kTabName2);
            ASSERT_FALSE(tab2->active);

            ASSERT_EQ(1u, other_window->tabs.size());
            auto* tab3 = other_window->tabs[0].get();
            ExpectNewTab(tab3, tab_url3_.spec(), kTabName3);
            ASSERT_TRUE(tab3->active);

            tab_id2 = tab2->tab_id;
            tab_id3 = tab3->tab_id;
          });
  handler()->GetProfileData(std::move(callback1));

  // Switch to 2nd tab.
  auto switch_to_tab_info = tab_search::mojom::SwitchToTabInfo::New();
  switch_to_tab_info->tab_id = tab_id2;
  handler()->SwitchToTab(std::move(switch_to_tab_info));
  ASSERT_TRUE(WaitForActiveTab(browser1(), tab_url2_));

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
  ASSERT_TRUE(WaitForActiveTab(browser2(), tab_url3_));

  // Get Tabs again to verify tab switch.
  tab_search::mojom::PageHandler::GetProfileDataCallback callback3 =
      base::BindLambdaForTesting(
          [&](tab_search::mojom::ProfileDataPtr profile_tabs) {
            ExpectProfileTabs(profile_tabs.get());
          });
  handler()->GetProfileData(std::move(callback3));
}

IN_PROC_BROWSER_TEST_F(TabSearchPageHandlerTest,
                       TabActivationChangedByInteraction) {
  AddTabWithTitle(browser1(), tab_url1_, kTabName1);
  AddTabWithTitle(browser1(), tab_url2_, kTabName2);

  ClearSetupExpectations();

  EXPECT_CALL(page_, TabUpdated(_)).Times(0);
  EXPECT_CALL(page_, TabsRemoved(_)).Times(0);

  base::TimeTicks tab1_ticks;
  base::TimeTicks tab2_ticks;

  // Get initial last active time ticks.
  tab_search::mojom::PageHandler::GetProfileDataCallback callback1 =
      base::BindLambdaForTesting(
          [&](tab_search::mojom::ProfileDataPtr profile_tabs) {
            ASSERT_EQ(2u, profile_tabs->windows.size());
            const tab_search::mojom::Window* host_window = nullptr;
            for (const auto& window : profile_tabs->windows) {
              if (window->is_host_window) {
                host_window = window.get();
                break;
              }
            }
            ASSERT_TRUE(host_window);
            ASSERT_EQ(3u, host_window->tabs.size());
            // Tabs are in index order: ?2 (index 0), ?1 (index 1), about:blank
            // (index 2)
            tab1_ticks = host_window->tabs[0]->last_active_time_ticks;
            tab2_ticks = host_window->tabs[1]->last_active_time_ticks;
          });
  handler()->GetProfileData(std::move(callback1));

  // Simulate interaction with the first tab (which is at index 0: ?2).
  browser1()->tab_strip_model()->GetWebContentsAt(0)->Copy();

  // Get last active time ticks again and verify.
  tab_search::mojom::PageHandler::GetProfileDataCallback callback2 =
      base::BindLambdaForTesting(
          [&](tab_search::mojom::ProfileDataPtr profile_tabs) {
            ASSERT_EQ(2u, profile_tabs->windows.size());
            const tab_search::mojom::Window* host_window = nullptr;
            for (const auto& window : profile_tabs->windows) {
              if (window->is_host_window) {
                host_window = window.get();
                break;
              }
            }
            ASSERT_TRUE(host_window);
            ASSERT_EQ(3u, host_window->tabs.size());
            base::TimeTicks new_tab1_ticks =
                host_window->tabs[0]->last_active_time_ticks;
            base::TimeTicks new_tab2_ticks =
                host_window->tabs[1]->last_active_time_ticks;
            EXPECT_GT(new_tab1_ticks, tab1_ticks);
            EXPECT_EQ(new_tab2_ticks, tab2_ticks);
          });
  handler()->GetProfileData(std::move(callback2));
}

IN_PROC_BROWSER_TEST_F(TabSearchPageHandlerTest, TabsAndGroups) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

  // Add tabs to a browser.
  AddTabWithTitle(browser1(), tab_url1_, kTabName1);
  AddTabWithTitle(browser1(), tab_url2_, kTabName2);

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
            const tab_search::mojom::Window* host_window = nullptr;
            for (const auto& window : profile_tabs->windows) {
              if (window->is_host_window) {
                host_window = window.get();
                break;
              }
            }
            ASSERT_TRUE(host_window);
            ASSERT_EQ(3u, host_window->tabs.size());

            ASSERT_EQ(1u, profile_tabs->tab_groups.size());
            auto* tab_group = profile_tabs->tab_groups[0].get();
            ASSERT_EQ(sample_color, tab_group->color);
            ASSERT_EQ(base::UTF16ToUTF8(sample_title), tab_group->title);
          });
  handler()->GetProfileData(std::move(callback1));

  ClearSetupExpectations();

  EXPECT_CALL(page_, TabsRemoved(_)).Times(1);
  EXPECT_CALL(page_, TabUpdated(_)).Times(testing::AnyNumber());

  // Close a group's tab.
  const int tab_id =
      browser1()->tab_strip_model()->GetTabAtIndex(0)->GetHandle().raw_value();
  handler()->CloseTab(tab_id);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return browser1()->tab_strip_model()->count() == 2; }));

  // Assert the closed tab's data is correct in ProfileData.
  tab_search::mojom::PageHandler::GetProfileDataCallback callback2 =
      base::BindLambdaForTesting(
          [&](tab_search::mojom::ProfileDataPtr profile_tabs) {
            ASSERT_EQ(2u, profile_tabs->windows.size());
            const tab_search::mojom::Window* host_window = nullptr;
            for (const auto& window : profile_tabs->windows) {
              if (window->is_host_window) {
                host_window = window.get();
                break;
              }
            }
            ASSERT_TRUE(host_window);
            ASSERT_EQ(2u, host_window->tabs.size());

            auto& tab_groups = profile_tabs->tab_groups;
            ASSERT_EQ(1u, tab_groups.size());
            tab_search::mojom::TabGroup* tab_group = tab_groups[0].get();
            ASSERT_EQ(sample_color, tab_group->color);
            ASSERT_EQ(base::UTF16ToUTF8(sample_title), tab_group->title);

            auto& recently_closed_tabs = profile_tabs->recently_closed_tabs;
            ASSERT_EQ(1u, recently_closed_tabs.size());
            tab_search::mojom::RecentlyClosedTab* tab =
                recently_closed_tabs[0].get();
            ExpectRecentlyClosedTab(tab, tab_url2_.spec(), kTabName2);
            ASSERT_TRUE(tab->group_id);
            ASSERT_EQ(tab_group->id, tab->group_id);
          });
  handler()->GetProfileData(std::move(callback2));
}

IN_PROC_BROWSER_TEST_F(TabSearchPageHandlerTest, MediaTabsTest) {
  AddTabWithTitle(browser(), tab_url1_, kTabName1);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  LOG(INFO) << "MediaTabsTest: browser profile matches handler profile: "
            << (browser()->GetProfile() == profile1());
  LOG(INFO) << "MediaTabsTest: browser type: "
            << static_cast<int>(browser()->GetType());
  LOG(INFO) << "MediaTabsTest: has committed entry: "
            << (web_contents->GetController().GetLastCommittedEntry() !=
                nullptr);
  if (web_contents->GetController().GetLastCommittedEntry()) {
    LOG(INFO) << "MediaTabsTest: committed URL: "
              << web_contents->GetController()
                     .GetLastCommittedEntry()
                     ->GetURL()
                     .spec();
  }

  RecentlyAudibleHelper* audible_helper =
      RecentlyAudibleHelper::FromWebContents(web_contents);
  ASSERT_TRUE(audible_helper);
  audible_helper->SetCurrentlyAudibleForTesting();

  tab_search::mojom::PageHandler::GetProfileDataCallback callback =
      base::BindLambdaForTesting(
          [&](tab_search::mojom::ProfileDataPtr profile_tabs) {
            LOG(INFO) << "MediaTabsTest: Callback invoked";
            LOG(INFO) << "MediaTabsTest: Windows count: "
                      << profile_tabs->windows.size();
            tab_search::mojom::Window* host_window = nullptr;
            for (const auto& window : profile_tabs->windows) {
              if (window->is_host_window) {
                host_window = window.get();
                break;
              }
            }
            ASSERT_TRUE(host_window);
            LOG(INFO) << "MediaTabsTest: Host window tabs count: "
                      << host_window->tabs.size();
            for (size_t i = 0; i < host_window->tabs.size(); ++i) {
              LOG(INFO) << "MediaTabsTest: Tab " << i
                        << " URL: " << host_window->tabs[i]->url.spec();
            }
            ASSERT_FALSE(host_window->tabs.empty());
            auto* tab1 = host_window->tabs[0].get();
            ASSERT_FALSE(tab1->alert_states.empty());
            EXPECT_EQ(tabs::TabAlert::kAudioPlaying, tab1->alert_states[0]);
          });
  handler()->GetProfileData(std::move(callback));

  EXPECT_CALL(page_, TabsRemoved(_)).Times(0);
}

IN_PROC_BROWSER_TEST_F(TabSearchPageHandlerTest, RecentlyClosedTabGroup) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

  // Add tabs to a browser.
  AddTabWithTitle(browser1(), tab_url1_, kTabName1);
  AddTabWithTitle(browser1(), tab_url2_, kTabName2);

  TabStripModel* tab_strip_model = browser1()->tab_strip_model();

  // Associate a tab to a given tab group.
  tab_groups::TabGroupId group1 = tab_strip_model->AddToNewGroup({0});

  std::u16string sample_title = u"Sample title";
  const tab_groups::TabGroupColorId sample_color =
      tab_groups::TabGroupColorId::kGrey;
  tab_groups::TabGroupVisualData visual_data1(sample_title, sample_color);
  tab_strip_model->ChangeTabGroupVisuals(group1, visual_data1);

  ClearSetupExpectations();

  EXPECT_CALL(page_, TabUpdated(_)).Times(testing::AnyNumber());
  EXPECT_CALL(page_, TabsRemoved(_)).Times(1);

  sessions::TabRestoreService* tab_restore_service =
      TabRestoreServiceFactory::GetForProfile(profile1());

  // Close a group and its tabs.
  tab_strip_model->CloseAllTabsInGroup(group1);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return tab_restore_service->entries().size() == 1u; }));

  // Assert the closed tab group and tab data is correct in ProfileData.
  base::test::TestFuture<tab_search::mojom::ProfileDataPtr> future;
  handler()->GetProfileData(future.GetCallback());
  tab_search::mojom::ProfileDataPtr profile_tabs = future.Take();

  ASSERT_EQ(2u, profile_tabs->windows.size());
  const tab_search::mojom::Window* host_window = nullptr;
  for (const auto& window : profile_tabs->windows) {
    if (window->is_host_window) {
      host_window = window.get();
      break;
    }
  }
  ASSERT_TRUE(host_window);
  ASSERT_EQ(2u, host_window->tabs.size());

  ASSERT_EQ(1u, profile_tabs->tab_groups.size());

  auto& recently_closed_tab_groups = profile_tabs->recently_closed_tab_groups;
  ASSERT_EQ(1u, recently_closed_tab_groups.size());
  tab_search::mojom::RecentlyClosedTabGroup* tab_group =
      recently_closed_tab_groups[0].get();
  ASSERT_EQ(sample_color, tab_group->color);
  ASSERT_EQ(base::UTF16ToUTF8(sample_title), tab_group->title);

  auto& recently_closed_tabs = profile_tabs->recently_closed_tabs;
  ASSERT_EQ(1u, recently_closed_tabs.size());
  tab_search::mojom::RecentlyClosedTab* tab = recently_closed_tabs[0].get();
  ExpectRecentlyClosedTab(tab, tab_url2_.spec(), kTabName2);
  ASSERT_TRUE(tab->group_id);
  ASSERT_EQ(tab_group->id, tab->group_id);
}

IN_PROC_BROWSER_TEST_F(TabSearchPageHandlerTest,
                       RecentlyClosedWindowWithGroupTabs) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

  // Add tabs to browser windows.
  AddTabWithTitle(browser1(), tab_url1_, kTabName1);
  AddTabWithTitle(browser1(), tab_url2_, kTabName2);
  AddTabWithTitle(browser2(), tab_url3_, kTabName3);
  AddTabWithTitle(browser2(), tab_url4_, kTabName4);

  // Associate a tab to a given tab group.
  TabStripModel* tab_strip_model = browser1()->tab_strip_model();
  tab_groups::TabGroupId group1 = tab_strip_model->AddToNewGroup({0});

  std::u16string sample_title = u"Sample title";
  const tab_groups::TabGroupColorId sample_color =
      tab_groups::TabGroupColorId::kGrey;
  tab_groups::TabGroupVisualData visual_data1(sample_title, sample_color);
  tab_strip_model->ChangeTabGroupVisuals(group1, visual_data1);

  ClearSetupExpectations();

  EXPECT_CALL(page_, TabsRemoved(_)).Times(1);
  EXPECT_CALL(page_, TabUpdated(_)).Times(testing::AnyNumber());
  EXPECT_CALL(page_, HostWindowChanged()).Times(testing::AnyNumber());

  sessions::TabRestoreService* tab_restore_service =
      TabRestoreServiceFactory::GetForProfile(profile1());

  // Close the tabs associated with a browser.
  browser1()->tab_strip_model()->CloseAllTabs();
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return tab_restore_service->entries().size() >= 2u; }));

  // Assert that the tabs that were in groups in the closed window contain the
  // associated group data necessary to render properly.
  base::test::TestFuture<tab_search::mojom::ProfileDataPtr> future;
  handler()->GetProfileData(future.GetCallback());
  tab_search::mojom::ProfileDataPtr profile_tabs = future.Take();

  ASSERT_EQ(1u, profile_tabs->windows.size());
  auto* window2 = profile_tabs->windows[0].get();
  ASSERT_EQ(2u, window2->tabs.size());

  ASSERT_EQ(1u, profile_tabs->tab_groups.size());
  tab_search::mojom::TabGroup* tab_group = profile_tabs->tab_groups[0].get();
  ASSERT_EQ(sample_color, tab_group->color);
  ASSERT_EQ(base::UTF16ToUTF8(sample_title), tab_group->title);

  ASSERT_EQ(0u, profile_tabs->recently_closed_tab_groups.size());

  auto& recently_closed_tabs = profile_tabs->recently_closed_tabs;
  ASSERT_EQ(3u, recently_closed_tabs.size());
  const tab_search::mojom::RecentlyClosedTab* target_tab = nullptr;
  for (const auto& tab : recently_closed_tabs) {
    if (tab->url == tab_url2_) {
      target_tab = tab.get();
      break;
    }
  }
  ASSERT_TRUE(target_tab);
  ExpectRecentlyClosedTab(target_tab, tab_url2_.spec(), kTabName2);
  ASSERT_TRUE(target_tab->group_id);
  ASSERT_EQ(tab_group->id, target_tab->group_id);
}

// Ensure that repeated tab model changes do not result in repeated calls to
// TabsChanged() and TabsChanged() is only called when the page handler's
// timer fires.
IN_PROC_BROWSER_TEST_F(TabSearchPageHandlerTest, TabsChanged) {
  webui::SetBrowserWindowInterface(web_contents_.get(), nullptr);
  browser1()->tab_strip_model()->AppendWebContents(std::move(web_contents_),
                                                   true);
  ASSERT_TRUE(IsTimerRunning());

  EXPECT_CALL(page_, TabsChanged(_)).Times(3);
  EXPECT_CALL(page_, TabUpdated(_)).Times(testing::AnyNumber());
  EXPECT_CALL(page_, TabsRemoved(_)).Times(1);

  FireTimer();  // Call 1.
  ASSERT_FALSE(IsTimerRunning());

  // Add 2 tabs in browser1 in background.
  chrome::AddTabAt(browser1(), tab_url1_, 0, false);
  ASSERT_TRUE(IsTimerRunning());
  chrome::AddTabAt(browser1(), tab_url2_, 0, false);
  ASSERT_TRUE(IsTimerRunning());
  FireTimer();  // Call 2.
  ASSERT_FALSE(IsTimerRunning());

  // Add 1 tab in browser2.
  AddTabWithTitle(browser2(), tab_url3_, kTabName3);
  ASSERT_TRUE(IsTimerRunning());
  FireTimer();  // Call 3.
  ASSERT_FALSE(IsTimerRunning());

  // Close a tab in browser 1.
  browser1()->tab_strip_model()->CloseWebContentsAt(
      0, TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return browser1()->tab_strip_model()->count() == 3; }));
  EXPECT_FALSE(IsTimerRunning());
}

// Assert that no browser -> renderer messages are sent when the WebUI is not
// visible.
IN_PROC_BROWSER_TEST_F(TabSearchPageHandlerTest,
                       EventsDoNotPropagatedWhenWebUIIsHidden) {
  HideWebContents();
  EXPECT_CALL(page_, TabsChanged(_)).Times(0);
  EXPECT_CALL(page_, TabUpdated(_)).Times(0);
  EXPECT_CALL(page_, TabsRemoved(_)).Times(0);
  FireTimer();

  // Inserting tabs should not cause the debounce timer to start running.
  ASSERT_FALSE(IsTimerRunning());
  AddTabWithTitle(browser1(), tab_url1_, kTabName1);
  ASSERT_FALSE(IsTimerRunning());

  // Adding the following tab would usually trigger TabUpdated() for the first
  // tab since the tab index will change from 0 to 1
  AddTabWithTitle(browser1(), tab_url2_, kTabName2);

  // Closing a tab would usually result in a call to TabsRemoved().
  browser1()->tab_strip_model()->CloseWebContentsAt(
      0, TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
}

// Ensure that tab model changes in a browser with a different profile
// will not call TabsChanged().
// TODO(crbug.com/537468010): Flaky on linux-chromeos-rel. Fix and re-enable.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_TabsNotChanged DISABLED_TabsNotChanged
#else
#define MAYBE_TabsNotChanged TabsNotChanged
#endif
IN_PROC_BROWSER_TEST_F(TabSearchPageHandlerTest, MAYBE_TabsNotChanged) {
  EXPECT_CALL(page_, TabsChanged(_)).Times(1);
  EXPECT_CALL(page_, TabUpdated(_)).Times(0);
  FireTimer();  // Will call TabsChanged().
  ASSERT_FALSE(IsTimerRunning());
  AddTabWithTitle(browser3(), tab_url1_,
                  kTabName1);  // Will not kick off timer.
  ASSERT_FALSE(IsTimerRunning());
#if !BUILDFLAG(IS_CHROMEOS)
  AddTabWithTitle(browser4(), tab_url2_,
                  kTabName2);  // Will not kick off timer.
  ASSERT_FALSE(IsTimerRunning());
#endif
}

// Verify tab update event is called correctly with data
// TODO(https://crbug.com/537538766): Fails on Linux MSan Tests and looks
// flaky on Linux and ChromeOS, generally.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_TabUpdated DISABLED_TabUpdated
#else
#define MAYBE_TabUpdated TabUpdated
#endif
IN_PROC_BROWSER_TEST_F(TabSearchPageHandlerTest, MAYBE_TabUpdated) {
  AddTabWithTitle(browser1(), tab_url1_, kTabName1);

  ClearSetupExpectations();

  const std::string updated_title = "Updated Tab 1";
  EXPECT_CALL(page_, TabsChanged(_)).Times(1);
  EXPECT_CALL(page_, TabUpdated(_)).Times(testing::AnyNumber());
  EXPECT_CALL(
      page_,
      TabUpdated(Truly(
          [this, updated_title](
              const tab_search::mojom::TabUpdateInfoPtr& tab_update_info) {
            const tab_search::mojom::TabPtr& tab = tab_update_info->tab;
            if (tab->url == this->tab_url1_.spec()) {
              ExpectNewTab(tab.get(), this->tab_url1_.spec(), updated_title);
              return true;
            }
            return false;
          })))
      .Times(1);
  EXPECT_CALL(page_, TabsRemoved(_)).Times(0);

  content::WebContents* web_contents =
      browser1()->GetTabStripModel()->GetActiveWebContents();
  content::TitleWatcher title_watcher(web_contents,
                                      base::UTF8ToUTF16(updated_title));
  ASSERT_TRUE(content::ExecJs(
      web_contents,
      base::StringPrintf("document.title = '%s';", updated_title.c_str())));
  ASSERT_EQ(base::UTF8ToUTF16(updated_title), title_watcher.WaitAndGetTitle());
  BrowserUiController::From(browser1())->ProcessPendingUIUpdates();
  page_.receiver_.FlushForTesting();

  AddTabWithTitle(browser1(), tab_url2_, kTabName2);
  FireTimer();
}

IN_PROC_BROWSER_TEST_F(TabSearchPageHandlerTest, CloseTab) {
  AddTabWithTitle(browser1(), tab_url1_, kTabName1);
  AddTabWithTitle(browser2(), tab_url2_, kTabName2);
  AddTabWithTitle(browser2(), tab_url2_, kTabName2);
  ASSERT_EQ(2, browser1()->tab_strip_model()->count());
  ASSERT_EQ(2, browser2()->tab_strip_model()->count());

  ClearSetupExpectations();

  const int tab_id =
      browser2()->tab_strip_model()->GetTabAtIndex(0)->GetHandle().raw_value();
  EXPECT_CALL(page_, TabUpdated(_)).Times(testing::AnyNumber());
  EXPECT_CALL(page_, TabsRemoved(_)).Times(1);
  handler()->CloseTab(tab_id);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return browser2()->tab_strip_model()->count() == 1; }));
  ASSERT_EQ(2, browser1()->tab_strip_model()->count());
  ASSERT_EQ(1, browser2()->tab_strip_model()->count());
}

IN_PROC_BROWSER_TEST_F(TabSearchPageHandlerTest, RecentlyClosedTab) {
  AddTabWithTitle(browser1(), tab_url1_, kTabName1);
  AddTabWithTitle(browser1(), tab_url2_, kTabName2);
  AddTabWithTitle(browser2(), tab_url3_, kTabName3);
  AddTabWithTitle(browser2(), tab_url4_, kTabName4);
  AddTabWithTitle(browser3(), tab_url5_, kTabName5);

  ClearSetupExpectations();

  EXPECT_CALL(page_, TabUpdated(_)).Times(testing::AnyNumber());
  EXPECT_CALL(page_, TabsRemoved(_)).Times(2);

  sessions::TabRestoreService* tab_restore_service =
      TabRestoreServiceFactory::GetForProfile(profile1());

  const int tab_id =
      browser1()->tab_strip_model()->GetTabAtIndex(0)->GetHandle().raw_value();
  handler()->CloseTab(tab_id);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return tab_restore_service->entries().size() >= 1u; }));

  browser2()->tab_strip_model()->CloseAllTabs();
  browser2_ = nullptr;
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return tab_restore_service->entries().size() >= 3u; }));

  // Browser 3 is incognito so does not show up in TabsRemoved.
  BrowserWindowInterface* browser3_interface = browser3_;
  browser3()->tab_strip_model()->CloseAllTabs();
  browser3_ = nullptr;
  ASSERT_TRUE(base::test::RunUntil([&]() {
    const auto& browsers = GetAllBrowserWindowInterfaces();
    return std::find(browsers.begin(), browsers.end(), browser3_interface) ==
           browsers.end();
  }));

  base::test::TestFuture<tab_search::mojom::ProfileDataPtr> future;
  handler()->GetProfileData(future.GetCallback());
  tab_search::mojom::ProfileDataPtr profile_tabs = future.Take();

  auto& tabs = profile_tabs->recently_closed_tabs;
  ASSERT_EQ(3u, tabs.size());
  ExpectRecentlyClosedTab(tabs[0].get(), tab_url4_.spec(), kTabName4);
  ExpectRecentlyClosedTab(tabs[1].get(), tab_url3_.spec(), kTabName3);
  ExpectRecentlyClosedTab(tabs[2].get(), tab_url2_.spec(), kTabName2);
}

IN_PROC_BROWSER_TEST_F(TabSearchPageHandlerTest, OpenRecentlyClosedTab) {
  const GURL tab_url1("data:text/html,<title>Tab 1</title>");
  const GURL tab_url2("data:text/html,<title>Tab 2</title>");
  AddTabWithTitle(browser1(), tab_url1, kTabName1);
  AddTabWithTitle(browser1(), tab_url2, kTabName2);

  ClearSetupExpectations();

  EXPECT_CALL(page_, TabsRemoved(_)).Times(1);
  EXPECT_CALL(page_, TabUpdated(_)).Times(testing::AnyNumber());

  sessions::TabRestoreService* tab_restore_service =
      TabRestoreServiceFactory::GetForProfile(profile1());

  int tab_id =
      browser1()->tab_strip_model()->GetTabAtIndex(0)->GetHandle().raw_value();
  handler()->CloseTab(tab_id);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return tab_restore_service->entries().size() >= 1u; }));

  base::test::TestFuture<tab_search::mojom::ProfileDataPtr> future1;
  handler()->GetProfileData(future1.GetCallback());
  tab_search::mojom::ProfileDataPtr profile_tabs1 = future1.Take();

  const tab_search::mojom::Window* host_window1 = nullptr;
  for (const auto& window : profile_tabs1->windows) {
    if (window->is_host_window) {
      host_window1 = window.get();
      break;
    }
  }
  ASSERT_TRUE(host_window1);
  ASSERT_EQ(2u, host_window1->tabs.size());
  const tab_search::mojom::Tab* target_tab = nullptr;
  for (const auto& tab : host_window1->tabs) {
    if (tab->url == tab_url1) {
      target_tab = tab.get();
      break;
    }
  }
  ASSERT_TRUE(target_tab);
  ExpectNewTab(target_tab, tab_url1.spec(), kTabName1);

  auto& recently_closed_tabs1 = profile_tabs1->recently_closed_tabs;
  ASSERT_EQ(1u, recently_closed_tabs1.size());
  ExpectRecentlyClosedTab(recently_closed_tabs1[0].get(), tab_url2.spec(),
                          kTabName2);
  int32_t restore_id = recently_closed_tabs1[0]->tab_id;

  EXPECT_CALL(page_, TabsRemoved(_)).Times(0);
  EXPECT_CALL(page_, TabUpdated(_)).Times(testing::AnyNumber());

  std::vector<content::WebContents*> typical_contents;
  for (int i = 0; i < browser1()->tab_strip_model()->count(); ++i) {
    typical_contents.push_back(
        browser1()->tab_strip_model()->GetWebContentsAt(i));
  }

  handler()->OpenRecentlyClosedEntry(restore_id);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return browser1()->tab_strip_model()->count() == 3; }));

  content::WebContents* restored_contents = nullptr;
  for (int i = 0; i < browser1()->tab_strip_model()->count(); ++i) {
    content::WebContents* wc =
        browser1()->tab_strip_model()->GetWebContentsAt(i);
    if (std::find(typical_contents.begin(), typical_contents.end(), wc) ==
        typical_contents.end()) {
      restored_contents = wc;
      break;
    }
  }
  ASSERT_TRUE(restored_contents);
  content::WaitForLoadStop(restored_contents);
  content::TitleWatcher title_watcher(restored_contents, u"Tab 2");
  ASSERT_EQ(u"Tab 2", title_watcher.WaitAndGetTitle());

  base::test::TestFuture<tab_search::mojom::ProfileDataPtr> future2;
  handler()->GetProfileData(future2.GetCallback());
  tab_search::mojom::ProfileDataPtr profile_tabs2 = future2.Take();

  const tab_search::mojom::Window* host_window2 = nullptr;
  for (const auto& window : profile_tabs2->windows) {
    if (window->is_host_window) {
      host_window2 = window.get();
      break;
    }
  }
  ASSERT_TRUE(host_window2);
  ASSERT_EQ(3u, host_window2->tabs.size());
  const tab_search::mojom::Tab* tab1 = nullptr;
  const tab_search::mojom::Tab* tab2 = nullptr;
  for (const auto& tab : host_window2->tabs) {
    if (tab->url == tab_url1) {
      tab1 = tab.get();
    } else if (tab->url == tab_url2) {
      tab2 = tab.get();
    }
  }
  ASSERT_TRUE(tab1);
  ASSERT_TRUE(tab2);
  ExpectNewTab(tab1, tab_url1.spec(), kTabName1);
  ExpectNewTab(tab2, tab_url2.spec(), kTabName2);

  auto& recently_closed_tabs2 = profile_tabs2->recently_closed_tabs;
  ASSERT_EQ(0u, recently_closed_tabs2.size());
}

IN_PROC_BROWSER_TEST_F(TabSearchPageHandlerTest,
                       RecentlyClosedTabsHaveNoRepeatedURLEntry) {
  AddTabWithTitle(browser1(), tab_url1_, kTabName1);
  AddTabWithTitle(browser1(), tab_url1_, kTabName1);

  ClearSetupExpectations();

  EXPECT_CALL(page_, TabsRemoved(_)).Times(1);
  EXPECT_CALL(page_, TabUpdated(_)).Times(testing::AnyNumber());
  EXPECT_CALL(page_, HostWindowChanged()).Times(testing::AnyNumber());

  sessions::TabRestoreService* tab_restore_service =
      TabRestoreServiceFactory::GetForProfile(profile1());

  browser1()->tab_strip_model()->CloseAllTabs();
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return tab_restore_service->entries().size() >= 2u; }));

  base::test::TestFuture<tab_search::mojom::ProfileDataPtr> future;
  handler()->GetProfileData(future.GetCallback());
  tab_search::mojom::ProfileDataPtr profile_tabs = future.Take();

  auto& recently_closed_tabs = profile_tabs->recently_closed_tabs;
  ASSERT_EQ(2u, recently_closed_tabs.size());
  const tab_search::mojom::RecentlyClosedTab* target_tab = nullptr;
  for (const auto& tab : recently_closed_tabs) {
    if (tab->url == tab_url1_) {
      target_tab = tab.get();
      break;
    }
  }
  ASSERT_TRUE(target_tab);
  ExpectRecentlyClosedTab(target_tab, tab_url1_.spec(), kTabName1);
}

IN_PROC_BROWSER_TEST_F(TabSearchPageHandlerTest,
                       RecentlyClosedTabGroupsHaveNoRepeatedURLEntries) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

  // Add tabs to a browser.
  AddTabWithTitle(browser1(), tab_url1_, kTabName1);
  AddTabWithTitle(browser1(), tab_url1_, kTabName1);
  AddTabWithTitle(browser2(), tab_url1_, kTabName1);
  AddTabWithTitle(browser2(), tab_url1_, kTabName1);

  // Associate tabs to a given tab group.
  TabStripModel* tab_strip_model = browser1()->tab_strip_model();
  tab_groups::TabGroupId group1 = tab_strip_model->AddToNewGroup({0, 1});

  std::u16string sample_title = u"Sample title";
  const tab_groups::TabGroupColorId sample_color =
      tab_groups::TabGroupColorId::kGrey;
  tab_groups::TabGroupVisualData visual_data1(sample_title, sample_color);
  tab_strip_model->ChangeTabGroupVisuals(group1, visual_data1);

  ClearSetupExpectations();

  EXPECT_CALL(page_, TabsRemoved(_)).Times(2);
  EXPECT_CALL(page_, TabUpdated(_)).Times(testing::AnyNumber());
  EXPECT_CALL(page_, HostWindowChanged()).Times(testing::AnyNumber());

  sessions::TabRestoreService* tab_restore_service =
      TabRestoreServiceFactory::GetForProfile(profile1());

  browser1()->tab_strip_model()->CloseAllTabs();
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return tab_restore_service->entries().size() >= 2u; }));

  browser2()->tab_strip_model()->CloseAllTabs();
  browser2_ = nullptr;
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return tab_restore_service->entries().size() >= 4u; }));

  base::test::TestFuture<tab_search::mojom::ProfileDataPtr> future;
  handler()->GetProfileData(future.GetCallback());
  tab_search::mojom::ProfileDataPtr profile_tabs = future.Take();

  auto& recently_closed_tabs = profile_tabs->recently_closed_tabs;
  ASSERT_EQ(3u, recently_closed_tabs.size());
  int found_grouped = 0;
  int found_ungrouped = 0;
  for (const auto& tab : recently_closed_tabs) {
    if (tab->url == tab_url1_) {
      if (tab->group_id.has_value()) {
        found_grouped++;
        ExpectRecentlyClosedTab(tab.get(), tab_url1_.spec(), kTabName1);
      } else {
        found_ungrouped++;
        ExpectRecentlyClosedTab(tab.get(), tab_url1_.spec(), kTabName1);
      }
    }
  }
  EXPECT_EQ(1, found_grouped);
  EXPECT_EQ(1, found_ungrouped);
}

IN_PROC_BROWSER_TEST_F(TabSearchPageHandlerTest,
                       RecentlyClosedTabEntriesFilterOpenTabUrls) {
  AddTabWithTitle(browser1(), tab_url1_, kTabName1);
  AddTabWithTitle(browser1(), tab_url1_, kTabName1);

  ClearSetupExpectations();

  EXPECT_CALL(page_, TabsRemoved(_)).Times(1);
  EXPECT_CALL(page_, TabUpdated(_)).Times(0);

  const int tab_id =
      browser1()->tab_strip_model()->GetTabAtIndex(0)->GetHandle().raw_value();
  handler()->CloseTab(tab_id);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return browser1()->tab_strip_model()->count() == 2; }));

  tab_search::mojom::PageHandler::GetProfileDataCallback callback1 =
      base::BindLambdaForTesting(
          [&](tab_search::mojom::ProfileDataPtr profile_tabs) {
            const tab_search::mojom::Window* host_window = nullptr;
            for (const auto& window : profile_tabs->windows) {
              if (window->is_host_window) {
                host_window = window.get();
                break;
              }
            }
            ASSERT_TRUE(host_window);
            ASSERT_EQ(2u, host_window->tabs.size());
            const tab_search::mojom::Tab* target_tab = nullptr;
            for (const auto& tab : host_window->tabs) {
              if (tab->url == tab_url1_) {
                target_tab = tab.get();
                break;
              }
            }
            ASSERT_TRUE(target_tab);
            ExpectNewTab(target_tab, tab_url1_.spec(), kTabName1);
            auto& recently_closed_tabs = profile_tabs->recently_closed_tabs;
            ASSERT_EQ(0u, recently_closed_tabs.size());
          });
  handler()->GetProfileData(std::move(callback1));
}

IN_PROC_BROWSER_TEST_F(TabSearchPageHandlerTest,
                       RecentlyClosedSectionExpandedUserPref) {
  AddTabWithTitle(browser1(), tab_url1_, kTabName1);
  AddTabWithTitle(browser1(), tab_url2_, kTabName2);

  ClearSetupExpectations();

  EXPECT_CALL(page_, TabsRemoved(_)).Times(1);
  EXPECT_CALL(page_, TabUpdated(_)).Times(testing::AnyNumber());

  const int tab_id =
      browser1()->tab_strip_model()->GetTabAtIndex(0)->GetHandle().raw_value();
  handler()->CloseTab(tab_id);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return browser1()->tab_strip_model()->count() == 2; }));

  tab_search::mojom::PageHandler::GetProfileDataCallback callback1 =
      base::BindLambdaForTesting(
          [&](tab_search::mojom::ProfileDataPtr profile_tabs) {
            const tab_search::mojom::Window* host_window = nullptr;
            for (const auto& window : profile_tabs->windows) {
              if (window->is_host_window) {
                host_window = window.get();
                break;
              }
            }
            ASSERT_TRUE(host_window);
            ASSERT_EQ(2u, host_window->tabs.size());
            const tab_search::mojom::Tab* target_tab = nullptr;
            for (const auto& tab : host_window->tabs) {
              if (tab->url == tab_url1_) {
                target_tab = tab.get();
                break;
              }
            }
            ASSERT_TRUE(target_tab);
            ExpectNewTab(target_tab, tab_url1_.spec(), kTabName1);
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

IN_PROC_BROWSER_TEST_F(TabSearchPageHandlerTest, RecentlyClosedTabInFuture) {
  AddTabWithTitle(browser1(), tab_url1_, kTabName1);
  AddTabWithTitle(browser1(), tab_url2_, kTabName2);

  ClearSetupExpectations();

  EXPECT_CALL(page_, TabsRemoved(_)).Times(1);
  EXPECT_CALL(page_, TabUpdated(_)).Times(0);

  int tab_id =
      browser1()->tab_strip_model()->GetTabAtIndex(0)->GetHandle().raw_value();
  handler()->CloseTab(tab_id);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return browser1()->tab_strip_model()->count() == 2; }));

  sessions::TabRestoreService* tab_restore_service =
      TabRestoreServiceFactory::GetForProfile(profile1());
  ASSERT_TRUE(tab_restore_service);
  ASSERT_FALSE(tab_restore_service->entries().empty());
  tab_restore_service->entries().front()->timestamp =
      base::Time::Now() + base::Hours(2);

  tab_search::mojom::PageHandler::GetProfileDataCallback callback =
      base::BindLambdaForTesting(
          [&](tab_search::mojom::ProfileDataPtr profile_tabs) {
            auto& recently_closed_tabs = profile_tabs->recently_closed_tabs;
            ASSERT_EQ(1u, recently_closed_tabs.size());
            ExpectRecentlyClosedTab(recently_closed_tabs[0].get(),
                                    tab_url2_.spec(), kTabName2);
            EXPECT_FALSE(
                recently_closed_tabs[0]->last_active_elapsed_text.empty());
          });
  handler()->GetProfileData(std::move(callback));
}

// TODO(crbug.com/537538766): Flaky on Linux and ChromeOS.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_ReplaceActiveSplitTab DISABLED_ReplaceActiveSplitTab
#else
#define MAYBE_ReplaceActiveSplitTab ReplaceActiveSplitTab
#endif
IN_PROC_BROWSER_TEST_F(TabSearchPageHandlerTest, MAYBE_ReplaceActiveSplitTab) {
  AddTabWithTitle(browser(), tab_url1_, kTabName1);
  AddTabWithTitle(browser(), tab_url2_, kTabName2);
  AddTabWithTitle(browser(), tab_url3_, kTabName3);
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  const split_tabs::SplitTabId split_id = tab_strip_model->AddToNewSplit(
      {1}, split_tabs::SplitTabVisualData(),
      split_tabs::SplitTabCreatedSource::kToolbarButton);

  const split_tabs::SplitTabData* split_data =
      tab_strip_model->GetSplitData(split_id);
  const std::vector<tabs::TabInterface*> tabs_in_split = split_data->ListTabs();
  EXPECT_EQ(tabs_in_split.size(), 2u);
  EXPECT_EQ(tab_url3_.spec(), tabs_in_split[0]->GetContents()->GetURL().spec());
  EXPECT_EQ(tab_url2_.spec(), tabs_in_split[1]->GetContents()->GetURL().spec());

  EXPECT_FALSE(tab_strip_model->GetTabAtIndex(2)->IsSplit());

  ClearSetupExpectations();

  EXPECT_CALL(page_, TabUpdated(_)).Times(0);
  EXPECT_CALL(page_, TabsRemoved(_)).Times(1);

  const int32_t replacement_tab_id =
      tab_strip_model->GetTabAtIndex(2)->GetHandle().raw_value();
  handler()->ReplaceActiveSplitTab(replacement_tab_id);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    const split_tabs::SplitTabData* split_data =
        tab_strip_model->GetSplitData(split_id);
    if (!split_data) {
      return false;
    }
    const std::vector<tabs::TabInterface*> tabs_in_split =
        split_data->ListTabs();
    return tabs_in_split.size() == 2u &&
           tabs_in_split[0]->GetContents()->GetURL() == tab_url1_;
  }));

  EXPECT_EQ(3, tab_strip_model->count());

  const split_tabs::SplitTabData* split_data_after_replacement =
      tab_strip_model->GetSplitData(split_id);
  const std::vector<tabs::TabInterface*> tabs_in_split_after_replacement =
      split_data_after_replacement->ListTabs();
  EXPECT_EQ(tabs_in_split_after_replacement.size(), 2u);
  EXPECT_EQ(tab_url1_.spec(),
            tabs_in_split_after_replacement[0]->GetContents()->GetURL().spec());
  EXPECT_EQ(tab_url2_.spec(),
            tabs_in_split_after_replacement[1]->GetContents()->GetURL().spec());
}

IN_PROC_BROWSER_TEST_F(TabSearchPageHandlerTest, TabSearchUsedPref) {
  AddTabWithTitle(browser1(), tab_url1_, kTabName1);
  AddTabWithTitle(browser1(), tab_url2_, kTabName2);

  PrefService* prefs = profile1()->GetPrefs();
  EXPECT_FALSE(prefs->GetBoolean(tab_search_prefs::kTabSearchUsed));

  ClearSetupExpectations();

  // 1. SwitchToTab (switch from index 0 (active) to index 1 (inactive))
  EXPECT_CALL(page_, TabUpdated(_)).Times(0);
  EXPECT_CALL(page_, TabsRemoved(_)).Times(0);

  const int32_t tab_id1 =
      browser1()->tab_strip_model()->GetTabAtIndex(1)->GetHandle().raw_value();
  auto switch_to_tab_info = tab_search::mojom::SwitchToTabInfo::New();
  switch_to_tab_info->tab_id = tab_id1;
  handler()->SwitchToTab(std::move(switch_to_tab_info));
  ASSERT_TRUE(WaitForActiveTab(browser1(), tab_url1_));

  EXPECT_TRUE(prefs->GetBoolean(tab_search_prefs::kTabSearchUsed));
  prefs->SetBoolean(tab_search_prefs::kTabSearchUsed, false);

  ClearSetupExpectations();

  // 2. CloseTab (close active tab)
  EXPECT_CALL(page_, TabsRemoved(_)).Times(1);
  EXPECT_CALL(page_, TabUpdated(_)).Times(0);

  handler()->CloseTab(tab_id1);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return browser1()->tab_strip_model()->count() == 2; }));

  EXPECT_TRUE(prefs->GetBoolean(tab_search_prefs::kTabSearchUsed));
  prefs->SetBoolean(tab_search_prefs::kTabSearchUsed, false);

  // 3. Setup for OpenRecentlyClosedEntry
  EXPECT_CALL(page_, TabUpdated(_)).Times(testing::AnyNumber());
  EXPECT_CALL(page_, TabsRemoved(_)).Times(testing::AnyNumber());

  AddTabWithTitle(browser1(), tab_url3_, kTabName3);
  const int32_t tab_id3 =
      browser1()->tab_strip_model()->GetTabAtIndex(0)->GetHandle().raw_value();
  handler()->CloseTab(tab_id3);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return browser1()->tab_strip_model()->count() == 2; }));

  sessions::TabRestoreService* tab_restore_service =
      TabRestoreServiceFactory::GetForProfile(profile1());
  ASSERT_TRUE(tab_restore_service);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return tab_restore_service->entries().size() >= 1u; }));

  EXPECT_TRUE(prefs->GetBoolean(tab_search_prefs::kTabSearchUsed));
  prefs->SetBoolean(tab_search_prefs::kTabSearchUsed, false);

  ClearSetupExpectations();

  // 4. OpenRecentlyClosedEntry
  EXPECT_CALL(page_, TabsRemoved(_)).Times(0);
  EXPECT_CALL(page_, TabUpdated(_)).Times(testing::AnyNumber());

  int32_t session_id = -1;
  tab_search::mojom::PageHandler::GetProfileDataCallback callback =
      base::BindLambdaForTesting(
          [&](tab_search::mojom::ProfileDataPtr profile_tabs) {
            auto& recently_closed_tabs = profile_tabs->recently_closed_tabs;
            ASSERT_GE(recently_closed_tabs.size(), 1u);
            for (const auto& tab : recently_closed_tabs) {
              if (tab->url == tab_url3_) {
                session_id = tab->tab_id;
                break;
              }
            }
          });
  handler()->GetProfileData(std::move(callback));
  ASSERT_TRUE(base::test::RunUntil([&]() { return session_id != -1; }));

  handler()->OpenRecentlyClosedEntry(session_id);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return browser1()->tab_strip_model()->count() == 3; }));

  EXPECT_TRUE(prefs->GetBoolean(tab_search_prefs::kTabSearchUsed));
}

// TODO(crbug.com/537538766): Flaky on Linux and ChromeOS.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_RemoveSplit_NTP DISABLED_RemoveSplit_NTP
#else
#define MAYBE_RemoveSplit_NTP RemoveSplit_NTP
#endif
IN_PROC_BROWSER_TEST_F(TabSearchPageHandlerTest, MAYBE_RemoveSplit_NTP) {
  EXPECT_CALL(page_, HostWindowChanged()).Times(testing::AnyNumber());
  EXPECT_CALL(page_, TabsChanged(_)).Times(testing::AnyNumber());
  EXPECT_CALL(page_, TabUpdated(_)).Times(testing::AnyNumber());
  EXPECT_CALL(page_, TabsRemoved(_)).Times(testing::AnyNumber());

  AddTabWithTitle(browser1(), tab_url1_, kTabName1);

  webui::SetBrowserWindowInterface(web_contents_.get(), nullptr);
  browser1()->tab_strip_model()->AppendWebContents(std::move(web_contents_),
                                                   true);

  TabStripModel* tab_strip_model = browser1()->tab_strip_model();
  const split_tabs::SplitTabId split_id = tab_strip_model->AddToNewSplit(
      {0}, split_tabs::SplitTabVisualData(),
      split_tabs::SplitTabCreatedSource::kToolbarButton);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser1(), GURL(chrome::kChromeUISplitViewNewTabPageURL)));

  base::RunLoop run_loop;
  EXPECT_CALL(page_, TabUnsplit()).WillOnce([&]() { run_loop.Quit(); });
  tab_strip_model->RemoveSplit(split_id);
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(TabSearchPageHandlerTest, RemoveSplit_OtherPage) {
  EXPECT_CALL(page_, HostWindowChanged()).Times(testing::AnyNumber());
  EXPECT_CALL(page_, TabsChanged(_)).Times(testing::AnyNumber());
  EXPECT_CALL(page_, TabUpdated(_)).Times(testing::AnyNumber());
  EXPECT_CALL(page_, TabsRemoved(_)).Times(testing::AnyNumber());

  AddTabWithTitle(browser1(), tab_url2_, kTabName2);
  webui::SetBrowserWindowInterface(web_contents_.get(), nullptr);
  browser1()->tab_strip_model()->AppendWebContents(std::move(web_contents_),
                                                   true);

  TabStripModel* tab_strip_model = browser1()->tab_strip_model();
  const split_tabs::SplitTabId split_id = tab_strip_model->AddToNewSplit(
      {0}, split_tabs::SplitTabVisualData(),
      split_tabs::SplitTabCreatedSource::kToolbarButton);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser1(), GURL(chrome::kChromeUITabSearchURL)));

  EXPECT_CALL(page_, TabUnsplit()).Times(0);
  tab_strip_model->RemoveSplit(split_id);

  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  page_.receiver_.FlushForTesting();
}

IN_PROC_BROWSER_TEST_F(TabSearchPageHandlerTest, RecentlyClosedSplitView) {
  AddTabWithTitle(browser1(), tab_url1_, kTabName1);
  AddTabWithTitle(browser1(), tab_url2_, kTabName2);

  TabStripModel* tab_strip_model = browser1()->tab_strip_model();

  EXPECT_EQ(3, tab_strip_model->count());
  EXPECT_EQ(0, tab_strip_model->active_index());

  // Create a split view with indices 1 and the active index 0.
  tab_strip_model->AddToNewSplit(
      {1},
      split_tabs::SplitTabVisualData(split_tabs::SplitTabLayout::kStacked, 0.5),
      split_tabs::SplitTabCreatedSource::kToolbarButton);

  EXPECT_TRUE(tab_strip_model->GetTabAtIndex(0)->GetSplit().has_value());
  EXPECT_TRUE(tab_strip_model->GetTabAtIndex(1)->GetSplit().has_value());

  // Close the split tabs.
  tab_strip_model->CloseSelectedTabs();

  // Assert the closed split view data is correct in ProfileData.
  base::test::TestFuture<tab_search::mojom::ProfileDataPtr> future;
  handler()->GetProfileData(future.GetCallback());
  tab_search::mojom::ProfileDataPtr profile_tabs = future.Take();

  auto& recently_closed_split_views = profile_tabs->recently_closed_split_views;
  ASSERT_EQ(1u, recently_closed_split_views.size());
  tab_search::mojom::RecentlyClosedSplitView* split_view =
      recently_closed_split_views[0].get();
  EXPECT_EQ(2u, split_view->tab_count);
  EXPECT_EQ(tab_search::mojom::SplitTabLayout::kStacked, split_view->layout);
  ASSERT_EQ(2u, split_view->tab_urls.size());
  EXPECT_EQ(tab_url2_.spec(), split_view->tab_urls[0].spec());
  EXPECT_EQ(tab_url1_.spec(), split_view->tab_urls[1].spec());

  EXPECT_CALL(page_, TabUpdated(_)).Times(testing::AnyNumber());
  EXPECT_CALL(page_, TabsRemoved(_)).Times(testing::AnyNumber());
}

IN_PROC_BROWSER_TEST_F(TabSearchPageHandlerTest,
                       CloseActionHistogram_NoAction) {
  base::HistogramTester histogram_tester;
  reset_handler();
  histogram_tester.ExpectUniqueSample("Tabs.TabSearch.CloseAction2",
                                      TabSearchCloseAction::kNoAction, 1);
}

IN_PROC_BROWSER_TEST_F(TabSearchPageHandlerTest,
                       CloseActionHistogram_CloseTab) {
  AddTabWithTitle(browser1(), tab_url1_, kTabName1);
  int32_t tab_id =
      browser1()->tab_strip_model()->GetTabAtIndex(0)->GetHandle().raw_value();
  handler()->CloseTab(tab_id);

  base::HistogramTester histogram_tester;
  reset_handler();
  histogram_tester.ExpectUniqueSample("Tabs.TabSearch.CloseAction2",
                                      TabSearchCloseAction::kCloseTab, 1);
}

IN_PROC_BROWSER_TEST_F(TabSearchPageHandlerTest,
                       CloseActionHistogram_SwitchTab) {
  AddTabWithTitle(browser1(), tab_url1_, kTabName1);
  int32_t tab_id =
      browser1()->tab_strip_model()->GetTabAtIndex(0)->GetHandle().raw_value();
  auto switch_to_tab_info = tab_search::mojom::SwitchToTabInfo::New();
  switch_to_tab_info->tab_id = tab_id;
  handler()->SwitchToTab(std::move(switch_to_tab_info));

  base::HistogramTester histogram_tester;
  reset_handler();
  histogram_tester.ExpectUniqueSample("Tabs.TabSearch.CloseAction2",
                                      TabSearchCloseAction::kSwitchTab, 1);
}

IN_PROC_BROWSER_TEST_F(TabSearchPageHandlerTest,
                       CloseActionHistogram_CloseThenSwitchTab) {
  AddTabWithTitle(browser1(), tab_url1_, kTabName1);
  AddTabWithTitle(browser1(), tab_url2_, kTabName2);
  int32_t tab_id1 =
      browser1()->tab_strip_model()->GetTabAtIndex(0)->GetHandle().raw_value();
  int32_t tab_id2 =
      browser1()->tab_strip_model()->GetTabAtIndex(1)->GetHandle().raw_value();

  handler()->CloseTab(tab_id1);

  auto switch_to_tab_info = tab_search::mojom::SwitchToTabInfo::New();
  switch_to_tab_info->tab_id = tab_id2;
  handler()->SwitchToTab(std::move(switch_to_tab_info));

  base::HistogramTester histogram_tester;
  reset_handler();
  histogram_tester.ExpectUniqueSample(
      "Tabs.TabSearch.CloseAction2",
      TabSearchCloseAction::kSwitchTabAndCloseTab, 1);
}

IN_PROC_BROWSER_TEST_F(TabSearchPageHandlerTest,
                       CloseActionHistogram_OpenRecentTab) {
  AddTabWithTitle(browser1(), tab_url1_, kTabName1);
  AddTabWithTitle(browser1(), tab_url2_, kTabName2);
  sessions::TabRestoreService* tab_restore_service =
      TabRestoreServiceFactory::GetForProfile(profile1());
  int32_t tab_id =
      browser1()->tab_strip_model()->GetTabAtIndex(0)->GetHandle().raw_value();
  handler()->CloseTab(tab_id);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return tab_restore_service->entries().size() >= 1u; }));

  handler()->BeforeBubbleWidgetShowed();

  int32_t session_id = tab_restore_service->entries().front()->id.id();
  handler()->OpenRecentlyClosedEntry(session_id);

  base::HistogramTester histogram_tester;
  reset_handler();
  histogram_tester.ExpectUniqueSample("Tabs.TabSearch.CloseAction2",
                                      TabSearchCloseAction::kOpenRecentTab, 1);
}

IN_PROC_BROWSER_TEST_F(TabSearchPageHandlerTest,
                       CloseActionHistogram_CloseThenOpenRecentTab) {
  AddTabWithTitle(browser1(), tab_url1_, kTabName1);
  AddTabWithTitle(browser1(), tab_url2_, kTabName2);
  sessions::TabRestoreService* tab_restore_service =
      TabRestoreServiceFactory::GetForProfile(profile1());

  int32_t tab_id1 =
      browser1()->tab_strip_model()->GetTabAtIndex(0)->GetHandle().raw_value();
  handler()->CloseTab(tab_id1);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return tab_restore_service->entries().size() >= 1u; }));

  handler()->BeforeBubbleWidgetShowed();

  int32_t tab_id2 =
      browser1()->tab_strip_model()->GetTabAtIndex(0)->GetHandle().raw_value();
  handler()->CloseTab(tab_id2);

  int32_t session_id = tab_restore_service->entries().front()->id.id();
  handler()->OpenRecentlyClosedEntry(session_id);

  base::HistogramTester histogram_tester;
  reset_handler();
  histogram_tester.ExpectUniqueSample(
      "Tabs.TabSearch.CloseAction2",
      TabSearchCloseAction::kOpenRecentTabAndCloseTab, 1);
}

IN_PROC_BROWSER_TEST_F(TabSearchPageHandlerTest,
                       CloseActionHistogram_MultipleSessionsWithCaching) {
  base::HistogramTester histogram_tester;

  // Session 1: Before showing bubble
  handler()->BeforeBubbleWidgetShowed();
  AddTabWithTitle(browser1(), tab_url1_, kTabName1);
  int32_t tab_id1 =
      browser1()->tab_strip_model()->GetTabAtIndex(0)->GetHandle().raw_value();
  handler()->CloseTab(tab_id1);

  // Bubble closes (visibility becomes HIDDEN)
  handler()->OnVisibilityChanged(content::Visibility::HIDDEN);

  histogram_tester.ExpectBucketCount("Tabs.TabSearch.CloseAction2",
                                     TabSearchCloseAction::kCloseTab, 1);

  // Session 2: Bubble re-opened (preloaded/cached)
  handler()->BeforeBubbleWidgetShowed();
  int32_t open_tab_id =
      browser1()->tab_strip_model()->GetTabAtIndex(0)->GetHandle().raw_value();
  auto switch_to_tab_info = tab_search::mojom::SwitchToTabInfo::New();
  switch_to_tab_info->tab_id = open_tab_id;
  handler()->SwitchToTab(std::move(switch_to_tab_info));

  // Bubble closes again (visibility becomes HIDDEN)
  handler()->OnVisibilityChanged(content::Visibility::HIDDEN);

  histogram_tester.ExpectBucketCount("Tabs.TabSearch.CloseAction2",
                                     TabSearchCloseAction::kCloseTab, 1);
  histogram_tester.ExpectBucketCount("Tabs.TabSearch.CloseAction2",
                                     TabSearchCloseAction::kSwitchTab, 1);
  histogram_tester.ExpectTotalCount("Tabs.TabSearch.CloseAction2", 2);
}

}  // namespace
