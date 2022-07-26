// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>

#include "base/strings/string_piece.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/browser/ui/webui/welcome/helpers.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "media/base/media_switches.h"

namespace {

class NavigationObserver : public content::WebContentsObserver {
 public:
  enum NavigationResult {
    NOT_FINISHED,
    ERROR_PAGE,
    SUCCESS,
  };

  explicit NavigationObserver(content::WebContents* web_contents)
      : WebContentsObserver(web_contents), navigation_result_(NOT_FINISHED) {}

  NavigationObserver(const NavigationObserver&) = delete;
  NavigationObserver& operator=(const NavigationObserver&) = delete;

  ~NavigationObserver() override = default;

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (!navigation_handle->IsInPrimaryMainFrame()) {
      return;
    }
    navigation_result_ =
        navigation_handle->IsErrorPage() ? ERROR_PAGE : SUCCESS;
    net_error_ = navigation_handle->GetNetErrorCode();
    got_navigation_ = true;
    if (navigation_handle->HasCommitted() &&
        !navigation_handle->IsSameDocument() &&
        !navigation_handle->IsErrorPage()) {
      http_status_code_ =
          navigation_handle->GetResponseHeaders()->response_code();
    }
  }

  NavigationResult navigation_result() const { return navigation_result_; }
  net::Error net_error() const { return net_error_; }
  bool got_navigation() const { return got_navigation_; }
  int http_status_code() const { return http_status_code_; }

  void Reset() {
    navigation_result_ = NOT_FINISHED;
    net_error_ = net::OK;
  }

 private:
  NavigationResult navigation_result_;
  net::Error net_error_ = net::OK;
  bool got_navigation_ = false;
  int http_status_code_ = -1;
};

}  // namespace

class ChromeURLDataManagerTest : public InProcessBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    content::URLDataSource::Add(
        browser()->profile(),
        std::make_unique<ThemeSource>(browser()->profile()));
  }
};

// Makes sure navigating to the new tab page results in a http status code
// of 200.
IN_PROC_BROWSER_TEST_F(ChromeURLDataManagerTest, 200) {
  NavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUINewTabURL)));
  EXPECT_TRUE(observer.got_navigation());
  EXPECT_EQ(200, observer.http_status_code());
}

// Makes sure browser does not crash when navigating to an unknown resource.
IN_PROC_BROWSER_TEST_F(ChromeURLDataManagerTest, UnknownResource) {
  // Known resource
  NavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("chrome://theme/IDR_SETTINGS_FAVICON")));
  EXPECT_EQ(NavigationObserver::SUCCESS, observer.navigation_result());
  EXPECT_EQ(net::OK, observer.net_error());

  // Unknown resource
  observer.Reset();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("chrome://theme/IDR_ASDFGHJKL")));
  EXPECT_EQ(NavigationObserver::ERROR_PAGE, observer.navigation_result());
  // The presence of net error means that navigation did not commit to the
  // original url.
  EXPECT_NE(net::OK, observer.net_error());
}

// Makes sure browser does not crash when the resource scale is very large.
IN_PROC_BROWSER_TEST_F(ChromeURLDataManagerTest, LargeResourceScale) {
  // Valid scale
  NavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("chrome://theme/IDR_SETTINGS_FAVICON@2x")));
  EXPECT_EQ(NavigationObserver::SUCCESS, observer.navigation_result());
  EXPECT_EQ(net::OK, observer.net_error());

  // Unreasonably large scale
  observer.Reset();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("chrome://theme/IDR_SETTINGS_FAVICON@99999x")));
  EXPECT_EQ(NavigationObserver::ERROR_PAGE, observer.navigation_result());
  // The presence of net error means that navigation did not commit to the
  // original url.
  EXPECT_NE(net::OK, observer.net_error());
}

class ChromeURLDataManagerWebUITrustedTypesTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<const char*> {
 public:
  ChromeURLDataManagerWebUITrustedTypesTest() {
    std::vector<base::Feature> enabled_features;
#if !BUILDFLAG(IS_CHROMEOS)
    if (GetParam() == std::string("chrome://welcome"))
      enabled_features.push_back(welcome::kForceEnabled);
#endif
    enabled_features.push_back(media::kUseMediaHistoryStore);
    feature_list_.InitWithFeatures(enabled_features, {});
  }

  void CheckTrustedTypesViolation(base::StringPiece url) {
    std::string message_filter1 = "*This document requires*assignment*";
    std::string message_filter2 = "*Refused to create a TrustedTypePolicy*";
    content::WebContents* content =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::WebContentsConsoleObserver console_observer(content);
    console_observer.SetPattern(message_filter1);
    console_observer.SetPattern(message_filter2);

    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(url)));

    if (url == "chrome://network-error" || url == "chrome://dino") {
      // We don't ASSERT_TRUE here because some WebUI pages are
      // PAGE_TYPE_ERROR by design.
      content::WaitForLoadStop(content);
    } else {
      ASSERT_TRUE(content::WaitForLoadStop(content));
    }

    EXPECT_TRUE(console_observer.messages().empty());
  }

  static std::string ParamInfoToString(
      const ::testing::TestParamInfo<const char*>& info) {
    std::string name(info.param);
    std::replace_if(
        name.begin(), name.end(), [](char c) { return !std::isalnum(c); }, '_');
    return name;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  policy::FakeBrowserDMTokenStorage fake_dm_token_storage_;
#endif
};

// Verify that there's no Trusted Types violation in chrome://chrome-urls
IN_PROC_BROWSER_TEST_P(ChromeURLDataManagerWebUITrustedTypesTest,
                       NoTrustedTypesViolation) {
  CheckTrustedTypesViolation(GetParam());
}

// Non-exhaustive list of chrome:// URLs to test for trusted types violations.
// This list was derived from chrome://about. :)
static constexpr const char* const kChromeUrls[] = {
    "chrome://accessibility",
    "chrome://apc-internals",
    "chrome://attribution-internals",
    "chrome://autofill-internals",
    "chrome://blob-internals",
    "chrome://bookmarks",
    "chrome://chrome-urls",
    "chrome://components",
    "chrome://connection-help",
    "chrome://connection-monitoring-detected",
    "chrome://crashes",
    "chrome://credits",
    "chrome://device-log",
    "chrome://dino",
    // TODO(crbug.com/1113446): Test failure due to excessive output.
    // "chrome://discards",
    "chrome://domain-reliability-internals",
    "chrome://download-internals",
    "chrome://downloads",
    "chrome://extensions",
    "chrome://extensions-internals",
    "chrome://flags",
    "chrome://gcm-internals",
    "chrome://gpu",
    "chrome://histograms",
    "chrome://history",
    "chrome://identity-internals",
    "chrome://indexeddb-internals",
    "chrome://inspect",
    "chrome://interstitials/ssl",
    "chrome://invalidations",
    "chrome://local-state",
    "chrome://management",
    "chrome://media-engagement",
    "chrome://media-history",
    "chrome://media-internals",
    "chrome://media-router-internals",
    // TODO(crbug.com/1217395): DCHECK failure
    // "chrome://memory-internals",
    "chrome://net-export",
    "chrome://net-internals",
    "chrome://network-error",
    "chrome://network-errors",
    "chrome://new-tab-page",
    "chrome://newtab",
    "chrome://ntp-tiles-internals",
    "chrome://omnibox",
    "chrome://password-manager-internals",
    "chrome://policy",
    "chrome://predictors",
    "chrome://prefs-internals",
    "chrome://print",
    "chrome://process-internals",
    "chrome://quota-internals",
    "chrome://reset-password",
    "chrome://safe-browsing",
    "chrome://serviceworker-internals",
    "chrome://settings",
    // TODO(crbug.com/1115600): DCHECK failure when opening
    // chrome://signin-dice-web-intercept.
    // "chrome://signin-dice-web-intercept",
    "chrome://signin-internals",
    "chrome://site-engagement",
    // TODO(crbug.com/1099564): Navigating to chrome://sync-confirmation and
    // quickly navigating away cause DCHECK failure.
    // "chrome://sync-confirmation",
    "chrome://sync-internals",
    "chrome://syncfs-internals",
    "chrome://system",
    // TODO(crbug.com/1099565): Navigating to chrome://tab-strip and quickly
    // navigating away cause DCHECK failure.
    // "chrome://tab-strip",
    "chrome://terms",
    "chrome://tracing",
    "chrome://translate-internals",
    "chrome://ukm",
    "chrome://usb-internals",
    "chrome://user-actions",
    "chrome://version",
    "chrome://web-app-internals",
    "chrome://webrtc-internals",
    "chrome://webrtc-logs",
#if BUILDFLAG(IS_ANDROID)
    "chrome://explore-sites-internals",
    "chrome://internals/notifications",
    "chrome://internals/query-tiles",
    "chrome://offline-internals",
    "chrome://snippets-internals",
    "chrome://webapks",
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
    "chrome://account-manager-error",
    "chrome://account-migration-welcome",
    "chrome://add-supervision",
    // TODO(crbug.com/1102129): DCHECK failure in
    // ArcGraphicsTracingHandler::ArcGraphicsTracingHandler.
    // "chrome://arc-graphics-tracing",
    // "chrome://arc-overview-tracing",
    "chrome://assistant-optin",
    "chrome://bluetooth-pairing",
    "chrome://certificate-manager",
    "chrome://crostini-credits",
    "chrome://crostini-installer",
    "chrome://cryptohome",
    "chrome://drive-internals",
    "chrome://family-link-user-internals",
    "chrome://help-app",
    "chrome://internet-config-dialog",
    "chrome://internet-detail-dialog",
    "chrome://linux-proxy-config",
    "chrome://multidevice-setup",
    "chrome://network",
    // TODO(crbug.com/1329058): Eliminate chrome://oobe/login
    "chrome://oobe/login",
    "chrome://os-credits",
    "chrome://os-settings",
    "chrome://power",
    "chrome://proximity-auth/proximity_auth.html",
    "chrome://set-time",
    "chrome://slow",
    "chrome://smb-credentials-dialog",
    "chrome://smb-share-dialog",
    "chrome://sys-internals",
    "chrome-untrusted://terminal",
#endif
#if !BUILDFLAG(IS_CHROMEOS)
    "chrome://apps",
    "chrome://browser-switch",
    "chrome://welcome",
#endif
#if !BUILDFLAG(IS_CHROMEOS_ASH)
    "chrome://signin-email-confirmation",
#endif
#if !BUILDFLAG(IS_MAC)
    "chrome://sandbox",
    "chrome://nacl",
    // TODO(https://crbug.com/1219651): this test is flaky on mac.
    "chrome://bluetooth-internals",
#endif
#if BUILDFLAG(IS_WIN)
    "chrome://conflicts",
#endif
};

INSTANTIATE_TEST_SUITE_P(
    ,
    ChromeURLDataManagerWebUITrustedTypesTest,
    ::testing::ValuesIn(kChromeUrls),
    ChromeURLDataManagerWebUITrustedTypesTest::ParamInfoToString);
