// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>
#include <vector>

#include "base/stl_util.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/preloading/scoped_prewarm_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/constants.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/security_principal.h"
#include "content/public/browser/spare_render_process_host_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/child_process_id.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "content/public/common/content_features.h"
#else
#include "base/functional/callback_helpers.h"
#include "base/memory_coordinator/memory_limit.h"
#include "base/test/mock_callback.h"
#include "base/test/run_until.h"
#include "chrome/browser/new_tab_page/prefs/ntp_pref_names.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"
#include "content/public/test/memory_coordinator_browsertest_util.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/test/test_utils.h"
#include "ui/webui/buildflags.h"
#if BUILDFLAG(ENABLE_WEBUI_GENERATE_CODE_CACHE)
#include "base/containers/flat_map.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/webui/util/webui_util_desktop.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "content/public/common/content_features.h"
#endif  // BUILDFLAG(ENABLE_WEBUI_GENERATE_CODE_CACHE)
#endif  // BUILDFLAG(IS_ANDROID)

namespace {

#if !BUILDFLAG(IS_ANDROID)
// Observes TabStripModelChange::kRemoved notifications and records the
// WebContents remove reason and the TabModel detach reason.
class WebContentsRemovedObserver : public TabStripModelObserver {
 public:
  WebContentsRemovedObserver() = default;
  WebContentsRemovedObserver(const WebContentsRemovedObserver&) = delete;
  WebContentsRemovedObserver& operator=(const WebContentsRemovedObserver&) =
      delete;
  ~WebContentsRemovedObserver() override = default;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    if (change.type() == TabStripModelChange::kRemoved) {
      const TabStripModelChange::RemovedTab& removed_tab =
          change.GetRemove()->contents[0];
      remove_reason_ = removed_tab.remove_reason;
      tab_detach_reason_ = removed_tab.tab_detach_reason;
    }
  }

  std::optional<TabRemovedReason> remove_reason() const {
    return remove_reason_;
  }
  std::optional<tabs::TabInterface::DetachReason> tab_detach_reason() const {
    return tab_detach_reason_;
  }

 private:
  std::optional<TabRemovedReason> remove_reason_;
  std::optional<tabs::TabInterface::DetachReason> tab_detach_reason_;
};
#endif  // !BUILDFLAG(IS_ANDROID)

void ExpectIsWebUiNtp(content::WebContents* tab) {
  EXPECT_EQ(chrome::ChromeUINewTabPageURLAsGURL().spec(),
            EvalJs(tab, "window.location.href",
                   content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, /*world_id=*/1));
}

std::set<content::ChildProcessId> LiveRenderProcessHostIds() {
  std::set<content::ChildProcessId> result;
  for (auto iter = content::RenderProcessHost::AllHostsIterator();
       !iter.IsAtEnd(); iter.Advance()) {
    result.insert(iter.GetCurrentKey());
  }
  return result;
}

class WebUiNtpBrowserTest : public PlatformBrowserTest {
 public:
  WebUiNtpBrowserTest() {
#if BUILDFLAG(IS_ANDROID)
    // When `AndroidWarmUpSpareRendererWithTimeout` is enabled with
    // `only_for_navigation=true` (e.g. via the fieldtrial testing config),
    // root frame initialization (`kRFHInitRoot`) during new-tab creation
    // cannot consume a spare renderer. Disable it so `SpareRenderer` behaves
    // the same regardless of the field trial configuration.
    feature_list_.InitAndDisableFeature(
        features::kAndroidWarmUpSpareRendererWithTimeout);
#endif
  }

  ~WebUiNtpBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->StartAcceptingConnections();
  }

 protected:
  // Opens the NTP in a new foreground tab, waits for it to finish loading and
  // returns its WebContents.
  content::WebContents* OpenNewTabPage() {
    tabs::TabInterface* tab = GetTabListInterface()->OpenTab(
        chrome::ChromeUINewTabURLAsGURL(), tab_list::kNoTabIndex);
    if (!tab) {
      ADD_FAILURE() << "Failed to open a new tab.";
      return nullptr;
    }
    content::WebContents* web_contents = tab->GetContents();
    EXPECT_TRUE(content::WaitForLoadStop(web_contents));
    return web_contents;
  }

 private:
  // TODO(https://crbug.com/423465927): Explore a better approach to make the
  // existing tests run with the prewarm feature enabled.
  test::ScopedPrewarmFeatureList scoped_prewarm_feature_list_{
      test::ScopedPrewarmFeatureList::PrewarmState::kDisabled};
#if BUILDFLAG(IS_ANDROID)
  base::test::ScopedFeatureList feature_list_;
#endif
};

// Verify that the WebUI NTP commits in a SiteInstance with the WebUI URL.
IN_PROC_BROWSER_TEST_F(WebUiNtpBrowserTest, VerifySiteInstance) {
  const GURL& ntp_url = chrome::ChromeUINewTabURLAsGURL();
  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(content::NavigateToURL(web_contents, ntp_url));
  ASSERT_EQ(ntp_url, web_contents->GetLastCommittedURL());

  const GURL& webui_ntp_url = chrome::ChromeUINewTabPageURLAsGURL();
  ASSERT_EQ(webui_ntp_url, web_contents->GetPrimaryMainFrame()
                               ->GetSiteInstance()
                               ->GetSecurityPrincipal()
                               .GetDeprecatedSiteURL());
}

// Verify that the WebUI NTP uses process-per-site.
IN_PROC_BROWSER_TEST_F(WebUiNtpBrowserTest, ProcessPerSite) {
  std::vector<content::WebContents*> tabs;

  // Open a few NTPs.
  for (size_t i = 0; i < 3; i++) {
    content::WebContents* tab = OpenNewTabPage();
    ASSERT_TRUE(tab);

    // Sanity check: the NTP should be a WebUI NTP (and not chrome://newtab/ or
    // some other NTP).
    ExpectIsWebUiNtp(tab);

    tabs.push_back(tab);
  }

  // Verify that all NTPs share a process.
  for (size_t i = 1; i < tabs.size(); i++) {
    EXPECT_EQ(tabs[0]->GetPrimaryMainFrame()->GetProcess()->GetID(),
              tabs[i]->GetPrimaryMainFrame()->GetProcess()->GetID());
  }
}

class WebUiNtpSpareRendererBrowserTest : public WebUiNtpBrowserTest {
#if !BUILDFLAG(IS_ANDROID)
 public:
  WebUiNtpSpareRendererBrowserTest() {
    // Bundled V8 code caching disallows V8 flag overrides, bypassing warm spare
    // renderers; disable it here to test spare reuse when caching is off.
    feature_list_.InitAndDisableFeature(
        features::kWebUIBundledCodeCacheForNewTabPage);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
#endif
};

// Verify that the WebUI NTP uses an available spare process and does not
// discard it as in https://crbug.com/40699271.
IN_PROC_BROWSER_TEST_F(WebUiNtpSpareRendererBrowserTest, SpareRenderer) {
#if BUILDFLAG(IS_ANDROID)
  // Android does not keep a spare renderer around by default, so create one.
  content::SpareRenderProcessHostManager::Get().WarmupSpare(GetProfile());
#endif

  // Capture current spare renderer.
  std::vector<content::ChildProcessId> spare_ids_before_navigation =
      content::SpareRenderProcessHostManager::Get().GetSpareIds();
  ASSERT_FALSE(spare_ids_before_navigation.empty());

  // Note the current render processes before the navigation. These should all
  // remain alive after the navigation.
  const std::set<content::ChildProcessId> starting_rph_ids =
      LiveRenderProcessHostIds();

  // Open an NTP.
  content::WebContents* ntp = OpenNewTabPage();
  ASSERT_TRUE(ntp);
  ExpectIsWebUiNtp(ntp);

  // Check spare was taken.
  EXPECT_THAT(
      spare_ids_before_navigation,
      testing::Contains(ntp->GetPrimaryMainFrame()->GetProcess()->GetID()));

  // No processes should be unnecessarily terminated.
  const std::set<content::ChildProcessId> ending_rph_ids =
      LiveRenderProcessHostIds();
  EXPECT_THAT(base::STLSetDifference<std::set<content::ChildProcessId>>(
                  starting_rph_ids, ending_rph_ids),
              testing::IsEmpty());
}

// The tests below exercise the desktop TabStripModel, which is not available
// on Android.
#if !BUILDFLAG(IS_ANDROID)

// Currently it is possible to load the NTP absent a TabModel in an app window
// context. Assert the NTP loads without a TabModel successfully.
// TODO(crbug.com/378475391): This can be removed once an owning TabModel is
// guaranteed to be always present and fixed for the lifetime of the NTP.
IN_PROC_BROWSER_TEST_F(WebUiNtpBrowserTest, LoadsSuccessfullyWithoutTabModel) {
  // Add a new about:blank tab to the browser tab strip.
  TabStripModel* tab_strip_model = browser()->GetTabStripModel();
  chrome::AddTabAt(browser(), GURL(url::kAboutBlankURL), 1, true);
  tabs::TabInterface* initial_tab = tab_strip_model->GetTabAtIndex(1);
  EXPECT_EQ(2, tab_strip_model->count());

  base::MockCallback<tabs::TabInterface::WillDetach> tab_detached_callback;
  base::CallbackListSubscription tab_subscription =
      initial_tab->RegisterWillDetach(tab_detached_callback.Get());
  WebContentsRemovedObserver removed_observer;
  tab_strip_model->AddObserver(&removed_observer);

  // Extract the about:blank tab WebContents while destroying its TabModel.
  EXPECT_CALL(tab_detached_callback,
              Run(tab_strip_model->GetTabAtIndex(1),
                  tabs::TabInterface::DetachReason::kDelete));
  std::unique_ptr<content::WebContents> extracted_contents =
      tab_strip_model->DetachWebContentsAtForInsertion(1);
  EXPECT_EQ(TabRemovedReason::kInsertedIntoOtherTabStrip,
            removed_observer.remove_reason());
  EXPECT_EQ(tabs::TabInterface::DetachReason::kDelete,
            removed_observer.tab_detach_reason());

  // Load the NTP into the detached tab. The NTP should load without crashing.
  EXPECT_TRUE(content::NavigateToURL(extracted_contents.get(),
                                     chrome::ChromeUINewTabURLAsGURL()));

  // Re-insert the tab into the tab strip.
  tab_strip_model->AppendWebContents(std::move(extracted_contents), true);
}

// Currently it is possible to load the NTP in an app window context that
// supports re-parenting the tab into a normal browser window. Assert the NTP
// supports such TabModel transitions.
// TODO(crbug.com/378475391): This can be removed once an owning TabModel is
// guaranteed to be always present and fixed for the lifetime of the NTP.
IN_PROC_BROWSER_TEST_F(WebUiNtpBrowserTest, HandlesTabModelChanges) {
  // Add a new NTP tab to the browser tab strip.
  TabStripModel* tab_strip_model = browser()->GetTabStripModel();
  chrome::AddTabAt(browser(), chrome::ChromeUINewTabURLAsGURL(), 1, true);
  tabs::TabInterface* initial_tab = tab_strip_model->GetTabAtIndex(1);
  EXPECT_EQ(2, tab_strip_model->count());

  base::MockCallback<tabs::TabInterface::WillDetach> tab_detached_callback;
  base::CallbackListSubscription tab_subscription =
      initial_tab->RegisterWillDetach(tab_detached_callback.Get());
  WebContentsRemovedObserver removed_observer;
  tab_strip_model->AddObserver(&removed_observer);

  // Extract the NTP WebContents while destroying its TabModel, simulating what
  // occurs during a browser-type transition.
  EXPECT_CALL(tab_detached_callback,
              Run(tab_strip_model->GetTabAtIndex(1),
                  tabs::TabInterface::DetachReason::kDelete));
  std::unique_ptr<content::WebContents> extracted_contents =
      tab_strip_model->DetachWebContentsAtForInsertion(1);
  EXPECT_EQ(TabRemovedReason::kInsertedIntoOtherTabStrip,
            removed_observer.remove_reason());
  EXPECT_EQ(tabs::TabInterface::DetachReason::kDelete,
            removed_observer.tab_detach_reason());

  // Re-insert the tab into the tab strip, creating a new TabModel.
  tab_strip_model->AppendWebContents(std::move(extracted_contents), true);
}
#endif  // !BUILDFLAG(IS_ANDROID)

// TODO(b:514161985): Enable on Android once enterprise shortcuts are
// implemented there.
#if !BUILDFLAG(IS_ANDROID)
class WebUiNtpEnterpriseShortcutsBrowserTest : public WebUiNtpBrowserTest {
 public:
  WebUiNtpEnterpriseShortcutsBrowserTest() = default;
};

// TODO(crbug.com/442038064): Re-enable once flakiness is addressed.
IN_PROC_BROWSER_TEST_F(WebUiNtpEnterpriseShortcutsBrowserTest,
                       DISABLED_EnterpriseShortcuts) {
  // 1. Set the user preference to use enterprise shortcuts.
  GetProfile()->GetPrefs()->SetBoolean(ntp_prefs::kNtpCustomLinksVisible,
                                       false);
  GetProfile()->GetPrefs()->SetBoolean(
      ntp_prefs::kNtpEnterpriseShortcutsVisible, true);

  // 2. Navigate to the New Tab Page.
  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(content::NavigateToURL(web_contents,
                                     chrome::ChromeUINewTabPageURLAsGURL()));

  // 3. Verify through JavaScript that the page is configured correctly.
  // - Enterprise shortcuts are displayed.
  const std::string js_check =
      R"((() => {
          const mostVisited = document.querySelector('ntp-app')?.shadowRoot
                                    ?.querySelector('cr-most-visited');
          if (!mostVisited) {
            return false;
          }
          const hasEnterprise =
              mostVisited.hasAttribute('enterprise-shortcuts-enabled_');
          const hasCustom =
              mostVisited.hasAttribute('custom-links-enabled_');
          return hasEnterprise === true && hasCustom === false;
        })())";

  ASSERT_TRUE(base::test::RunUntil(
      [&] { return content::EvalJs(web_contents, js_check).ExtractBool(); }));
}
#endif  // !BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
class WebUiNtpKeepAliveBrowserTest : public WebUiNtpBrowserTest {
 public:
  explicit WebUiNtpKeepAliveBrowserTest(bool enable = true) {
    // Disable the OneGoogleBar iframe so its cross-site
    // `chrome-untrusted://new-tab-page` subframe does not consume the spare
    // renderer.
    keep_alive_feature_list_.InitWithFeatureStates({
        {ntp_features::kKeepNtpRendererAlive, enable},
        {ntp_features::kNtpOneGoogleBar, false},
    });
  }

 private:
  base::test::ScopedFeatureList keep_alive_feature_list_;
};

class WebUiNtpKeepAliveDisabledBrowserTest
    : public WebUiNtpKeepAliveBrowserTest {
 public:
  WebUiNtpKeepAliveDisabledBrowserTest()
      : WebUiNtpKeepAliveBrowserTest(/*enable=*/false) {}
};

IN_PROC_BROWSER_TEST_F(WebUiNtpKeepAliveDisabledBrowserTest,
                       DoesNotKeepAliveWhenDisabled) {
  content::WebContents* tab = chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(content::NavigateToURL(tab, chrome::ChromeUINewTabURLAsGURL()));
  content::RenderProcessHost* ntp_rph =
      tab->GetPrimaryMainFrame()->GetProcess();
  EXPECT_EQ(ntp_rph->GetPendingReuseRefCountForTesting(), 0);

  content::RenderProcessHostWatcher exit_watcher(
      ntp_rph, content::RenderProcessHostWatcher::WATCH_FOR_HOST_DESTRUCTION);
  ASSERT_TRUE(content::NavigateToURL(
      tab, embedded_test_server()->GetURL("a.com", "/title1.html")));
  exit_watcher.Wait();
}

IN_PROC_BROWSER_TEST_F(WebUiNtpKeepAliveBrowserTest,
                       ReuseWarmProcessAcrossTabsAndWindows) {
  content::WebContents* tab = chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(content::NavigateToURL(tab, chrome::ChromeUINewTabURLAsGURL()));
  ExpectIsWebUiNtp(tab);
  content::RenderProcessHost* ntp_rph =
      tab->GetPrimaryMainFrame()->GetProcess();
  const content::ChildProcessId ntp_id = ntp_rph->GetID();
  EXPECT_EQ(ntp_rph->GetPendingReuseRefCountForTesting(), 1);

  // Navigating away keeps the NTP renderer alive with zero frames and does not
  // pin the destination site's renderer.
  content::RenderFrameDeletedObserver ntp_rfh_deleted(
      tab->GetPrimaryMainFrame());
  ASSERT_TRUE(content::NavigateToURL(
      tab, embedded_test_server()->GetURL("a.com", "/title1.html")));
  ntp_rfh_deleted.WaitUntilDeleted();
  content::RenderProcessHost* host = content::RenderProcessHost::FromID(ntp_id);
  ASSERT_NE(host, nullptr);
  EXPECT_TRUE(host->IsInitializedAndNotDead());
  EXPECT_EQ(host->GetRenderFrameHostCount(), 0);
  EXPECT_EQ(host->GetPendingReuseRefCountForTesting(), 1);
  EXPECT_EQ(tab->GetPrimaryMainFrame()
                ->GetProcess()
                ->GetPendingReuseRefCountForTesting(),
            0);

  // Opening a new NTP tab reuses the warm process without consuming the spare.
  content::SpareRenderProcessHostManager::Get().WarmupSpare(GetProfile());
  const std::vector<content::ChildProcessId> spare_ids =
      content::SpareRenderProcessHostManager::Get().GetSpareIds();
  ASSERT_FALSE(spare_ids.empty());
  content::WebContents* ntp2 = OpenNewTabPage();
  ASSERT_TRUE(ntp2);
  ExpectIsWebUiNtp(ntp2);
  EXPECT_EQ(ntp2->GetPrimaryMainFrame()->GetProcess()->GetID(), ntp_id);
  EXPECT_EQ(host->GetPendingReuseRefCountForTesting(), 1);
  EXPECT_EQ(content::SpareRenderProcessHostManager::Get().GetSpareIds(),
            spare_ids);

  // Closing the NTP tab leaves the process warm with zero frames, and opening
  // an NTP in a new browser window reuses it.
  content::RenderFrameDeletedObserver ntp2_rfh_deleted(
      ntp2->GetPrimaryMainFrame());
  browser()->GetTabStripModel()->CloseWebContentsAt(
      browser()->GetTabStripModel()->GetIndexOfWebContents(ntp2),
      TabCloseTypes::CLOSE_NONE);
  ntp2_rfh_deleted.WaitUntilDeleted();
  EXPECT_EQ(host->GetRenderFrameHostCount(), 0);
  EXPECT_EQ(host->GetPendingReuseRefCountForTesting(), 1);

  BrowserWindowInterface* window2 = CreateBrowser(GetProfile());
  content::WebContents* window2_tab =
      TabListInterface::From(window2)->GetActiveTab()->GetContents();
  ASSERT_TRUE(
      content::NavigateToURL(window2_tab, chrome::ChromeUINewTabURLAsGURL()));
  ExpectIsWebUiNtp(window2_tab);
  EXPECT_EQ(window2_tab->GetPrimaryMainFrame()->GetProcess()->GetID(), ntp_id);
}

IN_PROC_BROWSER_TEST_F(WebUiNtpKeepAliveBrowserTest,
                       ReleaseIdleProcessOnMemoryPressureAndRepinFreshHost) {
  content::WebContents* tab = chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(content::NavigateToURL(tab, chrome::ChromeUINewTabURLAsGURL()));
  content::RenderProcessHost* initial_rph =
      tab->GetPrimaryMainFrame()->GetProcess();
  const content::ChildProcessId initial_id = initial_rph->GetID();
  ASSERT_EQ(initial_rph->GetPendingReuseRefCountForTesting(), 1);

  // Navigate away so the pinned NTP process is idle with zero frames.
  content::RenderFrameDeletedObserver initial_rfh_deleted(
      tab->GetPrimaryMainFrame());
  ASSERT_TRUE(content::NavigateToURL(
      tab, embedded_test_server()->GetURL("a.com", "/title1.html")));
  initial_rfh_deleted.WaitUntilDeleted();
  ASSERT_EQ(initial_rph->GetRenderFrameHostCount(), 0);

  // Moderate pressure releases the pin and destroys the idle NTP process.
  content::RenderProcessHostWatcher destroy_watcher(
      initial_rph,
      content::RenderProcessHostWatcher::WATCH_FOR_HOST_DESTRUCTION);
  content::test::ScopedMemoryLimitOverride memory_limit("NtpRendererKeepAlive");
  memory_limit.SetLimit(base::MemoryLimit::ModeratePressureThreshold());
  memory_limit.NotifyReleaseMemory();
  destroy_watcher.Wait();
  EXPECT_EQ(content::RenderProcessHost::FromID(initial_id), nullptr);

  // Loading an NTP while still under pressure does not pin the new host.
  ASSERT_TRUE(content::NavigateToURL(tab, chrome::ChromeUINewTabURLAsGURL()));
  EXPECT_EQ(tab->GetPrimaryMainFrame()
                ->GetProcess()
                ->GetPendingReuseRefCountForTesting(),
            0);

  // Clearing pressure allows the next NTP load to lazily pin the fresh host.
  memory_limit.ClearLimit();
  ASSERT_TRUE(content::NavigateToURL(tab, chrome::ChromeUINewTabURLAsGURL()));
  content::RenderProcessHost* repinned_rph =
      tab->GetPrimaryMainFrame()->GetProcess();
  EXPECT_NE(repinned_rph->GetID(), initial_id);
  EXPECT_EQ(repinned_rph->GetPendingReuseRefCountForTesting(), 1);
}

IN_PROC_BROWSER_TEST_F(WebUiNtpKeepAliveBrowserTest,
                       ReleaseOnCrashAndReacquireOnReload) {
  content::WebContents* tab = chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(content::NavigateToURL(tab, chrome::ChromeUINewTabURLAsGURL()));
  content::RenderProcessHost* ntp_rph =
      tab->GetPrimaryMainFrame()->GetProcess();
  ASSERT_EQ(ntp_rph->GetPendingReuseRefCountForTesting(), 1);

  {
    content::ScopedAllowRendererCrashes allow_crashes(ntp_rph);
    content::CrashTab(tab);
  }
  EXPECT_EQ(ntp_rph->GetPendingReuseRefCountForTesting(), 0);

  ASSERT_TRUE(content::NavigateToURL(tab, chrome::ChromeUINewTabURLAsGURL()));
  EXPECT_EQ(tab->GetPrimaryMainFrame()
                ->GetProcess()
                ->GetPendingReuseRefCountForTesting(),
            1);
}

IN_PROC_BROWSER_TEST_F(WebUiNtpKeepAliveBrowserTest,
                       UnpinsWhenProcessLimitReached) {
  base::ScopedClosureRunner reset_limit(base::BindOnce(
      &content::RenderProcessHost::SetMaxRendererProcessCount, 0u));

  content::WebContents* tab = chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(content::NavigateToURL(tab, chrome::ChromeUINewTabURLAsGURL()));
  content::RenderProcessHost* ntp_rph =
      tab->GetPrimaryMainFrame()->GetProcess();
  ASSERT_EQ(ntp_rph->GetPendingReuseRefCountForTesting(), 1);

  // Lowering the max renderer count to 1 puts the browser at the process limit,
  // so the next NTP commit releases the keep-alive rather than retaining an
  // idle slot at the limit.
  content::RenderProcessHost::SetMaxRendererProcessCount(1);
  ASSERT_TRUE(content::RenderProcessHost::IsProcessLimitReached());
  ASSERT_TRUE(content::NavigateToURL(tab, chrome::ChromeUINewTabURLAsGURL()));
  EXPECT_EQ(tab->GetPrimaryMainFrame()
                ->GetProcess()
                ->GetPendingReuseRefCountForTesting(),
            0);
}

#if BUILDFLAG(ENABLE_WEBUI_GENERATE_CODE_CACHE)
struct BundledCodeCacheTestParams {
  const char* test_suffix;
  bool code_cache_enabled;
  bool lazy_load_composebox;
};

class WebUiNtpBundledCodeCacheBrowserTest
    : public WebUiNtpBrowserTest,
      public testing::WithParamInterface<BundledCodeCacheTestParams> {
 public:
  WebUiNtpBundledCodeCacheBrowserTest() {
    // Disable `kInitialWebUI` so async TopChrome InitialWebUI loads at browser
    // startup do not record into the code-cache histograms asserted below.
    scoped_feature_list_.InitWithFeatureStates({
        {features::kInitialWebUI, false},
        {features::kWebUIBundledCodeCache, true},
        {features::kWebUIBundledCodeCacheForNewTabPage,
         GetParam().code_cache_enabled},
        {ntp_features::kNtpLazyLoadComposebox, GetParam().lazy_load_composebox},
    });
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(WebUiNtpBundledCodeCacheBrowserTest,
                       SuccessfullyLoadsCodeCache) {
  static constexpr char kDidReceiveCachedCodeHistogram[] =
      "Blink.ResourceRequest.WebUIBundledCodeCacheFetcher.DidReceiveCachedCode";
  static constexpr char kConsumeCacheHistogram[] =
      "Blink.ResourceRequest.WebUIBundledCachedMetadataHandler.ConsumeCache";

  // Every NTP bundle chunk maps to a code cache only when the feature is on.
  const base::flat_map<GURL, int> code_cache_map =
      webui::GetWebUIResourceUrlToCodeCacheMap();
  for (const char* script_url : {"chrome://new-tab-page/new_tab_page.js",
                                 "chrome://new-tab-page/lazy_load.js",
                                 "chrome://new-tab-page/shared.rollup.js"}) {
    EXPECT_EQ(GetParam().code_cache_enabled,
              code_cache_map.contains(GURL(script_url)))
        << script_url;
  }

  auto fetch_histograms = [] {
    content::FetchHistogramsFromChildProcesses();
    metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  };
  // Count only the samples recorded by the navigation below.
  fetch_histograms();
  base::HistogramTester histogram_tester;
  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(content::NavigateToURL(web_contents,
                                     chrome::ChromeUINewTabPageURLAsGURL()));
  ExpectIsWebUiNtp(web_contents);
  // Wait for deferred `lazy_load.js` execution so all 3 NTP chunks are tested.
  ASSERT_TRUE(content::ExecJs(
      web_contents, "customElements.whenDefined('ntp-lens-upload-dialog');"));
  fetch_histograms();

  // V8 rejects a cache built from different source than the served bundle,
  // e.g. on a lazy/eager mismatch or a build-time $i18n{} replacement.
  histogram_tester.ExpectBucketCount(kDidReceiveCachedCodeHistogram, false, 0);
  histogram_tester.ExpectBucketCount(kConsumeCacheHistogram, false, 0);
  if (GetParam().code_cache_enabled) {
    EXPECT_GE(
        histogram_tester.GetBucketCount(kDidReceiveCachedCodeHistogram, true),
        4);
    EXPECT_GE(histogram_tester.GetBucketCount(kConsumeCacheHistogram, true), 4);
  } else {
    histogram_tester.ExpectTotalCount(kDidReceiveCachedCodeHistogram, 0);
    histogram_tester.ExpectTotalCount(kConsumeCacheHistogram, 0);
  }
}

INSTANTIATE_TEST_SUITE_P(
    ,
    WebUiNtpBundledCodeCacheBrowserTest,
    testing::Values(
        BundledCodeCacheTestParams{"NtpCodeCacheEnabled_LazyLoadComposebox",
                                   true, true},
        BundledCodeCacheTestParams{"NtpCodeCacheEnabled_EagerComposebox", true,
                                   false},
        BundledCodeCacheTestParams{"NtpCodeCacheDisabled_EagerComposebox",
                                   false, false}),
    [](const testing::TestParamInfo<BundledCodeCacheTestParams>& info) {
      return info.param.test_suffix;
    });
#endif  // BUILDFLAG(ENABLE_WEBUI_GENERATE_CODE_CACHE)
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace
