// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/feed_network_impl.h"

#include <memory>
#include <utility>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/chromeos_buildflags.h"
#include "components/feed/core/common/pref_names.h"
#include "components/feed/core/proto/v2/wire/client_info.pb.h"
#include "components/feed/core/proto/v2/wire/feed_query.pb.h"
#include "components/feed/core/proto/v2/wire/request.pb.h"
#include "components/feed/core/proto/v2/wire/response.pb.h"
#include "components/feed/core/proto/v2/wire/upload_actions_request.pb.h"
#include "components/feed/core/proto/v2/wire/upload_actions_response.pb.h"
#include "components/feed/core/proto/v2/wire/web_feeds.pb.h"
#include "components/feed/core/v2/feed_network.h"
#include "components/feed/core/v2/public/types.h"
#include "components/feed/core/v2/test/callback_receiver.h"
#include "components/feed/feed_feature_list.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/protobuf/src/google/protobuf/io/coded_stream.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl_lite.h"
#include "third_party/zlib/google/compression_utils.h"
#include "url/gurl.h"

namespace feed {
namespace {

constexpr char kEmail[] = "example@gmail.com";

MATCHER_P(EqualsProto,
          message,
          "Match a proto Message equal to the matcher's argument.") {
  return arg.ShortDebugString() == message.ShortDebugString();
}

using testing::ElementsAre;
using QueryRequestResult = FeedNetwork::QueryRequestResult;

feedwire::ClientInfo ExpectHasClientInfoHeader(
    network::ResourceRequest request) {
  EXPECT_TRUE(request.headers.HasHeader(feed::kClientInfoHeader));
  std::optional<std::string> clientinfo =
      request.headers.GetHeader(feed::kClientInfoHeader);
  CHECK(clientinfo.has_value());
  std::string decoded_clientinfo;
  EXPECT_TRUE(base::Base64Decode(clientinfo.value(), &decoded_clientinfo));
  feedwire::ClientInfo clientinfo_proto;
  EXPECT_TRUE(clientinfo_proto.ParseFromString(decoded_clientinfo));
  return clientinfo_proto;
}

void ExpectNoClientInfoHeader(network::ResourceRequest request) {
  EXPECT_FALSE(request.headers.HasHeader(feed::kClientInfoHeader));
}

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
  request.add_feed_actions()->mutable_client_data()->set_duration_ms(123);
  return request;
}

feedwire::UploadActionsResponse GetTestActionResponse() {
  feedwire::UploadActionsResponse response;
  response.mutable_consistency_token()->set_token("tok");
  return response;
}

class TestDelegate : public FeedNetworkImpl::Delegate {
 public:
  explicit TestDelegate(signin::IdentityTestEnvironment* identity_test_env)
      : identity_test_env_(identity_test_env) {}

  std::string GetLanguageTag() override { return "en"; }
  AccountInfo GetAccountInfo() override {
    return AccountInfo{
        identity_test_env_->identity_manager()->GetPrimaryAccountInfo(
            GetConsentLevelNeededForPersonalizedFeed())};
  }
  bool IsOffline() override { return is_offline_; }

  bool is_offline_ = false;
  raw_ptr<signin::IdentityTestEnvironment> identity_test_env_;
};

class FeedNetworkTest : public testing::Test {
 public:
  FeedNetworkTest() = default;
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
        shared_url_loader_factory_, &profile_prefs_);
    SignIn(signin::ConsentLevel::kSync);
  }

  void SignIn(signin::ConsentLevel consent_level) {
    identity_test_env_.MakePrimaryAccountAvailable(kEmail, consent_level);
    identity_test_env_.SetAutomaticIssueOfAccessTokens(true);
  }

  FeedNetwork* feed_network() { return feed_network_.get(); }

  signin::IdentityTestEnvironment* identity_env() {
    return &identity_test_env_;
  }

  AccountInfo account_info() { return delegate_.GetAccountInfo(); }
  RequestMetadata request_metadata() {
    RequestMetadata request_metadata;
    request_metadata.chrome_info.version = base::Version({1, 2, 3, 4});
    request_metadata.chrome_info.channel = version_info::Channel::STABLE;
    request_metadata.display_metrics.density = 1;
    request_metadata.display_metrics.width_pixels = 2;
    request_metadata.display_metrics.height_pixels = 3;
    request_metadata.language_tag = "en-US";
    request_metadata.client_instance_id = "client_instance_id";

    return request_metadata;
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

    ExpectNoClientInfoHeader(resource_request);

    Respond(pending_request->request.url,
            PrependResponseLength(response_string), code);
    task_environment_.FastForwardUntilNoTasksRemain();
    return resource_request;
  }

  network::ResourceRequest RespondToDiscoverRequest(
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
    network::ResourceRequest resource_request =
        RespondToDiscoverRequest(binary_proto, code);
    ExpectHasClientInfoHeader(resource_request);
    return resource_request;
  }

 protected:
  scoped_refptr<net::HttpResponseHeaders> response_headers_;

 protected:
  signin::IdentityTestEnvironment identity_test_env_;
  TestDelegate delegate_{&identity_test_env_};
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  std::unique_ptr<FeedNetwork> feed_network_;
  RequestMetadata request_metadata_;
  network::TestURLLoaderFactory test_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  TestingPrefServiceSimple profile_prefs_;
  base::HistogramTester histogram_;
};

TEST_F(FeedNetworkTest, SendQueryRequestEmpty) {
  CallbackReceiver<QueryRequestResult> receiver;
  feed_network()->SendQueryRequest(NetworkRequestType::kFeedQuery,
                                   feedwire::Request(), account_info(),
                                   receiver.Bind());

  ASSERT_TRUE(receiver.GetResult());
  const QueryRequestResult& result = *receiver.GetResult();
  EXPECT_EQ(0, result.response_info.status_code);
  EXPECT_FALSE(result.response_body);
}

TEST_F(FeedNetworkTest, SendQueryRequestSendsValidRequest) {
  CallbackReceiver<QueryRequestResult> receiver;
  feed_network()->SendQueryRequest(NetworkRequestType::kFeedQuery,
                                   GetTestFeedRequest(), account_info(),
                                   receiver.Bind());
  network::ResourceRequest resource_request =
      RespondToQueryRequest("", net::HTTP_OK);

  EXPECT_EQ(
      "https://www.google.com/httpservice/retry/TrellisClankService/"
      "FeedQuery?reqpld=CAHCPgQSAggB&fmt=bin&hl=en",
      resource_request.url);
  EXPECT_EQ("GET", resource_request.method);
  EXPECT_FALSE(resource_request.headers.HasHeader("content-encoding"));
  EXPECT_EQ(resource_request.headers.GetHeader("Authorization"),
            "Bearer access_token");
  histogram().ExpectBucketCount(
      "ContentSuggestions.Feed.Network.ResponseStatus.FeedQuery", 200, 1);
  histogram().ExpectBucketCount(
      "ContentSuggestions.Feed.Network.FeedQueryRequestSize", 165, 1);
}

// These tests need ClearPrimaryAccount() which isn't supported by ChromeOS.
// RevokeSyncConsent() sometimes clears the account rather than just changing
// the consent level so we may as well sign out and sign back in ourselves.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(FeedNetworkTest, SendQueryRequestPersonalized_AccountSignin) {
  // Request should be signed in if account consent level is kSignin.
  identity_env()->ClearPrimaryAccount();
  SignIn(signin::ConsentLevel::kSignin);
  base::test::ScopedFeatureList feature_list;

  CallbackReceiver<QueryRequestResult> receiver;

  feed_network()->SendQueryRequest(NetworkRequestType::kFeedQuery,
                                   GetTestFeedRequest(), account_info(),
                                   receiver.Bind());
  network::ResourceRequest resource_request =
      RespondToQueryRequest(GetTestFeedResponse(), net::HTTP_OK);

  EXPECT_EQ(
      "https://www.google.com/httpservice/retry/TrellisClankService/"
      "FeedQuery?reqpld=CAHCPgQSAggB&fmt=bin&hl=en",
      resource_request.url);
  EXPECT_EQ("GET", resource_request.method);
  EXPECT_FALSE(resource_request.headers.HasHeader("content-encoding"));

  // Verify that it's a signed-in request.
  EXPECT_EQ(resource_request.headers.GetHeader("Authorization"),
            "Bearer access_token");

  histogram().ExpectBucketCount(
      "ContentSuggestions.Feed.Network.ResponseStatus.FeedQuery", 200, 1);
}

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(FeedNetworkTest, SendQueryRequestPersonalized_AccountSync) {
  // Request should be signed in if account consent level is kSync.
  CallbackReceiver<QueryRequestResult> receiver;

  feed_network()->SendQueryRequest(NetworkRequestType::kFeedQuery,
                                   GetTestFeedRequest(), account_info(),
                                   receiver.Bind());
  network::ResourceRequest resource_request =
      RespondToQueryRequest(GetTestFeedResponse(), net::HTTP_OK);

  EXPECT_EQ(
      "https://www.google.com/httpservice/retry/TrellisClankService/"
      "FeedQuery?reqpld=CAHCPgQSAggB&fmt=bin&hl=en",
      resource_request.url);
  EXPECT_EQ("GET", resource_request.method);
  EXPECT_FALSE(resource_request.headers.HasHeader("content-encoding"));

  // Verify that it's a signed-in request.
  EXPECT_EQ(resource_request.headers.GetHeader("Authorization"),
            "Bearer access_token");

  histogram().ExpectBucketCount(
      "ContentSuggestions.Feed.Network.ResponseStatus.FeedQuery", 200, 1);
}

TEST_F(FeedNetworkTest, SendQueryRequestForceSignedOut) {
  CallbackReceiver<QueryRequestResult> receiver;
  feed_network()->SendQueryRequest(NetworkRequestType::kFeedQuery,
                                   GetTestFeedRequest(), AccountInfo{},
                                   receiver.Bind());
  network::ResourceRequest resource_request =
      RespondToQueryRequest("", net::HTTP_OK);

  EXPECT_EQ(
      "https://www.google.com/httpservice/retry/TrellisClankService/"
      "FeedQuery?reqpld=CAHCPgQSAggB&fmt=bin&hl=en&key=dummy_api_key",
      resource_request.url);
  EXPECT_EQ(AccountInfo{},
            receiver.RunAndGetResult().response_info.account_info);
  EXPECT_FALSE(resource_request.headers.HasHeader("Authorization"));
}

TEST_F(FeedNetworkTest, SendQueryRequestInvalidResponse) {
  CallbackReceiver<QueryRequestResult> receiver;
  feed_network()->SendQueryRequest(NetworkRequestType::kFeedQuery,
                                   GetTestFeedRequest(), account_info(),
                                   receiver.Bind());
  RespondToQueryRequest("invalid", net::HTTP_OK);

  ASSERT_TRUE(receiver.GetResult());
  const QueryRequestResult& result = *receiver.GetResult();
  EXPECT_EQ(net::HTTP_OK, result.response_info.status_code);
  EXPECT_FALSE(result.response_body);
}

TEST_F(FeedNetworkTest, SendQueryRequestReceivesResponse) {
  CallbackReceiver<QueryRequestResult> receiver;
  feed_network()->SendQueryRequest(NetworkRequestType::kFeedQuery,
                                   GetTestFeedRequest(), account_info(),
                                   receiver.Bind());
  RespondToQueryRequest(GetTestFeedResponse(), net::HTTP_OK);

  ASSERT_TRUE(receiver.GetResult());
  const QueryRequestResult& result = *receiver.GetResult();
  EXPECT_EQ(net::HTTP_OK, result.response_info.status_code);
  EXPECT_EQ(
      "https://www.google.com/httpservice/retry/TrellisClankService/FeedQuery",
      result.response_info.base_request_url);
  EXPECT_NE(base::Time(), result.response_info.fetch_time);
  EXPECT_EQ(account_info(), result.response_info.account_info);
  EXPECT_EQ(GetTestFeedResponse().response_version(),
            result.response_body->response_version());
}

TEST_F(FeedNetworkTest, SendQueryRequestIgnoresBodyForNon200Response) {
  CallbackReceiver<QueryRequestResult> receiver;
  feed_network()->SendQueryRequest(NetworkRequestType::kFeedQuery,
                                   GetTestFeedRequest(), account_info(),
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

TEST_F(FeedNetworkTest, SendQueryRequestFailsForWrongUser) {
  CallbackReceiver<QueryRequestResult> receiver;
  feed_network()->SendQueryRequest(
      NetworkRequestType::kFeedQuery, GetTestFeedRequest(),
      {"other-gaia", "other@foo.com"}, receiver.Bind());
  task_environment_.RunUntilIdle();
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      test_factory()->GetPendingRequest(0);
  EXPECT_FALSE(pending_request);

  ASSERT_TRUE(receiver.GetResult());
  const QueryRequestResult& result = *receiver.GetResult();
  EXPECT_EQ(net::ERR_INVALID_ARGUMENT, result.response_info.status_code);
  EXPECT_EQ(AccountTokenFetchStatus::kUnexpectedAccount,
            result.response_info.account_token_fetch_status);
  EXPECT_FALSE(result.response_body);
  histogram().ExpectBucketCount(
      "ContentSuggestions.Feed.Network.ResponseStatus.FeedQuery",
      net::ERR_INVALID_ARGUMENT, 1);
}

TEST_F(FeedNetworkTest, CancelRequest) {
  CallbackReceiver<QueryRequestResult> receiver;
  feed_network()->SendQueryRequest(NetworkRequestType::kFeedQuery,
                                   GetTestFeedRequest(), account_info(),
                                   receiver.Bind());
  feed_network()->CancelRequests();
  task_environment_.FastForwardUntilNoTasksRemain();

  EXPECT_FALSE(receiver.GetResult());
}

TEST_F(FeedNetworkTest, RequestTimeout) {
  base::HistogramTester histogram_tester;
  CallbackReceiver<QueryRequestResult> receiver;
  feed_network()->SendQueryRequest(NetworkRequestType::kFeedQuery,
                                   GetTestFeedRequest(), account_info(),
                                   receiver.Bind());
  task_environment_.FastForwardBy(base::Seconds(30));

  ASSERT_TRUE(receiver.GetResult());
  const QueryRequestResult& result = *receiver.GetResult();
  EXPECT_EQ(net::ERR_TIMED_OUT, result.response_info.status_code);
  histogram_tester.ExpectTimeBucketCount(
      "ContentSuggestions.Feed.Network.Duration", base::Seconds(30), 1);
}

TEST_F(FeedNetworkTest, AccountTokenFetchTimeout) {
  identity_test_env_.RemoveRefreshTokenForPrimaryAccount();
  identity_test_env_.SetAutomaticIssueOfAccessTokens(false);

  CallbackReceiver<QueryRequestResult> receiver;
  feed_network()->SendQueryRequest(NetworkRequestType::kFeedQuery,
                                   GetTestFeedRequest(), account_info(),
                                   receiver.Bind());
  task_environment_.FastForwardBy(kAccessTokenFetchTimeout - base::Seconds(1));
  ASSERT_FALSE(receiver.GetResult());

  task_environment_.FastForwardBy(base::Seconds(1));

  ASSERT_TRUE(receiver.GetResult());
  const QueryRequestResult& result = *receiver.GetResult();
  EXPECT_EQ(AccountTokenFetchStatus::kTimedOut,
            result.response_info.account_token_fetch_status);
  EXPECT_EQ(net::ERR_TIMED_OUT, result.response_info.status_code);
}

TEST_F(FeedNetworkTest, AccountTokenRefreshCompleteAfterFetchTimeout) {
  identity_test_env_.SetAutomaticIssueOfAccessTokens(false);

  CallbackReceiver<QueryRequestResult> receiver;
  feed_network()->SendQueryRequest(NetworkRequestType::kFeedQuery,
                                   GetTestFeedRequest(), account_info(),
                                   receiver.Bind());
  // Time-out the token fetch and then complete it.
  task_environment_.FastForwardBy(kAccessTokenFetchTimeout);
  identity_test_env_.SetAutomaticIssueOfAccessTokens(true);
  identity_test_env_.SetRefreshTokenForPrimaryAccount();

  // Ensure the fetch failed.
  const QueryRequestResult& result = receiver.RunAndGetResult();
  EXPECT_EQ(AccountTokenFetchStatus::kTimedOut,
            result.response_info.account_token_fetch_status);
  EXPECT_EQ(net::ERR_TIMED_OUT, result.response_info.status_code);
}

TEST_F(FeedNetworkTest, AccountTokenRefreshCompleteBeforeFetchTimeout) {
  identity_test_env_.SetAutomaticIssueOfAccessTokens(false);

  CallbackReceiver<QueryRequestResult> receiver;
  feed_network()->SendQueryRequest(NetworkRequestType::kFeedQuery,
                                   GetTestFeedRequest(), account_info(),
                                   receiver.Bind());
  // Time-out the token fetch just after it completes.
  identity_test_env_.SetAutomaticIssueOfAccessTokens(true);
  identity_test_env_.SetRefreshTokenForPrimaryAccount();
  task_environment_.FastForwardBy(kAccessTokenFetchTimeout);
  RespondToQueryRequest(GetTestFeedResponse(), net::HTTP_OK);

  // Ensure the fetch failed.
  const QueryRequestResult& result = receiver.RunAndGetResult();
  EXPECT_EQ(AccountTokenFetchStatus::kUnspecified,
            result.response_info.account_token_fetch_status);
  EXPECT_EQ(net::HTTP_OK, result.response_info.status_code);
}

TEST_F(FeedNetworkTest, FetchImmediatelyAbortsIfOffline) {
  // Trying to fetch the token would timeout, but because the device is offline,
  // the fetch quits immediately.
  identity_test_env_.RemoveRefreshTokenForPrimaryAccount();
  identity_test_env_.SetAutomaticIssueOfAccessTokens(false);
  delegate_.is_offline_ = true;

  CallbackReceiver<QueryRequestResult> receiver;
  feed_network()->SendQueryRequest(NetworkRequestType::kFeedQuery,
                                   GetTestFeedRequest(), account_info(),
                                   receiver.Bind());
  ASSERT_TRUE(receiver.GetResult());
  const QueryRequestResult& result = *receiver.GetResult();
  EXPECT_EQ(AccountTokenFetchStatus::kUnspecified,
            result.response_info.account_token_fetch_status);
  EXPECT_EQ(net::ERR_INTERNET_DISCONNECTED, result.response_info.status_code);
}

TEST_F(FeedNetworkTest, ParallelRequests) {
  CallbackReceiver<QueryRequestResult> receiver1, receiver2;
  feed_network()->SendQueryRequest(NetworkRequestType::kFeedQuery,
                                   GetTestFeedRequest(), account_info(),
                                   receiver1.Bind());
  // Make another request with a different URL so Respond() won't affect both
  // requests.
  feed_network()->SendQueryRequest(
      NetworkRequestType::kFeedQuery,
      GetTestFeedRequest(feedwire::FeedQuery::NEXT_PAGE_SCROLL), account_info(),
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
  feed_network()->SendQueryRequest(NetworkRequestType::kFeedQuery,
                                   GetTestFeedRequest(), account_info(),
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

  feed_network()->SendQueryRequest(NetworkRequestType::kFeedQuery,
                                   GetTestFeedRequest(), account_info(),
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
#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(FeedNetworkTest, ShouldIncludeAPIKeyForNoSignedInUser) {
  identity_env()->ClearPrimaryAccount();
  CallbackReceiver<QueryRequestResult> receiver;
  feed_network()->SendQueryRequest(NetworkRequestType::kFeedQuery,
                                   GetTestFeedRequest(), account_info(),
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
  const base::TimeDelta kDuration = base::Milliseconds(12345);

  feed_network()->SendQueryRequest(NetworkRequestType::kFeedQuery,
                                   GetTestFeedRequest(), account_info(),
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
  feed_network()->SendQueryRequest(NetworkRequestType::kFeedQuery,
                                   GetTestFeedRequest(), account_info(),
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

TEST_F(FeedNetworkTest, TestHostOverrideWithPath) {
  CallbackReceiver<QueryRequestResult> receiver;
  profile_prefs().SetString(feed::prefs::kHostOverrideHost,
                            "http://www.newhost.com/testpath");
  feed_network()->SendQueryRequest(NetworkRequestType::kFeedQuery,
                                   GetTestFeedRequest(), account_info(),
                                   receiver.Bind());

  ASSERT_EQ("www.newhost.com", GetPendingRequestURL().host());
  ASSERT_EQ("/testpath/httpservice/retry/TrellisClankService/FeedQuery",
            GetPendingRequestURL().path());
}

TEST_F(FeedNetworkTest, TestHostOverrideWithPathTrailingSlash) {
  CallbackReceiver<QueryRequestResult> receiver;
  profile_prefs().SetString(feed::prefs::kHostOverrideHost,
                            "http://www.newhost.com/testpath/");
  feed_network()->SendQueryRequest(NetworkRequestType::kFeedQuery,
                                   GetTestFeedRequest(), account_info(),
                                   receiver.Bind());

  ASSERT_EQ("www.newhost.com", GetPendingRequestURL().host());
  ASSERT_EQ("/testpath/httpservice/retry/TrellisClankService/FeedQuery",
            GetPendingRequestURL().path());
}

TEST_F(FeedNetworkTest, SendApiRequest_UploadActions) {
  CallbackReceiver<FeedNetwork::ApiResult<feedwire::UploadActionsResponse>>
      receiver;
  feed_network()->SendApiRequest<UploadActionsDiscoverApi>(
      GetTestActionRequest(), account_info(), request_metadata(),
      receiver.Bind());

  network::ResourceRequest request =
      RespondToActionRequest(GetTestActionResponse(), net::HTTP_OK);

  ASSERT_TRUE(receiver.GetResult());
  const FeedNetwork::ApiResult<feedwire::UploadActionsResponse>& result =
      *receiver.GetResult();
  EXPECT_EQ(net::HTTP_OK, result.response_info.status_code);
  EXPECT_TRUE(result.response_body);

  histogram().ExpectBucketCount(
      "ContentSuggestions.Feed.Network.ResponseStatus.UploadActions", 200, 1);
}

TEST_F(FeedNetworkTest, SendApiRequest_DecodesClientInfo_WithClientInstanceId) {
  CallbackReceiver<FeedNetwork::ApiResult<feedwire::UploadActionsResponse>>
      receiver;
  feed_network()->SendApiRequest<UploadActionsDiscoverApi>(
      GetTestActionRequest(), account_info(), request_metadata(),
      receiver.Bind());

  network::ResourceRequest request =
      RespondToActionRequest(GetTestActionResponse(), net::HTTP_OK);
  feedwire::ClientInfo client_info = ExpectHasClientInfoHeader(request);

  EXPECT_EQ(feedwire::ClientInfo::CHROME_ANDROID, client_info.app_type());
  EXPECT_EQ(feedwire::Version::RELEASE, client_info.app_version().build_type());
  EXPECT_EQ(1, client_info.app_version().major());
  EXPECT_EQ(2, client_info.app_version().minor());
  EXPECT_EQ(3, client_info.app_version().build());
  EXPECT_EQ(4, client_info.app_version().revision());
  EXPECT_FALSE(client_info.chrome_client_info().start_surface());
  EXPECT_EQ("client_instance_id", client_info.client_instance_id());
}

TEST_F(FeedNetworkTest, SendApiRequest_DecodesClientInfo_WithSessionId) {
  RequestMetadata request_metadata_with_session = request_metadata();
  request_metadata_with_session.session_id = "session_id";
  request_metadata_with_session.client_instance_id = "";

  CallbackReceiver<FeedNetwork::ApiResult<feedwire::UploadActionsResponse>>
      receiver;
  feed_network()->SendApiRequest<UploadActionsDiscoverApi>(
      GetTestActionRequest(), account_info(),
      std::move(request_metadata_with_session), receiver.Bind());
  network::ResourceRequest request =
      RespondToActionRequest(GetTestActionResponse(), net::HTTP_OK);
  feedwire::ClientInfo client_info = ExpectHasClientInfoHeader(request);

  EXPECT_EQ("session_id", client_info.chrome_client_info().session_id());
  EXPECT_EQ("", client_info.client_instance_id());
}

TEST_F(FeedNetworkTest, SendApiRequest_UploadActionsFailsForWrongUser) {
  CallbackReceiver<FeedNetwork::ApiResult<feedwire::UploadActionsResponse>>
      receiver;
  AccountInfo other_account;
  other_account.gaia = "some_other_gaia";
  other_account.email = "some@other.com";
  feed_network()->SendApiRequest<UploadActionsDiscoverApi>(
      GetTestActionRequest(), other_account, request_metadata(),
      receiver.Bind());
  task_environment_.RunUntilIdle();
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      test_factory()->GetPendingRequest(0);
  EXPECT_FALSE(pending_request);

  ASSERT_TRUE(receiver.GetResult());
  const FeedNetwork::ApiResult<feedwire::UploadActionsResponse>& result =
      *receiver.GetResult();
  EXPECT_EQ(net::ERR_INVALID_ARGUMENT, result.response_info.status_code);
  EXPECT_EQ(AccountTokenFetchStatus::kUnexpectedAccount,
            result.response_info.account_token_fetch_status);
  EXPECT_FALSE(result.response_body);
  histogram().ExpectBucketCount(
      "ContentSuggestions.Feed.Network.ResponseStatus.UploadActions",
      net::ERR_INVALID_ARGUMENT, 1);
}

TEST_F(FeedNetworkTest, SendApiRequestSendsValidRequest_UploadActions) {
  CallbackReceiver<FeedNetwork::ApiResult<feedwire::UploadActionsResponse>>
      receiver;
  feed_network()->SendApiRequest<UploadActionsDiscoverApi>(
      GetTestActionRequest(), account_info(), request_metadata(),
      receiver.Bind());
  network::ResourceRequest resource_request =
      RespondToActionRequest(GetTestActionResponse(), net::HTTP_OK);

  EXPECT_EQ(GURL("https://discover-pa.googleapis.com/v1/actions:upload"),
            resource_request.url);

  EXPECT_EQ("POST", resource_request.method);
  EXPECT_EQ("gzip", resource_request.headers.GetHeader("content-encoding"));
  EXPECT_EQ(resource_request.headers.GetHeader("Authorization"),
            "Bearer access_token");

  // Check that the body content is correct. This requires some work to extract
  // the bytes and unzip them.
  auto* elements = resource_request.request_body->elements();
  ASSERT_TRUE(elements);
  ASSERT_EQ(1UL, elements->size());
  ASSERT_EQ(network::DataElement::Tag::kBytes, elements->at(0).type());
  std::string sent_body(
      elements->at(0).As<network::DataElementBytes>().AsStringPiece());
  std::string sent_body_uncompressed;
  ASSERT_TRUE(compression::GzipUncompress(sent_body, &sent_body_uncompressed));
  std::string expected_body;
  ASSERT_TRUE(GetTestActionRequest().SerializeToString(&expected_body));
  EXPECT_EQ(expected_body, sent_body_uncompressed);
}

TEST_F(FeedNetworkTest, SendApiRequest_Unfollow) {
  CallbackReceiver<
      FeedNetwork::ApiResult<feedwire::webfeed::UnfollowWebFeedResponse>>
      receiver;
  feed_network()->SendApiRequest<UnfollowWebFeedDiscoverApi>(
      {}, account_info(), request_metadata(), receiver.Bind());
  RespondToDiscoverRequest("", net::HTTP_OK);

  ASSERT_TRUE(receiver.GetResult());
  const FeedNetwork::ApiResult<feedwire::webfeed::UnfollowWebFeedResponse>&
      result = *receiver.GetResult();
  EXPECT_EQ(net::HTTP_OK, result.response_info.status_code);
  EXPECT_TRUE(result.response_body);
  histogram().ExpectBucketCount(
      "ContentSuggestions.Feed.Network.ResponseStatus.UnfollowWebFeed", 200, 1);
}

TEST_F(FeedNetworkTest, SendApiRequest_ListWebFeedsSendsCorrectContentType) {
  feed_network()->SendApiRequest<ListWebFeedsDiscoverApi>(
      {}, account_info(), request_metadata(), base::DoNothing());
  EXPECT_EQ("application/x-protobuf", RespondToDiscoverRequest("", net::HTTP_OK)
                                          .headers.GetHeader("content-type"));
}

TEST_F(FeedNetworkTest,
       SendApiRequest_DiscoFeedRequestsSendResponseEncodingHeader) {
  feed_network()->SendApiRequest<QueryBackgroundFeedDiscoverApi>(
      {}, account_info(), request_metadata(), base::DoNothing());

  EXPECT_EQ("gzip", RespondToDiscoverRequest("", net::HTTP_OK)
                        .headers.GetHeader("x-response-encoding"));
}

TEST_F(FeedNetworkTest, TestOverrideHostDoesNotAffectDiscoverApis) {
  profile_prefs().SetString(feed::prefs::kHostOverrideHost,
                            "http://www.newhost.com/");
  feed_network()->SendApiRequest<UploadActionsDiscoverApi>(
      GetTestActionRequest(), account_info(), request_metadata(),
      base::DoNothing());

  EXPECT_EQ(GURL("https://discover-pa.googleapis.com/v1/actions:upload"),
            GetPendingRequestURL());
}

TEST_F(FeedNetworkTest, TestOverrideDiscoverEndpoint) {
  profile_prefs().SetString(feed::prefs::kDiscoverAPIEndpointOverride,
                            "http://www.newhost.com/");
  feed_network()->SendApiRequest<UploadActionsDiscoverApi>(
      GetTestActionRequest(), account_info(), request_metadata(),
      base::DoNothing());

  EXPECT_EQ(GURL("http://www.newhost.com/v1/actions:upload"),
            GetPendingRequestURL());
}

TEST_F(FeedNetworkTest, AppCloseRefreshRequestReasonHasUrl) {
  CallbackReceiver<QueryRequestResult> receiver;
  feedwire::Request request = GetTestFeedRequest();
  request.mutable_feed_request()->mutable_feed_query()->set_reason(
      feedwire::FeedQuery::APP_CLOSE_REFRESH);
  feed_network()->SendQueryRequest(NetworkRequestType::kFeedQuery, request,
                                   account_info(), receiver.Bind());
  RespondToQueryRequest(GetTestFeedResponse(), net::HTTP_OK);

  ASSERT_TRUE(receiver.GetResult());
  const QueryRequestResult& result = *receiver.GetResult();
  EXPECT_EQ(net::HTTP_OK, result.response_info.status_code);
  EXPECT_EQ(
      "https://www.google.com/httpservice/noretry/TrellisClankService/"
      "FeedQuery",
      result.response_info.base_request_url);
}

TEST_F(FeedNetworkTest, SendAsynccDataRequest) {
  CallbackReceiver<FeedNetwork::RawResponse> receiver;
  feed_network()->SendAsyncDataRequest(GURL("https://example.com"), "POST",
                                       net::HttpRequestHeaders(), "post data",
                                       account_info(), receiver.Bind());
  response_headers_ = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(
          "HTTP/1.1 200 OK\nname1: value1\nname2: value2\n\n"));
  RespondToDiscoverRequest("dummy response", net::HTTP_OK);

  ASSERT_TRUE(receiver.GetResult());
  const FeedNetwork::RawResponse& result = *receiver.GetResult();
  EXPECT_EQ(net::HTTP_OK, result.response_info.status_code);
  std::vector<std::string> expected_response_headers = {"name1", "value1",
                                                        "name2", "value2"};
  EXPECT_EQ(expected_response_headers,
            result.response_info.response_header_names_and_values);
  EXPECT_EQ("dummy response", result.response_bytes);
}

}  // namespace
}  // namespace feed
