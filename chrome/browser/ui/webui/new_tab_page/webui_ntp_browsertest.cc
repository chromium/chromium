// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>
#include <vector>

#include "base/stl_util.h"
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

#if BUILDFLAG(IS_ANDROID)
#include "base/test/scoped_feature_list.h"
#include "content/public/common/content_features.h"
#else
#include "base/test/mock_callback.h"
#include "base/test/run_until.h"
#include "chrome/browser/new_tab_page/prefs/ntp_pref_names.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/prefs/pref_service.h"
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

}  // namespace

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

// Verify that the WebUI NTP uses an available spare process and does not
// discard it as in https://crbug.com/40699271.
IN_PROC_BROWSER_TEST_F(WebUiNtpBrowserTest, SpareRenderer) {
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
