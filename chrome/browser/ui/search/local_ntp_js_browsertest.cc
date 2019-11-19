// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "build/build_config.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/search/instant_test_utils.h"
#include "chrome/browser/ui/search/local_ntp_test_utils.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

// A test class that sets up a local HTML file as the NTP URL.
class LocalNTPJavascriptTestBase : public InProcessBrowserTest {
 public:
  explicit LocalNTPJavascriptTestBase(const std::string& ntp_file_path)
      : https_test_server_(net::EmbeddedTestServer::TYPE_HTTPS),
        ntp_file_path_(ntp_file_path) {
    // Note: We can't use embedded_test_server() from BrowserTestBase because
    // that one uses http:// while we want https://.
    https_test_server_.ServeFilesFromSourceDirectory(
        "chrome/test/data/local_ntp");
  }

 private:
  void SetUpOnMainThread() override {
    ASSERT_TRUE(https_test_server_.Start());
    GURL ntp_url = https_test_server_.GetURL(ntp_file_path_);
    local_ntp_test_utils::SetUserSelectedDefaultSearchProvider(
        browser()->profile(), https_test_server_.base_url().spec(),
        ntp_url.spec());
  }

  net::EmbeddedTestServer https_test_server_;
  const std::string ntp_file_path_;
};

// A test class that sets up local_ntp_browsertest.html as the NTP URL. It's
// mostly a copy of the real local_ntp.html, but it adds some testing JS.
class LocalNTPJavascriptTest : public LocalNTPJavascriptTestBase {
 public:
  LocalNTPJavascriptTest()
      : LocalNTPJavascriptTestBase("/local_ntp_browsertest.html") {}
};

// This runs a bunch of pure JS-side tests, i.e. those that don't require any
// interaction from the native side.
IN_PROC_BROWSER_TEST_F(LocalNTPJavascriptTest, LocalNTPTests) {
  content::WebContents* active_tab = local_ntp_test_utils::OpenNewTab(
      browser(), GURL(chrome::kChromeUINewTabURL));
  ASSERT_TRUE(search::IsInstantNTP(active_tab));

  // Run the tests.
  bool success = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "!!runSimpleTests('localNtp')", &success));
  EXPECT_TRUE(success);
}

// This runs a bunch of pure JS-side tests for the original custom backgrounds
// menu (i.e. before the richer picker).
IN_PROC_BROWSER_TEST_F(LocalNTPJavascriptTest, CustomBackgroundsTests) {
  content::WebContents* active_tab = local_ntp_test_utils::OpenNewTab(
      browser(), GURL(chrome::kChromeUINewTabURL));
  ASSERT_TRUE(search::IsInstantNTP(active_tab));

  // Ensure the window is big enough the the customize button is visible.
  browser()->window()->SetBounds(gfx::Rect(0, 0, 1000, 1000));

  // Run the tests.
  bool success = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "!!runSimpleTests('customBackgrounds')", &success));
  EXPECT_TRUE(success);
}

// This runs a bunch of pure JS-side tests for the richer picker.
IN_PROC_BROWSER_TEST_F(LocalNTPJavascriptTest, CustomizeMenuTests) {
  content::WebContents* active_tab = local_ntp_test_utils::OpenNewTab(
      browser(), GURL(chrome::kChromeUINewTabURL));
  ASSERT_TRUE(search::IsInstantNTP(active_tab));

  // Ensure the window is big enough the the customize button is visible.
  browser()->window()->SetBounds(gfx::Rect(0, 0, 1000, 1000));

  // Run the tests.
  bool success = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "!!runSimpleTests('customizeMenu')", &success));
  EXPECT_TRUE(success);
}

#if !(defined(LEAK_SANITIZER) || defined(ADDRESS_SANITIZER))
IN_PROC_BROWSER_TEST_F(LocalNTPJavascriptTest, Realbox1Tests) {
  content::WebContents* active_tab = local_ntp_test_utils::OpenNewTab(
      browser(), GURL(chrome::kChromeUINewTabURL));
  ASSERT_TRUE(search::IsInstantNTP(active_tab));

  bool success = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "!!runSimpleTests('realbox1')", &success));
  EXPECT_TRUE(success);
}

IN_PROC_BROWSER_TEST_F(LocalNTPJavascriptTest, Realbox2Tests) {
  content::WebContents* active_tab = local_ntp_test_utils::OpenNewTab(
      browser(), GURL(chrome::kChromeUINewTabURL));
  ASSERT_TRUE(search::IsInstantNTP(active_tab));

  bool success = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "!!runSimpleTests('realbox2')", &success));
  EXPECT_TRUE(success);
}
#endif

// A test class that sets up most_visited_browsertest.html as the NTP URL. It's
// mostly a copy of the real most_visited_single.html, but it adds some testing
// JS.
class LocalNTPMostVisitedJavascriptTest : public LocalNTPJavascriptTestBase {
 public:
  LocalNTPMostVisitedJavascriptTest()
      : LocalNTPJavascriptTestBase("/most_visited_browsertest.html") {}
};

// This runs a bunch of pure JS-side tests for the most visited iframe, i.e.
// those that don't require any interaction from the native side.
IN_PROC_BROWSER_TEST_F(LocalNTPMostVisitedJavascriptTest, MostVistedTests) {
  content::WebContents* active_tab = local_ntp_test_utils::OpenNewTab(
      browser(), GURL(chrome::kChromeUINewTabURL));

  // Run the tests.
  bool success = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "!!runSimpleTests('mostVisited')", &success));
  EXPECT_TRUE(success);
}

// A test class that sets up voice_browsertest.html as the NTP URL. It's
// mostly a copy of the real local_ntp.html, but it adds some testing JS.
class LocalNTPVoiceJavascriptTest : public LocalNTPJavascriptTestBase {
 public:
  LocalNTPVoiceJavascriptTest()
      : LocalNTPJavascriptTestBase("/voice_browsertest.html") {}
};

IN_PROC_BROWSER_TEST_F(LocalNTPVoiceJavascriptTest, MicrophoneTests) {
  content::WebContents* active_tab = local_ntp_test_utils::OpenNewTab(
      browser(), GURL(chrome::kChromeUINewTabURL));

  // Run the tests.
  bool success = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "!!runSimpleTests('microphone')", &success));
  EXPECT_TRUE(success);
}

IN_PROC_BROWSER_TEST_F(LocalNTPVoiceJavascriptTest, TextTests) {
  content::WebContents* active_tab = local_ntp_test_utils::OpenNewTab(
      browser(), GURL(chrome::kChromeUINewTabURL));

  // Run the tests.
  bool success = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "!!runSimpleTests('text')", &success));
  EXPECT_TRUE(success);
}

IN_PROC_BROWSER_TEST_F(LocalNTPVoiceJavascriptTest, SpeechTests) {
  content::WebContents* active_tab = local_ntp_test_utils::OpenNewTab(
      browser(), GURL(chrome::kChromeUINewTabURL));

  // Run the tests.
  bool success = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "!!runSimpleTests('speech')", &success));
  EXPECT_TRUE(success);
}

IN_PROC_BROWSER_TEST_F(LocalNTPVoiceJavascriptTest, ViewTests) {
  content::WebContents* active_tab = local_ntp_test_utils::OpenNewTab(
      browser(), GURL(chrome::kChromeUINewTabURL));

  // Run the tests.
  bool success = false;
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "!!runSimpleTests('view')", &success));
  EXPECT_TRUE(success);
}
