// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/search/instant_test_base.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class ThirdPartyNTPBrowserTest : public InProcessBrowserTest,
                                 public InstantTestBase {
 public:
  ThirdPartyNTPBrowserTest() = default;

  ThirdPartyNTPBrowserTest(const ThirdPartyNTPBrowserTest&) = delete;
  ThirdPartyNTPBrowserTest& operator=(const ThirdPartyNTPBrowserTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(https_test_server().Start());
  }
};

// Verifies that a third party NTP can successfully embed the most visited
// iframe.
IN_PROC_BROWSER_TEST_F(ThirdPartyNTPBrowserTest, EmbeddedMostVisitedIframe) {
  GURL base_url =
      https_test_server().GetURL("ntp.com", "/instant_extended.html");
  GURL ntp_url =
      https_test_server().GetURL("ntp.com", "/instant_extended_ntp.html");
  SetupInstant(browser()->profile(), base_url, ntp_url);

  // Navigate to the NTP URL and verify that the resulting process is marked as
  // an Instant process.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), ntp_url));
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(browser()->profile());
  EXPECT_TRUE(instant_service->IsInstantProcess(
      contents->GetPrimaryMainFrame()->GetProcess()->GetID()));

  // Add a chrome-search://most-visited/title.html?rid=1&fs=0 subframe and
  // verify that navigation completes successfully, with no kills.
  content::TestNavigationObserver nav_observer(contents);
  const char* kScript = R"(
      const frame = document.createElement('iframe');
      frame.src = 'chrome-search://most-visited/title.html?rid=1&fs=0';
      document.body.appendChild(frame);
  )";
  ASSERT_TRUE(content::ExecJs(contents, kScript));
  nav_observer.WaitForNavigationFinished();

  // Verify that the subframe exists and has the expected origin.
  content::RenderFrameHost* subframe = ChildFrameAt(contents, 0);
  ASSERT_TRUE(subframe);
  EXPECT_EQ("chrome-search://most-visited",
            content::EvalJs(subframe, "window.origin"));
}

// Verifies that Chrome won't spawn a separate renderer process for
// every single NTP tab.  This behavior goes all the way back to
// the initial commit [1] which achieved that behavior by forcing
// process-per-site mode for NTP tabs.  It seems desirable to preserve this
// behavior going forward.
//
// [1] https://chromium.googlesource.com/chromium/src/+/09911bf300f1a419907a9412154760efd0b7abc3/chrome/browser/browsing_instance.cc#55
IN_PROC_BROWSER_TEST_F(ThirdPartyNTPBrowserTest, ProcessPerSite) {
  GURL base_url =
      https_test_server().GetURL("ntp.com", "/instant_extended.html");
  GURL ntp_url =
      https_test_server().GetURL("ntp.com", "/instant_extended_ntp.html");
  SetupInstant(browser()->profile(), base_url, ntp_url);

  // Open NTP in |tab1|.
  content::WebContents* tab1;
  {
    content::WebContentsAddedObserver tab1_observer;

    // Try to simulate as closely as possible what would have happened in the
    // real user interaction.
    chrome::NewTab(browser());

    // Wait for the new tab.
    tab1 = tab1_observer.GetWebContents();
    ASSERT_TRUE(WaitForLoadStop(tab1));

    // Sanity check: the NTP should be provided by |ntp_url| and not by
    // chrome://new-tab-page [1P WebUI NTP] or chrome://newtab [incognito].
    EXPECT_EQ(ntp_url, content::EvalJs(tab1, "window.location.href"));
  }

  // Open another NTP in |tab2|.
  content::WebContents* tab2;
  {
    content::WebContentsAddedObserver tab2_observer;
    chrome::NewTab(browser());
    tab2 = tab2_observer.GetWebContents();
    ASSERT_TRUE(WaitForLoadStop(tab2));
    EXPECT_EQ(ntp_url, content::EvalJs(tab2, "window.location.href"));
  }

  // Verify that |tab1| and |tab2| share a process.
  EXPECT_EQ(tab1->GetPrimaryMainFrame()->GetProcess(),
            tab2->GetPrimaryMainFrame()->GetProcess());
}

// Verify that a third-party NTP commits in a remote NTP SiteInstance.
IN_PROC_BROWSER_TEST_F(ThirdPartyNTPBrowserTest, VerifySiteInstance) {
  // Setup and navigate to third-party NTP.
  GURL base_url =
      https_test_server().GetURL("ntp.com", "/instant_extended.html");
  GURL ntp_url =
      https_test_server().GetURL("ntp.com", "/instant_extended_ntp.html");
  SetupInstant(browser()->profile(), base_url, ntp_url);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), ntp_url));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Sanity check: the NTP should be provided by |ntp_url| and not by
  // chrome://new-tab-page [1P WebUI NTP] or chrome://newtab [incognito].
  EXPECT_EQ(ntp_url, content::EvalJs(web_contents, "window.location.href"));

  // Verify that NTP committed in remote NTP SiteInstance.
  EXPECT_EQ(
      GURL("chrome-search://remote-ntp/"),
      web_contents->GetPrimaryMainFrame()->GetSiteInstance()->GetSiteURL());
}
