// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/domain_reliability/monitor.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/strings/string_piece.h"
#include "base/test/task_environment.h"
#include "components/domain_reliability/baked_in_configs.h"
#include "components/domain_reliability/beacon.h"
#include "components/domain_reliability/config.h"
#include "components/domain_reliability/google_configs.h"
#include "components/domain_reliability/test_util.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace domain_reliability {

namespace {

typedef std::vector<const DomainReliabilityBeacon*> BeaconVector;

scoped_refptr<net::HttpResponseHeaders> MakeHttpResponseHeaders(
    base::StringPiece headers) {
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
      : time_(new MockTime()),
        monitor_(&url_request_context_,
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
    request.response_info.was_fetched_via_proxy = false;
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
      const GURL& origin,
      bool wildcard) {
    std::unique_ptr<DomainReliabilityConfig> config(
        MakeTestConfigWithOrigin(origin));
    config->include_subdomains = wildcard;
    return monitor_.AddContextForTesting(std::move(config));
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  net::TestURLRequestContext url_request_context_;
  MockTime* time_;
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
  request.response_info.was_fetched_via_proxy = true;
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
}

TEST_F(DomainReliabilityMonitorTest, ClearBeaconsWithFilter) {
  // Create two contexts, each with one beacon.
  GURL origin1("http://example.com/");
  GURL origin2("http://example.org/");

  const DomainReliabilityContext* context1 =
      CreateAndAddContextForOrigin(origin1, false);
  RequestInfo request = MakeRequestInfo();
  request.url = origin1;
  request.net_error = net::ERR_CONNECTION_RESET;
  OnRequestLegComplete(request);

  const DomainReliabilityContext* context2 =
      CreateAndAddContextForOrigin(origin2, false);
  request = MakeRequestInfo();
  request.url = origin2;
  request.net_error = net::ERR_CONNECTION_RESET;
  OnRequestLegComplete(request);

  // Delete the beacons for |origin1|.
  monitor_.ClearBrowsingData(
      CLEAR_BEACONS,
      base::BindRepeating(
          [](const GURL& url1, const GURL& url2) { return url1 == url2; },
          origin1));

  // Beacons for |context1| were cleared. Beacons for |context2| and
  // the contexts themselves were not.
  EXPECT_EQ(2u, monitor_.contexts_size_for_testing());
  EXPECT_EQ(0u, CountQueuedBeacons(context1));
  EXPECT_EQ(1u, CountQueuedBeacons(context2));
}

TEST_F(DomainReliabilityMonitorTest, ClearContexts) {
  CreateAndAddContext();

  // Initially the monitor should have just the test context.
  EXPECT_EQ(1u, monitor_.contexts_size_for_testing());

  monitor_.ClearBrowsingData(CLEAR_CONTEXTS, base::NullCallback());

  // Clearing contexts should leave the monitor with none.
  EXPECT_EQ(0u, monitor_.contexts_size_for_testing());
}

TEST_F(DomainReliabilityMonitorTest, ClearContextsWithFilter) {
  GURL origin1("http://example.com/");
  GURL origin2("http://example.org/");

  CreateAndAddContextForOrigin(origin1, false);
  CreateAndAddContextForOrigin(origin2, false);

  EXPECT_EQ(2u, monitor_.contexts_size_for_testing());

  // Delete the contexts for |origin1|.
  monitor_.ClearBrowsingData(
      CLEAR_CONTEXTS,
      base::BindRepeating(
          [](const GURL& url1, const GURL& url2) { return url1 == url2; },
          origin1));

  // Only one of the contexts should have been deleted.
  EXPECT_EQ(1u, monitor_.contexts_size_for_testing());
}

TEST_F(DomainReliabilityMonitorTest, WildcardMatchesSelf) {
  const DomainReliabilityContext* context =
      CreateAndAddContextForOrigin(GURL("https://wildcard/"), true);

  RequestInfo request = MakeRequestInfo();
  request.url = GURL("http://wildcard/");
  request.net_error = net::ERR_CONNECTION_RESET;
  OnRequestLegComplete(request);

  EXPECT_EQ(1u, CountQueuedBeacons(context));
}

TEST_F(DomainReliabilityMonitorTest, WildcardMatchesSubdomain) {
  const DomainReliabilityContext* context =
      CreateAndAddContextForOrigin(GURL("https://wildcard/"), true);

  RequestInfo request = MakeRequestInfo();
  request.url = GURL("http://test.wildcard/");
  request.net_error = net::ERR_CONNECTION_RESET;
  OnRequestLegComplete(request);

  EXPECT_EQ(1u, CountQueuedBeacons(context));
}

TEST_F(DomainReliabilityMonitorTest, WildcardDoesntMatchSubsubdomain) {
  const DomainReliabilityContext* context =
      CreateAndAddContextForOrigin(GURL("https://wildcard/"), true);

  RequestInfo request = MakeRequestInfo();
  request.url = GURL("http://test.test.wildcard/");
  request.net_error = net::ERR_CONNECTION_RESET;
  OnRequestLegComplete(request);

  EXPECT_EQ(0u, CountQueuedBeacons(context));
}

TEST_F(DomainReliabilityMonitorTest, WildcardPrefersSelfToParentWildcard) {
  const DomainReliabilityContext* context1 =
      CreateAndAddContextForOrigin(GURL("https://test.wildcard/"), false);
  const DomainReliabilityContext* context2 =
      CreateAndAddContextForOrigin(GURL("https://wildcard/"), true);

  RequestInfo request = MakeRequestInfo();
  request.url = GURL("http://test.wildcard/");
  request.net_error = net::ERR_CONNECTION_RESET;
  OnRequestLegComplete(request);

  EXPECT_EQ(1u, CountQueuedBeacons(context1));
  EXPECT_EQ(0u, CountQueuedBeacons(context2));
}

TEST_F(DomainReliabilityMonitorTest,
    WildcardPrefersSelfWildcardToParentWildcard) {
  const DomainReliabilityContext* context1 =
      CreateAndAddContextForOrigin(GURL("https://test.wildcard/"), true);
  const DomainReliabilityContext* context2 =
      CreateAndAddContextForOrigin(GURL("https://wildcard/"), true);

  RequestInfo request = MakeRequestInfo();
  request.url = GURL("http://test.wildcard/");
  request.net_error = net::ERR_CONNECTION_RESET;
  OnRequestLegComplete(request);

  EXPECT_EQ(1u, CountQueuedBeacons(context1));
  EXPECT_EQ(0u, CountQueuedBeacons(context2));
}

}  // namespace

}  // namespace domain_reliability
