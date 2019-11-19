// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/location_bar_model.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/omnibox/common/omnibox_features.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/test_utils.h"
#include "ui/gfx/text_elider.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "extensions/common/extension_builder.h"
#endif

// Test data ------------------------------------------------------------------

namespace {

GURL GetViewSourceURL(const char* path) {
  GURL::Replacements replace_path;
  replace_path.SetPathStr(path);
  return GURL("view-source:").ReplaceComponents(replace_path);
}

struct TestItem {
  GURL url;
  const std::string expected_formatted_full_url;
  const std::string expected_elided_url_for_display =
      expected_formatted_full_url;
} test_items[] = {
    {
        GetViewSourceURL("http://www.google.com"),
        "view-source:www.google.com",
        "view-source:www.google.com",
    },
    {
        GURL(chrome::kChromeUINewTabURL),
        "",
    },
    {
        GetViewSourceURL(chrome::kChromeUINewTabURL),
        "view-source:" +
            content::GetWebUIURLString(chrome::kChromeUINewTabHost),
    },
    {
        GURL("chrome-search://local-ntp/local-ntp.html"),
        "",
    },
    {
        GURL("view-source:chrome-search://local-ntp/local-ntp.html"),
        "view-source:chrome-search://local-ntp/local-ntp.html",
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
        GURL("http://www.google.com/search?q=tractor+supply"),
        "www.google.com/search?q=tractor+supply",
        "google.com/search?q=tractor+supply",
    },
    {
        GURL("https://m.google.ca/search?q=tractor+supply"),
        "https://m.google.ca/search?q=tractor+supply",
        "m.google.ca/search?q=tractor+supply",
    },
};

}  // namespace

// LocationBarModelTest
// -----------------------------------------------------------

class LocationBarModelTest : public BrowserWithTestWindowTest {
 public:
  LocationBarModelTest();
  ~LocationBarModelTest() override;

  // BrowserWithTestWindowTest:
  void SetUp() override;

 protected:
  void NavigateAndCheckText(
      const GURL& url,
      const base::string16& expected_formatted_full_url,
      const base::string16& expected_elided_url_for_display);
  void NavigateAndCheckElided(const GURL& https_url);

 private:
  DISALLOW_COPY_AND_ASSIGN(LocationBarModelTest);
};

LocationBarModelTest::LocationBarModelTest() {}

LocationBarModelTest::~LocationBarModelTest() {}

void LocationBarModelTest::SetUp() {
  BrowserWithTestWindowTest::SetUp();
  AutocompleteClassifierFactory::GetInstance()->SetTestingFactoryAndUse(
      profile(),
      base::BindRepeating(&AutocompleteClassifierFactory::BuildInstanceFor));

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Install a fake extension so that the ID in the chrome-extension test URL is
  // valid. Invalid extension URLs may result in error pages (if blocked by
  // ExtensionNavigationThrottle), which this test doesn't wish to exercise.
  extensions::TestExtensionSystem* extension_system =
      static_cast<extensions::TestExtensionSystem*>(
          extensions::ExtensionSystem::Get(profile()));
  extension_system->CreateExtensionService(
      base::CommandLine::ForCurrentProcess(), base::FilePath(), false);
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("Test")
          .SetID("fooooooooooooooooooooooooooooooo")
          .Build();
  extension_system->extension_service()->AddExtension(extension.get());
#endif
}

void LocationBarModelTest::NavigateAndCheckText(
    const GURL& url,
    const base::string16& expected_formatted_full_url,
    const base::string16& expected_elided_url_for_display) {
  // Check while loading.
  content::NavigationController* controller =
      &browser()->tab_strip_model()->GetActiveWebContents()->GetController();
  controller->LoadURL(url, content::Referrer(), ui::PAGE_TRANSITION_LINK,
                      std::string());
  LocationBarModel* location_bar_model = browser()->location_bar_model();
  EXPECT_EQ(expected_formatted_full_url,
            location_bar_model->GetFormattedFullURL());
  EXPECT_NE(expected_formatted_full_url.empty(),
            location_bar_model->ShouldDisplayURL());
  EXPECT_EQ(expected_elided_url_for_display,
            location_bar_model->GetURLForDisplay());

  // Check after commit.
  CommitPendingLoad(controller);
  EXPECT_EQ(expected_formatted_full_url,
            location_bar_model->GetFormattedFullURL());
  EXPECT_NE(expected_formatted_full_url.empty(),
            location_bar_model->ShouldDisplayURL());
  EXPECT_EQ(expected_elided_url_for_display,
            location_bar_model->GetURLForDisplay());
}

void LocationBarModelTest::NavigateAndCheckElided(const GURL& url) {
  // Check while loading.
  content::NavigationController* controller =
      &browser()->tab_strip_model()->GetActiveWebContents()->GetController();
  controller->LoadURL(url, content::Referrer(), ui::PAGE_TRANSITION_LINK,
                      std::string());
  LocationBarModel* location_bar_model = browser()->location_bar_model();
  const base::string16 formatted_full_url_before(
      location_bar_model->GetFormattedFullURL());
  EXPECT_LT(formatted_full_url_before.size(), url.spec().size());
  EXPECT_TRUE(base::EndsWith(formatted_full_url_before,
                             base::string16(gfx::kEllipsisUTF16),
                             base::CompareCase::SENSITIVE));
  const base::string16 display_url_before(
      location_bar_model->GetURLForDisplay());
  EXPECT_LT(display_url_before.size(), url.spec().size());
  EXPECT_TRUE(base::EndsWith(display_url_before,
                             base::string16(gfx::kEllipsisUTF16),
                             base::CompareCase::SENSITIVE));

  // Check after commit.
  CommitPendingLoad(controller);
  const base::string16 formatted_full_url_after(
      location_bar_model->GetFormattedFullURL());
  EXPECT_LT(formatted_full_url_after.size(), url.spec().size());
  EXPECT_TRUE(base::EndsWith(formatted_full_url_after,
                             base::string16(gfx::kEllipsisUTF16),
                             base::CompareCase::SENSITIVE));
  const base::string16 display_url_after(
      location_bar_model->GetURLForDisplay());
  EXPECT_LT(display_url_after.size(), url.spec().size());
  EXPECT_TRUE(base::EndsWith(display_url_after,
                             base::string16(gfx::kEllipsisUTF16),
                             base::CompareCase::SENSITIVE));
}

// Actual tests ---------------------------------------------------------------

// Test URL display.
TEST_F(LocationBarModelTest, ShouldDisplayURL) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({omnibox::kHideSteadyStateUrlScheme,
                                 omnibox::kHideSteadyStateUrlTrivialSubdomains},
                                {});

  AddTab(browser(), GURL(url::kAboutBlankURL));

  for (const TestItem& test_item : test_items) {
    NavigateAndCheckText(
        test_item.url,
        base::ASCIIToUTF16(test_item.expected_formatted_full_url),
        base::ASCIIToUTF16(test_item.expected_elided_url_for_display));
  }
}

// Tests every combination of Steady State Elision flags.
TEST_F(LocationBarModelTest, SteadyStateElisionsFlags) {
  AddTab(browser(), GURL(url::kAboutBlankURL));

  // Hide Scheme and Hide Trivial Subdomains both Disabled.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        {}, {omnibox::kHideSteadyStateUrlScheme,
             omnibox::kHideSteadyStateUrlTrivialSubdomains});
    NavigateAndCheckText(GURL("https://www.google.com/"),
                         base::ASCIIToUTF16("https://www.google.com"),
                         base::ASCIIToUTF16("https://www.google.com"));
  }

  // Only Hide Scheme Enabled.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        {omnibox::kHideSteadyStateUrlScheme},
        {omnibox::kHideSteadyStateUrlTrivialSubdomains});
    NavigateAndCheckText(GURL("https://www.google.com/"),
                         base::ASCIIToUTF16("https://www.google.com"),
                         base::ASCIIToUTF16("www.google.com"));
  }

  // Only Hide Trivial Subdomains Enabled.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        {omnibox::kHideSteadyStateUrlTrivialSubdomains},
        {omnibox::kHideSteadyStateUrlScheme});
    NavigateAndCheckText(GURL("https://www.google.com/"),
                         base::ASCIIToUTF16("https://www.google.com"),
                         base::ASCIIToUTF16("https://google.com"));
  }

  // Hide Scheme and Hide Trivial Subdomains both Enabled.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        {omnibox::kHideSteadyStateUrlScheme,
         omnibox::kHideSteadyStateUrlTrivialSubdomains},
        {});
    NavigateAndCheckText(GURL("https://www.google.com/"),
                         base::ASCIIToUTF16("https://www.google.com"),
                         base::ASCIIToUTF16("google.com"));
  }
}

TEST_F(LocationBarModelTest, ShouldElideLongURLs) {
  AddTab(browser(), GURL(url::kAboutBlankURL));
  const std::string long_text(content::kMaxURLDisplayChars + 1024, '0');
  NavigateAndCheckElided(
      GURL(std::string("https://www.foo.com/?") + long_text));
  NavigateAndCheckElided(GURL(std::string("data:abc") + long_text));
}

// Regression test for crbug.com/792401.
TEST_F(LocationBarModelTest, ShouldDisplayURLWhileNavigatingAwayFromNTP) {
  LocationBarModel* location_bar_model = browser()->location_bar_model();

  // Open an NTP. Its URL should not be displayed.
  AddTab(browser(), GURL("chrome://newtab"));
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
  CommitPendingLoad(controller);
  EXPECT_TRUE(location_bar_model->ShouldDisplayURL());
  EXPECT_EQ(base::ASCIIToUTF16(other_url),
            location_bar_model->GetFormattedFullURL());
}
