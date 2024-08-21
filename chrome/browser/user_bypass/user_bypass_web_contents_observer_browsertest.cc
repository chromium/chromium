// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/user_bypass/user_bypass_web_contents_observer.h"

#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/render_frame_host_test_support.h"
#include "net/base/features.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace user_bypass {

namespace {
constexpr char kUserBypassEnabledHost[] = "a.test";
constexpr char kUserBypassDisabledHost[] = "b.test";
constexpr char kArbitraryPage[] = "/title1.html";
}  // anonymous namespace

class UserBypassWebContentsObserverBrowserTest : public InProcessBrowserTest {
 public:
  UserBypassWebContentsObserverBrowserTest()
      : scoped_feature_list_(net::features::kThirdPartyStoragePartitioning),
        https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  ~UserBypassWebContentsObserverBrowserTest() override = default;

  UserBypassWebContentsObserverBrowserTest(
      const UserBypassWebContentsObserverBrowserTest&) = delete;
  UserBypassWebContentsObserverBrowserTest& operator=(
      const UserBypassWebContentsObserverBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    host_resolver()->AddRule(kUserBypassEnabledHost, "127.0.0.1");
    host_resolver()->AddRule(kUserBypassDisabledHost, "127.0.0.1");

    https_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server()->AddDefaultHandlers(
        InProcessBrowserTest::GetChromeTestDataDir());
    ASSERT_TRUE(https_server()->Start());

    UserBypassWebContentsObserver::CreateForWebContents(GetActiveWebContents());
  }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  content_settings::CookieSettings* GetCookieSettings() {
    return UserBypassWebContentsObserver::FromWebContents(
               GetActiveWebContents())
        ->GetCookieSettingsForTesting();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  net::test_server::EmbeddedTestServer https_server_;
};

IN_PROC_BROWSER_TEST_F(UserBypassWebContentsObserverBrowserTest,
                       LoadUserBypassWithDisabledState) {
  // Navigate the top-level frame to |kUserBypassDisabledHost|.
  GURL url = https_server()->GetURL(kUserBypassDisabledHost, kArbitraryPage);
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), url));

  // Check that there are no changes to the RenderFrameHost.
  EXPECT_FALSE(IsDisableThirdPartyStoragePartitioning2Enabled(
      GetActiveWebContents()->GetPrimaryMainFrame()));
}

// Ensures we can enable Blink Runtime-Enabled Feature (BREF) per top-level
// frame's user-specified content settings for User Bypass.
IN_PROC_BROWSER_TEST_F(UserBypassWebContentsObserverBrowserTest,
                       LoadUserBypassWithEnabledState) {
  // Enable User Bypass for |kUserBypassEnabledHost|.
  GURL url = https_server()->GetURL(kUserBypassEnabledHost, kArbitraryPage);
  GetCookieSettings()->SetCookieSettingForUserBypass(url);
  EXPECT_TRUE(GetCookieSettings()->IsStoragePartitioningBypassEnabled(url));

  // Navigate the top-level frame to |kUserBypassEnabledHost|.
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), url));

  // Check that the changes were applied to the RenderFrameHost.
  EXPECT_TRUE(IsDisableThirdPartyStoragePartitioning2Enabled(
      GetActiveWebContents()->GetPrimaryMainFrame()));

  // Navigate the top-level frame to |kUserBypassDisabledHost|.
  url = https_server()->GetURL(kUserBypassDisabledHost, kArbitraryPage);
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), url));

  // Check that there are no changes to the RenderFrameHost.
  EXPECT_FALSE(IsDisableThirdPartyStoragePartitioning2Enabled(
      GetActiveWebContents()->GetPrimaryMainFrame()));
}

// Similar to |LoadUserBypassWithEnabledState| but makes sure the BREF's desired
// state is as expected even with redirects.
IN_PROC_BROWSER_TEST_F(UserBypassWebContentsObserverBrowserTest,
                       LoadUserBypassWithEnabledStateOnRedirectedNavigation) {
  // Enable User Bypass for |kUserBypassEnabledHost|.
  GURL url = https_server()->GetURL(kUserBypassEnabledHost, kArbitraryPage);
  GetCookieSettings()->SetCookieSettingForUserBypass(url);
  EXPECT_TRUE(GetCookieSettings()->IsStoragePartitioningBypassEnabled(url));

  // Perform a navigation of the top-level frame to |kUserBypassEnabledHost|
  // with a redirect to |kUserBypassDisabledHost|.
  GURL destination_url =
      https_server()->GetURL(kUserBypassDisabledHost, kArbitraryPage);

  GURL redirect_url = https_server()->GetURL(
      kUserBypassEnabledHost,
      base::StrCat({"/server-redirect?", destination_url.spec()}));

  ASSERT_TRUE(
      NavigateToURL(GetActiveWebContents(), redirect_url, destination_url));

  // Check that there are no changes to the RenderFrameHost.
  EXPECT_FALSE(IsDisableThirdPartyStoragePartitioning2Enabled(
      GetActiveWebContents()->GetPrimaryMainFrame()));
}

}  // namespace user_bypass
