// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace {

class NavigationNotificationObserver : public content::NotificationObserver {
 public:
  NavigationNotificationObserver()
      : got_navigation_(false),
        http_status_code_(0) {
    registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
                   content::NotificationService::AllSources());
  }

  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    DCHECK_EQ(content::NOTIFICATION_NAV_ENTRY_COMMITTED, type);
    got_navigation_ = true;
    http_status_code_ =
        content::Details<content::LoadCommittedDetails>(details)->
        http_status_code;
  }

  int http_status_code() const { return http_status_code_; }
  bool got_navigation() const { return got_navigation_; }

 private:
  content::NotificationRegistrar registrar_;
  int got_navigation_;
  int http_status_code_;

  DISALLOW_COPY_AND_ASSIGN(NavigationNotificationObserver);
};

class NavigationObserver : public content::WebContentsObserver {
public:
  enum NavigationResult {
    NOT_FINISHED,
    ERROR_PAGE,
    SUCCESS,
  };

  explicit NavigationObserver(content::WebContents* web_contents)
      : WebContentsObserver(web_contents), navigation_result_(NOT_FINISHED) {}
  ~NavigationObserver() override = default;

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    navigation_result_ =
        navigation_handle->IsErrorPage() ? ERROR_PAGE : SUCCESS;
    net_error_ = navigation_handle->GetNetErrorCode();
  }

  NavigationResult navigation_result() const { return navigation_result_; }
  net::Error net_error() const { return net_error_; }

  void Reset() {
    navigation_result_ = NOT_FINISHED;
    net_error_ = net::OK;
  }

 private:
  NavigationResult navigation_result_;
  net::Error net_error_ = net::OK;

  DISALLOW_COPY_AND_ASSIGN(NavigationObserver);
};

}  // namespace

typedef InProcessBrowserTest ChromeURLDataManagerTest;

// Makes sure navigating to the new tab page results in a http status code
// of 200.
IN_PROC_BROWSER_TEST_F(ChromeURLDataManagerTest, 200) {
  NavigationNotificationObserver observer;
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));
  EXPECT_TRUE(observer.got_navigation());
  EXPECT_EQ(200, observer.http_status_code());
}

// Makes sure browser does not crash when navigating to an unknown resource.
IN_PROC_BROWSER_TEST_F(ChromeURLDataManagerTest, UnknownResource) {
  // Known resource
  NavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  ui_test_utils::NavigateToURL(
      browser(), GURL("chrome://theme/IDR_SETTINGS_FAVICON"));
  EXPECT_EQ(NavigationObserver::SUCCESS, observer.navigation_result());
  EXPECT_EQ(net::OK, observer.net_error());

  // Unknown resource
  observer.Reset();
  ui_test_utils::NavigateToURL(
      browser(), GURL("chrome://theme/IDR_ASDFGHJKL"));
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
  ui_test_utils::NavigateToURL(
      browser(), GURL("chrome://theme/IDR_SETTINGS_FAVICON@2x"));
  EXPECT_EQ(NavigationObserver::SUCCESS, observer.navigation_result());
  EXPECT_EQ(net::OK, observer.net_error());

  // Unreasonably large scale
  observer.Reset();
  ui_test_utils::NavigateToURL(
      browser(), GURL("chrome://theme/IDR_SETTINGS_FAVICON@99999x"));
  EXPECT_EQ(NavigationObserver::ERROR_PAGE, observer.navigation_result());
  // The presence of net error means that navigation did not commit to the
  // original url.
  EXPECT_NE(net::OK, observer.net_error());
}

class ChromeURLDataManagerWebUITrustedTypesTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<const char*> {
 public:
  ChromeURLDataManagerWebUITrustedTypesTest() = default;

  void CheckTrustedTypesViolation(base::StringPiece url) {
    std::string message_filter1 = "*This document requires*assignment*";
    std::string message_filter2 = "*Refused to create a TrustedTypePolicy*";
    content::WebContents* content =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::WebContentsConsoleObserver console_observer(content);
    console_observer.SetPattern(message_filter1);
    console_observer.SetPattern(message_filter2);

    ASSERT_TRUE(embedded_test_server()->Start());
    ui_test_utils::NavigateToURL(browser(), GURL(url));
    // We don't ASSERT_TRUE here because some WebUI pages are by design not
    // PAGE_TYPE_NORMAL (e.g. chrome://interstitials/ssl).
    content::WaitForLoadStop(content);
    EXPECT_TRUE(console_observer.messages().empty());
  }
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
    // TODO(crbug.com/1114074): DCHECK failure when opening
    // chrome://appcache-internals.
    // "chrome://appcache-internals",
    "chrome://apps",
    "chrome://autofill-internals",
    "chrome://blob-internals",
    "chrome://bluetooth-internals",
    "chrome://bookmarks",
    "chrome://browser-switch",
    "chrome://chrome-urls",
    "chrome://components",
    "chrome://conflicts",
    "chrome://connection-help",
    "chrome://connection-monitoring-detected",
    "chrome://conversion-internals",
    "chrome://crashes",
    "chrome://credits",
    "chrome://device-log",
    // TODO(crbug.com/1114062): Crash when closing chrome://devices.
    // "chrome://devices",
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
    "chrome://internals/web-app",
    "chrome://interstitials/ssl",
    "chrome://interventions-internals",
    "chrome://invalidations",
    "chrome://local-state",
    "chrome://management",
    "chrome://md-user-manager",
    "chrome://media-engagement",
    "chrome://media-feeds",
    "chrome://media-history",
    "chrome://media-internals",
    "chrome://media-router-internals",
    "chrome://memory-internals",
    "chrome://nacl",
    "chrome://net-export",
    "chrome://net-internals",
    "chrome://network-error",
    "chrome://network-errors",
    "chrome://new-tab-page",
    "chrome://newtab",
    "chrome://notifications-internals",
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
    "chrome://sandbox",
    "chrome://serviceworker-internals",
    "chrome://settings",
    // TODO(crbug.com/1115600): DCHECK failure when opening
    // chrome://signin-dice-web-intercept.
    // "chrome://signin-dice-web-intercept",
    "chrome://signin-email-confirmation",
    "chrome://signin-internals",
    "chrome://site-engagement",
    "chrome://snippets-internals",
    "chrome://suggestions",
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
    "chrome://webrtc-internals",
    "chrome://webrtc-logs",
    "chrome://welcome",
#if defined(OS_ANDROID)
    "chrome://explore-sites-internals",
    "chrome://internals/notifications",
    "chrome://internals/query-tiles",
    "chrome://offline-internals",
    "chrome://webapks",
#endif
#if defined(OS_CHROMEOS)
    "chrome://account-manager-error",
    "chrome://account-manager-welcome",
    "chrome://account-migration-welcome",
    "chrome://add-supervision",
    // TODO(crbug.com/1102129): DCHECK failure in
    // ArcGraphicsTracingHandler::ArcGraphicsTracingHandler.
    // "chrome://arc-graphics-tracing",
    // "chrome://arc-overview-tracing",
    "chrome://assistant-optin",
    "chrome://bluetooth-pairing",
    "chrome://cellular-setup",
    "chrome://certificate-manager",
    "chrome://crostini-credits",
    "chrome://crostini-installer",
    "chrome://cryptohome",
    "chrome://drive-internals",
    "chrome://first-run",
    "chrome://help-app",
    "chrome://internet-config-dialog",
    "chrome://internet-detail-dialog",
    "chrome://linux-proxy-config",
    "chrome://machine-learning-internals",
    "chrome://multidevice-setup",
    "chrome://network",
    "chrome://oobe",
    "chrome://os-credits",
    "chrome://os-settings",
    "chrome://power",
    "chrome://proximity-auth/proximity_auth.html",
    "chrome://set-time",
    "chrome://slow",
    "chrome://smb-credentials-dialog",
    "chrome://smb-share-dialog",
    "chrome://supervised-user-internals",
    "chrome://sys-internals",
    // TODO(crbug.com/1115643): DCHECK failure when opening
    // chrome-untrusted://crosh.
    // "chrome-untrusted://crosh",
    "chrome-untrusted://terminal",
#endif
};

INSTANTIATE_TEST_SUITE_P(,
                         ChromeURLDataManagerWebUITrustedTypesTest,
                         ::testing::ValuesIn(kChromeUrls));
