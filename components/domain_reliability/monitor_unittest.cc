// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/domain_reliability/monitor.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/domain_reliability/baked_in_configs.h"
#include "components/domain_reliability/beacon.h"
#include "components/domain_reliability/config.h"
#include "components/domain_reliability/google_configs.h"
#include "components/domain_reliability/test_util.h"
#include "net/base/isolation_info.h"
#include "net/base/load_timing_info.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/proxy_chain.h"
#include "net/base/request_priority.h"
#include "net/http/http_connection_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/gtest_util.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_error_codes.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace domain_reliability {

namespace {

typedef std::vector<const DomainReliabilityBeacon*> BeaconVector;

const char kBeaconOutcomeHistogram[] = "Net.DomainReliability.BeaconOutcome";

scoped_refptr<net::HttpResponseHeaders> MakeHttpResponseHeaders(
    std::string_view headers) {
  return base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(headers));
}

size_t CountQueuedBeacons(const DomainReliabilityContext* context) {
  BeaconVector beacons;
  context->GetQueuedBeaconsForTesting(&beacons);
  return beacons.size();
}

}  // namespace

class DomainReliabilityMonitorTest : public testing::Test {
 protected:
  typedef DomainReliabilityMonitor::RequestInfo RequestInfo;

  DomainReliabilityMonitorTest()
      : url_request_context_(
            net::CreateTestURLRequestContextBuilder()->Build()),
        time_(new MockTime()),
        monitor_(url_request_context_.get(),
                 "test-reporter",
                 DomainReliabilityContext::UploadAllowedCallback(),
                 std::unique_ptr<MockableTime>(time_)) {
    monitor_.SetDiscardUploads(false);
  }

  ~DomainReliabilityMonitorTest() override {
    monitor_.Shutdown();
  }

  static RequestInfo MakeRequestInfo() {
    RequestInfo request;
    request.net_error = net::OK;
    request.response_info.remote_endpoint =
        net::IPEndPoint(net::IPAddress(12, 34, 56, 78), 80);
    request.response_info.headers = MakeHttpResponseHeaders(
        "HTTP/1.1 200 OK\n\n");
    request.response_info.was_cached = false;
    request.response_info.network_accessed = true;
    request.allow_credentials = true;
    request.upload_depth = 0;
    return request;
  }

  RequestInfo MakeFailedRequest(const GURL& url) {
    RequestInfo request = MakeRequestInfo();
    request.url = url;
    request.net_error = net::ERR_CONNECTION_RESET;
    return request;
  }

  void OnRequestLegComplete(const RequestInfo& info) {
    monitor_.OnRequestLegCompleteForTesting(info);
  }

  const DomainReliabilityContext* CreateAndAddContext() {
    return monitor_.AddContextForTesting(MakeTestConfig());
  }

  const DomainReliabilityContext* CreateAndAddContextForOrigin(
      const url::Origin& origin,
      bool wildcard) {
    std::unique_ptr<DomainReliabilityConfig> config(
        MakeTestConfigWithOrigin(origin));
    config->include_subdomains = wildcard;
    return monitor_.AddContextForTesting(std::move(config));
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  std::unique_ptr<net::URLRequestContext> url_request_context_;
  raw_ptr<MockTime, DanglingUntriaged> time_;
  DomainReliabilityMonitor monitor_;
  DomainReliabilityMonitor::RequestInfo request_;
};

namespace {

TEST_F(DomainReliabilityMonitorTest, Create) {
}

TEST_F(DomainReliabilityMonitorTest, NoContext) {
  const DomainReliabilityContext* context = CreateAndAddContext();

  RequestInfo request = MakeRequestInfo();
  request.url = GURL("http://no-context/");
  OnRequestLegComplete(request);

  EXPECT_EQ(0u, CountQueuedBeacons(context));
}

TEST_F(DomainReliabilityMonitorTest, NetworkFailure) {
  const DomainReliabilityContext* context = CreateAndAddContext();

  RequestInfo request = MakeRequestInfo();
  request.url = GURL("http://example/");
  request.net_error = net::ERR_CONNECTION_RESET;
  request.response_info.headers = nullptr;
  OnRequestLegComplete(request);

  EXPECT_EQ(1u, CountQueuedBeacons(context));
}

TEST_F(DomainReliabilityMonitorTest, GoAwayWithPortMigrationDetected) {
  const DomainReliabilityContext* context = CreateAndAddContext();

  RequestInfo request = MakeRequestInfo();
  request.url = GURL("http://example/");
  request.details.quic_port_migration_detected = true;
  request.response_info.headers = nullptr;
  OnRequestLegComplete(request);

  EXPECT_EQ(1u, CountQueuedBeacons(context));
}

TEST_F(DomainReliabilityMonitorTest, ServerFailure) {
  const DomainReliabilityContext* context = CreateAndAddContext();

  RequestInfo request = MakeRequestInfo();
  request.url = GURL("http://example/");
  request.response_info.headers =
      MakeHttpResponseHeaders("HTTP/1.1 500 :(\n\n");
  OnRequestLegComplete(request);

  EXPECT_EQ(1u, CountQueuedBeacons(context));
}

// Make sure the monitor does not log requests that did not access the network.
TEST_F(DomainReliabilityMonitorTest, DidNotAccessNetwork) {
  const DomainReliabilityContext* context = CreateAndAddContext();

  RequestInfo request = MakeRequestInfo();
  request.url = GURL("http://example/");
  request.response_info.network_accessed = false;
  OnRequestLegComplete(request);

  EXPECT_EQ(0u, CountQueuedBeacons(context));
}

// Make sure the monitor does not log requests that don't send credentials.
TEST_F(DomainReliabilityMonitorTest, DoNotSendCookies) {
  const DomainReliabilityContext* context = CreateAndAddContext();

  RequestInfo request = MakeRequestInfo();
  request.url = GURL("http://example/");
  request.allow_credentials = false;
  OnRequestLegComplete(request);

  EXPECT_EQ(0u, CountQueuedBeacons(context));
}

// Make sure the monitor does not log a network-local error.
TEST_F(DomainReliabilityMonitorTest, LocalError) {
  const DomainReliabilityContext* context = CreateAndAddContext();

  RequestInfo request = MakeRequestInfo();
  request.url = GURL("http://example/");
  request.net_error = net::ERR_PROXY_CONNECTION_FAILED;
  OnRequestLegComplete(request);

  EXPECT_EQ(0u, CountQueuedBeacons(context));
}

// Make sure the monitor does not log the proxy's IP if one was used.
TEST_F(DomainReliabilityMonitorTest, WasFetchedViaProxy) {
  const DomainReliabilityContext* context = CreateAndAddContext();

  RequestInfo request = MakeRequestInfo();
  request.url = GURL("http://example/");
  request.net_error = net::ERR_CONNECTION_RESET;
  request.response_info.remote_endpoint =
      net::IPEndPoint(net::IPAddress(127, 0, 0, 1), 3128);
  request.response_info.proxy_chain = net::ProxyChain::FromSchemeHostAndPort(
      net::ProxyServer::SCHEME_HTTP, "foo", 80);
  OnRequestLegComplete(request);

  BeaconVector beacons;
  context->GetQueuedBeaconsForTesting(&beacons);
  EXPECT_EQ(1u, beacons.size());
  EXPECT_TRUE(beacons[0]->server_ip.empty());
}

// Make sure the monitor does not log the cached IP returned after a successful
// cache revalidation request.
TEST_F(DomainReliabilityMonitorTest,
       NoCachedIPFromSuccessfulRevalidationRequest) {
  std::unique_ptr<DomainReliabilityConfig> config = MakeTestConfig();
  config->success_sample_rate = 1.0;
  const DomainReliabilityContext* context =
      monitor_.AddContextForTesting(std::move(config));

  RequestInfo request = MakeRequestInfo();
  request.url = GURL("http://example/");
  request.response_info.was_cached = true;
  OnRequestLegComplete(request);

  BeaconVector beacons;
  context->GetQueuedBeaconsForTesting(&beacons);
  EXPECT_EQ(1u, beacons.size());
  EXPECT_TRUE(beacons[0]->server_ip.empty());
}

// Make sure the monitor does not log the cached IP returned with a failed
// cache revalidation request.
TEST_F(DomainReliabilityMonitorTest, NoCachedIPFromFailedRevalidationRequest) {
  const DomainReliabilityContext* context = CreateAndAddContext();

  RequestInfo request = MakeRequestInfo();
  request.url = GURL("http://example/");
  request.response_info.was_cached = true;
  request.net_error = net::ERR_NAME_RESOLUTION_FAILED;
  OnRequestLegComplete(request);

  BeaconVector beacons;
  context->GetQueuedBeaconsForTesting(&beacons);
  EXPECT_EQ(1u, beacons.size());
  EXPECT_TRUE(beacons[0]->server_ip.empty());
}

// Make sure the monitor does log uploads, even when credentials are not
// allowed.
TEST_F(DomainReliabilityMonitorTest, Upload) {
  const DomainReliabilityContext* context = CreateAndAddContext();

  RequestInfo request = MakeRequestInfo();
  request.url = GURL("http://example/");
  request.allow_credentials = false;
  request.net_error = net::ERR_CONNECTION_RESET;
  request.upload_depth = 1;
  OnRequestLegComplete(request);

  EXPECT_EQ(1u, CountQueuedBeacons(context));
}

// Make sure IsolationInfo is populated in the beacon, or not, depending on
// whether cache partitioning is enabled.
TEST_F(DomainReliabilityMonitorTest, IsolationInfo) {
  const auto kReportOrigin =
      url::Origin::Create(GURL("https://www.example.com/"));
  const auto kReportIsolationInfo = net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kMainFrame, kReportOrigin, kReportOrigin,
      net::SiteForCookies::FromOrigin(kReportOrigin));

  // The IsolationInfo used for the upload should be derived from the request
  // but should reflect that the upload is not a navigation and should not be
  // sent with credentials.
  const auto kExpectedIsolationInfo = net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kOther, kReportOrigin, kReportOrigin,
      net::SiteForCookies());

  const DomainReliabilityContext* context = CreateAndAddContext();

  size_t index = 0;
  for (bool partitioning_enabled : {false, true}) {
    SCOPED_TRACE(partitioning_enabled);

    base::test::ScopedFeatureList feature_list;
    if (partitioning_enabled) {
      feature_list.InitAndEnableFeature(
          net::features::kSplitCacheByNetworkIsolationKey);
    } else {
      feature_list.InitAndDisableFeature(
          net::features::kSplitCacheByNetworkIsolationKey);
    }
    RequestInfo request = MakeRequestInfo();
    request.url = GURL("http://example/");
    request.allow_credentials = false;
    request.net_error = net::ERR_CONNECTION_RESET;
    request.upload_depth = 1;
    request.isolation_info = kReportIsolationInfo;
    OnRequestLegComplete(request);

    BeaconVector beacons;
    context->GetQueuedBeaconsForTesting(&beacons);
    ASSERT_EQ(index + 1, beacons.size());
    if (partitioning_enabled) {
      EXPECT_TRUE(kExpectedIsolationInfo.IsEqualForTesting(
          beacons[index]->isolation_info));
    } else {
      EXPECT_TRUE(beacons[index]->isolation_info.IsEmpty());
    }

    ++index;
  }
}

// Will fail when baked-in configs expire, as a reminder to update them.
// (File a bug in Internals>Network>ReportingAndNEL if this starts failing.)
TEST_F(DomainReliabilityMonitorTest, BakedInAndGoogleConfigs) {
  // AddBakedInConfigs DCHECKs that the baked-in configs parse correctly and are
  // valid, so this unittest will fail if someone tries to add an invalid config
  // to the source tree.
  monitor_.AddBakedInConfigs();

  // Count the number of baked-in configs.
  size_t num_baked_in_configs = 0u;
  for (const char* const* p = kBakedInJsonConfigs; *p; ++p) {
    ++num_baked_in_configs;
  }
  EXPECT_GT(num_baked_in_configs, 0u);

  EXPECT_EQ(num_baked_in_configs, monitor_.contexts_size_for_testing());

  // Also count the Google configs stored in abbreviated form.
  std::vector<std::unique_ptr<const DomainReliabilityConfig>> google_configs =
      GetAllGoogleConfigsForTesting();
  size_t num_google_configs = google_configs.size();

  for (std::unique_ptr<const DomainReliabilityConfig>& config :
       google_configs) {
    monitor_.AddContextForTesting(std::move(config));
  }

  // The monitor should have contexts for all of the baked-in configs and Google
  // configs. This also ensures that the configs have unique hostnames, i.e.
  // none of them have overwritten each other.
  EXPECT_EQ(num_baked_in_configs + num_google_configs,
            monitor_.contexts_size_for_testing());
}

// Test that Google configs are created only when needed.
TEST_F(DomainReliabilityMonitorTest, GoogleConfigOnDemand) {
  ASSERT_EQ(0u, monitor_.contexts_size_for_testing());

  // Failed request is required here to ensure the beacon is queued (since all
  // the Google configs have a 1.0 sample rate for failures, but a much lower
  // sample rate for successes).
  OnRequestLegComplete(MakeFailedRequest(GURL("https://google.ac")));
  EXPECT_EQ(1u, monitor_.contexts_size_for_testing());
  const DomainReliabilityContext* google_domain_context =
      monitor_.LookupContextForTesting("google.ac");
  EXPECT_TRUE(google_domain_context);
  EXPECT_EQ(1u, CountQueuedBeacons(google_domain_context));

  // This domain generates a config specific to the www subdomain.
  OnRequestLegComplete(MakeFailedRequest(GURL("https://www.google.ac")));
  EXPECT_EQ(2u, monitor_.contexts_size_for_testing());
  const DomainReliabilityContext* www_google_domain_context =
      monitor_.LookupContextForTesting("www.google.ac");
  EXPECT_TRUE(www_google_domain_context);
  EXPECT_EQ(1u, CountQueuedBeacons(www_google_domain_context));

  // Some other subdomain does not generate a new context because the google.ac
  // config includes subdomains. It queues a beacon for the already-existing
  // context.
  ASSERT_TRUE(google_domain_context->config().include_subdomains);
  OnRequestLegComplete(MakeFailedRequest(GURL("https://subdomain.google.ac")));
  EXPECT_EQ(2u, monitor_.contexts_size_for_testing());
  EXPECT_EQ(2u, CountQueuedBeacons(google_domain_context));

  // A domain with no Google config does not generate a new context.
  OnRequestLegComplete(MakeFailedRequest(GURL("https://not-google.com")));
  EXPECT_EQ(2u, monitor_.contexts_size_for_testing());
}

TEST_F(DomainReliabilityMonitorTest, ClearBeacons) {
  base::HistogramTester histograms;
  const DomainReliabilityContext* context = CreateAndAddContext();

  // Initially the monitor should have just the test context, with no beacons.
  EXPECT_EQ(1u, monitor_.contexts_size_for_testing());
  EXPECT_EQ(0u, CountQueuedBeacons(context));

  // Add a beacon.
  RequestInfo request = MakeRequestInfo();
  request.url = GURL("http://example/");
  request.net_error = net::ERR_CONNECTION_RESET;
  OnRequestLegComplete(request);

  // Make sure it was added.
  EXPECT_EQ(1u, CountQueuedBeacons(context));

  monitor_.ClearBrowsingData(CLEAR_BEACONS, base::NullCallback());

  // Make sure the beacon was cleared, but not the contexts.
  EXPECT_EQ(1u, monitor_.contexts_size_for_testing());
  EXPECT_EQ(0u, CountQueuedBeacons(context));

  histograms.ExpectBucketCount(kBeaconOutcomeHistogram,
                               DomainReliabilityBeacon::Outcome::kCleared, 1);
  histograms.ExpectTotalCount(kBeaconOutcomeHistogram, 1);
}

TEST_F(DomainReliabilityMonitorTest, ClearBeaconsWithFilter) {
  base::HistogramTester histograms;
  // Create two contexts, each with one beacon.
  GURL url1("http://example.com/");
  GURL url2("http://example.org/");
  auto origin1 = url::Origin::Create(url1);
  auto origin2 = url::Origin::Create(url2);

  const DomainReliabilityContext* context1 =
      CreateAndAddContextForOrigin(origin1, false);
  RequestInfo request = MakeRequestInfo();
  request.url = url1;
  request.net_error = net::ERR_CONNECTION_RESET;
  OnRequestLegComplete(request);

  const DomainReliabilityContext* context2 =
      CreateAndAddContextForOrigin(origin2, false);
  request = MakeRequestInfo();
  request.url = url2;
  request.net_error = net::ERR_CONNECTION_RESET;
  OnRequestLegComplete(request);

  // Delete the beacons for |origin1|.
  monitor_.ClearBrowsingData(CLEAR_BEACONS, base::BindRepeating(
                                                [](const url::Origin& origin1,
                                                   const url::Origin& origin2) {
                                                  return origin1 == origin2;
                                                },
                                                origin1));

  // Beacons for |context1| were cleared. Beacons for |context2| and
  // the contexts themselves were not.
  EXPECT_EQ(2u, monitor_.contexts_size_for_testing());
  EXPECT_EQ(0u, CountQueuedBeacons(context1));
  EXPECT_EQ(1u, CountQueuedBeacons(context2));

  histograms.ExpectBucketCount(kBeaconOutcomeHistogram,
                               DomainReliabilityBeacon::Outcome::kCleared, 1);
  histograms.ExpectTotalCount(kBeaconOutcomeHistogram, 1);
}

TEST_F(DomainReliabilityMonitorTest, ClearContexts) {
  base::HistogramTester histograms;
  const DomainReliabilityContext* context = CreateAndAddContext();

  // Initially the monitor should have just the test context.
  EXPECT_EQ(1u, monitor_.contexts_size_for_testing());

  // Add a beacon so we can test histogram behavior.
  RequestInfo request = MakeRequestInfo();
  request.url = GURL("http://example/");
  request.net_error = net::ERR_CONNECTION_RESET;
  OnRequestLegComplete(request);
  EXPECT_EQ(1u, CountQueuedBeacons(context));

  monitor_.ClearBrowsingData(CLEAR_CONTEXTS, base::NullCallback());

  // Clearing contexts should leave the monitor with none.
  EXPECT_EQ(0u, monitor_.contexts_size_for_testing());

  histograms.ExpectBucketCount(
      kBeaconOutcomeHistogram,
      DomainReliabilityBeacon::Outcome::kContextShutDown, 1);
  histograms.ExpectTotalCount(kBeaconOutcomeHistogram, 1);
}

TEST_F(DomainReliabilityMonitorTest, ClearContextsWithFilter) {
  auto origin1 = url::Origin::Create(GURL("http://example.com/"));
  auto origin2 = url::Origin::Create(GURL("http://example.org/"));

  CreateAndAddContextForOrigin(origin1, false);
  CreateAndAddContextForOrigin(origin2, false);

  EXPECT_EQ(2u, monitor_.contexts_size_for_testing());

  // Delete the contexts for |origin1|.
  monitor_.ClearBrowsingData(
      CLEAR_CONTEXTS,
      base::BindRepeating(
          [](const url::Origin& origin1, const url::Origin& origin2) {
            return origin1 == origin2;
          },
          origin1));

  // Only one of the contexts should have been deleted.
  EXPECT_EQ(1u, monitor_.contexts_size_for_testing());
}

TEST_F(DomainReliabilityMonitorTest, WildcardMatchesSelf) {
  const DomainReliabilityContext* context = CreateAndAddContextForOrigin(
      url::Origin::Create(GURL("https://wildcard/")), true);

  RequestInfo request = MakeRequestInfo();
  request.url = GURL("http://wildcard/");
  request.net_error = net::ERR_CONNECTION_RESET;
  OnRequestLegComplete(request);

  EXPECT_EQ(1u, CountQueuedBeacons(context));
}

TEST_F(DomainReliabilityMonitorTest, WildcardMatchesSubdomain) {
  const DomainReliabilityContext* context = CreateAndAddContextForOrigin(
      url::Origin::Create(GURL("https://wildcard/")), true);

  RequestInfo request = MakeRequestInfo();
  request.url = GURL("http://test.wildcard/");
  request.net_error = net::ERR_CONNECTION_RESET;
  OnRequestLegComplete(request);

  EXPECT_EQ(1u, CountQueuedBeacons(context));
}

TEST_F(DomainReliabilityMonitorTest, WildcardDoesntMatchSubsubdomain) {
  const DomainReliabilityContext* context = CreateAndAddContextForOrigin(
      url::Origin::Create(GURL("https://wildcard/")), true);

  RequestInfo request = MakeRequestInfo();
  request.url = GURL("http://test.test.wildcard/");
  request.net_error = net::ERR_CONNECTION_RESET;
  OnRequestLegComplete(request);

  EXPECT_EQ(0u, CountQueuedBeacons(context));
}

TEST_F(DomainReliabilityMonitorTest, WildcardPrefersSelfToParentWildcard) {
  const DomainReliabilityContext* context1 = CreateAndAddContextForOrigin(
      url::Origin::Create(GURL("https://test.wildcard/")), false);
  const DomainReliabilityContext* context2 = CreateAndAddContextForOrigin(
      url::Origin::Create(GURL("https://wildcard/")), true);

  RequestInfo request = MakeRequestInfo();
  request.url = GURL("http://test.wildcard/");
  request.net_error = net::ERR_CONNECTION_RESET;
  OnRequestLegComplete(request);

  EXPECT_EQ(1u, CountQueuedBeacons(context1));
  EXPECT_EQ(0u, CountQueuedBeacons(context2));
}

TEST_F(DomainReliabilityMonitorTest,
    WildcardPrefersSelfWildcardToParentWildcard) {
  const DomainReliabilityContext* context1 = CreateAndAddContextForOrigin(
      url::Origin::Create(GURL("https://test.wildcard/")), true);
  const DomainReliabilityContext* context2 = CreateAndAddContextForOrigin(
      url::Origin::Create(GURL("https://wildcard/")), true);

  RequestInfo request = MakeRequestInfo();
  request.url = GURL("http://test.wildcard/");
  request.net_error = net::ERR_CONNECTION_RESET;
  OnRequestLegComplete(request);

  EXPECT_EQ(1u, CountQueuedBeacons(context1));
  EXPECT_EQ(0u, CountQueuedBeacons(context2));
}

// Uses a real request, to make sure CreateBeaconFromAttempt() works as
// expected.
TEST_F(DomainReliabilityMonitorTest, RealRequest) {
  const net::IsolationInfo kIsolationInfo =
      net::IsolationInfo::CreateTransient();

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      net::features::kSplitCacheByNetworkIsolationKey);

  net::test_server::EmbeddedTestServer test_server;
  test_server.AddDefaultHandlers();
  ASSERT_TRUE(test_server.Start());

  std::unique_ptr<DomainReliabilityConfig> config(
      MakeTestConfigWithOrigin(test_server.GetOrigin()));
  const DomainReliabilityContext* context =
      monitor_.AddContextForTesting(std::move(config));

  base::TimeTicks start = base::TimeTicks::Now();

  net::TestDelegate test_delegate;
  std::unique_ptr<net::URLRequest> url_request =
      url_request_context_->CreateRequest(test_server.GetURL("/close-socket"),
                                          net::DEFAULT_PRIORITY, &test_delegate,
                                          TRAFFIC_ANNOTATION_FOR_TESTS);
  url_request->set_isolation_info(kIsolationInfo);
  url_request->Start();

  test_delegate.RunUntilComplete();
  EXPECT_THAT(test_delegate.request_status(),
              net::test::IsError(net::ERR_EMPTY_RESPONSE));

  net::LoadTimingInfo load_timing_info;
  url_request->GetLoadTimingInfo(&load_timing_info);
  base::TimeDelta expected_elapsed = base::Seconds(1);
  time_->Advance(load_timing_info.request_start - time_->NowTicks() +
                 expected_elapsed);

  monitor_.OnCompleted(url_request.get(), true /* started */,
                       test_delegate.request_status());
  BeaconVector beacons;
  context->GetQueuedBeaconsForTesting(&beacons);
  ASSERT_EQ(1u, beacons.size());
  EXPECT_EQ(url_request->url(), beacons[0]->url);
  EXPECT_TRUE(kIsolationInfo.IsEqualForTesting(beacons[0]->isolation_info));
  EXPECT_EQ("http.response.empty", beacons[0]->status);
  EXPECT_EQ("", beacons[0]->quic_error);
  EXPECT_EQ(net::ERR_EMPTY_RESPONSE, beacons[0]->chrome_error);
  EXPECT_EQ(test_server.base_url().host() + ":" + test_server.base_url().port(),
            beacons[0]->server_ip);
  EXPECT_FALSE(beacons[0]->was_proxied);
  EXPECT_EQ("HTTP", beacons[0]->protocol);
  EXPECT_FALSE(beacons[0]->details.quic_broken);
  EXPECT_EQ(quic::QUIC_NO_ERROR, beacons[0]->details.quic_connection_error);
  EXPECT_EQ(net::HttpConnectionInfo::kHTTP1_1,
            beacons[0]->details.connection_info);
  EXPECT_FALSE(beacons[0]->details.quic_port_migration_detected);
  EXPECT_EQ(-1, beacons[0]->http_response_code);
  EXPECT_EQ(expected_elapsed, beacons[0]->elapsed);
  EXPECT_LE(start, beacons[0]->start_time);
  EXPECT_EQ(0, beacons[0]->upload_depth);
  EXPECT_LE(0.99, beacons[0]->sample_rate);
}

}  // namespace

}  // namespace domain_reliability
