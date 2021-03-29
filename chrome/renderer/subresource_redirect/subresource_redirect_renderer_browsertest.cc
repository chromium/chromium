// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/login_detection/login_detection_type.h"
#include "chrome/browser/login_detection/login_detection_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/subresource_redirect/https_image_compression_infobar_decider.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/subresource_redirect/common/subresource_redirect_result.h"
#include "components/subresource_redirect/subresource_redirect_browser_test_util.h"
#include "components/subresource_redirect/subresource_redirect_test_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features.h"

namespace subresource_redirect {

// Test class that sets up logged-in sites from field trial.
class SubresourceRedirectLoggedInSitesBrowserTest
    : public InProcessBrowserTest {
 public:
  SubresourceRedirectLoggedInSitesBrowserTest()
      : https_test_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII("host-rules", "MAP * 127.0.0.1");
    command_line->AppendSwitch("enable-spdy-proxy-auth");

    // Disable infobar shown check to actually compress the pages.
    command_line->AppendSwitch("override-https-image-compression-infobar");
  }

  void SetUp() override {
    ASSERT_TRUE(robots_rules_server_.Start());
    ASSERT_TRUE(image_compression_server_.Start());
    https_test_server_.ServeFilesFromSourceDirectory("chrome/test/data");
    ASSERT_TRUE(https_test_server_.Start());

    std::vector<base::test::ScopedFeatureList::FeatureAndParams>
        enabled_features;
    base::FieldTrialParams params, login_detection_params;
    params["enable_public_image_hints_based_compression"] = "false";
    params["enable_login_robots_based_compression"] = "true";
    params["lite_page_robots_origin"] = robots_rules_server_.GetURL();
    params["lite_page_subresource_origin"] = image_compression_server_.GetURL();
    // This rules fetch timeout is chosen such that the tests would have
    // enough time to fetch the rules without causing a timeout.
    params["robots_rules_receive_timeout"] = "1000";
    enabled_features.emplace_back(blink::features::kSubresourceRedirect,
                                  params);

    login_detection_params["logged_in_sites"] = "https://loggedin.com";
    enabled_features.emplace_back(login_detection::kLoginDetection,
                                  login_detection_params);

    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features, {});
    InProcessBrowserTest::SetUp();
  }

  GURL GetHttpsTestURL(const std::string& path) const {
    return https_test_server_.GetURL("test_https_server.com", path);
  }

  void NavigateAndWaitForLoad(Browser* browser, const GURL& url) {
    ui_test_utils::NavigateToURL(browser, url);
    EXPECT_EQ(true, EvalJs(browser->tab_strip_model()->GetActiveWebContents(),
                           "checkImage()"));
    FetchHistogramsFromChildProcesses();
  }

  bool RunScriptExtractBool(const std::string& script,
                            content::WebContents* web_contents = nullptr) {
    if (!web_contents)
      web_contents = browser()->tab_strip_model()->GetActiveWebContents();
    return EvalJs(web_contents, script).ExtractBool();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

  // Simulates the LitePages servers that return the robots rules and compress
  // images.
  RobotsRulesTestServer robots_rules_server_;
  ImageCompressionTestServer image_compression_server_;
  net::EmbeddedTestServer https_test_server_;

  base::HistogramTester histogram_tester_;
};

// Enable tests for linux since LiteMode is enabled only for Android.
#if defined(OS_WIN) || defined(OS_MAC) || BUILDFLAG(IS_CHROMEOS_ASH)
#define DISABLE_ON_WIN_MAC_CHROMEOS(x) DISABLED_##x
#else
#define DISABLE_ON_WIN_MAC_CHROMEOS(x) x
#endif

// TODO(crbug.com/1166280): Enable the test after fixing the flake.
// Verify that when image load gets canceled due to subsequent page load, the
// subresource redirect for the image is canceled as well.
IN_PROC_BROWSER_TEST_F(SubresourceRedirectLoggedInSitesBrowserTest,
                       DISABLED_TestCancelBeforeImageLoad) {
  robots_rules_server_.set_failure_mode(
      RobotsRulesTestServer::FailureMode::kTimeout);
  robots_rules_server_.AddRobotsRules(GetHttpsTestURL("/"),
                                      {{kRuleTypeAllow, ""}});

  ui_test_utils::NavigateToURL(browser(),
                               GetHttpsTestURL("/load_image/image.html"));

  // Wait for the image request to start and its robots rules to be requested.
  while (robots_rules_server_.received_requests().empty()) {
    base::RunLoop().RunUntilIdle();
  }

  ui_test_utils::NavigateToURL(browser(),
                               GetHttpsTestURL("/load_image/simple.html"));
  FetchHistogramsFromChildProcesses();

  RetryForHistogramUntilCountReached(
      &histogram_tester_,
      "SubresourceRedirect.LoginRobotsDeciderAgent.RedirectResult", 1);
  histogram_tester_.ExpectUniqueSample(
      "SubresourceRedirect.LoginRobotsDeciderAgent.RedirectResult",
      SubresourceRedirectResult::kIneligibleRobotsTimeout, 1);
  histogram_tester_.ExpectUniqueSample(
      "SubresourceRedirect.CompressionAttempt.ResponseCode",
      net::HTTP_TEMPORARY_REDIRECT, 1);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.ServerResponded", 0);

  robots_rules_server_.VerifyRequestedOrigins({GetHttpsTestURL("/").spec()});
  image_compression_server_.VerifyRequestedImagePaths({});
}

// TODO(crbug.com/1166280): Enable the test after fixing the flake.
// Verify that when image load gets canceled due to subsequent navigation to a
// logged-in page, the subresource redirect for the image is disabled as well.
IN_PROC_BROWSER_TEST_F(SubresourceRedirectLoggedInSitesBrowserTest,
                       DISABLED_TestCancelBeforeImageLoadForLoggedInSite) {
  robots_rules_server_.set_failure_mode(
      RobotsRulesTestServer::FailureMode::kTimeout);
  robots_rules_server_.AddRobotsRules(GetHttpsTestURL("/"),
                                      {{kRuleTypeAllow, ""}});

  ui_test_utils::NavigateToURL(browser(),
                               GetHttpsTestURL("/load_image/image.html"));

  // Wait for the image request to start and its robots rules to be requested.
  while (robots_rules_server_.received_requests().empty()) {
    base::RunLoop().RunUntilIdle();
  }

  ui_test_utils::NavigateToURL(
      browser(),
      https_test_server_.GetURL("loggedin.com", "/load_image/simple.html"));
  FetchHistogramsFromChildProcesses();
  histogram_tester_.ExpectBucketCount(
      "Login.PageLoad.DetectionType",
      login_detection::LoginDetectionType::kFieldTrialLoggedInSite, 1);

  RetryForHistogramUntilCountReached(
      &histogram_tester_,
      "SubresourceRedirect.LoginRobotsDeciderAgent.RedirectResult", 1);
  histogram_tester_.ExpectUniqueSample(
      "SubresourceRedirect.LoginRobotsDeciderAgent.RedirectResult",
      SubresourceRedirectResult::kIneligibleRobotsTimeout, 1);
  histogram_tester_.ExpectUniqueSample(
      "SubresourceRedirect.CompressionAttempt.ResponseCode",
      net::HTTP_TEMPORARY_REDIRECT, 1);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.CompressionAttempt.ServerResponded", 0);

  robots_rules_server_.VerifyRequestedOrigins({GetHttpsTestURL("/").spec()});
  image_compression_server_.VerifyRequestedImagePaths({});
}

}  // namespace subresource_redirect
