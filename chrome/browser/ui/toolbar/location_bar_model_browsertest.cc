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
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
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

// Regression test for crbug.com/40553422.
IN_PROC_BROWSER_TEST_F(LocationBarModelTest,
                       ShouldDisplayURLWhileNavigatingAwayFromNTP) {
  LocationBarModel* location_bar_model =
      browser()->GetFeatures().location_bar_model();

  // Open an NTP. Its URL should not be displayed.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("chrome://newtab")));
  ASSERT_FALSE(location_bar_model->ShouldDisplayURL());
  ASSERT_TRUE(location_bar_model->GetFormattedFullURL().empty());

  const std::string other_url = "https://www.foo.com";

  // Start loading another page. Its URL should be displayed, even though the
  // current page is still the NTP.
  content::NavigationController* controller =
      &browser()->tab_strip_model()->GetActiveWebContents()->GetController();
  controller->LoadURL(GURL(other_url), content::Referrer(),
                      ui::PAGE_TRANSITION_LINK, std::string());
  EXPECT_TRUE(location_bar_model->ShouldDisplayURL());
  EXPECT_EQ(base::ASCIIToUTF16(other_url),
            location_bar_model->GetFormattedFullURL());

  // Of course the same should still hold after committing.
  content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents());
  EXPECT_TRUE(location_bar_model->ShouldDisplayURL());
  EXPECT_EQ(base::ASCIIToUTF16(other_url),
            location_bar_model->GetFormattedFullURL());
}
