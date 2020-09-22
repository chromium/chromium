// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/well_known_change_password_navigation_throttle.h"

#include <map>
#include <utility>

#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/password_manager/affiliation_service_factory.h"
#include "chrome/browser/password_manager/change_password_url_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/cert_verifier_browser_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/common/url_constants.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_api.pb.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_fetcher.h"
#include "components/password_manager/core/browser/change_password_url_service_impl.h"
#include "components/password_manager/core/browser/site_affiliation/affiliation_service_impl.h"
#include "components/password_manager/core/browser/well_known_change_password_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/sync/driver/test_sync_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/cert/x509_certificate.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {
using content::NavigationThrottle;
using content::TestNavigationObserver;
using net::test_server::BasicHttpResponse;
using net::test_server::DelayedHttpResponse;
using net::test_server::EmbeddedTestServer;
using net::test_server::EmbeddedTestServerHandle;
using net::test_server::HttpRequest;
using net::test_server::HttpResponse;
using password_manager::kWellKnownChangePasswordPath;
using password_manager::kWellKnownNotExistingResourcePath;
using password_manager::WellKnownChangePasswordResult;
using testing::Return;

constexpr char kMockChangePasswordPath[] = "/change-password-override";

// ServerResponse describes how a server should respond to a given path.
struct ServerResponse {
  net::HttpStatusCode status_code;
  std::vector<std::pair<std::string, std::string>> headers;
  int resolve_time_in_milliseconds;
};

// The NavigationThrottle is making 2 requests in parallel. With this config we
// simulate the different orders for the arrival of the responses. The value
// represents the delay in milliseconds.
struct ResponseDelayParams {
  int change_password_delay;
  int not_exist_delay;
};

}  // namespace

class MockChangePasswordUrlService
    : public password_manager::ChangePasswordUrlService {
 public:
  void PrefetchURLs() override {}
  MOCK_METHOD(GURL, GetChangePasswordUrl, (const GURL&), (override));
};

class ChangePasswordNavigationThrottleBrowserTestBase
    : public CertVerifierBrowserTest,
      public testing::WithParamInterface<
          std::tuple<ui::PageTransition, ResponseDelayParams>> {
 public:
  using UkmBuilder =
      ukm::builders::PasswordManager_WellKnownChangePasswordResult;
  ChangePasswordNavigationThrottleBrowserTestBase() {
    test_server_->RegisterRequestHandler(base::BindRepeating(
        &ChangePasswordNavigationThrottleBrowserTestBase::HandleRequest,
        base::Unretained(this)));
  }

  void Initialize() {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(test_server_->InitializeAndListen());
    test_server_->StartAcceptingConnections();
    test_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  void ExpectUkmMetric(WellKnownChangePasswordResult expected) {
    auto entries = test_recorder_->GetEntriesByName(UkmBuilder::kEntryName);
    // Expect one recorded metric.
    ASSERT_EQ(1, static_cast<int>(entries.size()));
    test_recorder_->ExpectEntryMetric(
        entries[0], UkmBuilder::kWellKnownChangePasswordResultName,
        static_cast<int64_t>(expected));
  }

  ukm::TestAutoSetUkmRecorder* test_recorder() { return test_recorder_.get(); }

  ui::PageTransition page_transition() const { return std::get<0>(GetParam()); }
  ResponseDelayParams response_delays() const {
    return std::get<1>(GetParam());
  }

 protected:
  // Navigates to |navigate_url| from the mock server using |transition|. It
  // waits until the navigation to |expected_url| happened.
  void TestNavigationThrottle(
      const GURL& navigate_url,
      const GURL& expected_url,
      base::Optional<url::Origin> initiator_origin = base::nullopt);

  // Whitelist all https certs for the |test_server_|.
  void AddHttpsCertificate() {
    auto cert = test_server_->GetCertificate();
    net::CertVerifyResult verify_result;
    verify_result.cert_status = 0;
    verify_result.verified_cert = cert;
    mock_cert_verifier()->AddResultForCert(cert.get(), verify_result, net::OK);
  }

  // Maps a path to a ServerResponse config object.
  base::flat_map<std::string, ServerResponse> path_response_map_;
  std::unique_ptr<EmbeddedTestServer> test_server_ =
      std::make_unique<EmbeddedTestServer>(EmbeddedTestServer::TYPE_HTTPS);

 private:
  // Returns a response for the given request. Uses |path_response_map_| to
  // construct the response. Returns nullptr when the path is not defined in
  // |path_response_map_|.
  std::unique_ptr<HttpResponse> HandleRequest(const HttpRequest& request);
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_recorder_;
};

std::unique_ptr<HttpResponse>
ChangePasswordNavigationThrottleBrowserTestBase::HandleRequest(
    const HttpRequest& request) {
  GURL absolute_url = test_server_->GetURL(request.relative_url);
  std::string path = absolute_url.path();
  auto it = path_response_map_.find(absolute_url.path_piece());
  if (it == path_response_map_.end())
    return nullptr;
  const ServerResponse& config = it->second;
  auto http_response = std::make_unique<DelayedHttpResponse>(
      base::TimeDelta::FromMilliseconds(config.resolve_time_in_milliseconds));
  http_response->set_code(config.status_code);
  http_response->set_content_type("text/plain");
  for (auto header_pair : config.headers) {
    http_response->AddCustomHeader(header_pair.first, header_pair.second);
  }
  return http_response;
}

void ChangePasswordNavigationThrottleBrowserTestBase::TestNavigationThrottle(
    const GURL& navigate_url,
    const GURL& expected_url,
    base::Optional<url::Origin> initiator_origin) {
  AddHttpsCertificate();

  NavigateParams params(browser(), navigate_url, page_transition());
  params.initiator_origin = std::move(initiator_origin);
  TestNavigationObserver observer(expected_url);
  observer.WatchExistingWebContents();
  Navigate(&params);
  observer.Wait();

  EXPECT_EQ(observer.last_navigation_url(), expected_url);
}

// Browser Test that checks navigation to /.well-known/change-password path and
// redirection to change password URL returned by Change Password Service.
// Enables kWellKnownChangePassword feature.
class WellKnownChangePasswordNavigationThrottleBrowserTest
    : public ChangePasswordNavigationThrottleBrowserTestBase {
 public:
  WellKnownChangePasswordNavigationThrottleBrowserTest() {
    feature_list_.InitAndEnableFeature(
        password_manager::features::kWellKnownChangePassword);
  }

  void SetUpOnMainThread() override;
  void TestNavigationThrottleForLocalhost(const std::string& expected_path);

  MockChangePasswordUrlService* url_service_ = nullptr;

 private:
  base::test::ScopedFeatureList feature_list_;
};

void WellKnownChangePasswordNavigationThrottleBrowserTest::SetUpOnMainThread() {
  Initialize();
  url_service_ =
      ChangePasswordUrlServiceFactory::GetInstance()
          ->SetTestingSubclassFactoryAndUse(
              browser()->profile(),
              base::BindRepeating([](content::BrowserContext*) {
                return std::make_unique<
                    testing::StrictMock<MockChangePasswordUrlService>>();
              }));
}

void WellKnownChangePasswordNavigationThrottleBrowserTest::
    TestNavigationThrottleForLocalhost(const std::string& expected_path) {
  GURL navigate_url = test_server_->GetURL(kWellKnownChangePasswordPath);
  GURL expected_url = test_server_->GetURL(expected_path);

  TestNavigationThrottle(navigate_url, expected_url);
}

IN_PROC_BROWSER_TEST_P(WellKnownChangePasswordNavigationThrottleBrowserTest,
                       SupportForChangePassword) {
  path_response_map_[kWellKnownChangePasswordPath] = {
      net::HTTP_OK, {}, response_delays().change_password_delay};
  path_response_map_[kWellKnownNotExistingResourcePath] = {
      net::HTTP_NOT_FOUND, {}, response_delays().not_exist_delay};

  TestNavigationThrottleForLocalhost(
      /*expected_path=*/kWellKnownChangePasswordPath);
  if (page_transition() & ui::PAGE_TRANSITION_FROM_API) {
    ExpectUkmMetric(
        WellKnownChangePasswordResult::kUsedWellKnownChangePassword);
  } else {
    EXPECT_TRUE(
        test_recorder()->GetEntriesByName(UkmBuilder::kEntryName).empty());
  }
}

IN_PROC_BROWSER_TEST_P(WellKnownChangePasswordNavigationThrottleBrowserTest,
                       SupportForChangePassword_WithRedirect) {
  path_response_map_[kWellKnownChangePasswordPath] = {
      net::HTTP_PERMANENT_REDIRECT,
      {std::make_pair("Location", "/change-password")},
      response_delays().change_password_delay};
  path_response_map_[kWellKnownNotExistingResourcePath] = {
      net::HTTP_NOT_FOUND, {}, response_delays().not_exist_delay};
  path_response_map_["/change-password"] = {net::HTTP_OK, {}, 0};

  TestNavigationThrottleForLocalhost(/*expected_path=*/"/change-password");
  if (page_transition() & ui::PAGE_TRANSITION_FROM_API) {
    ExpectUkmMetric(
        WellKnownChangePasswordResult::kUsedWellKnownChangePassword);
  } else {
    EXPECT_TRUE(
        test_recorder()->GetEntriesByName(UkmBuilder::kEntryName).empty());
  }
}

// Tests that the throttle behaves correctly for all types of page transitions
// when initiated from within Chrome settings.
IN_PROC_BROWSER_TEST_P(
    WellKnownChangePasswordNavigationThrottleBrowserTest,
    SupportForChangePassword_WithRedirect_FromChromeSettings) {
  path_response_map_[kWellKnownChangePasswordPath] = {
      net::HTTP_PERMANENT_REDIRECT,
      {std::make_pair("Location", "/change-password")},
      response_delays().change_password_delay};
  path_response_map_[kWellKnownNotExistingResourcePath] = {
      net::HTTP_NOT_FOUND, {}, response_delays().not_exist_delay};
  path_response_map_["/change-password"] = {net::HTTP_OK, {}, 0};

  GURL navigate_url = test_server_->GetURL(kWellKnownChangePasswordPath);
  GURL expected_url = test_server_->GetURL("/change-password");
  TestNavigationThrottle(
      navigate_url, expected_url,
      url::Origin::Create(GURL("chrome://settings/passwords/check")));

  ExpectUkmMetric(WellKnownChangePasswordResult::kUsedWellKnownChangePassword);
}

// Tests that the throttle behaves correctly for all types of page transitions
// when initiated from p.g.c.'s checkup.
IN_PROC_BROWSER_TEST_P(
    WellKnownChangePasswordNavigationThrottleBrowserTest,
    SupportForChangePassword_WithRedirect_FromPasswordCheckup) {
  path_response_map_[kWellKnownChangePasswordPath] = {
      net::HTTP_PERMANENT_REDIRECT,
      {std::make_pair("Location", "/change-password")},
      response_delays().change_password_delay};
  path_response_map_[kWellKnownNotExistingResourcePath] = {
      net::HTTP_NOT_FOUND, {}, response_delays().not_exist_delay};
  path_response_map_["/change-password"] = {net::HTTP_OK, {}, 0};

  GURL navigate_url = test_server_->GetURL(kWellKnownChangePasswordPath);
  GURL expected_url = test_server_->GetURL("/change-password");
  TestNavigationThrottle(
      navigate_url, expected_url,
      url::Origin::Create(GURL("https://passwords.google.com/checkup")));

  ExpectUkmMetric(WellKnownChangePasswordResult::kUsedWellKnownChangePassword);
}

IN_PROC_BROWSER_TEST_P(WellKnownChangePasswordNavigationThrottleBrowserTest,
                       SupportForChangePassword_PartialContent) {
  path_response_map_[kWellKnownChangePasswordPath] = {
      net::HTTP_PARTIAL_CONTENT, {}, response_delays().change_password_delay};
  path_response_map_[kWellKnownNotExistingResourcePath] = {
      net::HTTP_NOT_FOUND, {}, response_delays().not_exist_delay};

  TestNavigationThrottleForLocalhost(
      /*expected_path=*/kWellKnownChangePasswordPath);
  if (page_transition() & ui::PAGE_TRANSITION_FROM_API) {
    ExpectUkmMetric(
        WellKnownChangePasswordResult::kUsedWellKnownChangePassword);
  } else {
    EXPECT_TRUE(
        test_recorder()->GetEntriesByName(UkmBuilder::kEntryName).empty());
  }
}

IN_PROC_BROWSER_TEST_P(
    WellKnownChangePasswordNavigationThrottleBrowserTest,
    SupportForChangePassword_WithXOriginRedirectToNotFoundPage) {
  GURL change_password_url =
      test_server_->GetURL("example.com", "/change-password");
  path_response_map_[kWellKnownChangePasswordPath] = {
      net::HTTP_PERMANENT_REDIRECT,
      {std::make_pair("Location", change_password_url.spec())},
      response_delays().change_password_delay};
  path_response_map_[kWellKnownNotExistingResourcePath] = {
      net::HTTP_PERMANENT_REDIRECT,
      {std::make_pair("Location", "/not-found")},
      response_delays().not_exist_delay};
  path_response_map_["/change-password"] = {net::HTTP_OK, {}, 0};
  path_response_map_["/not-found"] = {net::HTTP_NOT_FOUND, {}, 0};

  TestNavigationThrottle(test_server_->GetURL(kWellKnownChangePasswordPath),
                         change_password_url);
  if (page_transition() & ui::PAGE_TRANSITION_FROM_API) {
    ExpectUkmMetric(
        WellKnownChangePasswordResult::kUsedWellKnownChangePassword);
  } else {
    EXPECT_TRUE(
        test_recorder()->GetEntriesByName(UkmBuilder::kEntryName).empty());
  }
}

IN_PROC_BROWSER_TEST_P(WellKnownChangePasswordNavigationThrottleBrowserTest,
                       NoSupportForChangePassword_NotFound) {
  path_response_map_[kWellKnownChangePasswordPath] = {
      net::HTTP_NOT_FOUND, {}, response_delays().change_password_delay};
  path_response_map_[kWellKnownNotExistingResourcePath] = {
      net::HTTP_NOT_FOUND, {}, response_delays().not_exist_delay};

  if (page_transition() & ui::PAGE_TRANSITION_FROM_API) {
    EXPECT_CALL(*url_service_, GetChangePasswordUrl(test_server_->GetURL(
                                   kWellKnownChangePasswordPath)))
        .WillOnce(Return(GURL()));
    TestNavigationThrottleForLocalhost(/*expected_path=*/"/");
    ExpectUkmMetric(WellKnownChangePasswordResult::kFallbackToOriginUrl);
  } else {
    TestNavigationThrottleForLocalhost(
        /*expected_path=*/kWellKnownChangePasswordPath);
    EXPECT_TRUE(
        test_recorder()->GetEntriesByName(UkmBuilder::kEntryName).empty());
  }
}

IN_PROC_BROWSER_TEST_P(WellKnownChangePasswordNavigationThrottleBrowserTest,
                       NoSupportForChangePassword_WithUrlOverride) {
  path_response_map_[kWellKnownChangePasswordPath] = {
      net::HTTP_NOT_FOUND, {}, response_delays().change_password_delay};
  path_response_map_[kWellKnownNotExistingResourcePath] = {
      net::HTTP_NOT_FOUND, {}, response_delays().not_exist_delay};

  if (page_transition() & ui::PAGE_TRANSITION_FROM_API) {
    EXPECT_CALL(*url_service_, GetChangePasswordUrl(test_server_->GetURL(
                                   kWellKnownChangePasswordPath)))
        .WillOnce(Return(test_server_->GetURL(kMockChangePasswordPath)));
    TestNavigationThrottleForLocalhost(
        /*expected_path=*/kMockChangePasswordPath);
    ExpectUkmMetric(WellKnownChangePasswordResult::kFallbackToOverrideUrl);
  } else {
    TestNavigationThrottleForLocalhost(
        /*expected_path=*/kWellKnownChangePasswordPath);
    EXPECT_TRUE(
        test_recorder()->GetEntriesByName(UkmBuilder::kEntryName).empty());
  }
}

// Single page applications often return 200 for all paths
IN_PROC_BROWSER_TEST_P(WellKnownChangePasswordNavigationThrottleBrowserTest,
                       NoSupportForChangePassword_Ok) {
  path_response_map_[kWellKnownChangePasswordPath] = {
      net::HTTP_OK, {}, response_delays().change_password_delay};
  path_response_map_[kWellKnownNotExistingResourcePath] = {
      net::HTTP_OK, {}, response_delays().not_exist_delay};

  if (page_transition() & ui::PAGE_TRANSITION_FROM_API) {
    EXPECT_CALL(*url_service_, GetChangePasswordUrl(test_server_->GetURL(
                                   kWellKnownChangePasswordPath)))
        .WillOnce(Return(GURL()));
    TestNavigationThrottleForLocalhost(/*expected_path=*/"/");
    ExpectUkmMetric(WellKnownChangePasswordResult::kFallbackToOriginUrl);
  } else {
    TestNavigationThrottleForLocalhost(
        /*expected_path=*/kWellKnownChangePasswordPath);
    EXPECT_TRUE(
        test_recorder()->GetEntriesByName(UkmBuilder::kEntryName).empty());
  }
}

IN_PROC_BROWSER_TEST_P(
    WellKnownChangePasswordNavigationThrottleBrowserTest,
    NoSupportForChangePassword_WithXOriginRedirectToNotFoundPage) {
  // Test a cross-origin redirect to a 404 page. Ensure that we try to obtain
  // the ChangePasswordUrl for the original origin.
  GURL not_found_url = test_server_->GetURL("example.com", "/not-found");
  path_response_map_[kWellKnownChangePasswordPath] = {
      net::HTTP_PERMANENT_REDIRECT,
      {std::make_pair("Location", not_found_url.spec())},
      response_delays().change_password_delay};
  path_response_map_[kWellKnownNotExistingResourcePath] = {
      net::HTTP_PERMANENT_REDIRECT,
      {std::make_pair("Location", not_found_url.spec())},
      response_delays().not_exist_delay};
  path_response_map_["/not-found"] = {net::HTTP_NOT_FOUND, {}, 0};

  if (page_transition() & ui::PAGE_TRANSITION_FROM_API) {
    EXPECT_CALL(*url_service_, GetChangePasswordUrl(test_server_->GetURL(
                                   kWellKnownChangePasswordPath)))
        .WillOnce(Return(GURL()));
    TestNavigationThrottleForLocalhost(/*expected_path=*/"/");
    ExpectUkmMetric(WellKnownChangePasswordResult::kFallbackToOriginUrl);
  } else {
    TestNavigationThrottle(test_server_->GetURL(kWellKnownChangePasswordPath),
                           not_found_url);
    EXPECT_TRUE(
        test_recorder()->GetEntriesByName(UkmBuilder::kEntryName).empty());
  }
}

IN_PROC_BROWSER_TEST_P(WellKnownChangePasswordNavigationThrottleBrowserTest,
                       NoSupportForChangePassword_WillFailRequest) {
  path_response_map_[kWellKnownChangePasswordPath] = {
      net::HTTP_PERMANENT_REDIRECT,
      {std::make_pair("Location", "/change-password")},
      response_delays().change_password_delay};
  path_response_map_[kWellKnownNotExistingResourcePath] = {
      net::HTTP_NOT_FOUND, {}, response_delays().not_exist_delay};

  // Make request fail.
  scoped_refptr<net::X509Certificate> cert = test_server_->GetCertificate();
  net::CertVerifyResult verify_result;
  verify_result.cert_status = 0;
  verify_result.verified_cert = cert;
  mock_cert_verifier()->AddResultForCert(cert.get(), verify_result,
                                         net::ERR_BLOCKED_BY_CLIENT);

  GURL url = test_server_->GetURL(kWellKnownChangePasswordPath);
  NavigateParams params(browser(), url, page_transition());
  Navigate(&params);
  TestNavigationObserver observer(params.navigated_or_inserted_contents);
  observer.Wait();

  EXPECT_EQ(observer.last_navigation_url(), url);
  // Expect no UKMs saved.
  EXPECT_TRUE(
      test_recorder()->GetEntriesByName(UkmBuilder::kEntryName).empty());
}

constexpr char kExample1Hostname[] = "example1.com";
constexpr char kExample1ChangePasswordRelativeUrl[] = "/settings/password";
constexpr char kExample2Hostname[] = "example2.com";
constexpr char kExample2ChangePasswordRelativeUrl[] = "/change-pwd";

// Browser Test that checks redirection to change password URL returned by
// Affiliation Service. Enables kWellKnownChangePassword and
// kChangePasswordAffiliationInfo features.
class AffiliationChangePasswordNavigationThrottleBrowserTest
    : public ChangePasswordNavigationThrottleBrowserTestBase {
 public:
  AffiliationChangePasswordNavigationThrottleBrowserTest() {
    feature_list_.InitWithFeatures(
        {password_manager::features::kWellKnownChangePassword,
         password_manager::features::kChangePasswordAffiliationInfo},
        {});
    sync_service_.SetFirstSetupComplete(true);
    sync_service_.SetIsUsingSecondaryPassphrase(false);
  }

  void SetUpOnMainThread() override;
  // The facet's |url| and corresponding |change_password_url| cannot be
  // hardcoded in the method as the the ports are randomly generated and the
  // response would no longer match requested or expected url.
  std::string CreateResponse(const GURL& requested_url,
                             const GURL& requested_change_password_url,
                             const GURL& other_url,
                             const GURL& other_change_password_url);

  syncer::TestSyncService sync_service_;
  network::TestURLLoaderFactory test_url_loader_factory_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

void AffiliationChangePasswordNavigationThrottleBrowserTest::
    SetUpOnMainThread() {
  Initialize();

  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory_);

  auto* affiliation_service =
      static_cast<password_manager::AffiliationServiceImpl*>(
          AffiliationServiceFactory::GetForProfile(browser()->profile()));
  affiliation_service->SetSyncServiceForTesting(&sync_service_);
  affiliation_service->SetURLLoaderFactoryForTesting(shared_url_loader_factory);
}

std::string
AffiliationChangePasswordNavigationThrottleBrowserTest::CreateResponse(
    const GURL& requested_url,
    const GURL& requested_change_password_url,
    const GURL& other_url,
    const GURL& other_change_password_url) {
  affiliation_pb::LookupAffiliationResponse response;
  affiliation_pb::FacetGroup* facet_group = response.add_group();

  affiliation_pb::Facet* requested_facet = facet_group->add_facet();
  requested_facet->set_id(requested_url.spec());
  requested_facet->mutable_change_password_info()->set_change_password_url(
      requested_change_password_url.spec());

  affiliation_pb::Facet* other_facet = facet_group->add_facet();
  other_facet->set_id(other_url.spec());
  other_facet->mutable_change_password_info()->set_change_password_url(
      other_change_password_url.spec());

  return response.SerializeAsString();
}

IN_PROC_BROWSER_TEST_P(AffiliationChangePasswordNavigationThrottleBrowserTest,
                       NavigatesToChangePasswordURLOfRequestedURL) {
  GURL requested_origin = test_server_->GetURL(kExample1Hostname, "/");
  GURL requested_change_password_url = test_server_->GetURL(
      kExample1Hostname, kExample1ChangePasswordRelativeUrl);

  GURL other_origin = test_server_->GetURL(kExample2Hostname, "/");
  GURL other_change_password_url = test_server_->GetURL(
      kExample2Hostname, kExample2ChangePasswordRelativeUrl);

  std::string fake_response =
      CreateResponse(requested_origin, requested_change_password_url,
                     other_origin, other_change_password_url);
  test_url_loader_factory_.AddResponse(
      password_manager::AffiliationFetcher::BuildQueryURL().spec(),
      fake_response);

  path_response_map_[kWellKnownChangePasswordPath] = {
      net::HTTP_NOT_FOUND, {}, response_delays().change_password_delay};
  path_response_map_[kWellKnownNotExistingResourcePath] = {
      net::HTTP_NOT_FOUND, {}, response_delays().not_exist_delay};

  TestNavigationThrottle(/*navigate_url=*/test_server_->GetURL(
                             kExample1Hostname, kWellKnownChangePasswordPath),
                         /*expected_url=*/requested_change_password_url);
  ExpectUkmMetric(WellKnownChangePasswordResult::kFallbackToOverrideUrl);
}

IN_PROC_BROWSER_TEST_P(AffiliationChangePasswordNavigationThrottleBrowserTest,
                       NavigatesToChangePasswordURLOfOtherURLIfEmpty) {
  GURL requested_origin = test_server_->GetURL(kExample1Hostname, "/");

  GURL other_origin = test_server_->GetURL(kExample2Hostname, "/");
  GURL other_change_password_url = test_server_->GetURL(
      kExample2Hostname, kExample2ChangePasswordRelativeUrl);

  std::string fake_response = CreateResponse(
      requested_origin, GURL(), other_origin, other_change_password_url);
  test_url_loader_factory_.AddResponse(
      password_manager::AffiliationFetcher::BuildQueryURL().spec(),
      fake_response);

  path_response_map_[kWellKnownChangePasswordPath] = {
      net::HTTP_NOT_FOUND, {}, response_delays().change_password_delay};
  path_response_map_[kWellKnownNotExistingResourcePath] = {
      net::HTTP_NOT_FOUND, {}, response_delays().not_exist_delay};

  TestNavigationThrottle(/*navigate_url=*/test_server_->GetURL(
                             kExample1Hostname, kWellKnownChangePasswordPath),
                         /*expected_url=*/other_change_password_url);
  ExpectUkmMetric(WellKnownChangePasswordResult::kFallbackToOverrideUrl);
}

constexpr ResponseDelayParams kDelayParams[] = {{0, 1}, {1, 0}};

INSTANTIATE_TEST_SUITE_P(
    All,
    WellKnownChangePasswordNavigationThrottleBrowserTest,
    ::testing::Combine(::testing::Values(ui::PAGE_TRANSITION_LINK,
                                         ui::PAGE_TRANSITION_FROM_API),
                       ::testing::ValuesIn(kDelayParams)));

INSTANTIATE_TEST_SUITE_P(
    All,
    AffiliationChangePasswordNavigationThrottleBrowserTest,
    ::testing::Combine(::testing::Values(ui::PAGE_TRANSITION_FROM_API),
                       ::testing::ValuesIn(kDelayParams)));
