// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/shelf/browser_app_status_observer.h"
#include "chrome/browser/ui/ash/shelf/browser_apps_tracker.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/common/constants.h"

// default implementation of RunTestOnMainThread() and TestBody()
#include "content/public/test/browser_test.h"

namespace {

using extension_misc::kChromeAppId;

// Generated from start URL "https://a.example.org/".
// See |web_app::GenerateAppId|.
constexpr char kAppAId[] = "dhehpanpcmiafdmbldplnfenbijejdfe";

// Generated from start URL "https://b.example.org/".
constexpr char kAppBId[] = "abhkhfladdfdlfmhaokoglcllbamaili";

struct TestInstance {
  static TestInstance Create(const std::string name,
                             const BrowserAppInstance& instance) {
    return {
        name,
        instance.app_id,
        instance.browser,
        instance.web_contents,
        instance.web_contents_id,
        instance.visible,
        instance.active,
    };
  }
  static TestInstance Create(const BrowserAppInstance* instance) {
    if (instance) {
      return Create("snapshot", *instance);
    }
    return {};
  }
  std::string name;
  std::string app_id;
  Browser* browser;
  content::WebContents* web_contents;
  uint32_t web_contents_id;
  bool visible;
  bool active;
};

// Make test sequence easier to scan
constexpr bool kVisible = true;
constexpr bool kHidden = false;
constexpr bool kActive = true;
constexpr bool kInactive = false;

bool operator==(const TestInstance& e1, const TestInstance& e2) {
  return e1.name == e2.name && e1.app_id == e2.app_id &&
         e1.browser == e2.browser && e1.web_contents == e2.web_contents &&
         e1.web_contents_id == e2.web_contents_id && e1.visible == e2.visible &&
         e1.active == e2.active;
}

bool operator!=(const TestInstance& e1, const TestInstance& e2) {
  return !(e1 == e2);
}

std::ostream& operator<<(std::ostream& os, const TestInstance& e) {
  if (e.name == "") {
    return os << "none";
  }
  os << e.name << "(app_id=" << e.app_id << ", browser=" << e.browser;
  if (e.web_contents) {
    os << ", tab=" << e.web_contents;
    os << ", tab_id=" << e.web_contents_id;
  }
  os << ", " << (e.visible ? "visible" : "hidden");
  os << ", " << (e.active ? "active" : "inactive");
  return os << ")";
}

class Recorder : public BrowserAppStatusObserver {
 public:
  explicit Recorder(BrowserAppsTracker* tracker) : tracker_(tracker) {
    DCHECK(tracker);
    tracker_->AddObserver(this);
  }

  ~Recorder() override { tracker_->RemoveObserver(this); }

  void OnBrowserAppAdded(const BrowserAppInstance& instance) override {
    calls_.push_back(TestInstance::Create("added", instance));
  }

  void OnBrowserAppUpdated(const BrowserAppInstance& instance) override {
    calls_.push_back(TestInstance::Create("updated", instance));
  }

  void OnBrowserAppRemoved(const BrowserAppInstance& instance) override {
    calls_.push_back(TestInstance::Create("removed", instance));
  }

  void Verify(const std::vector<TestInstance>& expected_calls) {
    EXPECT_EQ(calls_.size(), expected_calls.size());
    for (int i = 0; i < std::max(calls_.size(), expected_calls.size()); ++i) {
      EXPECT_EQ(Get(calls_, i), Get(expected_calls, i)) << "call #" << i;
    }
  }

 private:
  static const TestInstance Get(const std::vector<TestInstance>& calls, int i) {
    if (i < calls.size()) {
      return calls[i];
    }
    return {};
  }

  BrowserAppsTracker* tracker_;
  std::vector<TestInstance> calls_;
};

}  // namespace

class BrowserAppsTrackerTest : public InProcessBrowserTest {
 protected:
  Browser* CreateBrowser() {
    Profile* profile = ProfileManager::GetPrimaryUserProfile();
    Browser::CreateParams params(profile, true /* user_gesture */);
    Browser* browser = Browser::Create(params);
    browser->window()->Show();
    return browser;
  }

  Browser* CreateAppBrowser(const std::string& app_id) {
    Profile* profile = ProfileManager::GetPrimaryUserProfile();
    auto params = Browser::CreateParams::CreateForApp(
        "_crx_" + app_id, true /* trusted_source */,
        gfx::Rect(), /* window_bounts */
        profile, true /* user_gesture */);
    Browser* browser = Browser::Create(params);
    browser->window()->Show();
    return browser;
  }

  content::WebContents* NavigateAndWait(Browser* browser,
                                        const std::string& url,
                                        WindowOpenDisposition disposition) {
    NavigateParams params(browser, GURL(url),
                          ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
    params.disposition = disposition;
    Navigate(&params);
    auto* contents = params.navigated_or_inserted_contents;
    DCHECK_EQ(chrome::FindBrowserWithWebContents(
                  params.navigated_or_inserted_contents),
              browser);
    content::TestNavigationObserver observer(contents);
    observer.Wait();
    return contents;
  }

  void NavigateActiveTab(Browser* browser, const std::string& url) {
    NavigateAndWait(browser, url, WindowOpenDisposition::CURRENT_TAB);
  }

  content::WebContents* InsertBackgroundTab(Browser* browser,
                                            const std::string& url) {
    return NavigateAndWait(browser, url,
                           WindowOpenDisposition::NEW_BACKGROUND_TAB);
  }

  content::WebContents* InsertForegroundTab(Browser* browser,
                                            const std::string& url) {
    return NavigateAndWait(browser, url,
                           WindowOpenDisposition::NEW_FOREGROUND_TAB);
  }

  web_app::AppId InstallWebApp(const std::string& start_url) {
    auto info = std::make_unique<WebApplicationInfo>();
    info->start_url = GURL(start_url);
    Profile* profile = ProfileManager::GetPrimaryUserProfile();
    auto app_id = web_app::test::InstallWebApp(profile, std::move(info));
    FlushAppService();
    return app_id;
  }

  void UninstallWebApp(const web_app::AppId& app_id) {
    Profile* profile = ProfileManager::GetPrimaryUserProfile();
    web_app::test::UninstallWebApp(profile, app_id);
    FlushAppService();
  }

  void FlushAppService() {
    Profile* profile = ProfileManager::GetPrimaryUserProfile();
    apps::AppServiceProxyFactory::GetForProfile(profile)
        ->FlushMojoCallsForTesting();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    Profile* profile = ProfileManager::GetPrimaryUserProfile();
    apps::AppServiceProxyChromeOs* proxy =
        apps::AppServiceProxyFactory::GetForProfile(profile);
    tracker_ = std::make_unique<BrowserAppsTracker>(proxy->AppRegistryCache());
    tracker_->Initialize();

    ASSERT_EQ(kAppAId, InstallWebApp("https://a.example.org"));
    ASSERT_EQ(kAppBId, InstallWebApp("https://b.example.org"));
  }

  void TearDownOnMainThread() override {
    tracker_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest ::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kNoStartupWindow);
  }

 protected:
  std::unique_ptr<BrowserAppsTracker> tracker_;
};

IN_PROC_BROWSER_TEST_F(BrowserAppsTrackerTest, InsertAndCloseTabs) {
  Browser* browser = nullptr;
  content::WebContents* tab_app1 = nullptr;
  content::WebContents* tab_app2 = nullptr;

  // Open a foreground tab with a website.
  {
    SCOPED_TRACE("insert an initial foreground tab");
    Recorder recorder(tracker_.get());

    browser = CreateBrowser();
    tab_app1 = InsertForegroundTab(browser, "https://a.example.org");
    recorder.Verify({
        {"added", kChromeAppId, browser, nullptr, 0, kVisible, kActive},
        {"added", kAppAId, browser, tab_app1, 1, kVisible, kActive},
    });
  }

  // Open a second tab in foreground.
  {
    SCOPED_TRACE("insert a second foreground tab");
    Recorder recorder(tracker_.get());

    tab_app2 = InsertForegroundTab(browser, "https://b.example.org");
    recorder.Verify({
        {"updated", kAppAId, browser, tab_app1, 1, kVisible, kInactive},
        {"added", kAppBId, browser, tab_app2, 2, kVisible, kActive},
    });
  }

  // Open a third tab in foreground with no app.
  {
    SCOPED_TRACE("insert a third foreground tab without app");
    Recorder recorder(tracker_.get());

    InsertForegroundTab(browser, "https://c.example.org");
    recorder.Verify({
        {"updated", kAppBId, browser, tab_app2, 2, kVisible, kInactive},
    });
  }

  // Open two more tabs in foreground and close them.
  {
    SCOPED_TRACE("insert and close two more tabs");
    Recorder recorder(tracker_.get());

    auto* tab_app4 = InsertForegroundTab(browser, "https://a.example.org");
    auto* tab_app5 = InsertForegroundTab(browser, "https://b.example.org");
    // Close in reverse order.
    int i = browser->tab_strip_model()->GetIndexOfWebContents(tab_app5);
    browser->tab_strip_model()->CloseWebContentsAt(
        i, TabStripModel::CLOSE_USER_GESTURE);
    i = browser->tab_strip_model()->GetIndexOfWebContents(tab_app4);
    browser->tab_strip_model()->CloseWebContentsAt(
        i, TabStripModel::CLOSE_USER_GESTURE);

    recorder.Verify({
        // tab 4 opened: no events for tab 3 as it has no app
        {"added", kAppAId, browser, tab_app4, 3, kVisible, kActive},
        // tab 5 opened: tab 4 deactivates
        {"updated", kAppAId, browser, tab_app4, 3, kVisible, kInactive},
        {"added", kAppBId, browser, tab_app5, 4, kVisible, kActive},
        // tab 5 closed: tab 4 reactivates
        {"removed", kAppBId, browser, tab_app5, 4, kVisible, kActive},
        {"updated", kAppAId, browser, tab_app4, 3, kVisible, kActive},
        // tab closed: no events for tab 3 as it has no app
        {"removed", kAppAId, browser, tab_app4, 3, kVisible, kActive},
    });
  }

  // Close the browser.
  {
    SCOPED_TRACE("close browser");
    Recorder recorder(tracker_.get());

    browser->tab_strip_model()->CloseAllTabs();
    recorder.Verify({
        {"removed", kAppBId, browser, tab_app2, 2, kVisible, kInactive},
        {"removed", kAppAId, browser, tab_app1, 1, kVisible, kInactive},
        {"removed", kChromeAppId, browser, nullptr, 0, kVisible, kActive},
    });
  }
}

IN_PROC_BROWSER_TEST_F(BrowserAppsTrackerTest, ForegroundTabNavigate) {
  // Setup: one foreground tab with no app.
  auto* browser = CreateBrowser();
  auto* tab = InsertForegroundTab(browser, "https://c.example.org");

  // Navigate the foreground tab to app A.
  {
    SCOPED_TRACE("navigate tab to app A");
    Recorder recorder(tracker_.get());

    NavigateActiveTab(browser, "https://a.example.org");
    recorder.Verify({
        {"added", kAppAId, browser, tab, 1, kVisible, kActive},
    });
  }

  // Navigate the foreground tab to app B.
  {
    SCOPED_TRACE("navigate tab to app B");
    Recorder recorder(tracker_.get());

    NavigateActiveTab(browser, "https://b.example.org");
    recorder.Verify({
        {"removed", kAppAId, browser, tab, 1, kVisible, kActive},
        {"added", kAppBId, browser, tab, 2, kVisible, kActive},
    });
  }

  // Navigate the foreground tab to a different subdomain with no app.
  {
    SCOPED_TRACE("navigate tab from app B to a non-app subdomain");
    Recorder recorder(tracker_.get());

    NavigateActiveTab(browser, "https://c.example.org");
    recorder.Verify({
        {"removed", kAppBId, browser, tab, 2, kVisible, kActive},
    });
  }

  // Navigate the foreground tab from a non-app subdomain to app B.
  {
    SCOPED_TRACE("navigate tab from a non-app subdomain to app B");
    Recorder recorder(tracker_.get());

    NavigateActiveTab(browser, "https://b.example.org");
    recorder.Verify({
        {"added", kAppBId, browser, tab, 3, kVisible, kActive},
    });
  }

  // Navigate the foreground tab to a different domain with no app.
  {
    SCOPED_TRACE("navigate tab from app B to a non-app domain");
    Recorder recorder(tracker_.get());

    NavigateActiveTab(browser, "https://example.com");
    recorder.Verify({
        {"removed", kAppBId, browser, tab, 3, kVisible, kActive},
    });
  }

  // Navigate the foreground tab from a non-app domain to app B.
  {
    SCOPED_TRACE("navigate tab from a non-app domain to app B");
    Recorder recorder(tracker_.get());

    NavigateActiveTab(browser, "https://b.example.org");
    recorder.Verify({
        {"added", kAppBId, browser, tab, 4, kVisible, kActive},
    });
  }
}

IN_PROC_BROWSER_TEST_F(BrowserAppsTrackerTest, WindowedWebApp) {
  Browser* browser = nullptr;
  content::WebContents* tab = nullptr;

  // Open app A in a window.
  {
    SCOPED_TRACE("create a windowed app");
    Recorder recorder(tracker_.get());

    browser = CreateAppBrowser(kAppAId);
    tab = InsertForegroundTab(browser, "https://a.example.org");
    recorder.Verify({
        {"added", kAppAId, browser, tab, 1, kVisible, kActive},
    });
  }

  // Close the browser.
  {
    SCOPED_TRACE("close browser");
    Recorder recorder(tracker_.get());

    browser->tab_strip_model()->CloseAllTabs();
    recorder.Verify({
        {"removed", kAppAId, browser, tab, 1, kVisible, kActive},
    });
  }
}

IN_PROC_BROWSER_TEST_F(BrowserAppsTrackerTest, SwitchTabs) {
  // Setup: one foreground tab and one background tab.
  auto* browser = CreateBrowser();
  auto* tab0 = InsertForegroundTab(browser, "https://a.example.org");
  auto* tab1 = InsertForegroundTab(browser, "https://b.example.org");
  InsertForegroundTab(browser, "https://c.example.org");

  // Switch tabs: no app -> app A
  {
    SCOPED_TRACE("switch tabs to app A");
    Recorder recorder(tracker_.get());

    browser->tab_strip_model()->ActivateTabAt(0);
    recorder.Verify({
        {"updated", kAppAId, browser, tab0, 1, kVisible, kActive},
    });
  }

  // Switch tabs: app A -> app B
  {
    SCOPED_TRACE("switch tabs to app B");
    Recorder recorder(tracker_.get());

    browser->tab_strip_model()->ActivateTabAt(1);
    recorder.Verify({
        {"updated", kAppBId, browser, tab1, 2, kVisible, kActive},
        {"updated", kAppAId, browser, tab0, 1, kVisible, kInactive},
    });
  }

  // Switch tabs: app B -> no app
  {
    SCOPED_TRACE("switch tabs to no app");
    Recorder recorder(tracker_.get());

    browser->tab_strip_model()->ActivateTabAt(2);
    recorder.Verify({
        {"updated", kAppBId, browser, tab1, 2, kVisible, kInactive},
    });
  }
}

IN_PROC_BROWSER_TEST_F(BrowserAppsTrackerTest, WindowVisibility) {
  // Setup: one foreground tab and one background tab.
  auto* browser = CreateBrowser();
  auto* bg_tab = InsertForegroundTab(browser, "https://a.example.org");
  auto* fg_tab = InsertForegroundTab(browser, "https://b.example.org");
  InsertForegroundTab(browser, "https://c.example.org");
  // Prevent spurious deactivation events.
  browser->window()->Deactivate();

  // Hide the window.
  {
    SCOPED_TRACE("hide window");
    Recorder recorder(tracker_.get());

    browser->window()->GetNativeWindow()->Hide();
    recorder.Verify({
        {"updated", kChromeAppId, browser, nullptr, 0, kHidden, kInactive},
        {"updated", kAppAId, browser, bg_tab, 1, kHidden, kInactive},
        {"updated", kAppBId, browser, fg_tab, 2, kHidden, kInactive},
    });
  }

  // Show the window.
  {
    SCOPED_TRACE("show window");
    Recorder recorder(tracker_.get());

    browser->window()->GetNativeWindow()->Show();
    recorder.Verify({
        {"updated", kChromeAppId, browser, nullptr, 0, kVisible, kInactive},
        {"updated", kAppAId, browser, bg_tab, 1, kVisible, kInactive},
        {"updated", kAppBId, browser, fg_tab, 2, kVisible, kInactive},
    });
  }
}

IN_PROC_BROWSER_TEST_F(BrowserAppsTrackerTest, WindowActivation) {
  // Setup: two browsers with two tabs each.
  auto* browser1 = CreateBrowser();
  InsertForegroundTab(browser1, "https://a.example.org");
  InsertForegroundTab(browser1, "https://c.example.org");
  auto* fg_tab1 = InsertForegroundTab(browser1, "https://b.example.org");

  auto* browser2 = CreateBrowser();
  InsertForegroundTab(browser2, "https://a.example.org");
  InsertForegroundTab(browser2, "https://c.example.org");
  auto* fg_tab2 = InsertForegroundTab(browser2, "https://b.example.org");

  ASSERT_FALSE(browser1->window()->IsActive());
  ASSERT_TRUE(browser2->window()->IsActive());

  // Activate window 1.
  {
    SCOPED_TRACE("activate window 1");
    Recorder recorder(tracker_.get());

    browser1->window()->Activate();
    recorder.Verify({
        // deactivated first
        {"updated", kChromeAppId, browser2, nullptr, 0, kVisible, kInactive},
        {"updated", kAppBId, browser2, fg_tab2, 4, kVisible, kInactive},
        // then activated
        {"updated", kChromeAppId, browser1, nullptr, 0, kVisible, kActive},
        {"updated", kAppBId, browser1, fg_tab1, 2, kVisible, kActive},
    });
  }

  // Activate window 2.
  {
    SCOPED_TRACE("activate window 2");
    Recorder recorder(tracker_.get());

    browser2->window()->Activate();
    recorder.Verify({
        // deactivated first
        {"updated", kChromeAppId, browser1, nullptr, 0, kVisible, kInactive},
        {"updated", kAppBId, browser1, fg_tab1, 2, kVisible, kInactive},
        // then activated
        {"updated", kChromeAppId, browser2, nullptr, 0, kVisible, kActive},
        {"updated", kAppBId, browser2, fg_tab2, 4, kVisible, kActive},
    });
  }
}

IN_PROC_BROWSER_TEST_F(BrowserAppsTrackerTest, TabDrag) {
  // Setup: two browsers: one with two, another with three tabs.
  auto* browser1 = CreateBrowser();
  InsertForegroundTab(browser1, "https://a.example.org");
  auto* fg_tab1 = InsertForegroundTab(browser1, "https://b.example.org");

  auto* browser2 = CreateBrowser();
  InsertForegroundTab(browser2, "https://a.example.org");
  auto* bg_tab2 = InsertForegroundTab(browser2, "https://a.example.org");
  auto* fg_tab2 = InsertForegroundTab(browser2, "https://b.example.org");

  ASSERT_FALSE(browser1->window()->IsActive());
  ASSERT_TRUE(browser2->window()->IsActive());

  // Drag the active tab of browser 2 and rop it into the last position in
  // browser 1.
  SCOPED_TRACE("tab drag and drop");
  Recorder recorder(tracker_.get());

  // We skip a step where a detached tab gets inserted into a temporary browser
  // but the sequence there is identical.

  // Detach.
  int src_index = browser2->tab_strip_model()->GetIndexOfWebContents(fg_tab2);
  auto detached =
      browser2->tab_strip_model()->DetachWebContentsAtForInsertion(src_index);

  // Target browser window goes into foreground right before drop.
  browser1->window()->Activate();

  // Attach.
  int dst_index = browser1->tab_strip_model()->count();
  browser1->tab_strip_model()->InsertWebContentsAt(
      dst_index, std::move(detached), TabStripModel::ADD_ACTIVE);
  recorder.Verify({
      // background tab in the dragged-from browser gets activated when the
      // active tab is detached
      {"updated", kAppAId, browser2, bg_tab2, 4, kVisible, kActive},
      // dragged-from browser goes into background
      {"updated", kChromeAppId, browser2, nullptr, 0, kVisible, kInactive},
      {"updated", kAppAId, browser2, bg_tab2, 4, kVisible, kInactive},
      // dragged-into browser window goes into foreground
      {"updated", kChromeAppId, browser1, nullptr, 0, kVisible, kActive},
      {"updated", kAppBId, browser1, fg_tab1, 2, kVisible, kActive},
      // previously foreground tab in the dragged-into browser goes into
      // background when the dragged tab is attached to the new browser
      {"updated", kAppBId, browser1, fg_tab1, 2, kVisible, kInactive},
      // dragged tab gets reparented and becomes active in the new browser
      {"updated", kAppBId, browser1, fg_tab2, 5, kVisible, kActive},
  });
}

IN_PROC_BROWSER_TEST_F(BrowserAppsTrackerTest, Accessors) {
  // Setup: two regular browsers, and one app window browser.
  auto* browser1 = CreateBrowser();
  auto* b1_tab1 = InsertForegroundTab(browser1, "https://a.example.org");
  auto* b1_tab2 = InsertForegroundTab(browser1, "https://c.example.org");
  auto* b1_tab3 = InsertForegroundTab(browser1, "https://b.example.org");

  auto* browser2 = CreateBrowser();
  auto* b2_tab1 = InsertForegroundTab(browser2, "https://c.example.org");
  auto* b2_tab2 = InsertForegroundTab(browser2, "https://b.example.org");

  auto* browser3 = CreateAppBrowser(kAppBId);
  auto* b3_tab1 = InsertForegroundTab(browser3, "https://b.example.org");

  ASSERT_FALSE(browser1->window()->IsActive());
  ASSERT_FALSE(browser2->window()->IsActive());
  ASSERT_TRUE(browser3->window()->IsActive());

  auto* b1_app = tracker_->GetChromeInstance(browser1);
  auto* b1_tab1_app = tracker_->GetAppInstance(b1_tab1);
  auto* b1_tab2_app = tracker_->GetAppInstance(b1_tab2);
  auto* b1_tab3_app = tracker_->GetAppInstance(b1_tab3);

  auto* b2_app = tracker_->GetChromeInstance(browser2);
  auto* b2_tab1_app = tracker_->GetAppInstance(b2_tab1);
  auto* b2_tab2_app = tracker_->GetAppInstance(b2_tab2);

  auto* b3_app = tracker_->GetChromeInstance(browser3);
  auto* b3_tab1_app = tracker_->GetAppInstance(b3_tab1);

  EXPECT_EQ(TestInstance::Create(b1_app),
            (TestInstance{"snapshot", kChromeAppId, browser1, nullptr, 0,
                          kVisible, kInactive}));
  EXPECT_EQ(TestInstance::Create(b1_tab1_app),
            (TestInstance{"snapshot", kAppAId, browser1, b1_tab1, 1, kVisible,
                          kInactive}));
  EXPECT_EQ(TestInstance::Create(b1_tab2_app), TestInstance{});
  EXPECT_EQ(TestInstance::Create(b1_tab3_app),
            (TestInstance{"snapshot", kAppBId, browser1, b1_tab3, 2, kVisible,
                          kInactive}));

  EXPECT_EQ(TestInstance::Create(b2_app),
            (TestInstance{"snapshot", kChromeAppId, browser2, nullptr, 0,
                          kVisible, kInactive}));
  EXPECT_EQ(TestInstance::Create(b2_tab1_app), TestInstance{});
  EXPECT_EQ(TestInstance::Create(b2_tab2_app),
            (TestInstance{"snapshot", kAppBId, browser2, b2_tab2, 3, kVisible,
                          kInactive}));

  EXPECT_EQ(TestInstance::Create(b3_app), TestInstance{});
  EXPECT_EQ(TestInstance::Create(b3_tab1_app),
            (TestInstance{"snapshot", kAppBId, browser3, b3_tab1, 4, kVisible,
                          kActive}));

  EXPECT_EQ(tracker_->GetAppInstancesByAppId(kAppAId),
            std::set<const BrowserAppInstance*>{b1_tab1_app});
  EXPECT_EQ(tracker_->GetAppInstancesByAppId(kAppBId),
            (std::set<const BrowserAppInstance*>{b1_tab3_app, b2_tab2_app,
                                                 b3_tab1_app}));
  EXPECT_EQ(tracker_->GetAppInstancesByAppId(kChromeAppId),
            (std::set<const BrowserAppInstance*>{b1_app, b2_app}));

  EXPECT_TRUE(tracker_->IsAppRunning(kAppAId));
  EXPECT_TRUE(tracker_->IsAppRunning(kAppBId));
  EXPECT_TRUE(tracker_->IsAppRunning(kChromeAppId));
  EXPECT_FALSE(tracker_->IsAppRunning("non-existent-app"));

  EXPECT_EQ(TestInstance::Create(tracker_->GetAppInstanceByWebContentsId(1)),
            (TestInstance{"snapshot", kAppAId, browser1, b1_tab1, 1, kVisible,
                          kInactive}));
  EXPECT_EQ(TestInstance::Create(tracker_->GetAppInstanceByWebContentsId(10)),
            TestInstance{});

  // App A is closed, B and Chrome are still running.
  browser1->tab_strip_model()->CloseAllTabs();

  EXPECT_FALSE(tracker_->IsAppRunning(kAppAId));
  EXPECT_TRUE(tracker_->IsAppRunning(kAppBId));
  EXPECT_TRUE(tracker_->IsAppRunning(kChromeAppId));

  // App A and Chrome are closed, B is still running.
  browser2->tab_strip_model()->CloseAllTabs();

  EXPECT_FALSE(tracker_->IsAppRunning(kAppAId));
  EXPECT_TRUE(tracker_->IsAppRunning(kAppBId));
  EXPECT_FALSE(tracker_->IsAppRunning(kChromeAppId));

  // Everything is closed.
  browser3->tab_strip_model()->CloseAllTabs();

  EXPECT_FALSE(tracker_->IsAppRunning(kAppAId));
  EXPECT_FALSE(tracker_->IsAppRunning(kAppBId));
  EXPECT_FALSE(tracker_->IsAppRunning(kChromeAppId));
}

IN_PROC_BROWSER_TEST_F(BrowserAppsTrackerTest, AppInstall) {
  auto* browser1 = CreateBrowser();
  auto* tab1 = InsertForegroundTab(browser1, "https://c.example.org");
  InsertForegroundTab(browser1, "https://d.example.org");
  auto* tab3 = InsertForegroundTab(browser1, "https://c.example.org");

  std::string app_id;
  {
    SCOPED_TRACE("install app");
    Recorder recorder(tracker_.get());

    app_id = InstallWebApp("https://c.example.org");
    recorder.Verify({
        {"added", app_id, browser1, tab1, 1, kVisible, kInactive},
        {"added", app_id, browser1, tab3, 2, kVisible, kActive},
    });
  }

  {
    SCOPED_TRACE("uninstall app");
    Recorder recorder(tracker_.get());

    UninstallWebApp(app_id);
    recorder.Verify({
        {"removed", app_id, browser1, tab1, 1, kVisible, kInactive},
        {"removed", app_id, browser1, tab3, 2, kVisible, kActive},
    });
  }
}
