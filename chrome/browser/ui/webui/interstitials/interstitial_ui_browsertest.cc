// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "ui/base/l10n/l10n_util.h"

class InterstitialUITest : public InProcessBrowserTest {
 public:
  InterstitialUITest() {}
  ~InterstitialUITest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);

#if BUILDFLAG(IS_CHROMEOS)
    // These tests use chrome:// URLs and are written on the assumption devtools
    // are always available, so guarantee that assumption holds. Tests that
    // check if devtools can be disabled should use a test fixture without the
    // kForceDevToolsAvailable switch set.
    command_line->AppendSwitch(switches::kForceDevToolsAvailable);
#endif
  }

 protected:
  // Tests interstitial displayed at url to verify that it has the given
  // page title and body content that is expected.
  //
  // page_title must be an exact match, while body content may appear anywhere
  // in the rendered page. Thus an empty body_text never fails.
  void TestInterstitial(GURL url,
                        const std::string& page_title,
                        const std::u16string& body_text) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    EXPECT_EQ(base::ASCIIToUTF16(page_title),
              browser()->tab_strip_model()->GetActiveWebContents()->GetTitle());

    // Should also be able to open and close devtools.
    DevToolsWindow* window =
        DevToolsWindowTesting::OpenDevToolsWindowSync(browser(), true);
    EXPECT_TRUE(window);
    DevToolsWindowTesting::CloseDevToolsWindowSync(window);

    if (body_text.empty()) {
      return;
    }

    content::WebContents* contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    EXPECT_GE(ui_test_utils::FindInPage(contents, body_text, true, true,
                                        nullptr, nullptr),
              1);
  }

  // Convenience function to test interstitial pages without provided body_text.
  void TestInterstitial(GURL url, const std::string& page_title) {
    TestInterstitial(url, page_title, std::u16string());
  }

  // Convenience function to test interstitial pages with l10n message_ids as
  // body_text strings.
  void TestInterstitial(GURL url,
                        const std::string& page_title,
                        int message_id) {
    TestInterstitial(url, page_title, l10n_util::GetStringUTF16(message_id));
  }
};

IN_PROC_BROWSER_TEST_F(InterstitialUITest, HomePage) {
  TestInterstitial(GURL("chrome://interstitials"), "Interstitials");
}

IN_PROC_BROWSER_TEST_F(InterstitialUITest, InvalidURLShouldOpenHomePage) {
  // Invalid path should open the main page:
  TestInterstitial(GURL("chrome://interstitials/--invalid--"), "Interstitials");
}

IN_PROC_BROWSER_TEST_F(InterstitialUITest,
                       InvalidURLMatchingStartOfValidURLShouldBeInvalid) {
  // Path that matches the first characters of another should be invalid
  // (and therefore open the main page).
  TestInterstitial(GURL("chrome://interstitials/ssl--invalid--"),
                   "Interstitials");
}

IN_PROC_BROWSER_TEST_F(InterstitialUITest, SSLInterstitial) {
  TestInterstitial(GURL("chrome://interstitials/ssl"), "Privacy error",
                   IDS_SSL_V2_HEADING);
}

IN_PROC_BROWSER_TEST_F(InterstitialUITest, MITMSoftwareInterstitial) {
  TestInterstitial(GURL("chrome://interstitials/mitm-software-ssl"),
                   "Privacy error", IDS_MITM_SOFTWARE_HEADING);
}

IN_PROC_BROWSER_TEST_F(InterstitialUITest, PinnedCertInterstitial) {
  TestInterstitial(GURL("chrome://interstitials/ssl?type=hpkp_failure"),
                   "Privacy error",
                   u"NET::ERR_SSL_PINNED_KEY_NOT_IN_CERT_CHAIN");
}

IN_PROC_BROWSER_TEST_F(InterstitialUITest, CTInterstitial) {
  TestInterstitial(GURL("chrome://interstitials/ssl?type=ct_failure"),
                   "Privacy error",
                   u"NET::ERR_CERTIFICATE_TRANSPARENCY_REQUIRED");
}

IN_PROC_BROWSER_TEST_F(InterstitialUITest, EnterpriseBlockInterstitial) {
  TestInterstitial(GURL("chrome://interstitials/enterprise-block"),
                   "Blocked by Admin", IDS_ENTERPRISE_BLOCK_HEADING);
}

IN_PROC_BROWSER_TEST_F(InterstitialUITest, EnterpriseWarnInterstitial) {
  TestInterstitial(GURL("chrome://interstitials/enterprise-warn"),
                   "Admin warning", IDS_ENTERPRISE_WARN_HEADING);
}

IN_PROC_BROWSER_TEST_F(InterstitialUITest, MalwareInterstitial) {
  TestInterstitial(GURL("chrome://interstitials/safebrowsing?type=malware"),
                   "Security error", IDS_SAFEBROWSING_HEADING);
}

IN_PROC_BROWSER_TEST_F(InterstitialUITest, PhishingInterstitial) {
  TestInterstitial(GURL("chrome://interstitials/safebrowsing?type=phishing"),
                   "Security error", IDS_SAFEBROWSING_HEADING);
}

IN_PROC_BROWSER_TEST_F(InterstitialUITest, UnwantedSoftwareInterstitial) {
  TestInterstitial(GURL("chrome://interstitials/safebrowsing?type=unwanted"),
                   "Security error", IDS_SAFEBROWSING_HEADING);
}

IN_PROC_BROWSER_TEST_F(InterstitialUITest, MalwareInterstitialQuiet) {
  TestInterstitial(
      GURL("chrome://interstitials/quietsafebrowsing?type=malware"),
      "Security error", IDS_MALWARE_WEBVIEW_HEADING);
}

IN_PROC_BROWSER_TEST_F(InterstitialUITest, PhishingInterstitialQuiet) {
  TestInterstitial(
      GURL("chrome://interstitials/quietsafebrowsing?type=phishing"),
      "Security error", IDS_PHISHING_WEBVIEW_HEADING);
}

IN_PROC_BROWSER_TEST_F(InterstitialUITest, UnwantedSoftwareInterstitialQuiet) {
  TestInterstitial(
      GURL("chrome://interstitials/quietsafebrowsing?type=unwanted"),
      "Security error", IDS_HARMFUL_WEBVIEW_HEADING);
}

IN_PROC_BROWSER_TEST_F(InterstitialUITest, BillingInterstitialQuiet) {
  TestInterstitial(
      GURL("chrome://interstitials/quietsafebrowsing?type=billing"),
      "Page may charge money", IDS_BILLING_WEBVIEW_HEADING);
}

IN_PROC_BROWSER_TEST_F(InterstitialUITest, ClientsideMalwareInterstitial) {
  TestInterstitial(
      GURL("chrome://interstitials/safebrowsing?type=clientside_malware"),
      "Security error", IDS_SAFEBROWSING_HEADING);
}

IN_PROC_BROWSER_TEST_F(InterstitialUITest, ClientsidePhishingInterstitial) {
  TestInterstitial(
      GURL("chrome://interstitials/safebrowsing?type=clientside_phishing"),
      "Security error", IDS_SAFEBROWSING_HEADING);
}

IN_PROC_BROWSER_TEST_F(InterstitialUITest, BillingInterstitial) {
  TestInterstitial(GURL("chrome://interstitials/safebrowsing?type=billing"),
                   "Page may charge money", IDS_BILLING_HEADING);
}

IN_PROC_BROWSER_TEST_F(InterstitialUITest, CaptivePortalInterstitial) {
  TestInterstitial(GURL("chrome://interstitials/captiveportal"),
                   "Connect to network");
}

IN_PROC_BROWSER_TEST_F(InterstitialUITest, CaptivePortalInterstitialWifi) {
  TestInterstitial(GURL("chrome://interstitials/captiveportal?is_wifi=1"),
                   "Connect to Wi-Fi");
}

IN_PROC_BROWSER_TEST_F(InterstitialUITest, BlockedInterceptionInterstitial) {
  TestInterstitial(GURL("chrome://interstitials/blocked-interception"),
                   "Your activity on example.com is being monitored",
                   u"Anything you type");
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
// Tests that the interstitials have the expected title and content.
IN_PROC_BROWSER_TEST_F(InterstitialUITest,
                       SupervisedUserVerificationInterstitial) {
  TestInterstitial(GURL("chrome://interstitials/supervised-user-verify"),
                   "YouTube", IDS_SUPERVISED_USER_VERIFY_PAGE_PRIMARY_HEADING);
}

IN_PROC_BROWSER_TEST_F(InterstitialUITest,
                       SupervisedUserVerificationBlockedSiteInterstitial) {
  TestInterstitial(
      GURL("chrome://interstitials/supervised-user-verify-blocked-site"),
      "Site blocked", IDS_CHILD_BLOCK_INTERSTITIAL_MESSAGE_NOT_SIGNED_IN);
}

IN_PROC_BROWSER_TEST_F(InterstitialUITest,
                       SupervisedUserVerificationSubframeInterstitial) {
  TestInterstitial(
      GURL("chrome://interstitials/supervised-user-verify-subframe"), "YouTube",
      IDS_SUPERVISED_USER_VERIFY_PAGE_SUBFRAME_YOUTUBE_HEADING);
}

IN_PROC_BROWSER_TEST_F(
    InterstitialUITest,
    SupervisedUserVerificationBlockedSiteSubframeInterstitial) {
  TestInterstitial(
      GURL("chrome://interstitials/"
           "supervised-user-verify-blocked-site-subframe"),
      "Site blocked",
      IDS_SUPERVISED_USER_VERIFY_PAGE_SUBFRAME_BLOCKED_SITE_HEADING);
}
#endif

// Tests that back button works after opening an interstitial from
// chrome://interstitials.
IN_PROC_BROWSER_TEST_F(InterstitialUITest, InterstitialBackButton) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("chrome://interstitials")));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL("chrome://interstitials/ssl")));
  content::TestNavigationObserver navigation_observer(web_contents);
  chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
  navigation_observer.Wait();
  std::u16string title;
  ui_test_utils::GetCurrentTabTitle(browser(), &title);
  EXPECT_EQ(title, u"Interstitials");
}

// Tests that view-source: works correctly on chrome://interstitials.
IN_PROC_BROWSER_TEST_F(InterstitialUITest, InterstitialViewSource) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("view-source:chrome://interstitials/")));
  int found;
  std::u16string expected_title = u"<title>Interstitials</title>";
  found = ui_test_utils::FindInPage(
      browser()->tab_strip_model()->GetActiveWebContents(), expected_title,
      true, /* Forward */
      true, /* case_sensitive */
      nullptr, nullptr);
  EXPECT_EQ(found, 1);
}

// Tests that view-source: works correctly on a subpage of
// chrome://interstitials (using chrome://interstitials/ssl).

// Test is currently flaky on Windows (crbug.com/926392)
#if BUILDFLAG(IS_WIN)
#define MAYBE_InterstitialWithPathViewSource \
  DISABLED_InterstitialWithPathViewSource
#else
#define MAYBE_InterstitialWithPathViewSource InterstitialWithPathViewSource
#endif

IN_PROC_BROWSER_TEST_F(InterstitialUITest,
                       MAYBE_InterstitialWithPathViewSource) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("view-source:chrome://interstitials/ssl")));
  int found;
  std::u16string expected_title = u"<title>Privacy error</title";
  found = ui_test_utils::FindInPage(
      browser()->tab_strip_model()->GetActiveWebContents(), expected_title,
      true, /* Forward */
      true, /* case_sensitive */
      nullptr, nullptr);
  EXPECT_EQ(found, 1);
}

// Checks that the interstitial page uses correct web contents. If not, closing
// the tab might result in a freed web contents pointer and cause a crash.
// See https://crbug.com/611706 for details.
IN_PROC_BROWSER_TEST_F(InterstitialUITest, UseCorrectWebContents) {
  int current_tab = browser()->tab_strip_model()->active_index();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL("chrome://interstitials/ssl")));
  // Duplicate the tab and close it.
  chrome::DuplicateTab(browser());
  EXPECT_NE(current_tab, browser()->tab_strip_model()->active_index());
  chrome::CloseTab(browser());
  EXPECT_EQ(current_tab, browser()->tab_strip_model()->active_index());

  // Reloading the page shouldn't cause a crash.
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
}
