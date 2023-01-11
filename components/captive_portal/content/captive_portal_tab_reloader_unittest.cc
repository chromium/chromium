// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/captive_portal/content/captive_portal_tab_reloader.h"

#include <memory>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "components/captive_portal/content/captive_portal_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_status_flags.h"
#include "net/ssl/ssl_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace captive_portal {

// Used for testing CaptivePortalTabReloader in isolation from the observer.
// Exposes a number of private functions and mocks out others.
class TestCaptivePortalTabReloader : public CaptivePortalTabReloader {
 public:
  explicit TestCaptivePortalTabReloader(content::WebContents* web_contents)
      : CaptivePortalTabReloader(nullptr, web_contents, base::NullCallback()) {}

  TestCaptivePortalTabReloader(const TestCaptivePortalTabReloader&) = delete;
  TestCaptivePortalTabReloader& operator=(const TestCaptivePortalTabReloader&) =
      delete;

  ~TestCaptivePortalTabReloader() override {}

  bool TimerRunning() { return slow_ssl_load_timer_.IsRunning(); }

  // The following methods are aliased so they can be publicly accessed by the
  // unit tests.

  State state() const { return CaptivePortalTabReloader::state(); }

  void set_slow_ssl_load_time(base::TimeDelta slow_ssl_load_time) {
    EXPECT_FALSE(TimerRunning());
    CaptivePortalTabReloader::set_slow_ssl_load_time(slow_ssl_load_time);
  }

  // CaptivePortalTabReloader:
  MOCK_METHOD0(ReloadTab, void());
  MOCK_METHOD0(MaybeOpenCaptivePortalLoginTab, void());
  MOCK_METHOD0(CheckForCaptivePortal, void());
};

class CaptivePortalTabReloaderTest : public content::RenderViewHostTestHarness {
 public:
  // testing::Test:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    tab_reloader_ =
        std::make_unique<testing::StrictMock<TestCaptivePortalTabReloader>>(
            web_contents());

    // Most tests don't run the message loop, so don't use a timer for them.
    tab_reloader_->set_slow_ssl_load_time(base::TimeDelta());
  }

  void TearDown() override {
    EXPECT_FALSE(tab_reloader().TimerRunning());
    tab_reloader_.reset(NULL);
    content::RenderViewHostTestHarness::TearDown();
  }

  TestCaptivePortalTabReloader& tab_reloader() { return *tab_reloader_.get(); }

 private:
  std::unique_ptr<TestCaptivePortalTabReloader> tab_reloader_;
};

// Simulates a slow SSL load when the Internet is connected.
TEST_F(CaptivePortalTabReloaderTest, InternetConnected) {
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());

  tab_reloader().OnLoadStart(true);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_TIMER_RUNNING,
            tab_reloader().state());
  EXPECT_TRUE(tab_reloader().TimerRunning());

  EXPECT_CALL(tab_reloader(), CheckForCaptivePortal()).Times(1);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(tab_reloader().TimerRunning());
  EXPECT_EQ(CaptivePortalTabReloader::STATE_MAYBE_BROKEN_BY_PORTAL,
            tab_reloader().state());

  tab_reloader().OnCaptivePortalResults(RESULT_INTERNET_CONNECTED,
                                        RESULT_INTERNET_CONNECTED);

  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());
  EXPECT_FALSE(tab_reloader().TimerRunning());

  tab_reloader().OnLoadCommitted(net::OK, net::ResolveErrorInfo(net::OK));
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());
}

// Simulates a slow SSL load when the Internet is connected.  In this case,
// the timeout error occurs before the timer triggers.  Unlikely to happen
// in practice, but best if it still works.
TEST_F(CaptivePortalTabReloaderTest, InternetConnectedTimeout) {
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());

  tab_reloader().OnLoadStart(true);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_TIMER_RUNNING,
            tab_reloader().state());
  EXPECT_TRUE(tab_reloader().TimerRunning());

  EXPECT_CALL(tab_reloader(), CheckForCaptivePortal()).Times(1);
  tab_reloader().OnLoadCommitted(net::ERR_CONNECTION_TIMED_OUT,
                                 net::ResolveErrorInfo(net::OK));
  EXPECT_FALSE(tab_reloader().TimerRunning());
  EXPECT_EQ(CaptivePortalTabReloader::STATE_MAYBE_BROKEN_BY_PORTAL,
            tab_reloader().state());

  tab_reloader().OnCaptivePortalResults(RESULT_INTERNET_CONNECTED,
                                        RESULT_INTERNET_CONNECTED);

  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());
}

// Simulates a slow SSL load when captive portal checks return no response.
TEST_F(CaptivePortalTabReloaderTest, NoResponse) {
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());

  tab_reloader().OnLoadStart(true);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_TIMER_RUNNING,
            tab_reloader().state());
  EXPECT_TRUE(tab_reloader().TimerRunning());

  EXPECT_CALL(tab_reloader(), CheckForCaptivePortal()).Times(1);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(tab_reloader().TimerRunning());
  EXPECT_EQ(CaptivePortalTabReloader::STATE_MAYBE_BROKEN_BY_PORTAL,
            tab_reloader().state());

  tab_reloader().OnCaptivePortalResults(RESULT_NO_RESPONSE, RESULT_NO_RESPONSE);

  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());
  EXPECT_FALSE(tab_reloader().TimerRunning());

  tab_reloader().OnLoadCommitted(net::OK, net::ResolveErrorInfo(net::OK));
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());
}

// Simulates a slow HTTP load when behind a captive portal, that eventually.
// times out.  Since it's HTTP, the TabReloader should do nothing.
TEST_F(CaptivePortalTabReloaderTest, DoesNothingOnHttp) {
  tab_reloader().OnLoadStart(false);
  EXPECT_FALSE(tab_reloader().TimerRunning());
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());

  tab_reloader().OnCaptivePortalResults(RESULT_INTERNET_CONNECTED,
                                        RESULT_BEHIND_CAPTIVE_PORTAL);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());

  // The user logs in.
  tab_reloader().OnCaptivePortalResults(RESULT_BEHIND_CAPTIVE_PORTAL,
                                        RESULT_INTERNET_CONNECTED);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());

  // The page times out.
  tab_reloader().OnLoadCommitted(net::ERR_CONNECTION_TIMED_OUT,
                                 net::ResolveErrorInfo(net::OK));
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());
}

// Simulate the normal login process.  The user logs in before the error page
// in the original tab commits.
TEST_F(CaptivePortalTabReloaderTest, Login) {
  tab_reloader().OnLoadStart(true);

  EXPECT_CALL(tab_reloader(), CheckForCaptivePortal()).Times(1);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(tab_reloader().TimerRunning());
  EXPECT_EQ(CaptivePortalTabReloader::STATE_MAYBE_BROKEN_BY_PORTAL,
            tab_reloader().state());

  // The captive portal service detects a captive portal.  The TabReloader
  // should try and create a new login tab in response.
  EXPECT_CALL(tab_reloader(), MaybeOpenCaptivePortalLoginTab()).Times(1);
  tab_reloader().OnCaptivePortalResults(RESULT_INTERNET_CONNECTED,
                                        RESULT_BEHIND_CAPTIVE_PORTAL);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_BROKEN_BY_PORTAL,
            tab_reloader().state());
  EXPECT_FALSE(tab_reloader().TimerRunning());

  // The user logs on from another tab, and a captive portal check is triggered.
  tab_reloader().OnCaptivePortalResults(RESULT_BEHIND_CAPTIVE_PORTAL,
                                        RESULT_INTERNET_CONNECTED);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NEEDS_RELOAD,
            tab_reloader().state());

  // The error page commits, which should start an asynchronous reload.
  tab_reloader().OnLoadCommitted(net::ERR_CONNECTION_TIMED_OUT,
                                 net::ResolveErrorInfo(net::OK));
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NEEDS_RELOAD,
            tab_reloader().state());

  EXPECT_CALL(tab_reloader(), ReloadTab()).Times(1);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());
}

// Simulate the normal login process.  The user logs in after the tab finishes
// loading the error page.
TEST_F(CaptivePortalTabReloaderTest, LoginLate) {
  tab_reloader().OnLoadStart(true);

  EXPECT_CALL(tab_reloader(), CheckForCaptivePortal()).Times(1);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(tab_reloader().TimerRunning());
  EXPECT_EQ(CaptivePortalTabReloader::STATE_MAYBE_BROKEN_BY_PORTAL,
            tab_reloader().state());

  // The captive portal service detects a captive portal.  The TabReloader
  // should try and create a new login tab in response.
  EXPECT_CALL(tab_reloader(), MaybeOpenCaptivePortalLoginTab()).Times(1);
  tab_reloader().OnCaptivePortalResults(RESULT_INTERNET_CONNECTED,
                                        RESULT_BEHIND_CAPTIVE_PORTAL);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_BROKEN_BY_PORTAL,
            tab_reloader().state());
  EXPECT_FALSE(tab_reloader().TimerRunning());

  // The error page commits.
  tab_reloader().OnLoadCommitted(net::ERR_CONNECTION_TIMED_OUT,
                                 net::ResolveErrorInfo(net::OK));
  EXPECT_EQ(CaptivePortalTabReloader::STATE_BROKEN_BY_PORTAL,
            tab_reloader().state());

  // The user logs on from another tab, and a captive portal check is triggered.
  EXPECT_CALL(tab_reloader(), ReloadTab()).Times(1);
  tab_reloader().OnCaptivePortalResults(RESULT_BEHIND_CAPTIVE_PORTAL,
                                        RESULT_INTERNET_CONNECTED);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());
}

// Simulate a login after the tab times out unexpectedly quickly.
TEST_F(CaptivePortalTabReloaderTest, TimeoutFast) {
  tab_reloader().OnLoadStart(true);

  // The error page commits, which should trigger a captive portal check,
  // since the timer's still running.
  EXPECT_CALL(tab_reloader(), CheckForCaptivePortal()).Times(1);
  tab_reloader().OnLoadCommitted(net::ERR_CONNECTION_TIMED_OUT,
                                 net::ResolveErrorInfo(net::OK));
  EXPECT_EQ(CaptivePortalTabReloader::STATE_MAYBE_BROKEN_BY_PORTAL,
            tab_reloader().state());

  // The captive portal service detects a captive portal.  The TabReloader
  // should try and create a new login tab in response.
  EXPECT_CALL(tab_reloader(), MaybeOpenCaptivePortalLoginTab()).Times(1);
  tab_reloader().OnCaptivePortalResults(RESULT_INTERNET_CONNECTED,
                                        RESULT_BEHIND_CAPTIVE_PORTAL);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_BROKEN_BY_PORTAL,
            tab_reloader().state());
  EXPECT_FALSE(tab_reloader().TimerRunning());

  // The user logs on from another tab, and a captive portal check is triggered.
  EXPECT_CALL(tab_reloader(), ReloadTab()).Times(1);
  tab_reloader().OnCaptivePortalResults(RESULT_BEHIND_CAPTIVE_PORTAL,
                                        RESULT_INTERNET_CONNECTED);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());
}

// The secure DNS config is misconfigured. A secure DNS network error on a
// HTTP navigation triggers a captive portal probe. The probe does not find
// a captive portal.
TEST_F(CaptivePortalTabReloaderTest, HttpBadSecureDnsConfig) {
  tab_reloader().OnLoadStart(false);

  EXPECT_FALSE(tab_reloader().TimerRunning());
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());

  // The page encounters a secure DNS network error. The error page commits,
  // which should trigger a captive portal check, even for HTTP pages.
  EXPECT_CALL(tab_reloader(), CheckForCaptivePortal()).Times(1);
  tab_reloader().OnLoadCommitted(
      net::ERR_NAME_NOT_RESOLVED,
      net::ResolveErrorInfo(net::ERR_CERT_COMMON_NAME_INVALID,
                            true /* is_secure_network_error */));
  EXPECT_EQ(CaptivePortalTabReloader::STATE_MAYBE_BROKEN_BY_PORTAL,
            tab_reloader().state());

  // If the only issue was the secure DNS config not being valid, the probes
  // (which disable secure DNS) should indicate an internet connection.
  tab_reloader().OnCaptivePortalResults(RESULT_INTERNET_CONNECTED,
                                        RESULT_INTERNET_CONNECTED);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());
}

// The secure DNS config is misconfigured. A secure DNS network error on a
// HTTPS navigation triggers a captive portal probe before the SSL timer
// triggers. The probe does not find a captive portal.
TEST_F(CaptivePortalTabReloaderTest,
       HttpsBadSecureDnsConfigPageLoadsBeforeTimerTriggers) {
  tab_reloader().OnLoadStart(true);

  EXPECT_EQ(CaptivePortalTabReloader::STATE_TIMER_RUNNING,
            tab_reloader().state());
  EXPECT_TRUE(tab_reloader().TimerRunning());

  // The page encounters a secure DNS network error. The error page commits,
  // which should trigger a captive portal check. The SSL timer should be
  // cancelled.
  EXPECT_CALL(tab_reloader(), CheckForCaptivePortal()).Times(1);
  tab_reloader().OnLoadCommitted(
      net::ERR_NAME_NOT_RESOLVED,
      net::ResolveErrorInfo(net::ERR_CERT_COMMON_NAME_INVALID,
                            true /* is_secure_network_error */));
  EXPECT_EQ(CaptivePortalTabReloader::STATE_MAYBE_BROKEN_BY_PORTAL,
            tab_reloader().state());
  EXPECT_FALSE(tab_reloader().TimerRunning());

  // If the only issue was the secure DNS config not being valid, the probes
  // (which disable secure DNS) should indicate an internet connection.
  tab_reloader().OnCaptivePortalResults(RESULT_INTERNET_CONNECTED,
                                        RESULT_INTERNET_CONNECTED);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());
}

// The secure DNS config is misconfigured. The SSL timer triggers a captive
// portal probe, which does not complete before the page loads with a secure
// DNS network error. The probe does not find a captive portal.
TEST_F(CaptivePortalTabReloaderTest,
       HttpsBadSecureDnsConfigPageLoadsBeforeTimerTriggeredResults) {
  tab_reloader().OnLoadStart(true);

  EXPECT_EQ(CaptivePortalTabReloader::STATE_TIMER_RUNNING,
            tab_reloader().state());
  EXPECT_TRUE(tab_reloader().TimerRunning());

  EXPECT_CALL(tab_reloader(), CheckForCaptivePortal()).Times(1);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(tab_reloader().TimerRunning());
  EXPECT_EQ(CaptivePortalTabReloader::STATE_MAYBE_BROKEN_BY_PORTAL,
            tab_reloader().state());

  // The page encounters a secure DNS network error. The error page commits.
  // Since a probe is already scheduled, we don't schedule another one.
  tab_reloader().OnLoadCommitted(
      net::ERR_NAME_NOT_RESOLVED,
      net::ResolveErrorInfo(net::ERR_CERT_COMMON_NAME_INVALID,
                            true /* is_secure_network_error */));
  EXPECT_EQ(CaptivePortalTabReloader::STATE_MAYBE_BROKEN_BY_PORTAL,
            tab_reloader().state());

  // If the only issue was the secure DNS config not being valid, the probes
  // (which disable secure DNS) should indicate an internet connection.
  tab_reloader().OnCaptivePortalResults(RESULT_INTERNET_CONNECTED,
                                        RESULT_INTERNET_CONNECTED);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());
}

// The secure DNS config is misconfigured. The SSL timer triggers a captive
// portal probe, which completes before the page loads with a secure DNS
// network error, which triggers another captive portal probe. The probe does
// not find a captive portal.
TEST_F(CaptivePortalTabReloaderTest,
       HttpsBadSecureDnsConfigPageLoadsAfterTimerTriggeredResults) {
  tab_reloader().OnLoadStart(true);

  EXPECT_EQ(CaptivePortalTabReloader::STATE_TIMER_RUNNING,
            tab_reloader().state());
  EXPECT_TRUE(tab_reloader().TimerRunning());

  EXPECT_CALL(tab_reloader(), CheckForCaptivePortal()).Times(2);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(tab_reloader().TimerRunning());
  EXPECT_EQ(CaptivePortalTabReloader::STATE_MAYBE_BROKEN_BY_PORTAL,
            tab_reloader().state());

  // If the only issue was the secure DNS config not being valid, the probes
  // (which disable secure DNS) should indicate an internet connection.
  tab_reloader().OnCaptivePortalResults(RESULT_INTERNET_CONNECTED,
                                        RESULT_INTERNET_CONNECTED);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());

  // The page encounters a secure DNS network error. The error page commits,
  // which triggers another captive portal check.
  tab_reloader().OnLoadCommitted(
      net::ERR_NAME_NOT_RESOLVED,
      net::ResolveErrorInfo(net::ERR_CERT_COMMON_NAME_INVALID,
                            true /* is_secure_network_error */));
  EXPECT_EQ(CaptivePortalTabReloader::STATE_MAYBE_BROKEN_BY_PORTAL,
            tab_reloader().state());
}

// The secure DNS config is configured correctly. The SSL timer triggers a
// captive portal probe. This probe finds a captive portal and completes before
// the page loads with a secure DNS network error, which does not trigger
// another captive portal probe. The user then logs in, causing a page reload.
TEST_F(CaptivePortalTabReloaderTest,
       HttpsSecureDnsConfigPageLoadsAfterTimerTriggeredResults) {
  tab_reloader().OnLoadStart(true);

  EXPECT_EQ(CaptivePortalTabReloader::STATE_TIMER_RUNNING,
            tab_reloader().state());
  EXPECT_TRUE(tab_reloader().TimerRunning());

  EXPECT_CALL(tab_reloader(), CheckForCaptivePortal()).Times(1);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(tab_reloader().TimerRunning());
  EXPECT_EQ(CaptivePortalTabReloader::STATE_MAYBE_BROKEN_BY_PORTAL,
            tab_reloader().state());

  // The probe finds a captive portal and opens a login page.
  EXPECT_CALL(tab_reloader(), MaybeOpenCaptivePortalLoginTab()).Times(1);
  tab_reloader().OnCaptivePortalResults(RESULT_INTERNET_CONNECTED,
                                        RESULT_BEHIND_CAPTIVE_PORTAL);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_BROKEN_BY_PORTAL,
            tab_reloader().state());
  EXPECT_FALSE(tab_reloader().TimerRunning());

  // The original navigation encounters a secure DNS network error. The error
  // page commits but does not trigger another captive portal check.
  tab_reloader().OnLoadCommitted(
      net::ERR_NAME_NOT_RESOLVED,
      net::ResolveErrorInfo(net::ERR_CERT_COMMON_NAME_INVALID,
                            true /* is_secure_network_error */));
  EXPECT_EQ(CaptivePortalTabReloader::STATE_BROKEN_BY_PORTAL,
            tab_reloader().state());

  // The user logs on from another tab, and the page is reloaded.
  EXPECT_CALL(tab_reloader(), ReloadTab()).Times(1);
  tab_reloader().OnCaptivePortalResults(RESULT_BEHIND_CAPTIVE_PORTAL,
                                        RESULT_INTERNET_CONNECTED);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());
}

// The user logs in in a different tab, before the page loads with a secure
// DNS network error. A reload should occur when the page commits.
TEST_F(CaptivePortalTabReloaderTest, HttpsSecureDnsConfigErrorAlreadyLoggedIn) {
  tab_reloader().OnLoadStart(true);

  // The user logs in from another tab before the tab errors out.
  tab_reloader().OnCaptivePortalResults(RESULT_BEHIND_CAPTIVE_PORTAL,
                                        RESULT_INTERNET_CONNECTED);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NEEDS_RELOAD,
            tab_reloader().state());

  // The error page commits, which should trigger a reload.
  EXPECT_CALL(tab_reloader(), ReloadTab()).Times(1);
  tab_reloader().OnLoadCommitted(
      net::ERR_NAME_NOT_RESOLVED,
      net::ResolveErrorInfo(net::ERR_CERT_COMMON_NAME_INVALID,
                            true /* is_secure_network_error */));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());
}

// An SSL protocol error triggers a captive portal check behind a captive
// portal.  The user then logs in.
TEST_F(CaptivePortalTabReloaderTest, SSLProtocolError) {
  tab_reloader().OnLoadStart(true);

  // The error page commits, which should trigger a captive portal check,
  // since the timer's still running.
  EXPECT_CALL(tab_reloader(), CheckForCaptivePortal()).Times(1);
  tab_reloader().OnLoadCommitted(net::ERR_SSL_PROTOCOL_ERROR,
                                 net::ResolveErrorInfo(net::OK));
  EXPECT_EQ(CaptivePortalTabReloader::STATE_MAYBE_BROKEN_BY_PORTAL,
            tab_reloader().state());

  // The captive portal service detects a captive portal.  The TabReloader
  // should try and create a new login tab in response.
  EXPECT_CALL(tab_reloader(), MaybeOpenCaptivePortalLoginTab()).Times(1);
  tab_reloader().OnCaptivePortalResults(RESULT_INTERNET_CONNECTED,
                                        RESULT_BEHIND_CAPTIVE_PORTAL);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_BROKEN_BY_PORTAL,
            tab_reloader().state());
  EXPECT_FALSE(tab_reloader().TimerRunning());

  // The user logs on from another tab, and a captive portal check is triggered.
  EXPECT_CALL(tab_reloader(), ReloadTab()).Times(1);
  tab_reloader().OnCaptivePortalResults(RESULT_BEHIND_CAPTIVE_PORTAL,
                                        RESULT_INTERNET_CONNECTED);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());
}

// An SSL protocol error triggers a captive portal check behind a captive
// portal.  The user logs in before the results from the captive portal check
// completes.
TEST_F(CaptivePortalTabReloaderTest, SSLProtocolErrorFastLogin) {
  tab_reloader().OnLoadStart(true);

  // The error page commits, which should trigger a captive portal check,
  // since the timer's still running.
  EXPECT_CALL(tab_reloader(), CheckForCaptivePortal()).Times(1);
  tab_reloader().OnLoadCommitted(net::ERR_SSL_PROTOCOL_ERROR,
                                 net::ResolveErrorInfo(net::OK));
  EXPECT_EQ(CaptivePortalTabReloader::STATE_MAYBE_BROKEN_BY_PORTAL,
            tab_reloader().state());

  // The user has logged in from another tab.  The tab automatically reloads.
  EXPECT_CALL(tab_reloader(), ReloadTab()).Times(1);
  tab_reloader().OnCaptivePortalResults(RESULT_BEHIND_CAPTIVE_PORTAL,
                                        RESULT_INTERNET_CONNECTED);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());
}

// An SSL protocol error triggers a captive portal check behind a captive
// portal.  The user logs in before the results from the captive portal check
// completes.  This case is probably not too likely, but should be handled.
TEST_F(CaptivePortalTabReloaderTest, SSLProtocolErrorAlreadyLoggedIn) {
  tab_reloader().OnLoadStart(true);

  // The user logs in from another tab before the tab errors out.
  tab_reloader().OnCaptivePortalResults(RESULT_BEHIND_CAPTIVE_PORTAL,
                                        RESULT_INTERNET_CONNECTED);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NEEDS_RELOAD,
            tab_reloader().state());

  // The error page commits, which should trigger a reload.
  EXPECT_CALL(tab_reloader(), ReloadTab()).Times(1);
  tab_reloader().OnLoadCommitted(net::ERR_SSL_PROTOCOL_ERROR,
                                 net::ResolveErrorInfo(net::OK));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());
}

// Simulate the case that a user has already logged in before the tab receives a
// captive portal result, but a RESULT_BEHIND_CAPTIVE_PORTAL was received
// before the tab started loading.
TEST_F(CaptivePortalTabReloaderTest, AlreadyLoggedIn) {
  tab_reloader().OnLoadStart(true);

  EXPECT_CALL(tab_reloader(), CheckForCaptivePortal()).Times(1);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(tab_reloader().TimerRunning());
  EXPECT_EQ(CaptivePortalTabReloader::STATE_MAYBE_BROKEN_BY_PORTAL,
            tab_reloader().state());

  // The user has already logged in.  Since the last result found a captive
  // portal, the tab will be reloaded if a timeout is committed.
  tab_reloader().OnCaptivePortalResults(RESULT_BEHIND_CAPTIVE_PORTAL,
                                        RESULT_INTERNET_CONNECTED);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NEEDS_RELOAD,
            tab_reloader().state());

  // The error page commits, which should start an asynchronous reload.
  tab_reloader().OnLoadCommitted(net::ERR_CONNECTION_TIMED_OUT,
                                 net::ResolveErrorInfo(net::OK));
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NEEDS_RELOAD,
            tab_reloader().state());

  EXPECT_CALL(tab_reloader(), ReloadTab()).Times(1);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());
}

// Same as above, except the result is received even before the timer triggers,
// due to a captive portal test request from some external source, like a login
// tab.
TEST_F(CaptivePortalTabReloaderTest, AlreadyLoggedInBeforeTimerTriggers) {
  tab_reloader().OnLoadStart(true);

  // The user has already logged in.  Since the last result indicated there is
  // a captive portal, the tab will be reloaded if it times out.
  tab_reloader().OnCaptivePortalResults(RESULT_BEHIND_CAPTIVE_PORTAL,
                                        RESULT_INTERNET_CONNECTED);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NEEDS_RELOAD,
            tab_reloader().state());
  EXPECT_FALSE(tab_reloader().TimerRunning());

  // The error page commits, which should start an asynchronous reload.
  tab_reloader().OnLoadCommitted(net::ERR_CONNECTION_TIMED_OUT,
                                 net::ResolveErrorInfo(net::OK));
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NEEDS_RELOAD,
            tab_reloader().state());

  EXPECT_CALL(tab_reloader(), ReloadTab()).Times(1);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());
}

// Simulate the user logging in while the timer is still running.  May happen
// if the tab is reloaded just before logging in on another tab.
TEST_F(CaptivePortalTabReloaderTest, LoginWhileTimerRunning) {
  tab_reloader().OnLoadStart(true);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_TIMER_RUNNING,
            tab_reloader().state());
  EXPECT_TRUE(tab_reloader().TimerRunning());

  // The user has already logged in.
  tab_reloader().OnCaptivePortalResults(RESULT_BEHIND_CAPTIVE_PORTAL,
                                        RESULT_INTERNET_CONNECTED);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NEEDS_RELOAD,
            tab_reloader().state());

  // The error page commits, which should start an asynchronous reload.
  tab_reloader().OnLoadCommitted(net::ERR_CONNECTION_TIMED_OUT,
                                 net::ResolveErrorInfo(net::OK));
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NEEDS_RELOAD,
            tab_reloader().state());

  EXPECT_CALL(tab_reloader(), ReloadTab()).Times(1);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());
}

// Simulate a captive portal being detected while the time is still running.
// The captive portal check triggered by the timer detects the captive portal
// again, and then the user logs in.
TEST_F(CaptivePortalTabReloaderTest, BehindPortalResultWhileTimerRunning) {
  tab_reloader().OnLoadStart(true);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_TIMER_RUNNING,
            tab_reloader().state());
  EXPECT_TRUE(tab_reloader().TimerRunning());

  // The user is behind a captive portal, but since the tab hasn't timed out,
  // the message is ignored.
  tab_reloader().OnCaptivePortalResults(RESULT_INTERNET_CONNECTED,
                                        RESULT_BEHIND_CAPTIVE_PORTAL);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_TIMER_RUNNING,
            tab_reloader().state());

  // The rest proceeds as normal.
  EXPECT_CALL(tab_reloader(), CheckForCaptivePortal()).Times(1);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(CaptivePortalTabReloader::STATE_MAYBE_BROKEN_BY_PORTAL,
            tab_reloader().state());

  // The captive portal service detects a captive portal, and this time the
  // tab tries to create a login tab.
  EXPECT_CALL(tab_reloader(), MaybeOpenCaptivePortalLoginTab()).Times(1);
  tab_reloader().OnCaptivePortalResults(RESULT_BEHIND_CAPTIVE_PORTAL,
                                        RESULT_BEHIND_CAPTIVE_PORTAL);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_BROKEN_BY_PORTAL,
            tab_reloader().state());
  EXPECT_FALSE(tab_reloader().TimerRunning());

  // The user logs on from another tab, and a captive portal check is triggered.
  tab_reloader().OnCaptivePortalResults(RESULT_BEHIND_CAPTIVE_PORTAL,
                                        RESULT_INTERNET_CONNECTED);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NEEDS_RELOAD,
            tab_reloader().state());

  // The error page commits, which should start an asynchronous reload.
  tab_reloader().OnLoadCommitted(net::ERR_CONNECTION_TIMED_OUT,
                                 net::ResolveErrorInfo(net::OK));
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NEEDS_RELOAD,
            tab_reloader().state());

  EXPECT_CALL(tab_reloader(), ReloadTab()).Times(1);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());
}

// The CaptivePortalService detects the user has logged in to a captive portal
// while the timer is still running, but the original load succeeds, so no
// reload is done.
TEST_F(CaptivePortalTabReloaderTest, LogInWhileTimerRunningNoError) {
  tab_reloader().OnLoadStart(true);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_TIMER_RUNNING,
            tab_reloader().state());
  EXPECT_TRUE(tab_reloader().TimerRunning());

  // The user has already logged in.
  tab_reloader().OnCaptivePortalResults(RESULT_BEHIND_CAPTIVE_PORTAL,
                                        RESULT_INTERNET_CONNECTED);
  EXPECT_FALSE(tab_reloader().TimerRunning());
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NEEDS_RELOAD,
            tab_reloader().state());

  // The page successfully commits, so no reload is triggered.
  tab_reloader().OnLoadCommitted(net::OK, net::ResolveErrorInfo(net::OK));
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());
}

// Simulate an HTTP redirect to HTTPS, when the Internet is connected.
TEST_F(CaptivePortalTabReloaderTest, HttpToHttpsRedirectInternetConnected) {
  tab_reloader().OnLoadStart(false);
  // There should be no captive portal check pending.
  base::RunLoop().RunUntilIdle();

  // HTTP to HTTPS redirect.
  tab_reloader().OnRedirect(true);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_TIMER_RUNNING,
            tab_reloader().state());
  EXPECT_TRUE(tab_reloader().TimerRunning());

  EXPECT_CALL(tab_reloader(), CheckForCaptivePortal()).Times(1);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(tab_reloader().TimerRunning());
  EXPECT_EQ(CaptivePortalTabReloader::STATE_MAYBE_BROKEN_BY_PORTAL,
            tab_reloader().state());

  tab_reloader().OnCaptivePortalResults(RESULT_INTERNET_CONNECTED,
                                        RESULT_INTERNET_CONNECTED);

  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());
  EXPECT_FALSE(tab_reloader().TimerRunning());

  tab_reloader().OnLoadCommitted(net::OK, net::ResolveErrorInfo(net::OK));
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());
}

// Simulate an HTTP redirect to HTTPS and subsequent Login, when the user logs
// in before the original page commits.
TEST_F(CaptivePortalTabReloaderTest, HttpToHttpsRedirectLogin) {
  tab_reloader().OnLoadStart(false);
  // There should be no captive portal check pending.
  base::RunLoop().RunUntilIdle();

  // HTTP to HTTPS redirect.
  tab_reloader().OnRedirect(true);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_TIMER_RUNNING,
            tab_reloader().state());

  EXPECT_CALL(tab_reloader(), CheckForCaptivePortal()).Times(1);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(tab_reloader().TimerRunning());
  EXPECT_EQ(CaptivePortalTabReloader::STATE_MAYBE_BROKEN_BY_PORTAL,
            tab_reloader().state());

  // The captive portal service detects a captive portal.  The TabReloader
  // should try and create a new login tab in response.
  EXPECT_CALL(tab_reloader(), MaybeOpenCaptivePortalLoginTab()).Times(1);
  tab_reloader().OnCaptivePortalResults(RESULT_INTERNET_CONNECTED,
                                        RESULT_BEHIND_CAPTIVE_PORTAL);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_BROKEN_BY_PORTAL,
            tab_reloader().state());
  EXPECT_FALSE(tab_reloader().TimerRunning());

  // The user logs on from another tab, and a captive portal check is triggered.
  tab_reloader().OnCaptivePortalResults(RESULT_BEHIND_CAPTIVE_PORTAL,
                                        RESULT_INTERNET_CONNECTED);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NEEDS_RELOAD,
            tab_reloader().state());

  // The error page commits, which should start an asynchronous reload.
  tab_reloader().OnLoadCommitted(net::ERR_CONNECTION_TIMED_OUT,
                                 net::ResolveErrorInfo(net::OK));
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NEEDS_RELOAD,
            tab_reloader().state());

  EXPECT_CALL(tab_reloader(), ReloadTab()).Times(1);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());
}

// Simulate the case where an HTTPs page redirects to an HTTPS page, before
// the timer triggers.
TEST_F(CaptivePortalTabReloaderTest, HttpsToHttpRedirect) {
  tab_reloader().OnLoadStart(true);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_TIMER_RUNNING,
            tab_reloader().state());

  tab_reloader().OnRedirect(false);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());
  EXPECT_FALSE(tab_reloader().TimerRunning());

  // There should be no captive portal check pending after the redirect.
  base::RunLoop().RunUntilIdle();

  // Logging in shouldn't do anything.
  tab_reloader().OnCaptivePortalResults(RESULT_BEHIND_CAPTIVE_PORTAL,
                                        RESULT_INTERNET_CONNECTED);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());
}

// Check that an HTTPS to HTTPS redirect results in no timer running.
TEST_F(CaptivePortalTabReloaderTest, HttpsToHttpsRedirect) {
  tab_reloader().OnLoadStart(true);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_TIMER_RUNNING,
            tab_reloader().state());

  tab_reloader().OnRedirect(true);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());
  EXPECT_FALSE(tab_reloader().TimerRunning());
  // Nothing should happen.
  base::RunLoop().RunUntilIdle();
}

// Check that an HTTPS to HTTP to HTTPS redirect results in no timer running.
TEST_F(CaptivePortalTabReloaderTest, HttpsToHttpToHttpsRedirect) {
  tab_reloader().OnLoadStart(true);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_TIMER_RUNNING,
            tab_reloader().state());

  tab_reloader().OnRedirect(false);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());
  EXPECT_FALSE(tab_reloader().TimerRunning());

  tab_reloader().OnRedirect(true);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());
  EXPECT_FALSE(tab_reloader().TimerRunning());
  // Nothing should happen.
  base::RunLoop().RunUntilIdle();
}

// Check that an HTTP to HTTP redirect results in the timer not running.
TEST_F(CaptivePortalTabReloaderTest, HttpToHttpRedirect) {
  tab_reloader().OnLoadStart(false);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());

  tab_reloader().OnRedirect(false);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());
  EXPECT_FALSE(tab_reloader().TimerRunning());

  // There should be no captive portal check pending after the redirect.
  base::RunLoop().RunUntilIdle();

  // Logging in shouldn't do anything.
  tab_reloader().OnCaptivePortalResults(RESULT_BEHIND_CAPTIVE_PORTAL,
                                        RESULT_INTERNET_CONNECTED);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());
}

}  // namespace captive_portal
