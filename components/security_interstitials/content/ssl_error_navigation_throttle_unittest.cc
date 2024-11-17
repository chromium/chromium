// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/content/ssl_error_navigation_throttle.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "components/security_interstitials/content/security_interstitial_controller_client.h"
#include "components/security_interstitials/content/security_interstitial_page.h"
#include "components/security_interstitials/content/settings_page_helper.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "components/security_interstitials/core/ssl_error_ui.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/test_renderer_host.h"
#include "net/cert/cert_status_flags.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"

namespace {

// Constructs a MetricsHelper instance for usage in this context.
std::unique_ptr<security_interstitials::MetricsHelper>
CreateMetricsHelperForTest(const GURL& request_url) {
  security_interstitials::MetricsHelper::ReportDetails report_details;
  report_details.metric_prefix = "test";
  return std::make_unique<security_interstitials::MetricsHelper>(
      request_url, report_details, /*history_service=*/nullptr);
}

// A SecurityInterstitialPage implementation that does the minimum necessary
// to satisfy SSLErrorNavigationThrottle's expectations of the instance passed
// to its ShowInterstitial() method, in particular populates the data
// needed to instantiate the template HTML.
class FakeSSLBlockingPage
    : public security_interstitials::SecurityInterstitialPage {
 public:
  FakeSSLBlockingPage(content::WebContents* web_contents,
                      int cert_error,
                      const net::SSLInfo& ssl_info,
                      const GURL& request_url)
      : security_interstitials::SecurityInterstitialPage(
            web_contents,
            request_url,
            std::make_unique<
                security_interstitials::SecurityInterstitialControllerClient>(
                web_contents,
                CreateMetricsHelperForTest(request_url),
                /*prefs=*/nullptr,
                "en_US",
                GURL("about:blank"),
                /* settings_page_helper */ nullptr)),
        ssl_error_ui_(request_url,
                      cert_error,
                      ssl_info,
                      /*options_mask=*/0,
                      base::Time::NowFromSystemTime(),
                      /*support_url=*/GURL(),
                      controller()) {}

  ~FakeSSLBlockingPage() override = default;

  // SecurityInterstitialPage:
  void OnInterstitialClosing() override {}
  void PopulateInterstitialStrings(base::Value::Dict& load_time_data) override {
    ssl_error_ui_.PopulateStringsForHTML(load_time_data);
  }

 private:
  security_interstitials::SSLErrorUI ssl_error_ui_;
};

// Replacement for SSLErrorHandler::HandleSSLError that calls
// |blocking_page_ready_callback|. |async| specifies whether this call should be
// done synchronously or using PostTask().
void MockHandleSSLError(
    bool async,
    content::WebContents* web_contents,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    base::OnceCallback<
        void(std::unique_ptr<security_interstitials::SecurityInterstitialPage>)>
        blocking_page_ready_callback) {
  auto blocking_page = std::make_unique<FakeSSLBlockingPage>(
      web_contents, cert_error, ssl_info, request_url);
  if (async) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(blocking_page_ready_callback),
                                  std::move(blocking_page)));
  } else {
    std::move(blocking_page_ready_callback).Run(std::move(blocking_page));
  }
}

bool IsInHostedApp(content::WebContents* web_contents) {
  return false;
}

bool ShouldIgnoreInterstitialBecauseNavigationDefaultedToHttps(
    content::NavigationHandle* handle) {
  return false;
}

class TestSSLErrorNavigationThrottle : public SSLErrorNavigationThrottle {
 public:
  TestSSLErrorNavigationThrottle(
      content::NavigationHandle* handle,
      bool async_handle_ssl_error,
      base::OnceCallback<void(content::NavigationThrottle::ThrottleCheckResult)>
          on_cancel_deferred_navigation)
      : SSLErrorNavigationThrottle(
            handle,
            base::BindOnce(&MockHandleSSLError, async_handle_ssl_error),
            base::BindOnce(&IsInHostedApp),
            base::BindOnce(
                &ShouldIgnoreInterstitialBecauseNavigationDefaultedToHttps)),
        on_cancel_deferred_navigation_(
            std::move(on_cancel_deferred_navigation)) {}

  TestSSLErrorNavigationThrottle(const TestSSLErrorNavigationThrottle&) =
      delete;
  TestSSLErrorNavigationThrottle& operator=(
      const TestSSLErrorNavigationThrottle&) = delete;

  // NavigationThrottle:
  void CancelDeferredNavigation(
      content::NavigationThrottle::ThrottleCheckResult result) override {
    std::move(on_cancel_deferred_navigation_).Run(result);
  }

 private:
  base::OnceCallback<void(content::NavigationThrottle::ThrottleCheckResult)>
      on_cancel_deferred_navigation_;
};

class SSLErrorNavigationThrottleTest
    : public content::RenderViewHostTestHarness,
      public testing::WithParamInterface<bool> {
 public:
  SSLErrorNavigationThrottleTest() = default;

  SSLErrorNavigationThrottleTest(const SSLErrorNavigationThrottleTest&) =
      delete;
  SSLErrorNavigationThrottleTest& operator=(
      const SSLErrorNavigationThrottleTest&) = delete;

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    handle_ = std::make_unique<content::MockNavigationHandle>(web_contents());
    handle_->set_has_committed(true);
    async_ = GetParam();
    throttle_ = std::make_unique<TestSSLErrorNavigationThrottle>(
        handle_.get(), async_,
        base::BindOnce(&SSLErrorNavigationThrottleTest::RecordDeferredResult,
                       base::Unretained(this)));
  }

  void RecordDeferredResult(
      content::NavigationThrottle::ThrottleCheckResult result) {
    deferred_result_ = result;
  }

 protected:
  bool async_ = false;
  std::unique_ptr<content::MockNavigationHandle> handle_;
  std::unique_ptr<TestSSLErrorNavigationThrottle> throttle_;
  content::NavigationThrottle::ThrottleCheckResult deferred_result_ =
      content::NavigationThrottle::DEFER;
};

// Tests that the throttle ignores a request with a non SSL related network
// error code.
TEST_P(SSLErrorNavigationThrottleTest, NoSSLError) {
  SCOPED_TRACE(::testing::Message()
               << "Asynchronous MockHandleSSLError: " << async_);

  handle_->set_net_error_code(net::ERR_BLOCKED_BY_CLIENT);
  content::NavigationThrottle::ThrottleCheckResult result =
      throttle_->WillFailRequest();
  EXPECT_EQ(content::NavigationThrottle::PROCEED, result);
}

// Tests that the throttle defers and cancels a request with a net error that
// is a cert error.
TEST_P(SSLErrorNavigationThrottleTest, SSLInfoWithCertError) {
  SCOPED_TRACE(::testing::Message()
               << "Asynchronous MockHandleSSLError: " << async_);

  net::SSLInfo ssl_info;
  ssl_info.cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  ssl_info.cert_status = net::CERT_STATUS_COMMON_NAME_INVALID;
  handle_->set_net_error_code(net::ERR_CERT_COMMON_NAME_INVALID);
  handle_->set_ssl_info(ssl_info);
  content::NavigationThrottle::ThrottleCheckResult synchronous_result =
      throttle_->WillFailRequest();

  EXPECT_EQ(content::NavigationThrottle::DEFER, synchronous_result.action());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(content::NavigationThrottle::CANCEL, deferred_result_.action());
  EXPECT_EQ(net::ERR_CERT_COMMON_NAME_INVALID,
            deferred_result_.net_error_code());
  EXPECT_TRUE(deferred_result_.error_page_content().has_value());
}

INSTANTIATE_TEST_SUITE_P(All,
                         SSLErrorNavigationThrottleTest,
                         ::testing::Values(false, true));

}  // namespace
