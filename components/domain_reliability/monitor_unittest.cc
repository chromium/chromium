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
#include "base/strings/string_piece.h"
#include "base/test/test_simple_task_runner.h"
#include "components/domain_reliability/baked_in_configs.h"
#include "components/domain_reliability/beacon.h"
#include "components/domain_reliability/config.h"
#include "components/domain_reliability/google_configs.h"
#include "components/domain_reliability/test_util.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"
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

size_t CountQueuedBeacons(DomainReliabilityContext* context) {
  BeaconVector beacons;
  context->GetQueuedBeaconsForTesting(&beacons);
  return beacons.size();
}

}  // namespace

class DomainReliabilityMonitorTest : public testing::Test {
 protected:
  typedef DomainReliabilityMonitor::RequestInfo RequestInfo;

  DomainReliabilityMonitorTest()
      : network_task_runner_(new base::TestSimpleTaskRunner()),
        url_request_context_getter_(
            new net::TestURLRequestContextGetter(network_task_runner_)),
        time_(new MockTime()),
        monitor_("test-reporter",
                 DomainReliabilityContext::UploadAllowedCallback(),
                 std::unique_ptr<MockableTime>(time_)) {
    monitor_.InitURLRequestContext(url_request_context_getter_);
    monitor_.SetDiscardUploads(false);
  }

  ~DomainReliabilityMonitorTest() override {
    monitor_.Shutdown();
  }

  static RequestInfo MakeRequestInfo() {
    RequestInfo request;
    request.status = net::URLRequestStatus();
    request.response_info.remote_endpoint =
        net::IPEndPoint(net::IPAddress(12, 34, 56, 78), 80);
    request.response_info.headers = MakeHttpResponseHeaders(
        "HTTP/1.1 200 OK\n\n");
    request.response_info.was_cached = false;
    request.response_info.network_accessed = true;
    request.response_info.was_fetched_via_proxy = false;
    request.load_flags = 0;
    request.upload_depth = 0;
    return request;
  }

  void OnRequestLegComplete(const RequestInfo& info) {
    monitor_.OnRequestLegComplete(info);
  }

  DomainReliabilityContext* CreateAndAddContext() {
    return monitor_.AddContextForTesting(MakeTestConfig());
  }

  DomainReliabilityContext* CreateAndAddContextForOrigin(const GURL& origin,
                                                         bool wildcard) {
    std::unique_ptr<DomainReliabilityConfig> config(
        MakeTestConfigWithOrigin(origin));
    config->include_subdomains = wildcard;
    return monitor_.AddContextForTesting(std::move(config));
  }

  scoped_refptr<base::TestSimpleTaskRunner> network_task_runner_;
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;
  MockTime* time_;
  DomainReliabilityMonitor monitor_;
  DomainReliabilityMonitor::RequestInfo request_;
};

namespace {

TEST_F(DomainReliabilityMonitorTest, Create) {
}

TEST_F(DomainReliabilityMonitorTest, NoContext) {
  DomainReliabilityContext* context = CreateAndAddContext();

  RequestInfo request = MakeRequestInfo();
  request.url = GURL("http://no-context/");
  OnRequestLegComplete(request);

  EXPECT_EQ(0u, CountQueuedBeacons(context));
}

TEST_F(DomainReliabilityMonitorTest, NetworkFailure) {
  DomainReliabilityContext* context = CreateAndAddContext();

  RequestInfo request = MakeRequestInfo();
  request.url = GURL("http://example/");
  request.status = net::URLRequestStatus::FromError(net::ERR_CONNECTION_RESET);
  request.response_info.headers = nullptr;
  OnRequestLegComplete(request);

  EXPECT_EQ(1u, CountQueuedBeacons(context));
}

TEST_F(DomainReliabilityMonitorTest, GoAwayWithPortMigrationDetected) {
  DomainReliabilityContext* context = CreateAndAddContext();

  RequestInfo request = MakeRequestInfo();
  request.url = GURL("http://example/");
  request.details.quic_port_migration_detected = true;
  request.response_info.headers = nullptr;
  OnRequestLegComplete(request);

  EXPECT_EQ(1u, CountQueuedBeacons(context));
}

TEST_F(DomainReliabilityMonitorTest, ServerFailure) {
  DomainReliabilityContext* context = CreateAndAddContext();

  RequestInfo request = MakeRequestInfo();
  request.url = GURL("http://example/");
  request.response_info.headers =
      MakeHttpResponseHeaders("HTTP/1.1 500 :(\n\n");
  OnRequestLegComplete(request);

  EXPECT_EQ(1u, CountQueuedBeacons(context));
}

// Make sure the monitor does not log requests that did not access the network.
TEST_F(DomainReliabilityMonitorTest, DidNotAccessNetwork) {
  DomainReliabilityContext* context = CreateAndAddContext();

  RequestInfo request = MakeRequestInfo();
  request.url = GURL("http://example/");
  request.response_info.network_accessed = false;
  OnRequestLegComplete(request);

  EXPECT_EQ(0u, CountQueuedBeacons(context));
}

// Make sure the monitor does not log requests that don't send cookies.
TEST_F(DomainReliabilityMonitorTest, DoNotSendCookies) {
  DomainReliabilityContext* context = CreateAndAddContext();

  RequestInfo request = MakeRequestInfo();
  request.url = GURL("http://example/");
  request.load_flags = net::LOAD_DO_NOT_SEND_COOKIES;
  OnRequestLegComplete(request);

  EXPECT_EQ(0u, CountQueuedBeacons(context));
}

// Make sure the monitor does not log a network-local error.
TEST_F(DomainReliabilityMonitorTest, LocalError) {
  DomainReliabilityContext* context = CreateAndAddContext();

  RequestInfo request = MakeRequestInfo();
  request.url = GURL("http://example/");
  request.status =
      net::URLRequestStatus::FromError(net::ERR_PROXY_CONNECTION_FAILED);
  OnRequestLegComplete(request);

  EXPECT_EQ(0u, CountQueuedBeacons(context));
}

// Make sure the monitor does not log the proxy's IP if one was used.
TEST_F(DomainReliabilityMonitorTest, WasFetchedViaProxy) {
  DomainReliabilityContext* context = CreateAndAddContext();

  RequestInfo request = MakeRequestInfo();
  request.url = GURL("http://example/");
  request.status = net::URLRequestStatus::FromError(net::ERR_CONNECTION_RESET);
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
  DomainReliabilityContext* context =
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
  DomainReliabilityContext* context = CreateAndAddContext();

  RequestInfo request = MakeRequestInfo();
  request.url = GURL("http://example/");
  request.response_info.was_cached = true;
  request.status =
      net::URLRequestStatus::FromError(net::ERR_NAME_RESOLUTION_FAILED);
  OnRequestLegComplete(request);

  BeaconVector beacons;
  context->GetQueuedBeaconsForTesting(&beacons);
  EXPECT_EQ(1u, beacons.size());
  EXPECT_TRUE(beacons[0]->server_ip.empty());
}

TEST_F(DomainReliabilityMonitorTest, AtLeastOneBakedInConfig) {
  DCHECK(kBakedInJsonConfigs[0] != nullptr);
}

// Make sure the monitor does log uploads, even though they have
// LOAD_DO_NOT_SEND_COOKIES.
TEST_F(DomainReliabilityMonitorTest, Upload) {
  DomainReliabilityContext* context = CreateAndAddContext();

  RequestInfo request = MakeRequestInfo();
  request.url = GURL("http://example/");
  request.load_flags =
      net::LOAD_DO_NOT_SAVE_COOKIES | net::LOAD_DO_NOT_SEND_COOKIES;
  request.status = net::URLRequestStatus::FromError(net::ERR_CONNECTION_RESET);
  request.upload_depth = 1;
  OnRequestLegComplete(request);

  EXPECT_EQ(1u, CountQueuedBeacons(context));
}

// Will fail when baked-in configs expire, as a reminder to update them.
// (Contact juliatuttle@chromium.org if this starts failing.)
TEST_F(DomainReliabilityMonitorTest, AddBakedInConfigs) {
  // AddBakedInConfigs DCHECKs that the baked-in configs parse correctly, so
  // this unittest will fail if someone tries to add an invalid config to the
  // source tree.
  monitor_.AddBakedInConfigs();

  // Count the number of baked-in configs.
  size_t num_baked_in_configs = 0;
  for (const char* const* p = kBakedInJsonConfigs; *p; ++p)
    ++num_baked_in_configs;

  // Also count the Google configs stored in abbreviated form.
  std::vector<std::unique_ptr<DomainReliabilityConfig>> google_configs;
  GetAllGoogleConfigs(&google_configs);
  size_t num_google_configs = google_configs.size();

  // The monitor should have contexts for all of the baked-in configs.
  EXPECT_EQ(num_baked_in_configs + num_google_configs,
            monitor_.contexts_size_for_testing());
}

TEST_F(DomainReliabilityMonitorTest, ClearBeacons) {
  DomainReliabilityContext* context = CreateAndAddContext();

  // Initially the monitor should have just the test context, with no beacons.
  EXPECT_EQ(1u, monitor_.contexts_size_for_testing());
  EXPECT_EQ(0u, CountQueuedBeacons(context));

  // Add a beacon.
  RequestInfo request = MakeRequestInfo();
  request.url = GURL("http://example/");
  request.status = net::URLRequestStatus::FromError(net::ERR_CONNECTION_RESET);
  OnRequestLegComplete(request);

  // Make sure it was added.
  EXPECT_EQ(1u, CountQueuedBeacons(context));

  monitor_.ClearBrowsingData(
      CLEAR_BEACONS, base::Callback<bool(const GURL&)>());

  // Make sure the beacon was cleared, but not the contexts.
  EXPECT_EQ(1u, monitor_.contexts_size_for_testing());
  EXPECT_EQ(0u, CountQueuedBeacons(context));
}

TEST_F(DomainReliabilityMonitorTest, ClearBeaconsWithFilter) {
  // Create two contexts, each with one beacon.
  GURL origin1("http://example.com/");
  GURL origin2("http://example.org/");

  DomainReliabilityContext* context1 =
      CreateAndAddContextForOrigin(origin1, false);
  RequestInfo request = MakeRequestInfo();
  request.url = origin1;
  request.status =
      net::URLRequestStatus::FromError(net::ERR_CONNECTION_RESET);
  OnRequestLegComplete(request);

  DomainReliabilityContext* context2 =
      CreateAndAddContextForOrigin(origin2, false);
  request = MakeRequestInfo();
  request.url = origin2;
  request.status =
      net::URLRequestStatus::FromError(net::ERR_CONNECTION_RESET);
  OnRequestLegComplete(request);

  // Delete the beacons for |origin1|.
  monitor_.ClearBrowsingData(
      CLEAR_BEACONS,
      base::Bind(static_cast<bool (*)(const GURL&, const GURL&)>(operator==),
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

  monitor_.ClearBrowsingData(
      CLEAR_CONTEXTS, base::Callback<bool(const GURL&)>());

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
      base::Bind(static_cast<bool (*)(const GURL&, const GURL&)>(operator==),
                 origin1));

  // Only one of the contexts should have been deleted.
  EXPECT_EQ(1u, monitor_.contexts_size_for_testing());
}

TEST_F(DomainReliabilityMonitorTest, WildcardMatchesSelf) {
  DomainReliabilityContext* context =
      CreateAndAddContextForOrigin(GURL("https://wildcard/"), true);

  RequestInfo request = MakeRequestInfo();
  request.url = GURL("http://wildcard/");
  request.status = net::URLRequestStatus::FromError(net::ERR_CONNECTION_RESET);
  OnRequestLegComplete(request);

  EXPECT_EQ(1u, CountQueuedBeacons(context));
}

TEST_F(DomainReliabilityMonitorTest, WildcardMatchesSubdomain) {
  DomainReliabilityContext* context =
      CreateAndAddContextForOrigin(GURL("https://wildcard/"), true);

  RequestInfo request = MakeRequestInfo();
  request.url = GURL("http://test.wildcard/");
  request.status = net::URLRequestStatus::FromError(net::ERR_CONNECTION_RESET);
  OnRequestLegComplete(request);

  EXPECT_EQ(1u, CountQueuedBeacons(context));
}

TEST_F(DomainReliabilityMonitorTest, WildcardDoesntMatchSubsubdomain) {
  DomainReliabilityContext* context =
      CreateAndAddContextForOrigin(GURL("https://wildcard/"), true);

  RequestInfo request = MakeRequestInfo();
  request.url = GURL("http://test.test.wildcard/");
  request.status = net::URLRequestStatus::FromError(net::ERR_CONNECTION_RESET);
  OnRequestLegComplete(request);

  EXPECT_EQ(0u, CountQueuedBeacons(context));
}

TEST_F(DomainReliabilityMonitorTest, WildcardPrefersSelfToParentWildcard) {
  DomainReliabilityContext* context1 =
      CreateAndAddContextForOrigin(GURL("https://test.wildcard/"), false);
  DomainReliabilityContext* context2 =
      CreateAndAddContextForOrigin(GURL("https://wildcard/"), true);

  RequestInfo request = MakeRequestInfo();
  request.url = GURL("http://test.wildcard/");
  request.status = net::URLRequestStatus::FromError(net::ERR_CONNECTION_RESET);
  OnRequestLegComplete(request);

  EXPECT_EQ(1u, CountQueuedBeacons(context1));
  EXPECT_EQ(0u, CountQueuedBeacons(context2));
}

TEST_F(DomainReliabilityMonitorTest,
    WildcardPrefersSelfWildcardToParentWildcard) {
  DomainReliabilityContext* context1 =
      CreateAndAddContextForOrigin(GURL("https://test.wildcard/"), true);
  DomainReliabilityContext* context2 =
      CreateAndAddContextForOrigin(GURL("https://wildcard/"), true);

  RequestInfo request = MakeRequestInfo();
  request.url = GURL("http://test.wildcard/");
  request.status = net::URLRequestStatus::FromError(net::ERR_CONNECTION_RESET);
  OnRequestLegComplete(request);

  EXPECT_EQ(1u, CountQueuedBeacons(context1));
  EXPECT_EQ(0u, CountQueuedBeacons(context2));
}

}  // namespace

}  // namespace domain_reliability
