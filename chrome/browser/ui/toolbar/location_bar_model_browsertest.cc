// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/location_bar_model.h"

#include <stddef.h>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/search/instant_test_base.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/omnibox/common/omnibox_features.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/text_elider.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)

#include "extensions/browser/extension_registrar.h"
#include "extensions/common/extension_builder.h"
#endif

// Test data ------------------------------------------------------------------

namespace {

struct TestItem {
  GURL url;
  const std::string expected_formatted_full_url;
  const std::string expected_elided_url_for_display =
      expected_formatted_full_url;
};

const std::vector<TestItem>& TestItems() {
  static base::NoDestructor<std::vector<TestItem>> items{{
      {
          GURL("view-source:http://a.test"),
          "view-source:a.test",
      },
      {
          chrome::ChromeUINewTabURLAsGURL(),
          "",
      },
      // After executing the associated JS code, the "javascript:" scheme
      // will cause the address in the location bar to revert to whatever
      // it was prior to execution of the JS code (i.e. the URL of the
      // previous test case)
      {
          GURL("javascript:alert(1);"),
          "",
      },
      {
          GURL(std::string("view-source:") + chrome::kChromeUINewTabURL),
          "view-source:" +
              content::GetWebUIURLString(chrome::kChromeUINewTabHost),
      },
      {
          GURL("chrome-extension://fooooooooooooooooooooooooooooooo/bar.html"),
          "chrome-extension://fooooooooooooooooooooooooooooooo/bar.html",
      },
      {
          GURL(url::kAboutBlankURL),
          url::kAboutBlankURL,
      },
      {
          GURL("http://searchurl/?q=tractor+supply"),
          "searchurl/?q=tractor+supply",
      },
      {
          GURL("http://www.a.test/search?q=tractor+supply"),
          "www.a.test/search?q=tractor+supply",
          "a.test/search?q=tractor+supply",
      },
      {
          GURL("https://www.google.com/search?q=tractor+supply"),
          "https://www.google.com/search?q=tractor+supply",
          "google.com/search?q=tractor+supply",
      },
      {
          GURL("https://m.google.ca/search?q=tractor+supply"),
          "https://m.google.ca/search?q=tractor+supply",
          "m.google.ca/search?q=tractor+supply",
      },
      {
          GURL("http://m.a.test/search?q=tractor+supply"),
          "m.a.test/search?q=tractor+supply",
      },
      {
          GURL("http://en.a.test"),
          "en.a.test",
      },
      {
          GURL("https://en.wikipedia.org"),
          "https://en.wikipedia.org",
          "en.wikipedia.org",
      },
      {
          GURL("http://www3.a.test/nhkworld"),
          "www3.a.test/nhkworld",
      },
      {
          GURL("https://www3.nhk.or.jp/nhkworld"),
          "https://www3.nhk.or.jp/nhkworld",
          "www3.nhk.or.jp/nhkworld",
      },
#if BUILDFLAG(IS_WIN)
      {
          GURL("file:///c:/path/to/file"),
          "file:///C:/path/to/file",
          "C:/path/to/file",
      },
#else
      {
          GURL("file:///path/to/file"),
          "file:///path/to/file",
          "/path/to/file",
      },
#endif
      {
          GURL("data:text/plain;base64,SGVsbG8sIFdvcmxkIQ=="),
          "data:text/plain;base64,SGVsbG8sIFdvcmxkIQ==",
      },
  }};
  return *items;
}

}  // namespace

// LocationBarModelTest
// -----------------------------------------------------------

class LocationBarModelTest : public InProcessBrowserTest {
 public:
  LocationBarModelTest();

  LocationBarModelTest(const LocationBarModelTest&) = delete;
  LocationBarModelTest& operator=(const LocationBarModelTest&) = delete;

  ~LocationBarModelTest() override;

  // BrowserWithTestWindowTest:
  void SetUpOnMainThread() override;

 protected:
  void NavigateAndCheckText(
      const GURL& url,
      const std::u16string& expected_formatted_full_url,
      const std::u16string& expected_elided_url_for_display);
  void NavigateAndCheckElided(const GURL& https_url);
};

LocationBarModelTest::LocationBarModelTest() = default;

LocationBarModelTest::~LocationBarModelTest() = default;

void LocationBarModelTest::SetUpOnMainThread() {
  InProcessBrowserTest::SetUpOnMainThread();

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Install a fake extension so that the ID in the chrome-extension test URL is
  // valid. Invalid extension URLs may result in error pages (if blocked by
  // ExtensionNavigationThrottle), which this test doesn't wish to exercise.
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("Test")
          .SetID("fooooooooooooooooooooooooooooooo")
          .Build();
  extensions::ExtensionRegistrar::Get(browser()->profile())
      ->AddExtension(extension);
#endif
}

void LocationBarModelTest::NavigateAndCheckText(
    const GURL& url,
    const std::u16string& expected_formatted_full_url,
    const std::u16string& expected_elided_url_for_display) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::NavigationController* controller = &web_contents->GetController();

  // Use PAGE_TRANSITION_LINK to avoid auto-upgrading HTTP to HTTPS in typed
  // navigations
  controller->LoadURL(url, content::Referrer(), ui::PAGE_TRANSITION_LINK,
                      std::string());

  LocationBarModel* location_bar_model =
      browser()->GetFeatures().location_bar_model();

  if (!url.SchemeIs(url::kJavaScriptScheme)) {
    content::WaitForLoadStop(web_contents);
  }

  EXPECT_EQ(expected_formatted_full_url,
            location_bar_model->GetFormattedFullURL())
      << " URL: " << url;
  EXPECT_NE(expected_formatted_full_url.empty(),
            location_bar_model->ShouldDisplayURL())
      << " URL: " << url;
  EXPECT_EQ(expected_elided_url_for_display,
            location_bar_model->GetURLForDisplay())
      << " URL: " << url;
}

void LocationBarModelTest::NavigateAndCheckElided(const GURL& url) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::NavigationController* controller = &web_contents->GetController();

  controller->LoadURL(url, content::Referrer(), ui::PAGE_TRANSITION_LINK,
                      std::string());
  content::WaitForLoadStop(web_contents);

  LocationBarModel* location_bar_model =
      browser()->GetFeatures().location_bar_model();

  const std::u16string formatted_full_url_after(
      location_bar_model->GetFormattedFullURL());
  EXPECT_LT(formatted_full_url_after.size(), url.spec().size())
      << " URL: " << url;
  EXPECT_TRUE(base::EndsWith(formatted_full_url_after,
                             std::u16string(gfx::kEllipsisUTF16),
                             base::CompareCase::SENSITIVE));

  const std::u16string display_url_after(
      location_bar_model->GetURLForDisplay());
  EXPECT_LT(display_url_after.size(), url.spec().size());
  EXPECT_TRUE(base::EndsWith(display_url_after,
                             std::u16string(gfx::kEllipsisUTF16),
                             base::CompareCase::SENSITIVE));
}

// Actual tests ---------------------------------------------------------------

// Test URL display.
IN_PROC_BROWSER_TEST_F(LocationBarModelTest, ShouldDisplayURL) {
  for (const TestItem& test_item : TestItems()) {
    NavigateAndCheckText(
        test_item.url,
        base::ASCIIToUTF16(test_item.expected_formatted_full_url),
        base::ASCIIToUTF16(test_item.expected_elided_url_for_display));
  }
}

IN_PROC_BROWSER_TEST_F(LocationBarModelTest, ShouldElideLongURLs) {
  const std::string long_text(content::kMaxURLDisplayChars + 1024, '0');
  NavigateAndCheckElided(
      GURL(std::string("https://www.foo.com/?") + long_text));
  NavigateAndCheckElided(GURL(std::string("data:abc") + long_text));
}

// Regression tests for crbug.com/40553422 (and crbug.com/517847434).
//
// Chromium supports two NTP architectures and the omnibox-text refresh
// behavior on navigation differs between them, so both need coverage:
//
//   - WebUI NTP (e.g., chrome://new-tab-page): renderer-initiated link
//     clicks from this scheme are forked to the browser via OpenURL.
//   - Remote NTP (custom search provider whose new_tab_url is served at
//     a non-WebUI scheme such as HTTPS): renderer-initiated link clicks
//     stay on the regular BeginNavigation IPC path.
//
// The two tests below exercise each architecture. Both should leave the
// omnibox showing the destination URL while the navigation is pending.

IN_PROC_BROWSER_TEST_F(LocationBarModelTest,
                       ShouldDisplayURLWhileNavigatingAwayFromWebUiNTP) {
  ASSERT_TRUE(embedded_test_server()->Start());

  LocationBarModel* location_bar_model =
      browser()->GetFeatures().location_bar_model();

  // Open an NTP. Its URL should not be displayed.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("chrome://newtab")));
  ASSERT_FALSE(location_bar_model->ShouldDisplayURL());
  ASSERT_TRUE(location_bar_model->GetFormattedFullURL().empty());

  OmniboxView* omnibox_view =
      browser()->window()->GetLocationBar()->GetOmniboxView();
  ASSERT_TRUE(omnibox_view);

  content::WebContents* ntp_tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Inject and click an anchor in the NTP renderer. /hung keeps the
  // pending navigation open so we can inspect the omnibox before commit.
  GURL slow_url(embedded_test_server()->GetURL("/hung"));
  const char* kNavScriptTemplate = R"(
      var a = document.createElement('a');
      a.href = $1;
      a.innerText = 'Simulated most-visited link';
      document.body.appendChild(a);
      a.click();
  )";
  content::TestNavigationManager nav_manager(ntp_tab, slow_url);
  ASSERT_TRUE(content::ExecJs(
      ntp_tab, content::JsReplace(kNavScriptTemplate, slow_url)));
  ASSERT_TRUE(nav_manager.WaitForRequestStart());

  // The visible entry should now point at slow_url; the NTP entry is the
  // last committed one.
  content::NavigationEntry* pending_entry =
      ntp_tab->GetController().GetPendingEntry();
  ASSERT_TRUE(pending_entry);
  EXPECT_EQ(slow_url, pending_entry->GetURL());

  // LocationBarModel::GetFormattedFullURL() reads live from the visible
  // entry. Assert structurally because the model may strip the scheme
  // depending on platform and feature state.
  EXPECT_TRUE(location_bar_model->ShouldDisplayURL());
  std::string formatted_url =
      base::UTF16ToUTF8(location_bar_model->GetFormattedFullURL());
  EXPECT_THAT(formatted_url, ::testing::HasSubstr(slow_url.host()));
  EXPECT_THAT(formatted_url, ::testing::EndsWith(slow_url.path()));

  // OmniboxView::GetText() is the user-visible textfield contents.
  std::string omnibox_text = base::UTF16ToUTF8(omnibox_view->GetText());
  EXPECT_THAT(omnibox_text, ::testing::HasSubstr(slow_url.host()));
  EXPECT_THAT(omnibox_text, ::testing::EndsWith(slow_url.path()));
}

// Test fixture for the remote-NTP scenario. Configures a search
// provider whose new_tab_url is served by an HTTPS embedded test
// server, so the active NTP loads at a non-WebUI URL.
class LocationBarModelInstantNTPTest : public LocationBarModelTest,
                                       public InstantTestBase {
 public:
  LocationBarModelInstantNTPTest() = default;
  ~LocationBarModelInstantNTPTest() override = default;

  void SetUpOnMainThread() override {
    LocationBarModelTest::SetUpOnMainThread();
    ASSERT_TRUE(https_test_server().Start());
    GURL base_url = https_test_server().GetURL("/instant_extended.html?");
    GURL ntp_url = https_test_server().GetURL("/instant_extended_ntp.html?");
    ASSERT_NO_FATAL_FAILURE(
        SetupInstant(browser()->profile(), base_url, ntp_url));
  }

 protected:
  // Loads the remote NTP, then injects and clicks an anchor pointing at
  // /hung. /hung keeps the navigation pending so the test can inspect the
  // location bar before commit. Returns the slow_url so the test body can
  // make destination-URL assertions.
  GURL NavigateRemoteNTPAndClickSlowLink() {
    LocationBarModel* location_bar_model =
        browser()->GetFeatures().location_bar_model();

    // Open the remote NTP. The TemplateURLService rewrites
    // chrome::kChromeUINewTabURL into the configured new_tab_url.
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                             GURL(chrome::kChromeUINewTabURL)));
    content::WebContents* ntp_tab =
        browser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_TRUE(search::IsInstantNTP(ntp_tab));
    EXPECT_FALSE(location_bar_model->ShouldDisplayURL());

    GURL slow_url(https_test_server().GetURL("/hung"));
    const char* kNavScriptTemplate = R"(
        var a = document.createElement('a');
        a.href = $1;
        a.innerText = 'Simulated most-visited link';
        document.body.appendChild(a);
        a.click();
    )";
    content::TestNavigationManager nav_manager(ntp_tab, slow_url);
    EXPECT_TRUE(content::ExecJs(
        ntp_tab, content::JsReplace(kNavScriptTemplate, slow_url)));
    EXPECT_TRUE(nav_manager.WaitForRequestStart());

    content::NavigationEntry* pending_entry =
        ntp_tab->GetController().GetPendingEntry();
    EXPECT_TRUE(pending_entry);
    EXPECT_EQ(slow_url, pending_entry->GetURL());

    return slow_url;
  }
};

// Variant of the remote-NTP fixture with kNtpDisableBrowserInitiatedLinks
// enabled.
class LocationBarModelInstantNTPNoBrowserInitiatedLinksTest
    : public LocationBarModelInstantNTPTest {
 public:
  LocationBarModelInstantNTPNoBrowserInitiatedLinksTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kNtpDisableBrowserInitiatedLinks);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Variant of the remote-NTP fixture with kNtpDisableBrowserInitiatedLinks
// disabled (legacy reclassification behavior).
class LocationBarModelInstantNTPBrowserInitiatedLinksTest
    : public LocationBarModelInstantNTPTest {
 public:
  LocationBarModelInstantNTPBrowserInitiatedLinksTest() {
    scoped_feature_list_.InitAndDisableFeature(
        features::kNtpDisableBrowserInitiatedLinks);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// With the kill switch DISABLED (legacy behavior),
// ChromeContentBrowserClient::OverrideNavigationParams flips
// is_renderer_initiated to false on NTP-sourced link clicks, so
// NavigationControllerImpl::GetVisibleEntry exposes the pending entry as
// visible. The omnibox shows the destination URL during the pending
// window.
IN_PROC_BROWSER_TEST_F(LocationBarModelInstantNTPBrowserInitiatedLinksTest,
                       ShouldDisplayURLWhileNavigatingAwayFromRemoteNTP) {
  LocationBarModel* location_bar_model =
      browser()->GetFeatures().location_bar_model();
  OmniboxView* omnibox_view =
      browser()->window()->GetLocationBar()->GetOmniboxView();
  ASSERT_TRUE(omnibox_view);

  const GURL slow_url = NavigateRemoteNTPAndClickSlowLink();
  ASSERT_FALSE(::testing::Test::HasFatalFailure());

  EXPECT_TRUE(location_bar_model->ShouldDisplayURL());
  std::string formatted_url =
      base::UTF16ToUTF8(location_bar_model->GetFormattedFullURL());
  EXPECT_THAT(formatted_url, ::testing::HasSubstr(slow_url.host()));
  EXPECT_THAT(formatted_url, ::testing::EndsWith(slow_url.path()));

  std::string omnibox_text = base::UTF16ToUTF8(omnibox_view->GetText());
  EXPECT_THAT(omnibox_text, ::testing::HasSubstr(slow_url.host()));
  EXPECT_THAT(omnibox_text, ::testing::EndsWith(slow_url.path()));
}

// With the kill switch ENABLED (new behavior),
// ChromeContentBrowserClient::OverrideNavigationParams leaves
// is_renderer_initiated as true, so
// NavigationControllerImpl::GetVisibleEntry refuses to expose the pending
// entry as visible (URL-spoof protection for renderer-initiated
// navigations). The omnibox remains on the NTP -- displaying no URL --
// until the navigation actually commits.
IN_PROC_BROWSER_TEST_F(LocationBarModelInstantNTPNoBrowserInitiatedLinksTest,
                       ShouldNotDisplayURLWhileNavigatingAwayFromRemoteNTP) {
  LocationBarModel* location_bar_model =
      browser()->GetFeatures().location_bar_model();
  OmniboxView* omnibox_view =
      browser()->window()->GetLocationBar()->GetOmniboxView();
  ASSERT_TRUE(omnibox_view);

  const GURL slow_url = NavigateRemoteNTPAndClickSlowLink();
  ASSERT_FALSE(::testing::Test::HasFatalFailure());

  // GetVisibleEntry refuses to show the pending entry, so the visible
  // entry stays on the NTP and the location bar continues to suppress
  // the URL.
  EXPECT_FALSE(location_bar_model->ShouldDisplayURL());
  EXPECT_TRUE(location_bar_model->GetFormattedFullURL().empty());
  EXPECT_TRUE(omnibox_view->GetText().empty());
}
