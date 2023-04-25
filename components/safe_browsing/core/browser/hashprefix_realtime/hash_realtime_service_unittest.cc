// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/hashprefix_realtime/hash_realtime_service.h"

#include <memory>

#include "base/base64url.h"
#include "base/strings/escape.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/hashprefix_realtime/hash_realtime_utils.h"
#include "components/safe_browsing/core/browser/hashprefix_realtime/ohttp_key_service.h"
#include "components/safe_browsing/core/browser/verdict_cache_manager.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/safebrowsingv5_alpha1.pb.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "google_apis/google_api_keys.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_network_context.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace safe_browsing {

namespace {
// These two URLs have the same hash prefix. They were determined via a for loop
// up to 100,000 looking for the first two "https://example.a#" URLs whose hash
// prefixes matched.
constexpr char kUrlWithMatchingHashPrefix1[] = "https://example.a23549";
constexpr char kUrlWithMatchingHashPrefix2[] = "https://example.a3945";

constexpr char kTestRelayUrl[] = "https://ohttp.endpoint.test";
constexpr char kOhttpKey[] = "TestOhttpKey";

// A class for testing requests sent via OHTTP. Call |AddResponse| and
// |SetInterceptor| to set up response before |GetViaObliviousHttp| is
// triggered.
class OhttpTestNetworkContext : public network::TestNetworkContext {
 public:
  void GetViaObliviousHttp(
      network::mojom::ObliviousHttpRequestPtr request,
      mojo::PendingRemote<network::mojom::ObliviousHttpClient> client)
      override {
    ++total_requests_;
    remote_.Bind(std::move(client));
    GURL resource_url = request->resource_url;
    if (interceptor_) {
      interceptor_.Run(std::move(request));
    }
    auto it = responses_.find(resource_url);
    ASSERT_TRUE(it != responses_.end());
    if (responses_[resource_url].net_error.has_value()) {
      auto completion_result =
          network::mojom::ObliviousHttpCompletionResult::NewNetError(
              responses_[resource_url].net_error.value());
      remote_->OnCompleted(std::move(completion_result));
    } else if (responses_[resource_url].outer_response_error_code.has_value()) {
      auto completion_result = network::mojom::ObliviousHttpCompletionResult::
          NewOuterResponseErrorCode(
              responses_[resource_url].outer_response_error_code.value());
      remote_->OnCompleted(std::move(completion_result));
    } else {
      auto response = network::mojom::ObliviousHttpResponse::New();
      response->response_body = std::move(responses_[resource_url].body);
      response->headers =
          net::HttpResponseHeaders::TryToCreate("HTTP/1.1 200 OK\r\n");
      if (responses_[resource_url].inner_response_code.has_value()) {
        response->response_code =
            responses_[resource_url].inner_response_code.value();
      } else {
        response->response_code = net::HTTP_OK;
      }
      auto completion_result =
          network::mojom::ObliviousHttpCompletionResult::NewInnerResponse(
              std::move(response));
      remote_->OnCompleted(std::move(completion_result));
    }
    remote_.reset();
  }

  void AddResponse(std::string resource_url,
                   std::string body,
                   absl::optional<int> net_error,
                   absl::optional<int> outer_response_error_code,
                   absl::optional<int> inner_response_code) {
    Response response;
    response.body = body;
    response.net_error = net_error;
    response.outer_response_error_code = outer_response_error_code;
    response.inner_response_code = inner_response_code;
    responses_[GURL(resource_url)] = std::move(response);
  }

  using Interceptor = base::RepeatingCallback<void(
      const network::mojom::ObliviousHttpRequestPtr&)>;
  void SetInterceptor(const Interceptor& interceptor) {
    interceptor_ = interceptor;
  }

  size_t total_requests() const { return total_requests_; }

 private:
  struct Response {
    std::string body;
    absl::optional<int> net_error;
    absl::optional<int> outer_response_error_code;
    absl::optional<int> inner_response_code;
  };

  std::map<GURL, Response> responses_;
  size_t total_requests_ = 0;
  Interceptor interceptor_;
  mojo::Remote<network::mojom::ObliviousHttpClient> remote_;
};

class TestOhttpKeyService : public OhttpKeyService {
 public:
  TestOhttpKeyService()
      : OhttpKeyService(/*url_loader_factory=*/nullptr,
                        /*pref_service=*/nullptr) {}

  void GetOhttpKey(OhttpKeyService::Callback callback) override {
    std::move(callback).Run(ohttp_key_);
  }

  void SetOhttpKey(absl::optional<std::string> ohttp_key) {
    ohttp_key_ = ohttp_key;
  }

  void NotifyLookupResponse(
      const std::string& key,
      int response_code,
      scoped_refptr<net::HttpResponseHeaders> headers) override {
    lookup_response_notified_ = true;
  }

  bool lookup_response_notified() { return lookup_response_notified_; }

 private:
  absl::optional<std::string> ohttp_key_;
  bool lookup_response_notified_ = false;
};

}  // namespace

class HashRealTimeServiceTest : public PlatformTest {
 public:
  HashRealTimeServiceTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        kHashRealTimeOverOhttp,
        {{"SafeBrowsingHashRealTimeOverOhttpRelayUrl", kTestRelayUrl}});
  }

  network::mojom::NetworkContext* GetNetworkContext() {
    return &network_context_;
  }

  void CreateHashRealTimeService() {
    test_url_loader_factory_ =
        std::make_unique<network::TestURLLoaderFactory>();
    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            test_url_loader_factory_.get());
    auto network_context_callback = base::BindRepeating(
        [](HashRealTimeServiceTest* test) { return test->GetNetworkContext(); },
        base::Unretained(this));
    content_setting_map_ = base::MakeRefCounted<HostContentSettingsMap>(
        &test_pref_service_, /*is_off_the_record=*/false,
        /*store_last_modified=*/false, /*restore_session=*/false,
        /*should_record_metrics=*/false);
    VerdictCacheManager* cache_manager_ptr = nullptr;
    if (include_cache_manager_) {
      cache_manager_ = std::make_unique<VerdictCacheManager>(
          /*history_service=*/nullptr, content_setting_map_.get(),
          &test_pref_service_,
          /*sync_observer=*/nullptr);
      cache_manager_ptr = cache_manager_.get();
    }
    ohttp_key_service_ = std::make_unique<TestOhttpKeyService>();
    ohttp_key_service_->SetOhttpKey(kOhttpKey);
    service_ = std::make_unique<HashRealTimeService>(
        test_shared_loader_factory_, network_context_callback,
        cache_manager_ptr, ohttp_key_service_.get(), base::NullCallback());
  }
  void SetUp() override {
    PlatformTest::SetUp();
    HostContentSettingsMap::RegisterProfilePrefs(test_pref_service_.registry());
    CreateHashRealTimeService();
    std::string key = google_apis::GetAPIKey();
    key_param_ =
        !key.empty()
            ? base::StringPrintf("&key=%s",
                                 base::EscapeQueryParamValue(key, true).c_str())
            : "";
  }
  void TearDown() override {
    cache_manager_.reset();
    if (content_setting_map_) {
      content_setting_map_->ShutdownOnUIThread();
    }
    service_->Shutdown();
    PlatformTest::TearDown();
  }

 protected:
  std::vector<FullHashStr> UrlToFullHashes(const GURL& url) {
    std::vector<FullHashStr> full_hashes;
    V4ProtocolManagerUtil::UrlToFullHashes(url, &full_hashes);
    return full_hashes;
  }
  FullHashStr UrlToSingleFullHash(const GURL& url) {
    std::vector<FullHashStr> full_hashes;
    V4ProtocolManagerUtil::UrlToFullHashes(url, &full_hashes);
    EXPECT_EQ(full_hashes.size(), 1u);
    return full_hashes[0];
  }
  std::vector<HashPrefixStr> UrlToHashPrefixes(const GURL& url) {
    std::vector<HashPrefixStr> hash_prefixes;
    for (const auto& full_hash : UrlToFullHashes(url)) {
      hash_prefixes.push_back(hash_realtime_utils::GetHashPrefix(full_hash));
    }
    return hash_prefixes;
  }
  std::set<HashPrefixStr> UrlToHashPrefixesAsSet(const GURL& url) {
    auto hash_prefixes = UrlToHashPrefixes(url);
    return std::set(hash_prefixes.begin(), hash_prefixes.end());
  }
  FullHashStr UrlToSingleHashPrefix(const GURL& url) {
    auto hash_prefixes = UrlToHashPrefixes(url);
    EXPECT_EQ(hash_prefixes.size(), 1u);
    return hash_prefixes[0];
  }
  std::string GetExpectedRequestUrl(
      const std::unique_ptr<V5::SearchHashesRequest>& request) {
    std::string expected_request_base64;
    std::string expected_request_data;
    request->SerializeToString(&expected_request_data);
    base::Base64UrlEncode(expected_request_data,
                          base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                          &expected_request_base64);
    return "https://safebrowsing.googleapis.com/v5alpha1/hashes:search?$req=" +
           expected_request_base64 + "&$ct=application/x-protobuf" + key_param_;
  }
  void SetUpLookupResponseHelper(
      const std::string& request_url,
      const std::unique_ptr<V5::SearchHashesResponse>& response) {
    std::string expected_response_str;
    response->SerializeToString(&expected_response_str);
    network_context_.AddResponse(request_url, expected_response_str,
                                 /*net_error=*/absl::nullopt,
                                 /*outer_response_error_code=*/absl::nullopt,
                                 /*inner_response_code=*/absl::nullopt);
  }
  void SetUpLookupResponse(const std::string& request_url,
                           const std::vector<V5::FullHash>& full_hashes) {
    auto response = std::make_unique<V5::SearchHashesResponse>();
    response->mutable_full_hashes()->Assign(full_hashes.begin(),
                                            full_hashes.end());
    auto* cache_duration = response->mutable_cache_duration();
    cache_duration->set_seconds(300);
    SetUpLookupResponseHelper(request_url, response);
  }
  void ResetMetrics() {
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }
  void CheckPreRequestMetrics(bool expect_cache_hit_all_prefixes,
                              bool expected_backoff_mode_status) {
    histogram_tester_->ExpectTotalCount(
        /*name=*/"SafeBrowsing.HPRT.GetCache.Time", /*expected_count=*/1);
    histogram_tester_->ExpectUniqueSample(
        /*name=*/"SafeBrowsing.HPRT.CacheHitAllPrefixes",
        /*sample=*/expect_cache_hit_all_prefixes,
        /*expected_bucket_count=*/1);
    histogram_tester_->ExpectUniqueSample(
        /*name=*/"SafeBrowsing.HPRT.CacheHitAllPrefixesIfNoQueryParams",
        /*sample=*/expect_cache_hit_all_prefixes,
        /*expected_bucket_count=*/1);
    if (expect_cache_hit_all_prefixes) {
      histogram_tester_->ExpectTotalCount(
          /*name=*/"SafeBrowsing.HPRT.BackoffState", /*expected_count=*/0);
    } else {
      histogram_tester_->ExpectUniqueSample(
          /*name=*/"SafeBrowsing.HPRT.BackoffState",
          /*sample=*/expected_backoff_mode_status,
          /*expected_bucket_count=*/1);
    }
  }
  void CheckPostSuccessfulRequestMetrics(int expected_threat_info_size) {
    histogram_tester_->ExpectUniqueSample(
        /*name=*/"SafeBrowsing.HPRT.ThreatInfoSize",
        /*sample=*/expected_threat_info_size,
        /*expected_bucket_count=*/1);
  }
  void CheckNoPostSuccessfulRequestMetrics() {
    histogram_tester_->ExpectTotalCount(
        /*name=*/"SafeBrowsing.HPRT.ThreatInfoSize", /*expected_count=*/0);
  }
  void CheckRequestMetrics(
      int expected_prefix_count,
      int expected_network_result,
      HashRealTimeService::OperationResult expected_operation_result,
      absl::optional<bool> expected_found_unmatched_full_hashes) {
    histogram_tester_->ExpectUniqueSample(
        /*name=*/"SafeBrowsing.HPRT.Request.CountOfPrefixes",
        /*sample=*/expected_prefix_count,
        /*expected_bucket_count=*/1);
    histogram_tester_->ExpectTotalCount(
        /*name=*/"SafeBrowsing.HPRT.Network.Time", /*expected_count=*/1);
    histogram_tester_->ExpectUniqueSample(
        /*name=*/"SafeBrowsing.HPRT.Network.Result",
        /*sample=*/expected_network_result,
        /*expected_bucket_count=*/1);
    histogram_tester_->ExpectUniqueSample(
        /*name=*/"SafeBrowsing.HPRT.OperationResult",
        /*sample=*/expected_operation_result,
        /*expected_bucket_count=*/1);
    if (expected_found_unmatched_full_hashes.has_value()) {
      histogram_tester_->ExpectUniqueSample(
          /*name=*/"SafeBrowsing.HPRT.FoundUnmatchedFullHashes",
          /*sample=*/expected_found_unmatched_full_hashes.value(),
          /*expected_bucket_count=*/1);
    } else {
      histogram_tester_->ExpectTotalCount(
          /*name=*/"SafeBrowsing.HPRT.FoundUnmatchedFullHashes",
          /*expected_count=*/0);
    }
  }
  void CheckNoNetworkRequestMetric() {
    histogram_tester_->ExpectTotalCount(
        /*name=*/"SafeBrowsing.HPRT.Network.Result",
        /*expected_count=*/0);
  }
  V5::FullHash CreateFullHashProto(
      std::vector<V5::ThreatType> threat_types,
      std::string full_hash,
      absl::optional<std::vector<std::vector<V5::ThreatAttribute>>>
          threat_attributes = absl::nullopt) {
    if (threat_attributes.has_value()) {
      EXPECT_EQ(threat_attributes->size(), threat_types.size());
    }
    auto full_hash_proto = V5::FullHash();
    full_hash_proto.set_full_hash(full_hash);
    for (auto i = 0u; i < threat_types.size(); ++i) {
      auto* details = full_hash_proto.add_full_hash_details();
      details->set_threat_type(threat_types[i]);
      if (threat_attributes.has_value()) {
        for (const auto& attribute : threat_attributes.value()[i]) {
          details->add_attributes(attribute);
        }
      }
    }
    return full_hash_proto;
  }
  void StartSuccessRequest(
      const GURL& url,
      const std::set<FullHashStr>& cached_hash_prefixes,
      base::MockCallback<HPRTLookupResponseCallback>& response_callback,
      const std::vector<V5::FullHash>& response_full_hashes,
      SBThreatType expected_threat_type,
      SBThreatType expected_locally_cached_results_threat_type) {
    // Intercept search hashes request URL.
    auto request = std::make_unique<V5::SearchHashesRequest>();
    for (const auto& hash_prefix : UrlToHashPrefixesAsSet(url)) {
      if (!base::Contains(cached_hash_prefixes, hash_prefix)) {
        request->add_hash_prefixes(hash_prefix);
      }
    }
    std::string expected_url = GetExpectedRequestUrl(request);
    network_context_.SetInterceptor(base::BindLambdaForTesting(
        [&](const network::mojom::ObliviousHttpRequestPtr& ohttp_request) {
          EXPECT_EQ(ohttp_request->method, net::HttpRequestHeaders::kGetMethod);
          EXPECT_EQ(ohttp_request->relay_url, GURL(kTestRelayUrl));
          EXPECT_EQ(ohttp_request->resource_url, GURL(expected_url));
          EXPECT_EQ(ohttp_request->key_config, kOhttpKey);
          EXPECT_EQ(ohttp_request->timeout_duration, base::Seconds(3));
        }));

    // Set up request response.
    SetUpLookupResponse(/*request_url=*/expected_url,
                        /*full_hashes=*/response_full_hashes);

    // Start lookup.
    // Confirm request response will be called once with the relevant threat
    // type.
    EXPECT_CALL(response_callback,
                Run(/*is_lookup_successful=*/true,
                    /*sb_threat_type=*/testing::Optional(expected_threat_type),
                    /*locally_cached_results_threat_type=*/
                    expected_locally_cached_results_threat_type))
        .Times(1);
    service_->StartLookup(url, response_callback.Get(),
                          base::SequencedTaskRunner::GetCurrentDefault());
  }
  // Starts a lookup on |url| that is expected to succeed, simulating the server
  // response as |response_full_hashes|. Confirms that |expected_threat_type| is
  // returned through the lookup's callback. |cached_hash_prefixes| can be empty
  // or can specify which hash prefixes are already in the cache and should
  // therefore not be sent in the request.
  void RunRequestSuccessTest(
      const GURL& url,
      const std::set<FullHashStr>& cached_hash_prefixes,
      std::vector<V5::FullHash> response_full_hashes,
      SBThreatType expected_threat_type,
      SBThreatType expected_locally_cached_results_threat_type,
      int expected_prefix_count,
      int expected_threat_info_size,
      bool expected_found_unmatched_full_hashes) {
    auto num_requests = network_context_.total_requests();
    base::MockCallback<HPRTLookupResponseCallback> response_callback;
    StartSuccessRequest(url, cached_hash_prefixes, response_callback,
                        response_full_hashes, expected_threat_type,
                        expected_locally_cached_results_threat_type);
    task_environment_.RunUntilIdle();

    CheckPreRequestMetrics(/*expect_cache_hit_all_prefixes=*/false,
                           /*expected_backoff_mode_status=*/false);
    CheckRequestMetrics(
        /*expected_prefix_count=*/expected_prefix_count,
        /*expected_network_result=*/200,
        /*expected_operation_result=*/
        HashRealTimeService::OperationResult::kSuccess,
        /*expected_found_unmatched_full_hashes=*/
        expected_found_unmatched_full_hashes);
    CheckPostSuccessfulRequestMetrics(expected_threat_info_size);
    ResetMetrics();

    EXPECT_EQ(network_context_.total_requests(), num_requests + 1u);
    EXPECT_TRUE(ohttp_key_service_->lookup_response_notified());
  }
  // Starts a lookup on |url| that is expected to fail. The simulated server
  // response body can be specified either by |response_full_hashes| or by
  // |custom_response|. |net_error|, |outer_response_error_code| and
  // |inner_response_code| represent the simulated OHTTP handler error.
  // Confirms that the lookup fails.
  void RunRequestFailureTest(
      const GURL& url,
      const absl::optional<std::vector<V5::FullHash>>& response_full_hashes,
      const std::string& custom_response,
      absl::optional<net::Error> net_error,
      absl::optional<int> outer_response_error_code,
      absl::optional<int> inner_response_code,
      int expected_prefix_count,
      int expected_network_result,
      HashRealTimeService::OperationResult expected_operation_result) {
    auto num_requests = network_context_.total_requests();

    // Set up request and response.
    auto request = std::make_unique<V5::SearchHashesRequest>();
    for (const auto& hash_prefix : UrlToHashPrefixesAsSet(url)) {
      request->add_hash_prefixes(hash_prefix);
    }
    std::string expected_url = GetExpectedRequestUrl(request);
    if (response_full_hashes.has_value()) {
      SetUpLookupResponse(
          /*request_url=*/expected_url,
          /*full_hashes=*/response_full_hashes.value());
    } else {
      network_context_.AddResponse(expected_url, custom_response, net_error,
                                   outer_response_error_code,
                                   inner_response_code);
    }

    // Start lookup.
    base::MockCallback<HPRTLookupResponseCallback> response_callback;
    EXPECT_CALL(response_callback,
                Run(/*is_lookup_successful=*/false,
                    /*sb_threat_type=*/testing::Eq(absl::nullopt),
                    /*locally_cached_results_threat_type=*/testing::_))
        .Times(1);
    service_->StartLookup(url, response_callback.Get(),
                          base::SequencedTaskRunner::GetCurrentDefault());
    task_environment_.RunUntilIdle();

    CheckPreRequestMetrics(/*expect_cache_hit_all_prefixes=*/false,
                           /*expected_backoff_mode_status=*/false);
    CheckRequestMetrics(
        /*expected_prefix_count=*/expected_prefix_count,
        /*expected_network_result=*/expected_network_result,
        /*expected_operation_result=*/
        expected_operation_result, /*expected_found_unmatched_full_hashes=*/
        absl::nullopt);
    CheckNoPostSuccessfulRequestMetrics();
    ResetMetrics();

    EXPECT_EQ(network_context_.total_requests(), num_requests + 1u);
    EXPECT_TRUE(ohttp_key_service_->lookup_response_notified());
  }
  // Starts a lookup on |url| that should already be found entirely in the cache
  // and therefore not require a request to be sent. Confirms that the lookup's
  // callback is called with the |expected_threat_type| and that no requests are
  // made.
  void RunFullyCachedRequestTest(const GURL& url,
                                 SBThreatType expected_threat_type,
                                 int expected_threat_info_size) {
    auto num_requests = network_context_.total_requests();
    base::MockCallback<HPRTLookupResponseCallback> response_callback;
    // Confirm request response will be called once with the relevant threat
    // type.
    EXPECT_CALL(
        response_callback,
        Run(/*is_lookup_successful=*/true,
            /*sb_threat_type=*/testing::Optional(expected_threat_type),
            /*locally_cached_results_threat_type=*/expected_threat_type))
        .Times(1);
    service_->StartLookup(url, response_callback.Get(),
                          base::SequencedTaskRunner::GetCurrentDefault());
    task_environment_.RunUntilIdle();

    CheckPreRequestMetrics(/*expect_cache_hit_all_prefixes=*/true,
                           /*expected_backoff_mode_status=*/false);
    CheckNoNetworkRequestMetric();
    CheckPostSuccessfulRequestMetrics(
        /*expected_threat_info_size=*/expected_threat_info_size);
    ResetMetrics();

    EXPECT_EQ(network_context_.total_requests(), num_requests);
  }
  // Starts a lookup on |url| when the service is in backoff mode and a request
  // should not be made. Confirms that the lookup's callback is called noting
  // the lookup failed.
  void RunBackoffRequestTest(const GURL& url) {
    auto num_requests = network_context_.total_requests();
    base::MockCallback<HPRTLookupResponseCallback> response_callback;
    // Confirm request response will be called once with the relevant threat
    // type.
    EXPECT_CALL(response_callback,
                Run(/*is_lookup_successful=*/false,
                    /*sb_threat_type=*/testing::Eq(absl::nullopt),
                    /*locally_cached_results_threat_type=*/
                    SBThreatType::SB_THREAT_TYPE_SAFE))
        .Times(1);
    service_->StartLookup(url, response_callback.Get(),
                          base::SequencedTaskRunner::GetCurrentDefault());
    task_environment_.RunUntilIdle();

    CheckPreRequestMetrics(/*expect_cache_hit_all_prefixes=*/false,
                           /*expected_backoff_mode_status=*/true);
    CheckNoNetworkRequestMetric();
    CheckNoPostSuccessfulRequestMetrics();
    ResetMetrics();

    EXPECT_EQ(network_context_.total_requests(), num_requests);
  }
  void RunSimpleRequest(const GURL& url,
                        const std::vector<V5::FullHash>& response_full_hashes) {
    // Set up request response.
    auto request = std::make_unique<V5::SearchHashesRequest>();
    for (const auto& hash_prefix : UrlToHashPrefixesAsSet(url)) {
      request->add_hash_prefixes(hash_prefix);
    }
    std::string expected_url = GetExpectedRequestUrl(request);
    SetUpLookupResponse(/*request_url=*/expected_url,
                        /*full_hashes=*/response_full_hashes);

    // Start lookup.
    base::MockCallback<HPRTLookupResponseCallback> response_callback;
    EXPECT_CALL(response_callback, Run(testing::_, testing::_, testing::_));
    service_->StartLookup(url, response_callback.Get(),
                          base::SequencedTaskRunner::GetCurrentDefault());
    task_environment_.RunUntilIdle();
  }
  void RunSimpleFailingRequest(const GURL& url,
                               int net_error = net::ERR_FAILED) {
    // Set up request response.
    auto request = std::make_unique<V5::SearchHashesRequest>();
    for (const auto& hash_prefix : UrlToHashPrefixesAsSet(url)) {
      request->add_hash_prefixes(hash_prefix);
    }
    std::string expected_url = GetExpectedRequestUrl(request);
    network_context_.AddResponse(expected_url, "", net_error,
                                 /*outer_response_error_code=*/absl::nullopt,
                                 /*inner_response_code=*/absl::nullopt);

    // Start lookup.
    base::MockCallback<HPRTLookupResponseCallback> response_callback;
    EXPECT_CALL(response_callback, Run(testing::_, testing::_, testing::_));
    service_->StartLookup(url, response_callback.Get(),
                          base::SequencedTaskRunner::GetCurrentDefault());
    task_environment_.RunUntilIdle();
  }
  bool IsThreatTypeMoreSevere(const V5::ThreatType& candidate_threat_type,
                              const V5::ThreatType& baseline_threat_type) {
    return HashRealTimeService::IsThreatTypeMoreSevere(
        candidate_threat_type,
        HashRealTimeService::GetThreatSeverity(baseline_threat_type));
  }
  bool IsThreatTypeMoreSevereThanLeastSeverity(
      const V5::ThreatType& threat_type) {
    return HashRealTimeService::IsThreatTypeMoreSevere(
        threat_type, HashRealTimeService::kLeastSeverity);
  }

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<HashRealTimeService> service_;
  std::unique_ptr<network::TestURLLoaderFactory> test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  OhttpTestNetworkContext network_context_;
  std::string key_param_;
  std::unique_ptr<VerdictCacheManager> cache_manager_;
  std::unique_ptr<TestOhttpKeyService> ohttp_key_service_;
  scoped_refptr<HostContentSettingsMap> content_setting_map_;
  sync_preferences::TestingPrefServiceSyncable test_pref_service_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<base::HistogramTester> histogram_tester_ =
      std::make_unique<base::HistogramTester>();
  bool include_cache_manager_ = true;
};

class HashRealTimeServiceNoCacheManagerTest : public HashRealTimeServiceTest {
 public:
  HashRealTimeServiceNoCacheManagerTest() { include_cache_manager_ = false; }
};

TEST_F(HashRealTimeServiceTest, TestLookup_OneHash) {
  struct TestCase {
    absl::optional<V5::ThreatType> response_threat_type;
    SBThreatType expected_threat_type;
    int expected_threat_info_size;
  } test_cases[] = {
      {absl::nullopt, SB_THREAT_TYPE_SAFE, 0},
      {V5::ThreatType::SOCIAL_ENGINEERING, SB_THREAT_TYPE_URL_PHISHING, 1},
      {V5::ThreatType::MALWARE, SB_THREAT_TYPE_URL_MALWARE, 1},
      {V5::ThreatType::UNWANTED_SOFTWARE, SB_THREAT_TYPE_URL_UNWANTED, 1},
      {V5::ThreatType::SUSPICIOUS, SB_THREAT_TYPE_SUSPICIOUS_SITE, 1},
      {V5::ThreatType::TRICK_TO_BILL, SB_THREAT_TYPE_BILLING, 1},
      // Irrelevant threat type should return safe.
      {V5::ThreatType::API_ABUSE, SB_THREAT_TYPE_SAFE, 0},
  };

  GURL url = GURL("https://example.test");
  for (const auto& test_case : test_cases) {
    std::vector<V5::FullHash> response_full_hashes;
    if (test_case.response_threat_type.has_value()) {
      response_full_hashes.push_back(CreateFullHashProto(
          {test_case.response_threat_type.value()}, UrlToSingleFullHash(url)));
    }
    RunRequestSuccessTest(
        /*url=*/url, /*cached_hash_prefixes=*/{}, /*response_full_hashes=*/
        response_full_hashes,
        /*expected_threat_type=*/test_case.expected_threat_type,
        /*expected_locally_cached_results_threat_type=*/
        SBThreatType::SB_THREAT_TYPE_SAFE,
        /*expected_prefix_count=*/1,
        /*expected_threat_info_size=*/test_case.expected_threat_info_size,
        /*expected_found_unmatched_full_hashes=*/false);
    // Fast forward to avoid subsequent test cases just pulling from the cache.
    task_environment_.FastForwardBy(base::Minutes(10));
  }
}

TEST_F(HashRealTimeServiceTest, TestLookup_OverlappingHashPrefixes) {
  GURL url1 = GURL(kUrlWithMatchingHashPrefix1);
  GURL url2 = GURL(kUrlWithMatchingHashPrefix2);
  // To make sure this test is a useful test, sanity check that the URL hash
  // prefixes are indeed the same.
  EXPECT_EQ(UrlToSingleHashPrefix(url1), UrlToSingleHashPrefix(url2));
  RunRequestSuccessTest(
      /*url=*/url1, /*cached_hash_prefixes=*/{}, /*response_full_hashes=*/
      {CreateFullHashProto({V5::ThreatType::SOCIAL_ENGINEERING},
                           UrlToSingleFullHash(url2))},
      /*expected_threat_type=*/SBThreatType::SB_THREAT_TYPE_SAFE,
      /*expected_locally_cached_results_threat_type=*/
      SBThreatType::SB_THREAT_TYPE_SAFE,
      /*expected_prefix_count=*/1,
      /*expected_threat_info_size=*/0,
      /*expected_found_unmatched_full_hashes=*/false);
}

TEST_F(HashRealTimeServiceTest, TestLookup_MaxHashes_Phishing) {
  GURL url = GURL("https://a.b.c.d.e.f.g/1/2/3/4/5/6?param=x");
  // |expected_prefix_count| is 30 because this is the maximum number of host
  // suffix and path prefix combinations, as described in
  // https://developers.google.com/safe-browsing/v4/urls-hashing#suffixprefix-expressions.
  RunRequestSuccessTest(
      /*url=*/url, /*cached_hash_prefixes=*/{}, /*response_full_hashes=*/
      {CreateFullHashProto({V5::ThreatType::SOCIAL_ENGINEERING},
                           UrlToFullHashes(url)[0]),
       CreateFullHashProto({V5::ThreatType::SOCIAL_ENGINEERING},
                           UrlToFullHashes(url)[15])},
      /*expected_threat_type=*/SBThreatType::SB_THREAT_TYPE_URL_PHISHING,
      /*expected_locally_cached_results_threat_type=*/
      SBThreatType::SB_THREAT_TYPE_SAFE,
      /*expected_prefix_count=*/30,
      /*expected_threat_info_size=*/2,
      /*expected_found_unmatched_full_hashes=*/false);
}

TEST_F(HashRealTimeServiceTest,
       TestLookup_MaxHashes_SomeRelevantSomeIrrelevant) {
  GURL url = GURL("https://a.b.c.d.e.f.g/1/2/3/4/5/6?param=x");
  std::string non_matching_full_hash =
      UrlToHashPrefixes(url)[0] + "1111111111111111111111111111";
  // |expected_prefix_count| is 30 because this is the maximum number of host
  // suffix and path prefix combinations, as described in
  // https://developers.google.com/safe-browsing/v4/urls-hashing#suffixprefix-expressions.
  RunRequestSuccessTest(
      /*url=*/url, /*cached_hash_prefixes=*/{}, /*response_full_hashes=*/
      {CreateFullHashProto({V5::ThreatType::SOCIAL_ENGINEERING},
                           UrlToFullHashes(url)[0]),
       CreateFullHashProto({V5::ThreatType::MALWARE}, UrlToFullHashes(url)[2]),
       CreateFullHashProto({V5::ThreatType::API_ABUSE,
                            V5::ThreatType::POTENTIALLY_HARMFUL_APPLICATION},
                           UrlToFullHashes(url)[4]),
       CreateFullHashProto(
           {V5::ThreatType::MALWARE, V5::ThreatType::SOCIAL_ENGINEERING,
            V5::ThreatType::UNWANTED_SOFTWARE},
           UrlToFullHashes(url)[6]),
       CreateFullHashProto(
           {V5::ThreatType::MALWARE, V5::ThreatType::SOCIAL_ENGINEERING,
            V5::ThreatType::UNWANTED_SOFTWARE, V5::ThreatType::API_ABUSE},
           UrlToFullHashes(url)[8]),
       CreateFullHashProto({V5::ThreatType::SOCIAL_ENGINEERING},
                           UrlToFullHashes(url)[15]),
       CreateFullHashProto(
           {V5::ThreatType::SOCIAL_ENGINEERING, V5::ThreatType::MALWARE},
           non_matching_full_hash)},
      /*expected_threat_type=*/SBThreatType::SB_THREAT_TYPE_URL_PHISHING,
      /*expected_locally_cached_results_threat_type=*/
      SBThreatType::SB_THREAT_TYPE_SAFE,
      /*expected_prefix_count=*/30,
      /*expected_threat_info_size=*/9,
      /*expected_found_unmatched_full_hashes=*/false);
}

TEST_F(HashRealTimeServiceTest, TestLookup_MaxHashes_OnlyIrrelevant) {
  GURL url = GURL("https://a.b.c.d.e.f.g/1/2/3/4/5/6?param=x");
  std::string rest_of_hash = "1111111111111111111111111111";
  // |expected_prefix_count| is 30 because this is the maximum number of host
  // suffix and path prefix combinations, as described in
  // https://developers.google.com/safe-browsing/v4/urls-hashing#suffixprefix-expressions.
  RunRequestSuccessTest(
      /*url=*/url, /*cached_hash_prefixes=*/{}, /*response_full_hashes=*/
      {CreateFullHashProto({V5::ThreatType::SOCIAL_ENGINEERING},
                           UrlToHashPrefixes(url)[0] + rest_of_hash),
       CreateFullHashProto({V5::ThreatType::MALWARE},
                           UrlToHashPrefixes(url)[2] + rest_of_hash),
       CreateFullHashProto({V5::ThreatType::API_ABUSE,
                            V5::ThreatType::POTENTIALLY_HARMFUL_APPLICATION},
                           UrlToFullHashes(url)[4]),
       CreateFullHashProto(
           {V5::ThreatType::MALWARE, V5::ThreatType::SOCIAL_ENGINEERING,
            V5::ThreatType::UNWANTED_SOFTWARE},
           UrlToHashPrefixes(url)[6] + rest_of_hash),
       CreateFullHashProto(
           {V5::ThreatType::MALWARE, V5::ThreatType::SOCIAL_ENGINEERING,
            V5::ThreatType::UNWANTED_SOFTWARE, V5::ThreatType::API_ABUSE},
           UrlToHashPrefixes(url)[8] + rest_of_hash),
       CreateFullHashProto({V5::ThreatType::SOCIAL_ENGINEERING},
                           UrlToHashPrefixes(url)[15] + rest_of_hash)},
      /*expected_threat_type=*/SBThreatType::SB_THREAT_TYPE_SAFE,
      /*expected_locally_cached_results_threat_type=*/
      SBThreatType::SB_THREAT_TYPE_SAFE,
      /*expected_prefix_count=*/30,
      /*expected_threat_info_size=*/0,
      /*expected_found_unmatched_full_hashes=*/false);
}

TEST_F(HashRealTimeServiceTest, TestLookup_CompetingSeverities) {
  GURL url = GURL("https://example.test");
  RunRequestSuccessTest(
      /*url=*/url, /*cached_hash_prefixes=*/{}, /*response_full_hashes=*/
      {CreateFullHashProto(
          {V5::ThreatType::UNWANTED_SOFTWARE, V5::ThreatType::MALWARE,
           V5::ThreatType::SOCIAL_ENGINEERING},
          UrlToSingleFullHash(url))},
      /*expected_threat_type=*/SBThreatType::SB_THREAT_TYPE_URL_MALWARE,
      /*expected_locally_cached_results_threat_type=*/
      SBThreatType::SB_THREAT_TYPE_SAFE,
      /*expected_prefix_count=*/1,
      /*expected_threat_info_size=*/3,
      /*expected_found_unmatched_full_hashes=*/false);
}

TEST_F(HashRealTimeServiceTest, TestLookup_Attributes) {
  GURL url = GURL("https://example.test");
  std::vector<std::vector<V5::ThreatAttribute>> attributes = {
      {V5::ThreatAttribute::CANARY},
      {V5::ThreatAttribute::CANARY, V5::ThreatAttribute::FRAME_ONLY},
      {V5::ThreatAttribute::FRAME_ONLY}};
  RunRequestSuccessTest(
      /*url=*/url,
      /*cached_hash_prefixes=*/{}, /*response_full_hashes=*/
      {CreateFullHashProto(
          {V5::ThreatType::UNWANTED_SOFTWARE, V5::ThreatType::MALWARE,
           V5::ThreatType::SOCIAL_ENGINEERING},
          UrlToSingleFullHash(url), attributes)},
      /*expected_threat_type=*/SBThreatType::SB_THREAT_TYPE_URL_MALWARE,
      /*expected_locally_cached_results_threat_type=*/
      SBThreatType::SB_THREAT_TYPE_SAFE,
      /*expected_prefix_count=*/1,
      /*expected_threat_info_size=*/3,
      /*expected_found_unmatched_full_hashes=*/false);
}

TEST_F(HashRealTimeServiceTest, TestLookup_InvalidThreatTypes) {
  auto run_test = [this](GURL url, std::vector<V5::ThreatType> threat_types,
                         SBThreatType expected_threat_type,
                         int expected_threat_info_size) {
    RunRequestSuccessTest(
        /*url=*/url, /*cached_hash_prefixes=*/{}, /*response_full_hashes=*/
        {CreateFullHashProto(threat_types, UrlToSingleFullHash(url))},
        /*expected_threat_type=*/expected_threat_type,
        /*expected_locally_cached_results_threat_type=*/
        SBThreatType::SB_THREAT_TYPE_SAFE,
        /*expected_prefix_count=*/1,
        /*expected_threat_info_size=*/expected_threat_info_size,
        /*expected_found_unmatched_full_hashes=*/false);
  };
  // Sanity check the static casting on a valid threat type is not filtered out.
  run_test(GURL("https://example.test1"), {static_cast<V5::ThreatType>(2)},
           SBThreatType::SB_THREAT_TYPE_URL_PHISHING,
           /*expected_threat_info_size=*/1);
  // -1 is an invalid threat type and should be filtered out.
  run_test(GURL("https://example.test2"),
           {V5::ThreatType::UNWANTED_SOFTWARE, static_cast<V5::ThreatType>(-1),
            V5::ThreatType::SOCIAL_ENGINEERING},
           SBThreatType::SB_THREAT_TYPE_URL_PHISHING,
           /*expected_threat_info_size=*/2);
  // All are invalid threat types and should be filtered out.
  run_test(GURL("https://example.test3"),
           {static_cast<V5::ThreatType>(-3), static_cast<V5::ThreatType>(-1),
            static_cast<V5::ThreatType>(-2)},
           SBThreatType::SB_THREAT_TYPE_SAFE,
           /*expected_threat_info_size=*/0);
  // -3 and -2 are invalid threat types and should be filtered out.
  run_test(GURL("https://example.test4"),
           {static_cast<V5::ThreatType>(-3), V5::ThreatType::MALWARE,
            static_cast<V5::ThreatType>(-2)},
           SBThreatType::SB_THREAT_TYPE_URL_MALWARE,
           /*expected_threat_info_size=*/1);
}

TEST_F(HashRealTimeServiceTest, TestLookup_InvalidAttributes) {
  // Threat types are different.
  {
    GURL url = GURL("https://example.test1");
    std::vector<std::vector<V5::ThreatAttribute>> attributes = {
        {V5::ThreatAttribute::CANARY},
        {static_cast<V5::ThreatAttribute>(-1), V5::ThreatAttribute::FRAME_ONLY},
        {static_cast<V5::ThreatAttribute>(-2)}};
    RunRequestSuccessTest(
        /*url=*/url,
        /*cached_hash_prefixes=*/{}, /*response_full_hashes=*/
        {CreateFullHashProto(
            {V5::ThreatType::UNWANTED_SOFTWARE, V5::ThreatType::MALWARE,
             V5::ThreatType::SOCIAL_ENGINEERING},
            UrlToSingleFullHash(url), attributes)},
        /*expected_threat_type=*/SBThreatType::SB_THREAT_TYPE_URL_UNWANTED,
        /*expected_locally_cached_results_threat_type=*/
        SBThreatType::SB_THREAT_TYPE_SAFE,
        /*expected_prefix_count=*/1,
        /*expected_threat_info_size=*/1,
        /*expected_found_unmatched_full_hashes=*/false);
  }
  // Threat types are the same.
  {
    GURL url = GURL("https://example.test2");
    std::vector<std::vector<V5::ThreatAttribute>> attributes = {
        {V5::ThreatAttribute::CANARY},
        {V5::ThreatAttribute::FRAME_ONLY},
        {static_cast<V5::ThreatAttribute>(-1), V5::ThreatAttribute::FRAME_ONLY},
        {static_cast<V5::ThreatAttribute>(-2)}};
    RunRequestSuccessTest(
        /*url=*/url,
        /*cached_hash_prefixes=*/{}, /*response_full_hashes=*/
        {CreateFullHashProto({V5::ThreatType::UNWANTED_SOFTWARE,
                              V5::ThreatType::UNWANTED_SOFTWARE,
                              V5::ThreatType::UNWANTED_SOFTWARE,
                              V5::ThreatType::UNWANTED_SOFTWARE},
                             UrlToSingleFullHash(url), attributes)},
        /*expected_threat_type=*/SBThreatType::SB_THREAT_TYPE_URL_UNWANTED,
        /*expected_locally_cached_results_threat_type=*/
        SBThreatType::SB_THREAT_TYPE_SAFE,
        /*expected_prefix_count=*/1,
        /*expected_threat_info_size=*/2,
        /*expected_found_unmatched_full_hashes=*/false);
  }
}

TEST_F(HashRealTimeServiceTest, TestLookup_UnmatchedFullHashesInResponse) {
  // The code should still function properly even if the server sends back
  // FullHash objects that don't match the requested hash prefixes.
  {
    GURL url = GURL("https://example.test1");
    GURL other_url = GURL("https://example.test2");
    RunRequestSuccessTest(
        /*url=*/url,
        /*cached_hash_prefixes=*/{}, /*response_full_hashes=*/
        {CreateFullHashProto({V5::ThreatType::MALWARE},
                             UrlToSingleFullHash(other_url))},
        /*expected_threat_type=*/SBThreatType::SB_THREAT_TYPE_SAFE,
        /*expected_locally_cached_results_threat_type=*/
        SBThreatType::SB_THREAT_TYPE_SAFE,
        /*expected_prefix_count=*/1,
        /*expected_threat_info_size=*/0,
        /*expected_found_unmatched_full_hashes=*/true);
  }
  {
    GURL url = GURL("https://example.test3");
    GURL other_url = GURL("https://example.test4");
    RunRequestSuccessTest(
        /*url=*/url,
        /*cached_hash_prefixes=*/{}, /*response_full_hashes=*/
        {CreateFullHashProto({V5::ThreatType::MALWARE},
                             UrlToSingleFullHash(other_url)),
         CreateFullHashProto({V5::ThreatType::UNWANTED_SOFTWARE},
                             UrlToSingleFullHash(url))},
        /*expected_threat_type=*/SBThreatType::SB_THREAT_TYPE_URL_UNWANTED,
        /*expected_locally_cached_results_threat_type=*/
        SBThreatType::SB_THREAT_TYPE_SAFE,
        /*expected_prefix_count=*/1,
        /*expected_threat_info_size=*/1,
        /*expected_found_unmatched_full_hashes=*/true);
  }
}

TEST_F(HashRealTimeServiceTest, TestLookup_DuplicateFullHashesInResponse) {
  // The code should still function properly even if the server sends back
  // multiple FullHash objects with the same full_hash string.
  GURL url1 = GURL("https://example.test1");
  RunRequestSuccessTest(
      /*url=*/url1,
      /*cached_hash_prefixes=*/{}, /*response_full_hashes=*/
      {CreateFullHashProto({V5::ThreatType::MALWARE},
                           UrlToSingleFullHash(url1)),
       CreateFullHashProto({V5::ThreatType::UNWANTED_SOFTWARE},
                           UrlToSingleFullHash(url1))},
      /*expected_threat_type=*/SBThreatType::SB_THREAT_TYPE_URL_MALWARE,
      /*expected_locally_cached_results_threat_type=*/
      SBThreatType::SB_THREAT_TYPE_SAFE,
      /*expected_prefix_count=*/1,
      /*expected_threat_info_size=*/2,
      /*expected_found_unmatched_full_hashes=*/false);
  // Run it with the server responses backwards as well to confirm order doesn't
  // matter.
  GURL url2 = GURL("https://example.test2");
  RunRequestSuccessTest(
      /*url=*/url2,
      /*cached_hash_prefixes=*/{}, /*response_full_hashes=*/
      {CreateFullHashProto({V5::ThreatType::UNWANTED_SOFTWARE},
                           UrlToSingleFullHash(url2)),
       CreateFullHashProto({V5::ThreatType::MALWARE},
                           UrlToSingleFullHash(url2))},
      /*expected_threat_type=*/SBThreatType::SB_THREAT_TYPE_URL_MALWARE,
      /*expected_locally_cached_results_threat_type=*/
      SBThreatType::SB_THREAT_TYPE_SAFE,
      /*expected_prefix_count=*/1,
      /*expected_threat_info_size=*/2,
      /*expected_found_unmatched_full_hashes=*/false);
}

TEST_F(HashRealTimeServiceTest, TestLookup_DuplicateFullHashDetailsInResponse) {
  // The code should still function properly even if the server sends back
  // multiple FullHashDetail objects with the same threat_type.
  GURL url = GURL("https://example.test");
  RunRequestSuccessTest(
      /*url=*/url,
      /*cached_hash_prefixes=*/{}, /*response_full_hashes=*/
      {CreateFullHashProto(
          {V5::ThreatType::UNWANTED_SOFTWARE, V5::ThreatType::UNWANTED_SOFTWARE,
           V5::ThreatType::MALWARE, V5::ThreatType::UNWANTED_SOFTWARE},
          UrlToSingleFullHash(url))},
      /*expected_threat_type=*/SBThreatType::SB_THREAT_TYPE_URL_MALWARE,
      /*expected_locally_cached_results_threat_type=*/
      SBThreatType::SB_THREAT_TYPE_SAFE,
      /*expected_prefix_count=*/1,
      /*expected_threat_info_size=*/4,
      /*expected_found_unmatched_full_hashes=*/false);
}

TEST_F(HashRealTimeServiceTest, TestLookupFailure_NetError) {
  GURL url = GURL("https://example.test");
  RunRequestFailureTest(
      /*url=*/url, /*response_full_hashes=*/absl::nullopt,
      /*custom_response=*/"",
      /*net_error=*/net::ERR_FAILED,
      /*outer_response_error_code=*/absl::nullopt,
      /*inner_response_code=*/absl::nullopt,
      /*expected_prefix_count=*/1,
      /*expected_network_result=*/net::ERR_FAILED,
      /*expected_operation_result=*/
      HashRealTimeService::OperationResult::kNetworkError);
}
TEST_F(HashRealTimeServiceTest, TestLookupFailure_RetriableNetError) {
  GURL url = GURL("https://example.test");
  // Retriable errors are not yet implemented for the OHTTP path, so they should
  // still be considered a network error.
  RunRequestFailureTest(
      /*url=*/url, /*response_full_hashes=*/absl::nullopt,
      /*custom_response=*/"",
      /*net_error=*/net::ERR_INTERNET_DISCONNECTED,
      /*outer_response_error_code=*/absl::nullopt,
      /*inner_response_code=*/absl::nullopt,
      /*expected_prefix_count=*/1,
      /*expected_network_result=*/net::ERR_INTERNET_DISCONNECTED,
      /*expected_operation_result=*/
      HashRealTimeService::OperationResult::kNetworkError);
}
TEST_F(HashRealTimeServiceTest, TestLookupFailure_NetErrorHttpCodeFailure) {
  GURL url = GURL("https://example.test");
  RunRequestFailureTest(
      /*url=*/url, /*response_full_hashes=*/absl::nullopt,
      /*custom_response=*/"",
      /*net_error=*/net::ERR_HTTP_RESPONSE_CODE_FAILURE,
      /*outer_response_error_code=*/absl::nullopt,
      /*inner_response_code=*/absl::nullopt,
      /*expected_prefix_count=*/1,
      /*expected_network_result=*/0,
      /*expected_operation_result=*/
      HashRealTimeService::OperationResult::kHttpError);
}
TEST_F(HashRealTimeServiceTest, TestLookupFailure_OuterResponseCodeError) {
  GURL url = GURL("https://example.test");
  RunRequestFailureTest(
      /*url=*/url, /*response_full_hashes=*/absl::nullopt,
      /*custom_response=*/"",
      /*net_error=*/absl::nullopt,
      /*outer_response_error_code=*/net::HTTP_NOT_FOUND,
      /*inner_response_code=*/absl::nullopt,
      /*expected_prefix_count=*/1,
      /*expected_network_result=*/net::HTTP_NOT_FOUND,
      /*expected_operation_result=*/
      HashRealTimeService::OperationResult::kHttpError);
}
TEST_F(HashRealTimeServiceTest, TestLookupFailure_InnerResponseCodeError) {
  GURL url = GURL("https://example.test");
  RunRequestFailureTest(
      /*url=*/url, /*response_full_hashes=*/absl::nullopt,
      /*custom_response=*/"",
      /*net_error=*/absl::nullopt, /*outer_response_error_code=*/absl::nullopt,
      /*inner_response_code=*/net::HTTP_UNAUTHORIZED,
      /*expected_prefix_count=*/1,
      /*expected_network_result=*/net::HTTP_UNAUTHORIZED,
      /*expected_operation_result=*/
      HashRealTimeService::OperationResult::kHttpError);
}
TEST_F(HashRealTimeServiceTest, TestLookupFailure_ParseResponse) {
  GURL url = GURL("https://example.test");
  RunRequestFailureTest(
      /*url=*/url, /*response_full_hashes=*/absl::nullopt,
      /*custom_response=*/"howdy",
      /*net_error=*/absl::nullopt, /*outer_response_error_code=*/absl::nullopt,
      /*inner_response_code=*/absl::nullopt, /*expected_prefix_count=*/1,
      /*expected_network_result=*/net::HTTP_OK,
      /*expected_operation_result=*/
      HashRealTimeService::OperationResult::kParseError);
}
TEST_F(HashRealTimeServiceTest, TestLookupFailure_IncorrectFullHashLength) {
  GURL url = GURL("https://example.test");
  auto short_full_hash = UrlToSingleFullHash(url).substr(0, 31);
  RunRequestFailureTest(
      /*url=*/url, /*response_full_hashes=*/
      absl::optional<std::vector<V5::FullHash>>({CreateFullHashProto(
          {V5::ThreatType::SOCIAL_ENGINEERING}, short_full_hash)}),
      /*custom_response=*/"",
      /*net_error=*/absl::nullopt, /*outer_response_error_code=*/absl::nullopt,
      /*inner_response_code=*/absl::nullopt, /*expected_prefix_count=*/1,
      /*expected_network_result=*/net::HTTP_OK,
      /*expected_operation_result=*/
      HashRealTimeService::OperationResult::kIncorrectFullHashLengthError);
}
TEST_F(HashRealTimeServiceTest, TestLookupFailure_MissingCacheDuration) {
  GURL url = GURL("https://example.test");
  auto response = std::make_unique<V5::SearchHashesResponse>();
  std::string response_str;
  response->SerializeToString(&response_str);

  RunRequestFailureTest(
      /*url=*/url, /*response_full_hashes=*/{},
      /*custom_response=*/response_str,
      /*net_error=*/absl::nullopt, /*outer_response_error_code=*/absl::nullopt,
      /*inner_response_code=*/absl::nullopt,
      /*expected_prefix_count=*/1,
      /*expected_network_result=*/net::HTTP_OK,
      /*expected_operation_result=*/
      HashRealTimeService::OperationResult::kNoCacheDurationError);
}
TEST_F(HashRealTimeServiceTest, TestLookupFailure_MissingOhttpKey) {
  GURL url = GURL("https://example.test");
  ohttp_key_service_->SetOhttpKey(absl::nullopt);
  base::MockCallback<HPRTLookupResponseCallback> response_callback;
  EXPECT_CALL(response_callback,
              Run(/*is_lookup_successful=*/false,
                  /*sb_threat_type=*/testing::Eq(absl::nullopt),
                  /*locally_cached_results_threat_type=*/testing::_))
      .Times(1);
  service_->StartLookup(url, response_callback.Get(),
                        base::SequencedTaskRunner::GetCurrentDefault());
  task_environment_.RunUntilIdle();

  CheckNoNetworkRequestMetric();
  // If the OHTTP key is missing, lookup should fail before making a request to
  // network_context_.
  EXPECT_EQ(network_context_.total_requests(), 0u);
}

TEST_F(HashRealTimeServiceTest, TestFullyCached_OneHash_Safe) {
  GURL url = GURL("https://example.test");
  RunSimpleRequest(
      /*url=*/url, /*response_full_hashes=*/{});
  ResetMetrics();
  RunFullyCachedRequestTest(
      /*url=*/url,
      /*expected_threat_type=*/SBThreatType::SB_THREAT_TYPE_SAFE,
      /*expected_threat_info_size=*/0);
}

TEST_F(HashRealTimeServiceTest, TestFullyCached_OneHash_Phishing) {
  GURL url = GURL("https://example.test");
  RunSimpleRequest(
      /*url=*/url, /*response_full_hashes=*/
      {CreateFullHashProto({V5::ThreatType::SOCIAL_ENGINEERING},
                           UrlToSingleFullHash(url))});
  ResetMetrics();
  RunFullyCachedRequestTest(
      /*url=*/url,
      /*expected_threat_type=*/SBThreatType::SB_THREAT_TYPE_URL_PHISHING,
      /*expected_threat_info_size=*/1);
}

TEST_F(HashRealTimeServiceTest, TestFullyCached_MaxHashes) {
  GURL url = GURL("https://a.b.c.d.e.f.g/1/2/3/4/5/6?param=x");
  RunSimpleRequest(
      /*url=*/url, /*response_full_hashes=*/
      {CreateFullHashProto({V5::ThreatType::SOCIAL_ENGINEERING},
                           UrlToFullHashes(url)[0]),
       CreateFullHashProto(
           {V5::ThreatType::MALWARE, V5::ThreatType::SOCIAL_ENGINEERING},
           UrlToFullHashes(url)[2]),
       CreateFullHashProto({V5::ThreatType::API_ABUSE,
                            V5::ThreatType::POTENTIALLY_HARMFUL_APPLICATION},
                           UrlToFullHashes(url)[4])});
  ResetMetrics();
  RunFullyCachedRequestTest(
      /*url=*/url,
      /*expected_threat_type=*/SBThreatType::SB_THREAT_TYPE_URL_PHISHING,
      /*expected_threat_info_size=*/3);
}

TEST_F(HashRealTimeServiceTest, TestFullyCached_OverlappingHashPrefixes) {
  GURL url1 = GURL(kUrlWithMatchingHashPrefix1);
  GURL url2 = GURL(kUrlWithMatchingHashPrefix2);
  // To make sure this test is a useful test, sanity check that the URL hash
  // prefixes are indeed the same.
  EXPECT_EQ(UrlToSingleHashPrefix(url1), UrlToSingleHashPrefix(url2));
  // Start a lookup for url1, which is a phishing page.
  RunRequestSuccessTest(
      /*url=*/url1,
      /*cached_hash_prefixes=*/{}, /*response_full_hashes=*/
      {CreateFullHashProto({V5::ThreatType::SOCIAL_ENGINEERING},
                           UrlToSingleFullHash(url1))},
      /*expected_threat_type=*/SBThreatType::SB_THREAT_TYPE_URL_PHISHING,
      /*expected_locally_cached_results_threat_type=*/
      SBThreatType::SB_THREAT_TYPE_SAFE,
      /*expected_prefix_count=*/1, /*expected_threat_info_size=*/1,
      /*expected_found_unmatched_full_hashes=*/false);
  ResetMetrics();
  // Start a lookup for url2. This has the same hash prefix as url1, so the
  // results are fully cached, and no request is sent.
  RunFullyCachedRequestTest(
      /*url=*/url2,
      /*expected_threat_type=*/SBThreatType::SB_THREAT_TYPE_SAFE,
      /*expected_threat_info_size=*/0);
}

// Since phishing is more severe than unwanted software, the results from the
// second request should be used rather than the cached results.
TEST_F(HashRealTimeServiceTest, TestPartiallyCached_RequestResultsUsed) {
  GURL url1 = GURL("https://e.f.g/1/2/");
  RunSimpleRequest(
      /*url=*/url1, /*response_full_hashes=*/
      {CreateFullHashProto({V5::ThreatType::UNWANTED_SOFTWARE},
                           UrlToFullHashes(url1).back())});
  ResetMetrics();
  GURL url2 = GURL("https://a.b.c.d.e.f.g/1/2/3/4/5/6?param=x");
  RunRequestSuccessTest(
      /*url=*/url2, /*cached_hash_prefixes=*/UrlToHashPrefixesAsSet(url1),
      /*response_full_hashes=*/
      {CreateFullHashProto({V5::ThreatType::SOCIAL_ENGINEERING},
                           UrlToFullHashes(url2).back())},
      /*expected_threat_type=*/SBThreatType::SB_THREAT_TYPE_URL_PHISHING,
      /*expected_locally_cached_results_threat_type=*/
      SBThreatType::SB_THREAT_TYPE_URL_UNWANTED,
      /*expected_prefix_count=*/24, /*expected_threat_info_size=*/2,
      /*expected_found_unmatched_full_hashes=*/false);
}

// Since phishing is more severe than unwanted software, the cached results
// should be used rather than the results from the second request.
TEST_F(HashRealTimeServiceTest, TestPartiallyCached_CachedResultsUsed) {
  GURL url1 = GURL("https://e.f.g/1/2/");
  RunSimpleRequest(
      /*url=*/url1, /*response_full_hashes=*/
      {CreateFullHashProto({V5::ThreatType::SOCIAL_ENGINEERING},
                           UrlToFullHashes(url1).back())});
  ResetMetrics();
  GURL url2 = GURL("https://a.b.c.d.e.f.g/1/2/3/4/5/6?param=x");
  RunRequestSuccessTest(
      /*url=*/url2, /*cached_hash_prefixes=*/UrlToHashPrefixesAsSet(url1),
      /*response_full_hashes=*/
      {CreateFullHashProto({V5::ThreatType::UNWANTED_SOFTWARE},
                           UrlToFullHashes(url2).back())},
      /*expected_threat_type=*/SBThreatType::SB_THREAT_TYPE_URL_PHISHING,
      /*expected_locally_cached_results_threat_type=*/
      SBThreatType::SB_THREAT_TYPE_URL_PHISHING,
      /*expected_prefix_count=*/24, /*expected_threat_info_size=*/2,
      /*expected_found_unmatched_full_hashes=*/false);
}

TEST_F(HashRealTimeServiceTest, TestCacheDuration) {
  GURL url = GURL("https://example.test");
  RunSimpleRequest(
      /*url=*/url, /*response_full_hashes=*/{});
  ResetMetrics();
  // Time = 200 seconds. Re-check the URL, which should be cached.
  task_environment_.FastForwardBy(base::Seconds(200));
  RunFullyCachedRequestTest(
      /*url=*/url,
      /*expected_threat_type=*/SBThreatType::SB_THREAT_TYPE_SAFE,
      /*expected_threat_info_size=*/0);
  // Time = 300 seconds. Re-check the URL, which should no longer be cached and
  // therefore should send a request.
  task_environment_.FastForwardBy(base::Seconds(100));
  RunRequestSuccessTest(
      /*url=*/url, /*cached_hash_prefixes=*/{}, /*response_full_hashes=*/{},
      /*expected_threat_type=*/SBThreatType::SB_THREAT_TYPE_SAFE,
      /*expected_locally_cached_results_threat_type=*/
      SBThreatType::SB_THREAT_TYPE_SAFE,
      /*expected_prefix_count=*/1,
      /*expected_threat_info_size=*/0,
      /*expected_found_unmatched_full_hashes=*/false);
}

TEST_F(HashRealTimeServiceNoCacheManagerTest, TestNoCaching) {
  GURL url = GURL("https://example.test");
  RunSimpleRequest(
      /*url=*/url, /*response_full_hashes=*/
      {CreateFullHashProto({V5::ThreatType::SOCIAL_ENGINEERING},
                           UrlToSingleFullHash(url))});
  ResetMetrics();
  RunRequestSuccessTest(
      /*url=*/url, /*cached_hash_prefixes=*/{}, /*response_full_hashes=*/{},
      /*expected_threat_type=*/SBThreatType::SB_THREAT_TYPE_SAFE,
      /*expected_locally_cached_results_threat_type=*/
      SBThreatType::SB_THREAT_TYPE_SAFE,
      /*expected_prefix_count=*/1,
      /*expected_threat_info_size=*/0,
      /*expected_found_unmatched_full_hashes=*/false);
}

TEST_F(HashRealTimeServiceTest, TestShutdown) {
  {
    GURL url = GURL("https://example.test");
    // Set up request response.
    auto request = std::make_unique<V5::SearchHashesRequest>();
    for (const auto& hash_prefix : UrlToHashPrefixesAsSet(url)) {
      request->add_hash_prefixes(hash_prefix);
    }
    std::string expected_url = GetExpectedRequestUrl(request);
    SetUpLookupResponse(/*request_url=*/expected_url,
                        /*full_hashes=*/{});
    // Start lookup, setting up the expectation that the response_callback is
    // not called due to shutdown. It should still send the initial request
    // since it happens before Shutdown.
    base::MockCallback<HPRTLookupResponseCallback> response_callback;
    EXPECT_CALL(response_callback, Run(testing::_, testing::_, testing::_))
        .Times(0);
    service_->StartLookup(url, response_callback.Get(),
                          base::SequencedTaskRunner::GetCurrentDefault());
    histogram_tester_->ExpectTotalCount(
        /*name=*/"SafeBrowsing.HPRT.Request.CountOfPrefixes",
        /*expected_count=*/1);
  }
  ResetMetrics();
  service_->Shutdown();
  {
    // A new lookup should also not have the response_callback called (due to
    // shutdown). It should not even trigger a request.
    GURL url = GURL("https://example.test");
    base::MockCallback<HPRTLookupResponseCallback> response_callback;
    EXPECT_CALL(response_callback, Run(testing::_, testing::_, testing::_))
        .Times(0);
    service_->StartLookup(url, response_callback.Get(),
                          base::SequencedTaskRunner::GetCurrentDefault());
    histogram_tester_->ExpectTotalCount(
        /*name=*/"SafeBrowsing.HPRT.Request.CountOfPrefixes",
        /*expected_count=*/0);
  }
  task_environment_.RunUntilIdle();
}

TEST_F(HashRealTimeServiceTest, TestLookup_MultipleRequestsAtOnce) {
  GURL url1 = GURL("https://example.test1");
  GURL url2 = GURL("https://example.test2");
  base::MockCallback<HPRTLookupResponseCallback> response_callback1;
  StartSuccessRequest(
      /*url=*/url1, /*cached_hash_prefixes=*/{},
      /*response_callback=*/response_callback1, /*response_full_hashes=*/
      {CreateFullHashProto({V5::ThreatType::SOCIAL_ENGINEERING},
                           UrlToSingleFullHash(url1))},
      /*expected_threat_type=*/SBThreatType::SB_THREAT_TYPE_URL_PHISHING,
      /*expected_locally_cached_results_threat_type=*/
      SBThreatType::SB_THREAT_TYPE_SAFE);
  base::MockCallback<HPRTLookupResponseCallback> response_callback2;
  StartSuccessRequest(
      /*url=*/url2, /*cached_hash_prefixes=*/{},
      /*response_callback=*/response_callback2, /*response_full_hashes=*/
      {CreateFullHashProto({V5::ThreatType::MALWARE},
                           UrlToSingleFullHash(url2))},
      /*expected_threat_type=*/SBThreatType::SB_THREAT_TYPE_URL_MALWARE,
      /*expected_locally_cached_results_threat_type=*/
      SBThreatType::SB_THREAT_TYPE_SAFE);

  histogram_tester_->ExpectTotalCount(
      /*name=*/"SafeBrowsing.HPRT.Network.Result",
      /*expected_count=*/0);
  task_environment_.RunUntilIdle();
  histogram_tester_->ExpectTotalCount(
      /*name=*/"SafeBrowsing.HPRT.Network.Result",
      /*expected_count=*/2);
}

TEST_F(HashRealTimeServiceTest, TestBackoffModeSet) {
  GURL url = GURL("https://example.test");

  // Failing lookups 1 and 2 don't trigger backoff mode. Lookup 3 does.
  RunSimpleFailingRequest(url);
  EXPECT_FALSE(service_->backoff_operator_->IsInBackoffMode());
  RunSimpleFailingRequest(url);
  EXPECT_FALSE(service_->backoff_operator_->IsInBackoffMode());
  RunSimpleFailingRequest(url);
  EXPECT_TRUE(service_->backoff_operator_->IsInBackoffMode());

  // Backoff mode should still be set until 5 minutes later.
  task_environment_.FastForwardBy(base::Seconds(299));
  EXPECT_TRUE(service_->backoff_operator_->IsInBackoffMode());
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(service_->backoff_operator_->IsInBackoffMode());

  // Backoff mode only occurs after 3 *consecutive* failures.
  RunSimpleFailingRequest(url);
  EXPECT_FALSE(service_->backoff_operator_->IsInBackoffMode());
  RunSimpleFailingRequest(url);
  EXPECT_FALSE(service_->backoff_operator_->IsInBackoffMode());
  RunSimpleRequest(url, /*response_full_hashes=*/{});
  EXPECT_FALSE(service_->backoff_operator_->IsInBackoffMode());
  RunSimpleFailingRequest(url);
  EXPECT_FALSE(service_->backoff_operator_->IsInBackoffMode());
  RunSimpleFailingRequest(url);
  EXPECT_FALSE(service_->backoff_operator_->IsInBackoffMode());
}

TEST_F(HashRealTimeServiceTest, TestBackoffModeSet_RetriableError) {
  GURL url = GURL("https://example.test");

  // Retriable errors are not yet implemented for the OHTTP path, so they
  // should still trigger backoff.
  RunSimpleFailingRequest(url, /*net_error=*/net::ERR_INTERNET_DISCONNECTED);
  EXPECT_FALSE(service_->backoff_operator_->IsInBackoffMode());
  RunSimpleFailingRequest(url, /*net_error=*/net::ERR_INTERNET_DISCONNECTED);
  EXPECT_FALSE(service_->backoff_operator_->IsInBackoffMode());
  RunSimpleFailingRequest(url, /*net_error=*/net::ERR_INTERNET_DISCONNECTED);
  EXPECT_TRUE(service_->backoff_operator_->IsInBackoffMode());
}

TEST_F(HashRealTimeServiceTest, TestBackoffModeSet_MissingOhttpKey) {
  GURL url = GURL("https://example.test");
  ohttp_key_service_->SetOhttpKey(absl::nullopt);
  base::MockCallback<HPRTLookupResponseCallback> response_callback;
  EXPECT_CALL(response_callback,
              Run(/*is_lookup_successful=*/false,
                  /*sb_threat_type=*/testing::Eq(absl::nullopt),
                  /*locally_cached_results_threat_type=*/testing::_))
      .Times(3);
  service_->StartLookup(url, response_callback.Get(),
                        base::SequencedTaskRunner::GetCurrentDefault());
  service_->StartLookup(url, response_callback.Get(),
                        base::SequencedTaskRunner::GetCurrentDefault());
  service_->StartLookup(url, response_callback.Get(),
                        base::SequencedTaskRunner::GetCurrentDefault());
  task_environment_.RunUntilIdle();

  // Key related failure should also affect the backoff status.
  EXPECT_EQ(service_->backoff_operator_->IsInBackoffMode(), true);
}

TEST_F(HashRealTimeServiceTest, TestBackoffModeRespected_FullyCached) {
  // Kick off a request that will cache the response.
  GURL url1 = GURL("https://example1.test");
  RunSimpleRequest(
      /*url=*/url1, /*response_full_hashes=*/
      {CreateFullHashProto({V5::ThreatType::SOCIAL_ENGINEERING},
                           UrlToSingleFullHash(url1))});

  // Enable backoff mode.
  GURL url2 = GURL("https://example2.test");
  RunSimpleFailingRequest(url2);
  RunSimpleFailingRequest(url2);
  RunSimpleFailingRequest(url2);
  EXPECT_TRUE(service_->backoff_operator_->IsInBackoffMode());

  // In spite of being in backoff mode, the cached response should apply before
  // the lookup decides to quit due to backoff.
  ResetMetrics();
  RunFullyCachedRequestTest(
      /*url=*/url1,
      /*expected_threat_type=*/SBThreatType::SB_THREAT_TYPE_URL_PHISHING,
      /*expected_threat_info_size=*/1);
}

TEST_F(HashRealTimeServiceTest, TestBackoffModeRespected_NotCached) {
  // Enable backoff mode.
  GURL url = GURL("https://example.test");
  RunSimpleFailingRequest(url);
  RunSimpleFailingRequest(url);
  RunSimpleFailingRequest(url);
  histogram_tester_->ExpectUniqueSample(
      /*name=*/"SafeBrowsing.HPRT.BackoffState",
      /*sample=*/false,
      /*expected_bucket_count=*/3);
  EXPECT_TRUE(service_->backoff_operator_->IsInBackoffMode());

  // Since the response is not already cached, the lookup will fail since the
  // service is in backoff mode. This is checked within |RunBackoffRequestTest|.
  ResetMetrics();
  RunBackoffRequestTest(url);
}

TEST_F(HashRealTimeServiceTest, TestLogSearchCacheWithNoQueryParamsMetric) {
  auto check_metrics = [this](bool expect_cache_hit_if_no_query_params_log,
                              bool expect_cache_hit_log) {
    histogram_tester_->ExpectUniqueSample(
        /*name=*/"SafeBrowsing.HPRT.CacheHitAllPrefixesIfNoQueryParams",
        /*sample=*/expect_cache_hit_if_no_query_params_log,
        /*expected_bucket_count=*/1);
    histogram_tester_->ExpectTotalCount("SafeBrowsing.HPRT.CacheHit",
                                        expect_cache_hit_log ? 1 : 0);
    histogram_tester_->ExpectTotalCount("SafeBrowsing.HPRT.GetCache.Time",
                                        expect_cache_hit_log ? 1 : 0);
    ResetMetrics();
  };
  GURL url = GURL("https://example.test");
  RunSimpleRequest(
      /*url=*/url, /*response_full_hashes=*/{});
  check_metrics(/*expect_cache_hit_if_no_query_params_log=*/false,
                /*expect_cache_hit_log=*/true);

  GURL url2 = GURL("https://example.test?run=true");
  service_->LogSearchCacheWithNoQueryParamsMetric(url2);
  check_metrics(/*expect_cache_hit_if_no_query_params_log=*/true,
                /*expect_cache_hit_log=*/false);

  GURL url3 = GURL("https://foo.example.test?run=true");
  service_->LogSearchCacheWithNoQueryParamsMetric(url3);
  check_metrics(/*expect_cache_hit_if_no_query_params_log=*/false,
                /*expect_cache_hit_log=*/false);
}

TEST_F(HashRealTimeServiceTest, TestIsThreatTypeMoreSevere) {
  struct TestCase {
    V5::ThreatType candidate_threat_type;
    V5::ThreatType baseline_threat_type;
    bool expected_result;
  } test_cases[] = {
      {V5::ThreatType::MALWARE, V5::ThreatType::MALWARE, false},
      {V5::ThreatType::MALWARE, V5::ThreatType::SOCIAL_ENGINEERING, false},
      {V5::ThreatType::MALWARE, V5::ThreatType::UNWANTED_SOFTWARE, true},
      {V5::ThreatType::MALWARE, V5::ThreatType::SUSPICIOUS, true},
      {V5::ThreatType::MALWARE, V5::ThreatType::TRICK_TO_BILL, true},
      {V5::ThreatType::SOCIAL_ENGINEERING, V5::ThreatType::MALWARE, false},
      {V5::ThreatType::SOCIAL_ENGINEERING, V5::ThreatType::SOCIAL_ENGINEERING,
       false},
      {V5::ThreatType::SOCIAL_ENGINEERING, V5::ThreatType::UNWANTED_SOFTWARE,
       true},
      {V5::ThreatType::SOCIAL_ENGINEERING, V5::ThreatType::SUSPICIOUS, true},
      {V5::ThreatType::SOCIAL_ENGINEERING, V5::ThreatType::TRICK_TO_BILL, true},
      {V5::ThreatType::UNWANTED_SOFTWARE, V5::ThreatType::MALWARE, false},
      {V5::ThreatType::UNWANTED_SOFTWARE, V5::ThreatType::SOCIAL_ENGINEERING,
       false},
      {V5::ThreatType::UNWANTED_SOFTWARE, V5::ThreatType::UNWANTED_SOFTWARE,
       false},
      {V5::ThreatType::UNWANTED_SOFTWARE, V5::ThreatType::SUSPICIOUS, true},
      {V5::ThreatType::UNWANTED_SOFTWARE, V5::ThreatType::TRICK_TO_BILL, true},
      {V5::ThreatType::SUSPICIOUS, V5::ThreatType::MALWARE, false},
      {V5::ThreatType::SUSPICIOUS, V5::ThreatType::SOCIAL_ENGINEERING, false},
      {V5::ThreatType::SUSPICIOUS, V5::ThreatType::UNWANTED_SOFTWARE, false},
      {V5::ThreatType::SUSPICIOUS, V5::ThreatType::SUSPICIOUS, false},
      {V5::ThreatType::SUSPICIOUS, V5::ThreatType::TRICK_TO_BILL, true},
      {V5::ThreatType::TRICK_TO_BILL, V5::ThreatType::MALWARE, false},
      {V5::ThreatType::TRICK_TO_BILL, V5::ThreatType::SOCIAL_ENGINEERING,
       false},
      {V5::ThreatType::TRICK_TO_BILL, V5::ThreatType::UNWANTED_SOFTWARE, false},
      {V5::ThreatType::TRICK_TO_BILL, V5::ThreatType::SUSPICIOUS, false},
      {V5::ThreatType::TRICK_TO_BILL, V5::ThreatType::TRICK_TO_BILL, false}};

  for (const auto& test_case : test_cases) {
    EXPECT_EQ(IsThreatTypeMoreSevere(test_case.candidate_threat_type,
                                     test_case.baseline_threat_type),
              test_case.expected_result);
  }

  std::vector<V5::ThreatType> min_severity_test_cases = {
      V5::ThreatType::MALWARE, V5::ThreatType::SOCIAL_ENGINEERING,
      V5::ThreatType::UNWANTED_SOFTWARE, V5::ThreatType::SUSPICIOUS,
      V5::ThreatType::TRICK_TO_BILL};
  for (const auto& test_case : min_severity_test_cases) {
    EXPECT_TRUE(IsThreatTypeMoreSevereThanLeastSeverity(test_case));
  }
}

class HashRealTimeServiceDirectFetchTest : public HashRealTimeServiceTest {
 public:
  HashRealTimeServiceDirectFetchTest() {
    feature_list_.InitAndDisableFeature(kHashRealTimeOverOhttp);
  }

 protected:
  std::string GetResourceUrl(const GURL& url) {
    auto request = std::make_unique<V5::SearchHashesRequest>();
    for (const auto& hash_prefix : UrlToHashPrefixesAsSet(url)) {
      request->add_hash_prefixes(hash_prefix);
    }
    return GetExpectedRequestUrl(request);
  }

  std::string GetResponseStr(const GURL& url, V5::ThreatType threat_type) {
    std::vector<V5::FullHash> response_full_hashes;
    response_full_hashes.push_back(
        CreateFullHashProto({threat_type}, UrlToSingleFullHash(url)));
    auto response = std::make_unique<V5::SearchHashesResponse>();
    response->mutable_full_hashes()->Assign(response_full_hashes.begin(),
                                            response_full_hashes.end());
    auto* cache_duration = response->mutable_cache_duration();
    cache_duration->set_seconds(300);
    std::string response_str;
    response->SerializeToString(&response_str);
    return response_str;
  }

  // Starts a lookup on |url| that is expected to fail due to a |net_error|.
  // Confirms that the lookup fails.
  void RunRequestNetErrorFailureTest(
      const GURL& url,
      net::Error net_error,
      int expected_prefix_count,
      HashRealTimeService::OperationResult expected_operation_result) {
    auto num_requests = test_url_loader_factory_->total_requests();

    // Set up request and response.
    auto request = std::make_unique<V5::SearchHashesRequest>();
    for (const auto& hash_prefix : UrlToHashPrefixesAsSet(url)) {
      request->add_hash_prefixes(hash_prefix);
    }
    std::string expected_url = GetExpectedRequestUrl(request);

    auto head = network::CreateURLResponseHead(net::HTTP_OK);
    network::URLLoaderCompletionStatus status(net_error);
    test_url_loader_factory_->AddResponse(GURL(expected_url), std::move(head),
                                          "", status);

    // Start lookup.
    base::MockCallback<HPRTLookupResponseCallback> response_callback;
    EXPECT_CALL(response_callback,
                Run(/*is_lookup_successful=*/false,
                    /*sb_threat_type=*/testing::Eq(absl::nullopt),
                    /*locally_cached_results_threat_type=*/testing::_))
        .Times(1);
    service_->StartLookup(url, response_callback.Get(),
                          base::SequencedTaskRunner::GetCurrentDefault());
    task_environment_.RunUntilIdle();

    CheckPreRequestMetrics(/*expect_cache_hit_all_prefixes=*/false,
                           /*expected_backoff_mode_status=*/false);
    CheckRequestMetrics(
        /*expected_prefix_count=*/expected_prefix_count,
        /*expected_network_result=*/net_error,
        /*expected_operation_result=*/
        expected_operation_result,
        /*expected_found_unmatched_full_hashes=*/absl::nullopt);
    ResetMetrics();

    EXPECT_EQ(test_url_loader_factory_->total_requests(), num_requests + 1u);
  }

  base::test::ScopedFeatureList feature_list_;
};

TEST_F(HashRealTimeServiceDirectFetchTest, TestLookup_Success) {
  GURL url = GURL("https://example.test");
  std::string expected_url = GetResourceUrl(url);
  test_url_loader_factory_->AddResponse(
      expected_url, GetResponseStr(url, V5::ThreatType::SOCIAL_ENGINEERING));
  test_url_loader_factory_->SetInterceptor(base::BindLambdaForTesting(
      [&](const network::ResourceRequest& resource_request) {
        ASSERT_EQ(expected_url, resource_request.url.spec());
      }));

  base::MockCallback<HPRTLookupResponseCallback> response_callback;
  EXPECT_CALL(response_callback,
              Run(/*is_lookup_successful=*/true,
                  /*sb_threat_type=*/
                  testing::Optional(SBThreatType::SB_THREAT_TYPE_URL_PHISHING),
                  /*locally_cached_results_threat_type=*/testing::_))
      .Times(1);

  service_->StartLookup(url, response_callback.Get(),
                        base::SequencedTaskRunner::GetCurrentDefault());
  task_environment_.RunUntilIdle();
}

TEST_F(HashRealTimeServiceDirectFetchTest,
       TestLookupFailure_RetriableNetError) {
  GURL url = GURL("https://example.test");
  auto run_net_error_failure_test =
      [this, url](net::Error net_error, bool expected_is_retriable_error) {
        RunRequestNetErrorFailureTest(
            /*url=*/url, /*net_error=*/net_error, /*expected_prefix_count=*/1,
            /*expected_operation_result=*/
            expected_is_retriable_error
                ? HashRealTimeService::OperationResult::kRetriableError
                : HashRealTimeService::OperationResult::kNetworkError);
      };

  // Retriable errors should not trigger backoff mode.
  run_net_error_failure_test(net::ERR_INTERNET_DISCONNECTED,
                             /*expected_is_retriable_error=*/true);
  run_net_error_failure_test(net::ERR_NETWORK_CHANGED,
                             /*expected_is_retriable_error=*/true);
  run_net_error_failure_test(net::ERR_INTERNET_DISCONNECTED,
                             /*expected_is_retriable_error=*/true);
  run_net_error_failure_test(net::ERR_NETWORK_CHANGED,
                             /*expected_is_retriable_error=*/true);
  run_net_error_failure_test(net::ERR_INTERNET_DISCONNECTED,
                             /*expected_is_retriable_error=*/true);
  run_net_error_failure_test(net::ERR_NETWORK_CHANGED,
                             /*expected_is_retriable_error=*/true);
  EXPECT_FALSE(service_->backoff_operator_->IsInBackoffMode());

  // Retriable errors should not reset the backoff counter back to 0.
  run_net_error_failure_test(net::ERR_FAILED,
                             /*expected_is_retriable_error=*/false);
  run_net_error_failure_test(net::ERR_FAILED,
                             /*expected_is_retriable_error=*/false);
  run_net_error_failure_test(net::ERR_INTERNET_DISCONNECTED,
                             /*expected_is_retriable_error=*/true);
  run_net_error_failure_test(net::ERR_FAILED,
                             /*expected_is_retriable_error=*/false);
  EXPECT_TRUE(service_->backoff_operator_->IsInBackoffMode());
}

}  // namespace safe_browsing
