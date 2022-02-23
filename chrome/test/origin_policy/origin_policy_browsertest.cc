// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/security_interstitials/content/security_interstitial_page.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {
const base::FilePath::CharType kDataRoot[] =
    FILE_PATH_LITERAL("chrome/test/data/origin_policy_browsertest");

// The title of the Origin Policy error interstitial. This is used to determine
// whether the page load was blocked by the origin policy throttle.
const char16_t kErrorInterstitialTitle[] = u"Origin Policy Error";

class TestContentBrowserClient : public ChromeContentBrowserClient {
 public:
  TestContentBrowserClient(const GURL& site,
                           const std::string& partition_domain)
      : site_(site), partition_domain_(partition_domain) {}
  ~TestContentBrowserClient() override = default;
  TestContentBrowserClient(const TestContentBrowserClient&) = delete;
  TestContentBrowserClient& operator=(const TestContentBrowserClient&) = delete;

 protected:
  // ChromeContentBrowserClient:
  content::StoragePartitionConfig GetStoragePartitionConfigForSite(
      content::BrowserContext* browser_context,
      const GURL& site) override {
    if (site == site_) {
      return content::StoragePartitionConfig::Create(
          browser_context, partition_domain_, /*partition_name=*/"",
          browser_context->IsOffTheRecord());
    }
    return ChromeContentBrowserClient::GetStoragePartitionConfigForSite(
        browser_context, site);
  }

 private:
  GURL site_;
  std::string partition_domain_;
};

}  // namespace

namespace content {

// OriginPolicyBrowserTest tests several aspects of OriginPolicyThrottle (plus
// associated logic elsewhere). These tests focus on error conditions, since
// the normal operating conditions are already well covered in cross-browser
// Web Platform Tests (wpt/origin-policy/*).

class OriginPolicyBrowserTest : public InProcessBrowserTest {
 public:
  OriginPolicyBrowserTest() : status_(net::HTTP_OK) {}

  OriginPolicyBrowserTest(const OriginPolicyBrowserTest&) = delete;
  OriginPolicyBrowserTest& operator=(const OriginPolicyBrowserTest&) = delete;

  ~OriginPolicyBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    mock_cert_verifier_.SetUpCommandLine(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
    server_ = std::make_unique<net::test_server::EmbeddedTestServer>(
        net::test_server::EmbeddedTestServer::TYPE_HTTPS);
    server_->AddDefaultHandlers(base::FilePath(kDataRoot));
    server_->RegisterRequestHandler(base::BindRepeating(
        &OriginPolicyBrowserTest::HandleResponse, base::Unretained(this)));
    EXPECT_TRUE(server()->Start());

    feature_list_.InitAndEnableFeature(features::kOriginPolicy);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    mock_cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);
  }

  void TearDownInProcessBrowserTestFixture() override {
    server_.reset();
    mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
  }

  net::test_server::EmbeddedTestServer* server() { return server_.get(); }

  // Most tests here are set up to use the page title to distinguish between
  // successful load or the error page. For those tests, this method implements
  // the bulk of the test logic.
  std::u16string NavigateToAndReturnTitle(const char* path,
                                          const char* host = nullptr) {
    EXPECT_TRUE(server());
    GURL url =
        GURL(host ? server()->GetURL(host, path) : server()->GetURL(path));
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    std::u16string title;
    ui_test_utils::GetCurrentTabTitle(browser(), &title);
    return title;
  }

  void SetStatus(const net::HttpStatusCode& status) { status_ = status; }
  void SetLocationHeader(const std::string& header) {
    location_header_ = header;
  }

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleResponse(
      const net::test_server::HttpRequest& request) {
    std::unique_ptr<net::test_server::BasicHttpResponse> response =
        std::make_unique<net::test_server::BasicHttpResponse>();

    if (request.relative_url == "/.well-known/origin-policy") {
      response->set_code(status_);

      if (status_ == net::HTTP_OK) {
        response->set_content(R"({ "ids": ["my-policy"] })");
      } else if (location_header_.has_value()) {
        response->AddCustomHeader("Location", *location_header_);
      }

      return std::move(response);
    }

    // If we return nullptr, then the server will do the default behavior.
    return nullptr;
  }

  std::unique_ptr<net::test_server::EmbeddedTestServer> server_;
  content::ContentMockCertVerifier mock_cert_verifier_;
  base::test::ScopedFeatureList feature_list_;

  net::HttpStatusCode status_;
  absl::optional<std::string> location_header_;
};

IN_PROC_BROWSER_TEST_F(OriginPolicyBrowserTest, PageWithoutPolicy) {
  EXPECT_EQ(u"Page Without Policy",
            NavigateToAndReturnTitle("/page-without-policy.html"));
}

IN_PROC_BROWSER_TEST_F(OriginPolicyBrowserTest, PageWithoutPolicyPolicy404s) {
  SetStatus(net::HTTP_NOT_FOUND);
  EXPECT_EQ(u"Page Without Policy",
            NavigateToAndReturnTitle("/page-without-policy.html"));
}

IN_PROC_BROWSER_TEST_F(OriginPolicyBrowserTest, PageWithoutPolicyPolicy301s) {
  SetStatus(net::HTTP_MOVED_PERMANENTLY);
  SetLocationHeader("/.well-known/origin-policy/example-policy");
  EXPECT_EQ(u"Page Without Policy",
            NavigateToAndReturnTitle("/page-without-policy.html"));
}

IN_PROC_BROWSER_TEST_F(OriginPolicyBrowserTest, ApplyPolicy) {
  EXPECT_EQ(u"Page With Policy",
            NavigateToAndReturnTitle("/page-with-policy.html"));
}

IN_PROC_BROWSER_TEST_F(OriginPolicyBrowserTest, ErrorPolicy301Redirect) {
  SetStatus(net::HTTP_MOVED_PERMANENTLY);
  SetLocationHeader("/.well-known/origin-policy/example-policy");
  EXPECT_EQ(kErrorInterstitialTitle,
            NavigateToAndReturnTitle("/page-with-policy.html"));
}

IN_PROC_BROWSER_TEST_F(OriginPolicyBrowserTest, ErrorPolicy302Redirect) {
  SetStatus(net::HTTP_FOUND);
  SetLocationHeader("/.well-known/origin-policy/example-policy");
  EXPECT_EQ(kErrorInterstitialTitle,
            NavigateToAndReturnTitle("/page-with-policy.html"));
}

IN_PROC_BROWSER_TEST_F(OriginPolicyBrowserTest, ErrorPolicy307Redirect) {
  SetStatus(net::HTTP_TEMPORARY_REDIRECT);
  SetLocationHeader("/.well-known/origin-policy/example-policy");
  EXPECT_EQ(kErrorInterstitialTitle,
            NavigateToAndReturnTitle("/page-with-policy.html"));
}

IN_PROC_BROWSER_TEST_F(OriginPolicyBrowserTest, NonDefaultStoragePartition) {
  const char* partitioned_host = "partitioned.com";
  const char* partitioned_site = "https://partitioned.com/";
  const char* normal_host = "example.com";

  TestContentBrowserClient test_browser_client(GURL(partitioned_site),
                                               "test_partition");
  content::ContentBrowserClient* old_browser_client =
      content::SetBrowserClientForTesting(&test_browser_client);

  SetStatus(net::HTTP_TEMPORARY_REDIRECT);
  SetLocationHeader("/.well-known/origin-policy/example-policy");

  // Verify that both the normal and partitioned pages hit the interstitial.
  EXPECT_EQ(kErrorInterstitialTitle,
            NavigateToAndReturnTitle("/page-with-policy.html", normal_host));
  EXPECT_EQ(
      kErrorInterstitialTitle,
      NavigateToAndReturnTitle("/page-with-policy.html", partitioned_host));

  // Simulate clicking Allow in the interstitial.
  auto* helper =
      security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
          browser()->tab_strip_model()->GetActiveWebContents());
  auto* interstitial =
      helper->GetBlockingPageForCurrentlyCommittedNavigationForTesting();
  EXPECT_NE(nullptr, interstitial);
  interstitial->CommandReceived("1");  // "1" == Proceed

  // The normal site should still be blocked, but not the partitioned one.
  EXPECT_EQ(kErrorInterstitialTitle,
            NavigateToAndReturnTitle("/page-with-policy.html", normal_host));
  EXPECT_EQ(
      u"Page With Policy",
      NavigateToAndReturnTitle("/page-with-policy.html", partitioned_host));

  content::SetBrowserClientForTesting(old_browser_client);
}

}  // namespace content
