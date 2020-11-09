// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/feed_network_impl.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "components/feed/core/common/pref_names.h"
#include "components/feed/core/proto/v2/wire/discover_actions_service.pb.h"
#include "components/feed/core/proto/v2/wire/request.pb.h"
#include "components/feed/core/proto/v2/wire/response.pb.h"
#include "components/feed/core/v2/test/callback_receiver.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/protobuf/src/google/protobuf/io/coded_stream.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl_lite.h"
#include "third_party/zlib/google/compression_utils.h"
#include "url/gurl.h"

namespace feed {
namespace {

using base::TimeDelta;
using testing::ElementsAre;
using ActionRequestResult = FeedNetwork::ActionRequestResult;
using QueryRequestResult = FeedNetwork::QueryRequestResult;

feedwire::Request GetTestFeedRequest(feedwire::FeedQuery::RequestReason reason =
                                         feedwire::FeedQuery::MANUAL_REFRESH) {
  feedwire::Request request;
  request.set_request_version(feedwire::Request::FEED_QUERY);
  request.mutable_feed_request()->mutable_feed_query()->set_reason(reason);
  return request;
}

feedwire::Response GetTestFeedResponse() {
  feedwire::Response response;
  response.set_response_version(feedwire::Response::FEED_RESPONSE);
  return response;
}

feedwire::UploadActionsRequest GetTestActionRequest() {
  feedwire::UploadActionsRequest request;
  request.add_feed_actions()->mutable_content_id()->set_content_domain(
      "example.com");
  return request;
}

feedwire::UploadActionsResponse GetTestActionResponse() {
  feedwire::UploadActionsResponse response;
  response.mutable_consistency_token()->set_token("tok");
  return response;
}

class TestDelegate : public FeedNetworkImpl::Delegate {
 public:
  std::string GetLanguageTag() override { return "en"; }
};

class FeedNetworkTest : public testing::Test {
 public:
  FeedNetworkTest() {
    identity_test_env_.MakePrimaryAccountAvailable("example@gmail.com");
    identity_test_env_.SetAutomaticIssueOfAccessTokens(true);
  }
  FeedNetworkTest(FeedNetworkTest&) = delete;
  FeedNetworkTest& operator=(const FeedNetworkTest&) = delete;
  ~FeedNetworkTest() override = default;

  void SetUp() override {
    feed::RegisterProfilePrefs(profile_prefs_.registry());

    shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_factory_);
    feed_network_ = std::make_unique<FeedNetworkImpl>(
        &delegate_, identity_test_env_.identity_manager(), "dummy_api_key",
        shared_url_loader_factory_, task_environment_.GetMockTickClock(),
        &profile_prefs_);
  }

  FeedNetwork* feed_network() { return feed_network_.get(); }

  signin::IdentityTestEnvironment* identity_env() {
    return &identity_test_env_;
  }

  network::TestURLLoaderFactory* test_factory() { return &test_factory_; }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  TestingPrefServiceSimple& profile_prefs() { return profile_prefs_; }

  base::HistogramTester& histogram() { return histogram_; }

  void Respond(const GURL& url,
               const std::string& response_string,
               net::HttpStatusCode code = net::HTTP_OK,
               network::URLLoaderCompletionStatus status =
                   network::URLLoaderCompletionStatus()) {
    auto head = network::mojom::URLResponseHead::New();
    if (code >= 0) {
      if (response_headers_) {
        head->headers = response_headers_;
      } else {
        head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
            "HTTP/1.1 " + base::NumberToString(code));
      }
      status.decoded_body_length = response_string.length();
    }

    test_factory_.AddResponse(url, std::move(head), response_string, status);
  }

  std::string PrependResponseLength(const std::string& response) {
    std::string result;
    ::google::protobuf::io::StringOutputStream string_output_stream(&result);
    ::google::protobuf::io::CodedOutputStream stream(&string_output_stream);

    stream.WriteVarint32(static_cast<uint32_t>(response.size()));
    stream.WriteString(response);
    return result;
  }

  GURL GetPendingRequestURL() {
    task_environment_.RunUntilIdle();
    network::TestURLLoaderFactory::PendingRequest* pending_request =
        test_factory()->GetPendingRequest(0);
    if (!pending_request)
      return GURL();
    return pending_request->request.url;
  }

  network::ResourceRequest RespondToQueryRequest(
      const std::string& response_string,
      net::HttpStatusCode code) {
    task_environment_.RunUntilIdle();
    network::TestURLLoaderFactory::PendingRequest* pending_request =
        test_factory()->GetPendingRequest(0);
    CHECK(pending_request);
    network::ResourceRequest resource_request = pending_request->request;
    Respond(pending_request->request.url,
            PrependResponseLength(response_string), code);
    task_environment_.FastForwardUntilNoTasksRemain();
    return resource_request;
  }

  network::ResourceRequest RespondToActionRequest(
      const std::string& response_string,
      net::HttpStatusCode code) {
    task_environment_.RunUntilIdle();
    network::TestURLLoaderFactory::PendingRequest* pending_request =
        test_factory()->GetPendingRequest(0);
    CHECK(pending_request);
    network::ResourceRequest resource_request = pending_request->request;
    Respond(pending_request->request.url, response_string, code);
    task_environment_.FastForwardUntilNoTasksRemain();
    return resource_request;
  }

  network::ResourceRequest RespondToQueryRequest(
      feedwire::Response response_message,
      net::HttpStatusCode code) {
    std::string binary_proto;
    response_message.SerializeToString(&binary_proto);
    return RespondToQueryRequest(binary_proto, code);
  }

  network::ResourceRequest RespondToActionRequest(
      feedwire::UploadActionsResponse response_message,
      net::HttpStatusCode code) {
    std::string binary_proto;
    response_message.SerializeToString(&binary_proto);
    return RespondToActionRequest(binary_proto, code);
  }

 protected:
  scoped_refptr<net::HttpResponseHeaders> response_headers_;

 private:
  TestDelegate delegate_;
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<FeedNetwork> feed_network_;
  network::TestURLLoaderFactory test_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  base::SimpleTestTickClock test_tick_clock_;
  TestingPrefServiceSimple profile_prefs_;
  base::HistogramTester histogram_;
};

TEST_F(FeedNetworkTest, SendQueryRequestEmpty) {
  CallbackReceiver<QueryRequestResult> receiver;
  feed_network()->SendQueryRequest(feedwire::Request(), false, receiver.Bind());

  ASSERT_TRUE(receiver.GetResult());
  const QueryRequestResult& result = *receiver.GetResult();
  EXPECT_EQ(0, result.response_info.status_code);
  EXPECT_FALSE(result.response_body);
}

TEST_F(FeedNetworkTest, SendQueryRequestSendsValidRequest) {
  CallbackReceiver<QueryRequestResult> receiver;
  feed_network()->SendQueryRequest(GetTestFeedRequest(), false,
                                   receiver.Bind());
  network::ResourceRequest resource_request =
      RespondToQueryRequest("", net::HTTP_OK);

  EXPECT_EQ(
      "https://www.google.com/httpservice/retry/TrellisClankService/"
      "FeedQuery?reqpld=CAHCPgQSAggB&fmt=bin&hl=en",
      resource_request.url);
  EXPECT_EQ("GET", resource_request.method);
  EXPECT_FALSE(resource_request.headers.HasHeader("content-encoding"));
  std::string authorization;
  EXPECT_TRUE(
      resource_request.headers.GetHeader("Authorization", &authorization));
  EXPECT_EQ(authorization, "Bearer access_token");
  histogram().ExpectBucketCount(
      "ContentSuggestions.Feed.Network.ResponseStatus.FeedQuery", 200, 1);
}

TEST_F(FeedNetworkTest, SendQueryRequestForceSignedOut) {
  CallbackReceiver<QueryRequestResult> receiver;
  feed_network()->SendQueryRequest(
      GetTestFeedRequest(), /*force_signed_out_request=*/true, receiver.Bind());
  network::ResourceRequest resource_request =
      RespondToQueryRequest("", net::HTTP_OK);

  EXPECT_EQ(
      "https://www.google.com/httpservice/retry/TrellisClankService/"
      "FeedQuery?reqpld=CAHCPgQSAggB&fmt=bin&hl=en&key=dummy_api_key",
      resource_request.url);
  EXPECT_FALSE(resource_request.headers.HasHeader("Authorization"));
}

TEST_F(FeedNetworkTest, SendQueryRequestInvalidResponse) {
  CallbackReceiver<QueryRequestResult> receiver;
  feed_network()->SendQueryRequest(GetTestFeedRequest(), false,
                                   receiver.Bind());
  RespondToQueryRequest("invalid", net::HTTP_OK);

  ASSERT_TRUE(receiver.GetResult());
  const QueryRequestResult& result = *receiver.GetResult();
  EXPECT_EQ(net::HTTP_OK, result.response_info.status_code);
  EXPECT_FALSE(result.response_body);
}

TEST_F(FeedNetworkTest, SendQueryRequestReceivesResponse) {
  CallbackReceiver<QueryRequestResult> receiver;
  feed_network()->SendQueryRequest(GetTestFeedRequest(), false,
                                   receiver.Bind());
  RespondToQueryRequest(GetTestFeedResponse(), net::HTTP_OK);

  ASSERT_TRUE(receiver.GetResult());
  const QueryRequestResult& result = *receiver.GetResult();
  EXPECT_EQ(net::HTTP_OK, result.response_info.status_code);
  EXPECT_EQ(
      "https://www.google.com/httpservice/retry/TrellisClankService/FeedQuery",
      result.response_info.base_request_url);
  EXPECT_NE(base::Time(), result.response_info.fetch_time);
  EXPECT_TRUE(result.response_info.was_signed_in);
  EXPECT_EQ(GetTestFeedResponse().response_version(),
            result.response_body->response_version());
}

TEST_F(FeedNetworkTest, SendQueryRequestIgnoresBodyForNon200Response) {
  CallbackReceiver<QueryRequestResult> receiver;
  feed_network()->SendQueryRequest(GetTestFeedRequest(), false,
                                   receiver.Bind());
  RespondToQueryRequest(GetTestFeedResponse(), net::HTTP_FORBIDDEN);

  ASSERT_TRUE(receiver.GetResult());
  const QueryRequestResult& result = *receiver.GetResult();
  EXPECT_EQ(net::HTTP_FORBIDDEN, result.response_info.status_code);
  EXPECT_FALSE(result.response_body);
  histogram().ExpectBucketCount(
      "ContentSuggestions.Feed.Network.ResponseStatus.FeedQuery",
      net::HTTP_FORBIDDEN, 1);
}

TEST_F(FeedNetworkTest, CancelRequest) {
  CallbackReceiver<QueryRequestResult> receiver;
  feed_network()->SendQueryRequest(GetTestFeedRequest(), false,
                                   receiver.Bind());
  feed_network()->CancelRequests();
  task_environment_.FastForwardUntilNoTasksRemain();

  EXPECT_FALSE(receiver.GetResult());
}

TEST_F(FeedNetworkTest, RequestTimeout) {
  base::HistogramTester histogram_tester;
  CallbackReceiver<QueryRequestResult> receiver;
  feed_network()->SendQueryRequest(GetTestFeedRequest(), false,
                                   receiver.Bind());
  task_environment_.FastForwardBy(TimeDelta::FromSeconds(30));

  ASSERT_TRUE(receiver.GetResult());
  const QueryRequestResult& result = *receiver.GetResult();
  EXPECT_EQ(net::ERR_TIMED_OUT, result.response_info.status_code);
  histogram_tester.ExpectTimeBucketCount(
      "ContentSuggestions.Feed.Network.Duration", TimeDelta::FromSeconds(30),
      1);
}

TEST_F(FeedNetworkTest, ParallelRequests) {
  CallbackReceiver<QueryRequestResult> receiver1, receiver2;
  feed_network()->SendQueryRequest(GetTestFeedRequest(), false,
                                   receiver1.Bind());
  // Make another request with a different URL so Respond() won't affect both
  // requests.
  feed_network()->SendQueryRequest(
      GetTestFeedRequest(feedwire::FeedQuery::NEXT_PAGE_SCROLL), false,
      receiver2.Bind());

  // Respond to both requests, avoiding FastForwardUntilNoTasksRemain until
  // a response is added for both requests.
  ASSERT_EQ(2, test_factory()->NumPending());
  for (int i = 0; i < 2; ++i) {
    network::TestURLLoaderFactory::PendingRequest* pending_request =
        test_factory()->GetPendingRequest(0);
    ASSERT_TRUE(pending_request) << "for request #" << i;
    std::string binary_proto;
    GetTestFeedResponse().SerializeToString(&binary_proto);
    Respond(pending_request->request.url, binary_proto, net::HTTP_OK);
  }
  task_environment_.FastForwardUntilNoTasksRemain();

  EXPECT_TRUE(receiver1.GetResult());
  EXPECT_TRUE(receiver2.GetResult());
}

TEST_F(FeedNetworkTest, ShouldReportResponseStatusCode) {
  CallbackReceiver<QueryRequestResult> receiver;
  base::HistogramTester histogram_tester;
  feed_network()->SendQueryRequest(GetTestFeedRequest(), false,
                                   receiver.Bind());
  RespondToQueryRequest(GetTestFeedResponse(), net::HTTP_FORBIDDEN);

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "ContentSuggestions.Feed.Network.ResponseStatus.FeedQuery"),
      ElementsAre(base::Bucket(/*min=*/net::HTTP_FORBIDDEN, /*count=*/1)));
}

TEST_F(FeedNetworkTest, ShouldIncludeAPIKeyForAuthError) {
  identity_env()->SetAutomaticIssueOfAccessTokens(false);
  CallbackReceiver<QueryRequestResult> receiver;
  base::HistogramTester histogram_tester;

  feed_network()->SendQueryRequest(GetTestFeedRequest(), false,
                                   receiver.Bind());
  identity_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(
          GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS));

  network::ResourceRequest resource_request =
      RespondToQueryRequest(GetTestFeedResponse(), net::HTTP_OK);

  EXPECT_THAT(resource_request.url.spec(),
              testing::HasSubstr("key=dummy_api_key"));

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "ContentSuggestions.Feed.Network.TokenFetchStatus"),
      testing::ElementsAre(base::Bucket(
          /*min=*/GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS,
          /*count=*/1)));
}

// Disabled for chromeos, which doesn't allow for there not to be a signed in
// user.
#if !defined(OS_CHROMEOS)
TEST_F(FeedNetworkTest, ShouldIncludeAPIKeyForNoSignedInUser) {
  identity_env()->ClearPrimaryAccount();
  CallbackReceiver<QueryRequestResult> receiver;
  feed_network()->SendQueryRequest(GetTestFeedRequest(), false,
                                   receiver.Bind());

  network::ResourceRequest resource_request =
      RespondToQueryRequest(GetTestFeedResponse(), net::HTTP_OK);

  EXPECT_THAT(resource_request.url.spec(),
              testing::HasSubstr("key=dummy_api_key"));
}
#endif

TEST_F(FeedNetworkTest, TestDurationHistogram) {
  base::HistogramTester histogram_tester;
  CallbackReceiver<QueryRequestResult> receiver;
  const TimeDelta kDuration = TimeDelta::FromMilliseconds(12345);

  feed_network()->SendQueryRequest(GetTestFeedRequest(), false,
                                   receiver.Bind());
  task_environment_.FastForwardBy(kDuration);
  RespondToQueryRequest(GetTestFeedResponse(), net::HTTP_OK);

  EXPECT_TRUE(receiver.GetResult());
  histogram_tester.ExpectTimeBucketCount(
      "ContentSuggestions.Feed.Network.Duration", kDuration, 1);
}

// Verify that the kHostOverrideHost pref overrides the feed host
// and returns the Bless nonce if one sent in the response.
TEST_F(FeedNetworkTest, TestHostOverrideWithAuthHeader) {
  CallbackReceiver<QueryRequestResult> receiver;
  profile_prefs().SetString(feed::prefs::kHostOverrideHost,
                            "http://www.newhost.com/");
  feed_network()->SendQueryRequest(GetTestFeedRequest(), false,
                                   receiver.Bind());

  ASSERT_EQ("www.newhost.com", GetPendingRequestURL().host());

  response_headers_ = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(
          "HTTP/1.1 401 Unauthorized\nwww-authenticate: Foo "
          "nonce=\"1234123412341234\"\n\n"));
  RespondToQueryRequest(GetTestFeedResponse(), net::HTTP_FORBIDDEN);

  ASSERT_TRUE(receiver.GetResult());
  EXPECT_EQ("1234123412341234",
            receiver.GetResult()->response_info.bless_nonce);
}

TEST_F(FeedNetworkTest, SendActionRequest) {
  CallbackReceiver<ActionRequestResult> receiver;
  feed_network()->SendActionRequest(GetTestActionRequest(), receiver.Bind());
  RespondToActionRequest(GetTestActionResponse(), net::HTTP_OK);

  ASSERT_TRUE(receiver.GetResult());
  const ActionRequestResult& result = *receiver.GetResult();
  EXPECT_EQ(net::HTTP_OK, result.response_info.status_code);
  EXPECT_TRUE(result.response_body);
  histogram().ExpectBucketCount(
      "ContentSuggestions.Feed.Network.ResponseStatus.UploadActions", 200, 1);
}

TEST_F(FeedNetworkTest, SendActionRequestSendsValidRequest) {
  CallbackReceiver<ActionRequestResult> receiver;
  feed_network()->SendActionRequest(GetTestActionRequest(), receiver.Bind());
  network::ResourceRequest resource_request =
      RespondToActionRequest(GetTestActionResponse(), net::HTTP_OK);

  EXPECT_EQ(GURL("https://discover-pa.googleapis.com/v1/actions:upload"),
            resource_request.url);

  EXPECT_EQ("POST", resource_request.method);
  std::string content_encoding;
  EXPECT_TRUE(resource_request.headers.GetHeader("content-encoding",
                                                 &content_encoding));
  EXPECT_EQ("gzip", content_encoding);
  std::string authorization;
  EXPECT_TRUE(
      resource_request.headers.GetHeader("Authorization", &authorization));
  EXPECT_EQ(authorization, "Bearer access_token");

  // Check that the body content is correct. This requires some work to extract
  // the bytes and unzip them.
  auto* elements = resource_request.request_body->elements();
  ASSERT_TRUE(elements);
  ASSERT_EQ(1UL, elements->size());
  std::string sent_body((*elements)[0].bytes(), (*elements)[0].length());
  std::string sent_body_uncompressed;
  ASSERT_TRUE(compression::GzipUncompress(sent_body, &sent_body_uncompressed));
  std::string expected_body;
  ASSERT_TRUE(GetTestActionRequest().SerializeToString(&expected_body));
  EXPECT_EQ(expected_body, sent_body_uncompressed);
}

TEST_F(FeedNetworkTest, TestOverrideHostDoesNotAffectActionUpload) {
  profile_prefs().SetString(feed::prefs::kHostOverrideHost,
                            "http://www.newhost.com/");
  feed_network()->SendActionRequest(GetTestActionRequest(), base::DoNothing());

  EXPECT_EQ(GURL("https://discover-pa.googleapis.com/v1/actions:upload"),
            GetPendingRequestURL());
}

TEST_F(FeedNetworkTest, TestOverrideActionsEndpoint) {
  profile_prefs().SetString(feed::prefs::kActionsEndpointOverride,
                            "http://www.newhost.com/");
  feed_network()->SendActionRequest(GetTestActionRequest(), base::DoNothing());

  EXPECT_EQ(GURL("http://www.newhost.com/"), GetPendingRequestURL());
}

}  // namespace
}  // namespace feed
