// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>

#include "base/containers/contains.h"
#include "base/stl_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/search/ntp_features.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/spare_render_process_host_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"

namespace {

void ExpectIsWebUiNtp(content::WebContents* tab) {
  EXPECT_EQ(GURL(chrome::kChromeUINewTabPageURL).spec(),
            EvalJs(tab, "window.location.href",
                   content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, /*world_id=*/1));
}

std::set<int> LiveRenderProcessHostIds() {
  std::set<int> result;
  for (auto iter = content::RenderProcessHost::AllHostsIterator();
       !iter.IsAtEnd(); iter.Advance()) {
    result.insert(iter.GetCurrentKey());
  }
  return result;
}

}  // namespace

class WebUiNtpBrowserTest : public InProcessBrowserTest {
 public:
  WebUiNtpBrowserTest() = default;

  ~WebUiNtpBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->StartAcceptingConnections();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Verify that the WebUI NTP commits in a SiteInstance with the WebUI URL.
IN_PROC_BROWSER_TEST_F(WebUiNtpBrowserTest, VerifySiteInstance) {
  GURL ntp_url(chrome::kChromeUINewTabURL);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), ntp_url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(ntp_url, web_contents->GetLastCommittedURL());

  GURL webui_ntp_url(chrome::kChromeUINewTabPageURL);
  ASSERT_EQ(
      webui_ntp_url,
      web_contents->GetPrimaryMainFrame()->GetSiteInstance()->GetSiteURL());
}

// Verify that the WebUI NTP uses process-per-site.
IN_PROC_BROWSER_TEST_F(WebUiNtpBrowserTest, ProcessPerSite) {
  std::vector<content::WebContents*> tabs;

  // Open a few NTPs.
  for (size_t i = 0; i < 3; i++) {
    content::WebContentsAddedObserver tab_observer;
    chrome::NewTab(browser());

    // Wait for the new tab.
    auto* tab = tab_observer.GetWebContents();
    ASSERT_TRUE(WaitForLoadStop(tab));

    // Sanity check: the NTP should be a WebUI NTP (and not chrome://newtab/ or
    // some other NTP).
    ExpectIsWebUiNtp(tab);

    tabs.push_back(tab);
  }

  // Verify that all NTPs share a process.
  for (size_t i = 1; i < tabs.size(); i++) {
    EXPECT_EQ(tabs[0]->GetPrimaryMainFrame()->GetProcess(),
              tabs[i]->GetPrimaryMainFrame()->GetProcess());
  }
}

// Verify that the WebUI NTP uses an available spare process and does not
// discard it as in https://crbug.com/1094088.
IN_PROC_BROWSER_TEST_F(WebUiNtpBrowserTest, SpareRenderer) {
  // Capture current spare renderer.
  std::vector<int> spare_ids_before_navigation =
      content::SpareRenderProcessHostManager::Get().GetSpareIds();
  ASSERT_FALSE(spare_ids_before_navigation.empty());

  // Note the current render processes before the navigation. These should all
  // remain alive after the navigation.
  const std::set<int> starting_rph_ids = LiveRenderProcessHostIds();

  // Open an NTP.
  chrome::NewTab(browser());
  auto* ntp = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(WaitForLoadStop(ntp));
  ExpectIsWebUiNtp(ntp);

  // Check spare was taken.
  EXPECT_TRUE(
      base::Contains(spare_ids_before_navigation,
                     ntp->GetPrimaryMainFrame()->GetProcess()->GetID()));

  // No processes should be unnecessarily terminated.
  const std::set<int> ending_rph_ids = LiveRenderProcessHostIds();
  const std::set<int> terminated_rph_ids =
      base::STLSetDifference<std::set<int>>(starting_rph_ids, ending_rph_ids);
  EXPECT_TRUE(terminated_rph_ids.empty());
}
