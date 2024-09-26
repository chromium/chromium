// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/realtime/url_lookup_service.h"

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/core/browser/referrer_chain_provider.h"
#include "components/safe_browsing/core/browser/safe_browsing_token_fetcher.h"
#include "components/safe_browsing/core/browser/test_safe_browsing_token_fetcher.h"
#include "components/safe_browsing/core/browser/verdict_cache_manager.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/unified_consent/pref_names.h"
#include "components/unified_consent/unified_consent_service.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/platform_test.h"

using ::testing::_;
using testing::DoAll;
using testing::Return;
using testing::SetArgPointee;

namespace safe_browsing {

namespace {
constexpr char kRealTimeLookupUrlPrefix[] =
    "https://safebrowsing.google.com/safebrowsing/clientreport/realtime";

constexpr char kTestUrl[] = "http://example.test/";
constexpr char kTestReferrerUrl[] = "http://example.referrer/";
constexpr char kTestSubframeUrl[] = "http://iframe.example.test/";
constexpr char kTestSubframeReferrerUrl[] = "http://iframe.example.referrer/";

class MockReferrerChainProvider : public ReferrerChainProvider {
 public:
  virtual ~MockReferrerChainProvider() = default;
  MOCK_METHOD3(IdentifyReferrerChainByRenderFrameHost,
               AttributionResult(content::RenderFrameHost* rfh,
                                 int user_gesture_count_limit,
                                 ReferrerChain* out_referrer_chain));
  MOCK_METHOD5(IdentifyReferrerChainByEventURL,
               AttributionResult(const GURL& event_url,
                                 SessionID event_tab_id,
                                 const content::GlobalRenderFrameHostId&
                                     event_outermost_main_frame_id,
                                 int user_gesture_count_limit,
                                 ReferrerChain* out_referrer_chain));
  MOCK_METHOD4(IdentifyReferrerChainByEventURL,
               AttributionResult(const GURL& event_url,
                                 SessionID event_tab_id,
                                 int user_gesture_count_limit,
                                 ReferrerChain* out_referrer_chain));
  MOCK_METHOD3(IdentifyReferrerChainByPendingEventURL,
               AttributionResult(const GURL& event_url,
                                 int user_gesture_count_limit,
                                 ReferrerChain* out_referrer_chain));
};

bool GetRequestProto(const network::ResourceRequest& request,
                     RTLookupRequest* request_proto) {
  if (!request.request_body || !request.request_body->elements()) {
    return false;
  }

  // Supporting one DataElementBytes is sufficient here. If request
  // protos grow to need data pipes, we would need further test code
  // to read the contents of the pipe.
  const std::vector<network::DataElement>* elements =
      request.request_body->elements();
  if (elements->size() != 1 ||
      elements->at(0).type() !=
          network::mojom::DataElementDataView::Tag::kBytes) {
    return false;
  }
  return request_proto->ParseFromString(std::string(
      elements->at(0).As<network::DataElementBytes>().AsStringPiece()));
}

class MustRunInterceptor {
 public:
  MustRunInterceptor(network::TestURLLoaderFactory::Interceptor interceptor)
      : interceptor_(interceptor), has_run_(false) {}
  ~MustRunInterceptor() { EXPECT_TRUE(has_run_); }

  void Run(const network::ResourceRequest& resource_request) {
    has_run_ = true;
    interceptor_.Run(resource_request);
  }

  network::TestURLLoaderFactory::Interceptor GetCallback() {
    return base::BindRepeating(&MustRunInterceptor::Run,
                               base::Unretained(this));
  }

 private:
  network::TestURLLoaderFactory::Interceptor interceptor_;
  bool has_run_;
};

}  // namespace

class RealTimeUrlLookupServiceTest : public PlatformTest {
 public:
  void SetUp() override {
    HostContentSettingsMap::RegisterProfilePrefs(test_pref_service_.registry());
    safe_browsing::RegisterProfilePrefs(test_pref_service_.registry());
    PlatformTest::SetUp();

    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);

    content_setting_map_ = new HostContentSettingsMap(
        &test_pref_service_, false /* is_off_the_record */,
        false /* store_last_modified */, false /* restore_session */,
        false /* should_record_metrics */);
    cache_manager_ = std::make_unique<VerdictCacheManager>(
        /*history_service=*/nullptr, content_setting_map_.get(),
        &test_pref_service_,
        /*sync_observer=*/nullptr);
    referrer_chain_provider_ = std::make_unique<MockReferrerChainProvider>();

    auto token_fetcher = std::make_unique<TestSafeBrowsingTokenFetcher>();
    raw_token_fetcher_ = token_fetcher.get();
    rt_service_ = std::make_unique<RealTimeUrlLookupService>(
        test_shared_loader_factory_, cache_manager_.get(),
        base::BindRepeating(
            [](PrefService* pref_service) {
              ChromeUserPopulation population;
              population.set_user_population(
                  IsEnhancedProtectionEnabled(*pref_service)
                      ? ChromeUserPopulation::ENHANCED_PROTECTION
                  : IsExtendedReportingEnabled(*pref_service)
                      ? ChromeUserPopulation::EXTENDED_REPORTING
                      : ChromeUserPopulation::SAFE_BROWSING);
              population.set_profile_management_status(
                  ChromeUserPopulation::NOT_MANAGED);
              population.set_is_history_sync_enabled(true);
              population.set_is_under_advanced_protection(true);
              population.set_is_incognito(false);
              return population;
            },
            &test_pref_service_),
        &test_pref_service_, std::move(token_fetcher),
        base::BindRepeating(
            &RealTimeUrlLookupServiceTest::AreTokenFetchesConfiguredInClient,
            base::Unretained(this)),
        /*is_off_the_record=*/false, /*variations_service=*/nullptr,
        referrer_chain_provider_.get(),
        /*webui_delegate=*/nullptr);
  }

  void TearDown() override {
    if (content_setting_map_) {
      content_setting_map_->ShutdownOnUIThread();
    }
    raw_token_fetcher_ = nullptr;
    rt_service_->Shutdown();
    cache_manager_.reset();
  }

  bool CanCheckUrl(const GURL& url) { return rt_service_->CanCheckUrl(url); }
  bool IsInBackoffMode() { return rt_service_->IsInBackoffMode(); }
  bool CanSendRTSampleRequest() {
    return rt_service_->CanSendRTSampleRequest();
  }
  std::unique_ptr<RTLookupRequest> FillRequestProto(
      const GURL& url,
      bool is_sampled_report) {
    return rt_service_->FillRequestProto(url, is_sampled_report,
                                         SessionID::InvalidValue());
  }
  std::unique_ptr<RTLookupResponse> GetCachedRealTimeUrlVerdict(
      const GURL& url) {
    return rt_service_->GetCachedRealTimeUrlVerdict(url);
  }

  void MayBeCacheRealTimeUrlVerdict(
      RTLookupResponse::ThreatInfo::VerdictType verdict_type,
      RTLookupResponse::ThreatInfo::ThreatType threat_type,
      int cache_duration_sec,
      const std::string& cache_expression,
      RTLookupResponse::ThreatInfo::CacheExpressionMatchType
          cache_expression_match_type) {
    RTLookupResponse response;
    RTLookupResponse::ThreatInfo* new_threat_info = response.add_threat_info();
    new_threat_info->set_verdict_type(verdict_type);
    new_threat_info->set_threat_type(threat_type);
    new_threat_info->set_cache_duration_sec(cache_duration_sec);
    new_threat_info->set_cache_expression_using_match_type(cache_expression);
    new_threat_info->set_cache_expression_match_type(
        cache_expression_match_type);
    rt_service_->MayBeCacheRealTimeUrlVerdict(response);
  }

  void SetUpRTLookupResponse(
      RTLookupResponse::ThreatInfo::VerdictType verdict_type,
      RTLookupResponse::ThreatInfo::ThreatType threat_type,
      int cache_duration_sec,
      const std::string& cache_expression,
      RTLookupResponse::ThreatInfo::CacheExpressionMatchType
          cache_expression_match_type) {
    RTLookupResponse response;
    RTLookupResponse::ThreatInfo* new_threat_info = response.add_threat_info();
    RTLookupResponse::ThreatInfo threat_info;
    threat_info.set_verdict_type(verdict_type);
    threat_info.set_threat_type(threat_type);
    threat_info.set_cache_duration_sec(cache_duration_sec);
    threat_info.set_cache_expression_using_match_type(cache_expression);
    threat_info.set_cache_expression_match_type(cache_expression_match_type);
    *new_threat_info = threat_info;
    std::string expected_response_str;
    response.SerializeToString(&expected_response_str);
    test_url_loader_factory_.AddResponse(kRealTimeLookupUrlPrefix,
                                         expected_response_str);
  }

  void SetUpFailureResponse(net::HttpStatusCode http_status,
                            net::Error net_error = net::OK) {
    test_url_loader_factory_.ClearResponses();
    auto head = network::CreateURLResponseHead(http_status);
    network::URLLoaderCompletionStatus status(net_error);
    test_url_loader_factory_.AddResponse(GURL(kRealTimeLookupUrlPrefix),
                                         std::move(head), "", status);
  }

  RealTimeUrlLookupService* rt_service() { return rt_service_.get(); }

  void DisableTokenFetchesInClient() {
    token_fetches_configured_in_client_ = false;
  }

  void EnableMbb() {
    unified_consent::UnifiedConsentService::RegisterPrefs(
        test_pref_service_.registry());
    test_pref_service_.SetUserPref(
        unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
        std::make_unique<base::Value>(true));
  }

  void EnableExtendedReporting() {
    EnableMbb();
    test_pref_service_.SetUserPref(prefs::kSafeBrowsingEnabled,
                                   std::make_unique<base::Value>(true));
    test_pref_service_.SetUserPref(prefs::kSafeBrowsingScoutReportingEnabled,
                                   std::make_unique<base::Value>(true));
  }

  void EnableRealTimeUrlLookup(
      const std::vector<base::test::FeatureRef>& enabled_features,
      const std::vector<base::test::FeatureRef>& disabled_features) {
    EnableMbb();
    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  void EnableRealTimeUrlLookupWithParameters(
      const std::vector<base::test::FeatureRefAndParams>&
          enabled_features_and_params,
      const std::vector<base::test::FeatureRef>& disabled_features) {
    EnableMbb();
    feature_list_.InitWithFeaturesAndParameters(enabled_features_and_params,
                                                disabled_features);
  }

  void EnableTokenFetchesInClient() {
    token_fetches_configured_in_client_ = true;
  }

  void FulfillAccessTokenRequest(std::string token) {
    raw_token_fetcher_->RunAccessTokenCallback(token);
  }

  TestSafeBrowsingTokenFetcher* raw_token_fetcher() {
    return raw_token_fetcher_;
  }

  bool AreTokenFetchesConfiguredInClient(
      bool user_has_enabled_extended_protection) {
    return token_fetches_configured_in_client_;
  }

  std::unique_ptr<ReferrerChainEntry> CreateReferrerChainEntry(
      const std::string& url,
      const std::string& main_frame_url,
      const std::string& referrer_url,
      const std::string& referrer_main_frame_url,
      const double navigation_time_msec) {
    std::unique_ptr<ReferrerChainEntry> referrer_chain_entry =
        std::make_unique<ReferrerChainEntry>();
    referrer_chain_entry->set_url(url);
    if (!main_frame_url.empty()) {
      referrer_chain_entry->set_main_frame_url(main_frame_url);
    }
    referrer_chain_entry->set_referrer_url(referrer_url);
    if (!referrer_main_frame_url.empty()) {
      referrer_chain_entry->set_referrer_main_frame_url(
          referrer_main_frame_url);
    }
    referrer_chain_entry->set_navigation_time_msec(navigation_time_msec);
    return referrer_chain_entry;
  }

  ChromeUserPopulation::PageLoadToken CreatePageLoadToken(
      std::string token_value) {
    ChromeUserPopulation::PageLoadToken token;
    token.set_token_source(
        ChromeUserPopulation::PageLoadToken::CLIENT_GENERATION);
    token.set_token_time_msec(base::Time::Now().InMillisecondsSinceUnixEpoch());
    token.set_token_value(token_value);
    return token;
  }

  void RunSimpleFailingRequest(const GURL& url) {
    SetUpFailureResponse(net::HTTP_INTERNAL_SERVER_ERROR);
    base::MockCallback<network::TestURLLoaderFactory::Interceptor>
        request_callback;
    test_url_loader_factory_.SetInterceptor(request_callback.Get());
    base::MockCallback<RTLookupResponseCallback> response_callback;
    EXPECT_CALL(request_callback, Run(_)).Times(1);
    EXPECT_CALL(response_callback, Run(/* is_rt_lookup_successful */ false,
                                       /* is_cached_response */ false, _));
    rt_service()->StartLookup(url, response_callback.Get(),
                              base::SequencedTaskRunner::GetCurrentDefault(),
                              SessionID::InvalidValue());
    task_environment_.RunUntilIdle();
  }

  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  std::unique_ptr<RealTimeUrlLookupService> rt_service_;
  std::unique_ptr<VerdictCacheManager> cache_manager_;
  scoped_refptr<HostContentSettingsMap> content_setting_map_;
  bool token_fetches_configured_in_client_ = false;
  raw_ptr<TestSafeBrowsingTokenFetcher, DanglingUntriaged> raw_token_fetcher_ =
      nullptr;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  sync_preferences::TestingPrefServiceSyncable test_pref_service_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<MockReferrerChainProvider> referrer_chain_provider_;
};

TEST_F(RealTimeUrlLookupServiceTest, TestFillRequestProto) {
  GURL url("http://example.com/");
  for (size_t i = 0; i < 2; i++) {
    auto result = FillRequestProto(url, /*is_sampled_report=*/i % 2 == 0);
    EXPECT_EQ(url, result->url());
    if (i % 2 == 0) {
      EXPECT_EQ(/* sampled report */ 2, result->report_type());
    } else {
      EXPECT_EQ(/* full report */ 1, result->report_type());
    }
    EXPECT_EQ(RTLookupRequest::NAVIGATION, result->lookup_type());
    EXPECT_EQ(ChromeUserPopulation::SAFE_BROWSING,
              result->population().user_population());
    EXPECT_EQ(1, result->frame_type());

    // The value of is_history_sync_enabled() should reflect that of the
    // callback passed in by the client, which in this case is true.
    EXPECT_TRUE(result->population().is_history_sync_enabled());
    EXPECT_EQ(ChromeUserPopulation::NOT_MANAGED,
              result->population().profile_management_status());
#if BUILDFLAG(FULL_SAFE_BROWSING)
    EXPECT_TRUE(result->population().is_under_advanced_protection());
#endif
  }
}

TEST_F(RealTimeUrlLookupServiceTest, TestSanitizeURL) {
  struct SanitizeUrlCase {
    const char* url;
    const char* expected_url;
  } sanitize_url_cases[] = {
      {"http://example.com/", "http://example.com/"},
      {"http://user:pass@example.com/", "http://example.com/"},
      {"http://%123:bar@example.com/", "http://example.com/"},
      {"http://example.com/abc#123", "http://example.com/abc#123"}};
  for (auto& sanitize_url_case : sanitize_url_cases) {
    GURL url(sanitize_url_case.url);
    auto result = RealTimeUrlLookupServiceBase::SanitizeURL(url);
    EXPECT_EQ(sanitize_url_case.expected_url, result);
  }
}

TEST_F(RealTimeUrlLookupServiceTest, TestFillPageLoadToken_FeatureEnabled) {
  GURL url(kTestUrl);
  GURL subframe_url(kTestSubframeUrl);

  cache_manager_->SetPageLoadTokenForTesting(
      url, CreatePageLoadToken("url_page_load_token"));
  auto request = FillRequestProto(url, /*is_sampled_report=*/false);
  ASSERT_EQ(1, request->population().page_load_tokens_size());
  // The token should be re-generated for the mainframe URL.
  EXPECT_NE("url_page_load_token",
            request->population().page_load_tokens(0).token_value());
  EXPECT_EQ(ChromeUserPopulation::PageLoadToken::CLIENT_GENERATION,
            request->population().page_load_tokens(0).token_source());
}

TEST_F(RealTimeUrlLookupServiceTest, TestGetSBThreatTypeForRTThreatType) {
  using enum SBThreatType;

  EXPECT_EQ(SB_THREAT_TYPE_URL_MALWARE,
            RealTimeUrlLookupServiceBase::GetSBThreatTypeForRTThreatType(
                RTLookupResponse::ThreatInfo::WEB_MALWARE,
                RTLookupResponse::ThreatInfo::DANGEROUS));
  EXPECT_EQ(SB_THREAT_TYPE_URL_PHISHING,
            RealTimeUrlLookupServiceBase::GetSBThreatTypeForRTThreatType(
                RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING,
                RTLookupResponse::ThreatInfo::DANGEROUS));
  EXPECT_EQ(SB_THREAT_TYPE_URL_UNWANTED,
            RealTimeUrlLookupServiceBase::GetSBThreatTypeForRTThreatType(
                RTLookupResponse::ThreatInfo::UNWANTED_SOFTWARE,
                RTLookupResponse::ThreatInfo::DANGEROUS));
  EXPECT_EQ(SB_THREAT_TYPE_BILLING,
            RealTimeUrlLookupServiceBase::GetSBThreatTypeForRTThreatType(
                RTLookupResponse::ThreatInfo::UNCLEAR_BILLING,
                RTLookupResponse::ThreatInfo::DANGEROUS));
  EXPECT_EQ(SB_THREAT_TYPE_MANAGED_POLICY_BLOCK,
            RealTimeUrlLookupServiceBase::GetSBThreatTypeForRTThreatType(
                RTLookupResponse::ThreatInfo::MANAGED_POLICY,
                RTLookupResponse::ThreatInfo::DANGEROUS));
  EXPECT_EQ(SB_THREAT_TYPE_MANAGED_POLICY_WARN,
            RealTimeUrlLookupServiceBase::GetSBThreatTypeForRTThreatType(
                RTLookupResponse::ThreatInfo::MANAGED_POLICY,
                RTLookupResponse::ThreatInfo::WARN));
  EXPECT_EQ(SB_THREAT_TYPE_SAFE,
            RealTimeUrlLookupServiceBase::GetSBThreatTypeForRTThreatType(
                RTLookupResponse::ThreatInfo::MANAGED_POLICY,
                RTLookupResponse::ThreatInfo::SAFE));
  EXPECT_EQ(SB_THREAT_TYPE_SAFE,
            RealTimeUrlLookupServiceBase::GetSBThreatTypeForRTThreatType(
                RTLookupResponse::ThreatInfo::UNWANTED_SOFTWARE,
                RTLookupResponse::ThreatInfo::WARN));
}

TEST_F(RealTimeUrlLookupServiceTest, TestCanCheckUrl) {
  struct CanCheckUrlCases {
    const char* url;
    bool can_check;
  } can_check_url_cases[] = {
      {"ftp://example.test/path", false}, {"http://localhost/path", false},
      {"http://127.0.0.1/path", false},   {"http://127.0.0.1:2222/path", false},
      {"http://192.168.1.1/path", false}, {"http://172.16.2.2/path", false},
      {"http://10.1.1.9/path", false},    {"http://10.1.1.0xG/path", true},
      {"http://example.test/path", true}, {"http://nodothost/path", false},
      {"http://x.x/shorthost", false}};
  for (auto& can_check_url_case : can_check_url_cases) {
    GURL url(can_check_url_case.url);
    bool expected_can_check = can_check_url_case.can_check;
    EXPECT_EQ(expected_can_check, CanCheckUrl(url));
  }
}

TEST_F(RealTimeUrlLookupServiceTest, TestCacheNotInCacheManager) {
  GURL url("https://a.example.test/path1/path2");
  ASSERT_EQ(nullptr, GetCachedRealTimeUrlVerdict(url));
}

TEST_F(RealTimeUrlLookupServiceTest, TestCacheInCacheManager) {
  GURL url("https://a.example.test/path1/path2");
  MayBeCacheRealTimeUrlVerdict(RTLookupResponse::ThreatInfo::DANGEROUS,
                               RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING,
                               60, "a.example.test/path1/path2",
                               RTLookupResponse::ThreatInfo::COVERING_MATCH);
  task_environment_.RunUntilIdle();

  std::unique_ptr<RTLookupResponse> cache_response =
      GetCachedRealTimeUrlVerdict(url);
  ASSERT_NE(nullptr, cache_response);
  EXPECT_EQ(RTLookupResponse::ThreatInfo::DANGEROUS,
            cache_response->threat_info(0).verdict_type());
  EXPECT_EQ(RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING,
            cache_response->threat_info(0).threat_type());
}

TEST_F(RealTimeUrlLookupServiceTest, TestStartLookup_PendingRequestForSameUrl) {
  base::HistogramTester histograms;
  EnableRealTimeUrlLookup({kSafeBrowsingRemoveCookiesInAuthRequests}, {});
  GURL url(kTestUrl);
  SetUpRTLookupResponse(RTLookupResponse::ThreatInfo::DANGEROUS,
                        RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING, 60,
                        "example.test/",
                        RTLookupResponse::ThreatInfo::COVERING_MATCH);

  // Only one network request should be made.
  base::MockCallback<network::TestURLLoaderFactory::Interceptor>
      request_callback;
  test_url_loader_factory_.SetInterceptor(request_callback.Get());
  EXPECT_CALL(request_callback, Run(_)).Times(1);

  base::MockCallback<RTLookupResponseCallback> response_callback_1;
  rt_service()->StartLookup(url, response_callback_1.Get(),
                            base::SequencedTaskRunner::GetCurrentDefault(),
                            SessionID::InvalidValue());

  base::MockCallback<RTLookupResponseCallback> response_callback_2;
  rt_service()->StartLookup(url, response_callback_2.Get(),
                            base::SequencedTaskRunner::GetCurrentDefault(),
                            SessionID::InvalidValue());

  // Expect both callbacks to be invoked.
  EXPECT_CALL(response_callback_1, Run(/* is_rt_lookup_successful */ true,
                                       /* is_cached_response */ false, _));
  EXPECT_CALL(response_callback_2, Run(/* is_rt_lookup_successful */ true,
                                       /* is_cached_response */ false, _));

  task_environment_.RunUntilIdle();

  // The first request is considered not concurrent, the second one is.
  histograms.ExpectBucketCount("SafeBrowsing.RT.Request.Concurrent",
                               /* sample */ false,
                               /* expected_count */ 1);
  histograms.ExpectBucketCount("SafeBrowsing.RT.Request.Concurrent",
                               /* sample */ true,
                               /* expected_count */ 1);
}

TEST_F(RealTimeUrlLookupServiceTest, TestStartLookup_ResponseIsAlreadyCached) {
  base::HistogramTester histograms;
  GURL url(kTestUrl);
  MayBeCacheRealTimeUrlVerdict(RTLookupResponse::ThreatInfo::DANGEROUS,
                               RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING,
                               60, "example.test/",
                               RTLookupResponse::ThreatInfo::COVERING_MATCH);
  task_environment_.RunUntilIdle();

  base::MockCallback<network::TestURLLoaderFactory::Interceptor>
      request_callback;
  test_url_loader_factory_.SetInterceptor(request_callback.Get());
  EXPECT_CALL(request_callback, Run(_)).Times(0);

  base::MockCallback<RTLookupResponseCallback> response_callback;
  rt_service()->StartLookup(url, response_callback.Get(),
                            base::SequencedTaskRunner::GetCurrentDefault(),
                            SessionID::InvalidValue());

  EXPECT_CALL(response_callback, Run(/* is_rt_lookup_successful */ true,
                                     /* is_cached_response */ true, _));

  task_environment_.RunUntilIdle();

  // This metric is not recorded because the response is obtained from the
  // cache.
  histograms.ExpectUniqueSample("SafeBrowsing.RT.ThreatInfoSize",
                                /* sample */ 0,
                                /* expected_count */ 0);

  // No requests were sent.
  histograms.ExpectTotalCount("SafeBrowsing.RT.Request.Concurrent", 0);
}

TEST_F(RealTimeUrlLookupServiceTest,
       TestStartLookup_PingWithTokenUpdatesEsbProtegoPingWithTokenLastLogTime) {
  base::HistogramTester histograms;
  EnableRealTimeUrlLookup({kSafeBrowsingRemoveCookiesInAuthRequests}, {});
  EnableTokenFetchesInClient();
  SetSafeBrowsingState(&test_pref_service_,
                       SafeBrowsingState::ENHANCED_PROTECTION);
  GURL url(kTestUrl);
  SetUpRTLookupResponse(RTLookupResponse::ThreatInfo::DANGEROUS,
                        RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING, 60,
                        "example.test/",
                        RTLookupResponse::ThreatInfo::COVERING_MATCH);

  rt_service()->StartLookup(url, base::DoNothing(),
                            base::SequencedTaskRunner::GetCurrentDefault(),
                            SessionID::InvalidValue());

  FulfillAccessTokenRequest("access_token_string");
  task_environment_.RunUntilIdle();

  EXPECT_EQ(test_pref_service_.GetTime(
                prefs::kSafeBrowsingEsbProtegoPingWithTokenLastLogTime),
            base::Time::Now());

  // Only one not-concurent request is sent.
  histograms.ExpectUniqueSample("SafeBrowsing.RT.Request.Concurrent",
                                /* sample */ false,
                                /* expected_count */ 1);
}

TEST_F(
    RealTimeUrlLookupServiceTest,
    TestStartLookup_PingWithoutTokenSetsEsbProtegoPingWithoutTokenLastLogTime) {
  EnableRealTimeUrlLookup({kSafeBrowsingRemoveCookiesInAuthRequests}, {});
  DisableTokenFetchesInClient();
  SetSafeBrowsingState(&test_pref_service_,
                       SafeBrowsingState::ENHANCED_PROTECTION);
  GURL url(kTestUrl);
  SetUpRTLookupResponse(RTLookupResponse::ThreatInfo::DANGEROUS,
                        RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING, 60,
                        "example.test/",
                        RTLookupResponse::ThreatInfo::COVERING_MATCH);

  rt_service()->StartLookup(url, base::DoNothing(),
                            base::SequencedTaskRunner::GetCurrentDefault(),
                            SessionID::InvalidValue());

  task_environment_.RunUntilIdle();

  EXPECT_EQ(test_pref_service_.GetTime(
                prefs::kSafeBrowsingEsbProtegoPingWithoutTokenLastLogTime),
            base::Time::Now());
}

TEST_F(
    RealTimeUrlLookupServiceTest,
    TestStartLookup_DoesNotSetEsbProtegoPingWithTokenLastLogTimeWhenCacheIsHit) {
  EnableRealTimeUrlLookup({kSafeBrowsingRemoveCookiesInAuthRequests}, {});
  EnableTokenFetchesInClient();
  SetSafeBrowsingState(&test_pref_service_,
                       SafeBrowsingState::ENHANCED_PROTECTION);
  GURL url(kTestUrl);
  MayBeCacheRealTimeUrlVerdict(RTLookupResponse::ThreatInfo::DANGEROUS,
                               RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING,
                               60, "example.test/",
                               RTLookupResponse::ThreatInfo::COVERING_MATCH);
  task_environment_.RunUntilIdle();

  rt_service()->StartLookup(url, base::DoNothing(),
                            base::SequencedTaskRunner::GetCurrentDefault(),
                            SessionID::InvalidValue());

  task_environment_.RunUntilIdle();

  EXPECT_EQ(test_pref_service_.GetTime(
                prefs::kSafeBrowsingEsbProtegoPingWithTokenLastLogTime),
            base::Time());
}

TEST_F(
    RealTimeUrlLookupServiceTest,
    TestStartLookup_DoesNotSetEsbProtegoPingWithoutTokenLastLogTimeWhenCacheIsHit) {
  EnableRealTimeUrlLookup({kSafeBrowsingRemoveCookiesInAuthRequests}, {});
  DisableTokenFetchesInClient();
  SetSafeBrowsingState(&test_pref_service_,
                       SafeBrowsingState::ENHANCED_PROTECTION);

  GURL url(kTestUrl);
  MayBeCacheRealTimeUrlVerdict(RTLookupResponse::ThreatInfo::DANGEROUS,
                               RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING,
                               60, "example.test/",
                               RTLookupResponse::ThreatInfo::COVERING_MATCH);
  task_environment_.RunUntilIdle();

  rt_service()->StartLookup(url, base::DoNothing(),
                            base::SequencedTaskRunner::GetCurrentDefault(),
                            SessionID::InvalidValue());

  task_environment_.RunUntilIdle();

  EXPECT_EQ(test_pref_service_.GetTime(
                prefs::kSafeBrowsingEsbProtegoPingWithoutTokenLastLogTime),
            base::Time());
}

TEST_F(
    RealTimeUrlLookupServiceTest,
    TestStartLookup_DoesNotSetEsbProtegoPingWithTokenLastLogTimeWhenEsbIsDisabled) {
  EnableRealTimeUrlLookup({kSafeBrowsingRemoveCookiesInAuthRequests}, {});
  SetSafeBrowsingState(&test_pref_service_,
                       SafeBrowsingState::STANDARD_PROTECTION);
  EnableTokenFetchesInClient();
  GURL url(kTestUrl);
  SetUpRTLookupResponse(RTLookupResponse::ThreatInfo::DANGEROUS,
                        RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING, 60,
                        "example.test/",
                        RTLookupResponse::ThreatInfo::COVERING_MATCH);

  rt_service()->StartLookup(url, base::DoNothing(),
                            base::SequencedTaskRunner::GetCurrentDefault(),
                            SessionID::InvalidValue());

  FulfillAccessTokenRequest("access_token_string");
  task_environment_.RunUntilIdle();

  EXPECT_EQ(test_pref_service_.GetTime(
                prefs::kSafeBrowsingEsbProtegoPingWithTokenLastLogTime),
            base::Time());
}

TEST_F(
    RealTimeUrlLookupServiceTest,
    TestStartLookup_DoesNotSetEsbProtegoPingWithoutTokenLastLogTimeWhenEsbIsDisabled) {
  EnableRealTimeUrlLookup({kSafeBrowsingRemoveCookiesInAuthRequests}, {});
  SetSafeBrowsingState(&test_pref_service_,
                       SafeBrowsingState::STANDARD_PROTECTION);
  DisableTokenFetchesInClient();
  GURL url(kTestUrl);
  SetUpRTLookupResponse(RTLookupResponse::ThreatInfo::DANGEROUS,
                        RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING, 60,
                        "example.test/",
                        RTLookupResponse::ThreatInfo::COVERING_MATCH);

  rt_service()->StartLookup(url, base::DoNothing(),
                            base::SequencedTaskRunner::GetCurrentDefault(),
                            SessionID::InvalidValue());

  FulfillAccessTokenRequest("access_token_string");
  task_environment_.RunUntilIdle();

  EXPECT_EQ(test_pref_service_.GetTime(
                prefs::kSafeBrowsingEsbProtegoPingWithoutTokenLastLogTime),
            base::Time());
}

TEST_F(RealTimeUrlLookupServiceTest,
       TestStartLookup_AttachTokenWhenWithTokenIsEnabled) {
  base::HistogramTester histograms;
  EnableRealTimeUrlLookup({kSafeBrowsingRemoveCookiesInAuthRequests}, {});
  EnableTokenFetchesInClient();
  GURL url(kTestUrl);
  SetUpRTLookupResponse(RTLookupResponse::ThreatInfo::DANGEROUS,
                        RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING, 60,
                        "example.test/",
                        RTLookupResponse::ThreatInfo::COVERING_MATCH);

  base::MockCallback<RTLookupResponseCallback> response_callback;
  EXPECT_CALL(response_callback, Run(/* is_rt_lookup_successful */ true,
                                     /* is_cached_response */ false, _));

  MustRunInterceptor interceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        RTLookupRequest request_proto;
        ASSERT_TRUE(GetRequestProto(request, &request_proto));
        EXPECT_FALSE(request_proto.has_dm_token());
        EXPECT_FALSE(request_proto.has_email());
        EXPECT_FALSE(request_proto.has_browser_dm_token());
        EXPECT_FALSE(request_proto.has_profile_dm_token());
        EXPECT_FALSE(request_proto.has_client_reporting_metadata());

        // Cookies should be removed when token is set.
        EXPECT_EQ(request.credentials_mode,
                  network::mojom::CredentialsMode::kOmit);
        EXPECT_THAT(
            request.headers.GetHeader(net::HttpRequestHeaders::kAuthorization),
            testing::Optional(std::string("Bearer access_token_string")));
      }));
  test_url_loader_factory_.SetInterceptor(interceptor.GetCallback());

  rt_service()->StartLookup(url, response_callback.Get(),
                            base::SequencedTaskRunner::GetCurrentDefault(),
                            SessionID::InvalidValue());

  EXPECT_TRUE(raw_token_fetcher()->WasStartCalled());
  FulfillAccessTokenRequest("access_token_string");
  EXPECT_CALL(*raw_token_fetcher(), OnInvalidAccessToken(_)).Times(0);
  task_environment_.RunUntilIdle();

  // Check the response is cached.
  std::unique_ptr<RTLookupResponse> cache_response =
      GetCachedRealTimeUrlVerdict(url);
  EXPECT_NE(nullptr, cache_response);

  histograms.ExpectUniqueSample("SafeBrowsing.RT.ThreatInfoSize",
                                /* sample */ 1,
                                /* expected_count */ 1);
}

TEST_F(RealTimeUrlLookupServiceTest,
       TestStartLookup_NoTokenWhenTokenIsUnavailable) {
  base::HistogramTester histograms;
  EnableRealTimeUrlLookup({kSafeBrowsingRemoveCookiesInAuthRequests}, {});
  EnableTokenFetchesInClient();
  GURL url(kTestUrl);
  SetUpRTLookupResponse(RTLookupResponse::ThreatInfo::DANGEROUS,
                        RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING, 60,
                        "example.test/",
                        RTLookupResponse::ThreatInfo::COVERING_MATCH);

  base::MockCallback<RTLookupResponseCallback> response_callback;
  EXPECT_CALL(response_callback, Run(/* is_rt_lookup_successful */ true,
                                     /* is_cached_response */ false, _));

  MustRunInterceptor interceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        RTLookupRequest request_proto;
        ASSERT_TRUE(GetRequestProto(request, &request_proto));
        EXPECT_FALSE(request_proto.has_dm_token());
        EXPECT_FALSE(request_proto.has_email());
        EXPECT_FALSE(request_proto.has_browser_dm_token());
        EXPECT_FALSE(request_proto.has_profile_dm_token());
        EXPECT_FALSE(request_proto.has_client_reporting_metadata());

        EXPECT_FALSE(
            request.headers.HasHeader(net::HttpRequestHeaders::kAuthorization));
      }));
  test_url_loader_factory_.SetInterceptor(interceptor.GetCallback());

  rt_service()->StartLookup(url, response_callback.Get(),
                            base::SequencedTaskRunner::GetCurrentDefault(),
                            SessionID::InvalidValue());

  EXPECT_TRUE(raw_token_fetcher()->WasStartCalled());
  // Token fetcher returns empty string when the token is unavailable.
  FulfillAccessTokenRequest("");
  task_environment_.RunUntilIdle();

  histograms.ExpectUniqueSample("SafeBrowsing.RT.HasTokenInRequest",
                                /* sample */ 0,
                                /* expected_count */ 1);
}

TEST_F(RealTimeUrlLookupServiceTest,
       TestStartLookup_NoTokenWhenNotConfiguredInClient) {
  EnableMbb();
  GURL url(kTestUrl);
  SetUpRTLookupResponse(RTLookupResponse::ThreatInfo::DANGEROUS,
                        RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING, 60,
                        "example.test/",
                        RTLookupResponse::ThreatInfo::COVERING_MATCH);

  base::MockCallback<RTLookupResponseCallback> response_callback;
  EXPECT_CALL(response_callback, Run(/* is_rt_lookup_successful */ true,
                                     /* is_cached_response */ false, _));
  MustRunInterceptor interceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        // Cookies should be attached when token is empty.
        EXPECT_EQ(request.credentials_mode,
                  network::mojom::CredentialsMode::kInclude);

        EXPECT_FALSE(
            request.headers.HasHeader(net::HttpRequestHeaders::kAuthorization));
      }));
  test_url_loader_factory_.SetInterceptor(interceptor.GetCallback());

  rt_service()->StartLookup(url, response_callback.Get(),
                            base::SequencedTaskRunner::GetCurrentDefault(),
                            SessionID::InvalidValue());

  task_environment_.RunUntilIdle();

  // Check the response is cached.
  std::unique_ptr<RTLookupResponse> cache_response =
      GetCachedRealTimeUrlVerdict(url);
  EXPECT_NE(nullptr, cache_response);
}

TEST_F(RealTimeUrlLookupServiceTest,
       TestStartLookup_OnInvalidAccessTokenCalledResponseCodeUnauthorized) {
  EnableMbb();
  EnableTokenFetchesInClient();
  GURL url(kTestUrl);
  SetUpFailureResponse(net::HTTP_UNAUTHORIZED);

  base::MockCallback<network::TestURLLoaderFactory::Interceptor>
      request_callback;
  base::MockCallback<RTLookupResponseCallback> response_callback;
  test_url_loader_factory_.SetInterceptor(request_callback.Get());
  EXPECT_CALL(request_callback, Run(_)).Times(1);
  EXPECT_CALL(response_callback, Run(/* is_rt_lookup_successful */ false,
                                     /* is_cached_response */ false, _));

  rt_service()->StartLookup(url, response_callback.Get(),
                            base::SequencedTaskRunner::GetCurrentDefault(),
                            SessionID::InvalidValue());

  FulfillAccessTokenRequest("invalid_token_string");
  EXPECT_CALL(*raw_token_fetcher(),
              OnInvalidAccessToken("invalid_token_string"))
      .Times(1);
  task_environment_.RunUntilIdle();
}

TEST_F(RealTimeUrlLookupServiceTest,
       TestStartLookup_OnInvalidAccessTokenNotCalledResponseCodeForbidden) {
  EnableMbb();
  EnableTokenFetchesInClient();
  GURL url(kTestUrl);
  SetUpFailureResponse(net::HTTP_FORBIDDEN);

  base::MockCallback<RTLookupResponseCallback> response_callback;
  EXPECT_CALL(response_callback, Run(/* is_rt_lookup_successful */ false,
                                     /* is_cached_response */ false, _));

  rt_service()->StartLookup(url, response_callback.Get(),
                            base::SequencedTaskRunner::GetCurrentDefault(),
                            SessionID::InvalidValue());

  FulfillAccessTokenRequest("invalid_token_string");
  EXPECT_CALL(*raw_token_fetcher(), OnInvalidAccessToken(_)).Times(0);
  task_environment_.RunUntilIdle();
}

TEST_F(RealTimeUrlLookupServiceTest, TestReferrerChain_ReferrerChainAttached) {
  EnableMbb();
  GURL url(kTestUrl);
  SetUpRTLookupResponse(RTLookupResponse::ThreatInfo::DANGEROUS,
                        RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING, 60,
                        "example.test/",
                        RTLookupResponse::ThreatInfo::COVERING_MATCH);
  ReferrerChain returned_referrer_chain;
  double navigation_time_msec =
      base::Time::Now().InMillisecondsSinceUnixEpoch();
  returned_referrer_chain.Add()->Swap(
      CreateReferrerChainEntry(kTestUrl, /*main_frame_url=*/"",
                               /*referrer_url=*/"",
                               /*referrer_main_frame_url=*/"",
                               navigation_time_msec)
          .get());
  returned_referrer_chain.Add()->Swap(
      CreateReferrerChainEntry(kTestReferrerUrl, /*main_frame_url=*/"",
                               /*referrer_url=*/"",
                               /*referrer_main_frame_url=*/"",
                               navigation_time_msec)
          .get());
  EXPECT_CALL(*referrer_chain_provider_,
              IdentifyReferrerChainByPendingEventURL(
                  url, /*user_gesture_count_limit=*/2, _))
      .WillOnce(DoAll(SetArgPointee<2>(returned_referrer_chain),
                      Return(ReferrerChainProvider::SUCCESS)));
  EXPECT_CALL(*referrer_chain_provider_,
              IdentifyReferrerChainByEventURL(
                  url, _, /*user_gesture_count_limit=*/2, _))
      .Times(0);

  base::MockCallback<RTLookupResponseCallback> response_callback;
  EXPECT_CALL(response_callback, Run(/* is_rt_lookup_successful */ true,
                                     /* is_cached_response */ false, _));
  bool request_validated;
  MustRunInterceptor interceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        RTLookupRequest request_proto;
        ASSERT_TRUE(GetRequestProto(request, &request_proto));

        EXPECT_EQ(3, request_proto.version());
        // Check referrer chain is attached.
        EXPECT_EQ(2, request_proto.referrer_chain().size());
        EXPECT_EQ(kTestUrl, request_proto.referrer_chain().Get(0).url());
        EXPECT_EQ(kTestReferrerUrl,
                  request_proto.referrer_chain().Get(1).url());
        request_validated = true;
      }));
  test_url_loader_factory_.SetInterceptor(interceptor.GetCallback());

  rt_service()->StartLookup(url, response_callback.Get(),
                            base::SequencedTaskRunner::GetCurrentDefault(),
                            SessionID::InvalidValue());

  task_environment_.RunUntilIdle();
  EXPECT_TRUE(request_validated);
}

TEST_F(RealTimeUrlLookupServiceTest,
       TestReferrerChain_SanitizedIfSubresourceNotAllowed) {
  EnableMbb();
  GURL url(kTestUrl);
  SetUpRTLookupResponse(RTLookupResponse::ThreatInfo::DANGEROUS,
                        RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING, 60,
                        "example.test/",
                        RTLookupResponse::ThreatInfo::COVERING_MATCH);
  ReferrerChain returned_referrer_chain;
  double navigation_time_msec =
      base::Time::Now().InMillisecondsSinceUnixEpoch();
  returned_referrer_chain.Add()->Swap(
      CreateReferrerChainEntry(kTestSubframeUrl, kTestUrl,
                               kTestSubframeReferrerUrl, kTestReferrerUrl,
                               navigation_time_msec)
          .get());
  returned_referrer_chain.Add()->Swap(
      CreateReferrerChainEntry(kTestReferrerUrl, /*main_frame_url=*/"",
                               /*referrer_url=*/"",
                               /*referrer_main_frame_url=*/"",
                               navigation_time_msec)
          .get());
  EXPECT_CALL(*referrer_chain_provider_,
              IdentifyReferrerChainByPendingEventURL(
                  url, /*user_gesture_count_limit=*/2, _))
      .WillOnce(DoAll(SetArgPointee<2>(returned_referrer_chain),
                      Return(ReferrerChainProvider::SUCCESS)));
  EXPECT_CALL(*referrer_chain_provider_,
              IdentifyReferrerChainByEventURL(
                  url, _, /*user_gesture_count_limit=*/2, _))
      .Times(0);

  bool request_validated;
  base::MockCallback<RTLookupResponseCallback> response_callback;
  MustRunInterceptor interceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        RTLookupRequest request_proto;
        ASSERT_TRUE(GetRequestProto(request, &request_proto));

        EXPECT_EQ(3, request_proto.version());
        EXPECT_EQ(2, request_proto.referrer_chain().size());
        // The first entry is sanitized because it is triggered in a
        // subframe.
        EXPECT_EQ(kTestUrl, request_proto.referrer_chain().Get(0).url());
        EXPECT_FALSE(
            request_proto.referrer_chain().Get(0).has_main_frame_url());
        EXPECT_TRUE(
            request_proto.referrer_chain().Get(0).is_subframe_url_removed());
        EXPECT_EQ(kTestReferrerUrl,
                  request_proto.referrer_chain().Get(0).referrer_url());
        EXPECT_FALSE(request_proto.referrer_chain()
                         .Get(0)
                         .has_referrer_main_frame_url());
        EXPECT_TRUE(request_proto.referrer_chain()
                        .Get(0)
                        .is_subframe_referrer_url_removed());
        // The second entry is not sanitized because it is triggered in a
        // mainframe.
        EXPECT_EQ(kTestReferrerUrl,
                  request_proto.referrer_chain().Get(1).url());
        EXPECT_FALSE(
            request_proto.referrer_chain().Get(1).is_subframe_url_removed());
        EXPECT_FALSE(request_proto.referrer_chain()
                         .Get(1)
                         .is_subframe_referrer_url_removed());

        request_validated = true;
      }));
  test_url_loader_factory_.SetInterceptor(interceptor.GetCallback());

  EXPECT_CALL(response_callback, Run(/* is_rt_lookup_successful */ true,
                                     /* is_cached_response */ false, _));

  rt_service()->StartLookup(url, response_callback.Get(),
                            base::SequencedTaskRunner::GetCurrentDefault(),
                            SessionID::InvalidValue());

  task_environment_.RunUntilIdle();
  EXPECT_TRUE(request_validated);
}

TEST_F(RealTimeUrlLookupServiceTest,
       TestReferrerChain_NotSanitizedIfSubresourceAllowed) {
  EnableMbb();
  // Subresource is allowed when enhanced protection is enabled.
  SetSafeBrowsingState(&test_pref_service_,
                       SafeBrowsingState::ENHANCED_PROTECTION);
  GURL url(kTestUrl);
  SetUpRTLookupResponse(RTLookupResponse::ThreatInfo::DANGEROUS,
                        RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING, 60,
                        "example.test/",
                        RTLookupResponse::ThreatInfo::COVERING_MATCH);
  ReferrerChain returned_referrer_chain;
  double navigation_time_msec =
      base::Time::Now().InMillisecondsSinceUnixEpoch();
  returned_referrer_chain.Add()->Swap(
      CreateReferrerChainEntry(kTestSubframeUrl, kTestUrl,
                               kTestSubframeReferrerUrl, kTestReferrerUrl,
                               navigation_time_msec)
          .get());
  returned_referrer_chain.Add()->Swap(
      CreateReferrerChainEntry(kTestSubframeReferrerUrl, kTestReferrerUrl,
                               /*referrer_url=*/"",
                               /*referrer_main_frame_url=*/"",
                               navigation_time_msec)
          .get());
  EXPECT_CALL(*referrer_chain_provider_,
              IdentifyReferrerChainByPendingEventURL(
                  url, /*user_gesture_count_limit=*/2, _))
      .WillOnce(DoAll(SetArgPointee<2>(returned_referrer_chain),
                      Return(ReferrerChainProvider::SUCCESS)));
  EXPECT_CALL(*referrer_chain_provider_,
              IdentifyReferrerChainByEventURL(
                  url, _, /*user_gesture_count_limit=*/2, _))
      .Times(0);

  bool request_validated;
  base::MockCallback<RTLookupResponseCallback> response_callback;
  MustRunInterceptor interceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        RTLookupRequest request_proto;
        ASSERT_TRUE(GetRequestProto(request, &request_proto));
        EXPECT_EQ(3, request_proto.version());
        EXPECT_EQ(2, request_proto.referrer_chain().size());
        // Check referrer chain is not sanitized.
        EXPECT_EQ(kTestSubframeUrl,
                  request_proto.referrer_chain().Get(0).url());
        EXPECT_EQ(kTestUrl,
                  request_proto.referrer_chain().Get(0).main_frame_url());
        EXPECT_FALSE(
            request_proto.referrer_chain().Get(0).is_subframe_url_removed());
        EXPECT_EQ(kTestSubframeReferrerUrl,
                  request_proto.referrer_chain().Get(0).referrer_url());
        EXPECT_EQ(
            kTestReferrerUrl,
            request_proto.referrer_chain().Get(0).referrer_main_frame_url());
        EXPECT_FALSE(request_proto.referrer_chain()
                         .Get(0)
                         .is_subframe_referrer_url_removed());
        EXPECT_EQ(kTestSubframeReferrerUrl,
                  request_proto.referrer_chain().Get(1).url());
        EXPECT_FALSE(
            request_proto.referrer_chain().Get(1).is_subframe_url_removed());

        request_validated = true;
      }));
  test_url_loader_factory_.SetInterceptor(interceptor.GetCallback());

  EXPECT_CALL(response_callback, Run(/* is_rt_lookup_successful */ true,
                                     /* is_cached_response */ false, _));

  rt_service()->StartLookup(url, response_callback.Get(),
                            base::SequencedTaskRunner::GetCurrentDefault(),
                            SessionID::InvalidValue());

  task_environment_.RunUntilIdle();
  EXPECT_TRUE(request_validated);
}

TEST_F(RealTimeUrlLookupServiceTest,
       TestReferrerChain_SanitizedIfMinAllowedTimestampIsNotMet) {
  // Set the first referrer chain before real time URL lookup is enabled.
  // Note that this set up is different from the real referrer chain which
  // is stored in reverse chronological order, but it doesn't affect this test.
  ReferrerChain returned_referrer_chain;
  returned_referrer_chain.Add()->Swap(
      CreateReferrerChainEntry(kTestSubframeUrl, kTestUrl,
                               kTestSubframeReferrerUrl, kTestReferrerUrl,
                               base::Time::Now().InMillisecondsSinceUnixEpoch())
          .get());

  task_environment_.FastForwardBy(base::Minutes(1));
  EnableMbb();
  GURL url(kTestUrl);
  SetUpRTLookupResponse(RTLookupResponse::ThreatInfo::DANGEROUS,
                        RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING, 60,
                        "example.test/",
                        RTLookupResponse::ThreatInfo::COVERING_MATCH);
  returned_referrer_chain.Add()->Swap(
      CreateReferrerChainEntry(kTestSubframeReferrerUrl, kTestReferrerUrl,
                               /*referrer_url=*/"",
                               /*referrer_main_frame_url=*/"",
                               base::Time::Now().InMillisecondsSinceUnixEpoch())
          .get());
  EXPECT_CALL(*referrer_chain_provider_,
              IdentifyReferrerChainByPendingEventURL(
                  url, /*user_gesture_count_limit=*/2, _))
      .WillOnce(DoAll(SetArgPointee<2>(returned_referrer_chain),
                      Return(ReferrerChainProvider::SUCCESS)));
  EXPECT_CALL(*referrer_chain_provider_,
              IdentifyReferrerChainByEventURL(
                  url, _, /*user_gesture_count_limit=*/2, _))
      .Times(0);

  base::MockCallback<RTLookupResponseCallback> response_callback;
  EXPECT_CALL(response_callback, Run(/* is_rt_lookup_successful */ true,
                                     /* is_cached_response */ false, _));

  bool request_validated;
  MustRunInterceptor interceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        RTLookupRequest request_proto;
        ASSERT_TRUE(GetRequestProto(request, &request_proto));

        EXPECT_EQ(3, request_proto.version());
        EXPECT_EQ(2, request_proto.referrer_chain().size());
        // Check the first referrer chain is sanitized because it's logged
        // before real time URL lookup is enabled.
        EXPECT_EQ("", request_proto.referrer_chain().Get(0).url());
        EXPECT_EQ("", request_proto.referrer_chain().Get(0).main_frame_url());
        EXPECT_FALSE(
            request_proto.referrer_chain().Get(0).is_subframe_url_removed());
        EXPECT_EQ("", request_proto.referrer_chain().Get(0).referrer_url());
        EXPECT_EQ(
            "",
            request_proto.referrer_chain().Get(0).referrer_main_frame_url());
        EXPECT_FALSE(request_proto.referrer_chain()
                         .Get(0)
                         .is_subframe_referrer_url_removed());
        EXPECT_TRUE(
            request_proto.referrer_chain().Get(0).is_url_removed_by_policy());

        // The second referrer chain should be sanitized based on the user
        // consent.
        EXPECT_EQ(kTestReferrerUrl,
                  request_proto.referrer_chain().Get(1).url());
        EXPECT_TRUE(
            request_proto.referrer_chain().Get(1).is_subframe_url_removed());
        EXPECT_FALSE(
            request_proto.referrer_chain().Get(1).is_url_removed_by_policy());

        request_validated = true;
      }));
  test_url_loader_factory_.SetInterceptor(interceptor.GetCallback());

  rt_service()->StartLookup(url, response_callback.Get(),
                            base::SequencedTaskRunner::GetCurrentDefault(),
                            SessionID::InvalidValue());

  task_environment_.RunUntilIdle();
  EXPECT_TRUE(request_validated);
}

// Tests that when a call to IdentifyReferrerChainByPendingEventURL does not
// identify the referrer chain, the service falls back to
// IdentifyReferrerChainByEventURL.
TEST_F(RealTimeUrlLookupServiceTest,
       TestReferrerChain_FallbackToEventUrlReferrerChain) {
  base::HistogramTester histogram_tester;
  EnableRealTimeUrlLookup({kSafeBrowsingAsyncRealTimeCheck}, {});
  GURL url(kTestUrl);
  SetUpRTLookupResponse(RTLookupResponse::ThreatInfo::DANGEROUS,
                        RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING, 60,
                        "example.test/",
                        RTLookupResponse::ThreatInfo::COVERING_MATCH);
  ReferrerChain returned_referrer_chain;
  double navigation_time_msec =
      base::Time::Now().InMillisecondsSinceUnixEpoch();
  returned_referrer_chain.Add()->Swap(
      CreateReferrerChainEntry(kTestUrl, /*main_frame_url=*/"",
                               /*referrer_url=*/"",
                               /*referrer_main_frame_url=*/"",
                               navigation_time_msec)
          .get());
  returned_referrer_chain.Add()->Swap(
      CreateReferrerChainEntry(kTestReferrerUrl, /*main_frame_url=*/"",
                               /*referrer_url=*/"",
                               /*referrer_main_frame_url=*/"",
                               navigation_time_msec)
          .get());
  EXPECT_CALL(*referrer_chain_provider_,
              IdentifyReferrerChainByPendingEventURL(
                  url, /*user_gesture_count_limit=*/2, _))
      .WillOnce(Return(ReferrerChainProvider::NAVIGATION_EVENT_NOT_FOUND));
  EXPECT_CALL(*referrer_chain_provider_,
              IdentifyReferrerChainByEventURL(
                  url, _, /*user_gesture_count_limit=*/2, _))
      .WillOnce(DoAll(SetArgPointee<3>(returned_referrer_chain),
                      Return(ReferrerChainProvider::SUCCESS)));

  base::MockCallback<RTLookupResponseCallback> response_callback;
  EXPECT_CALL(response_callback, Run(/* is_rt_lookup_successful */ true,
                                     /* is_cached_response */ false, _));
  bool request_validated;
  MustRunInterceptor interceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        RTLookupRequest request_proto;
        ASSERT_TRUE(GetRequestProto(request, &request_proto));
        // Check referrer chain is attached.
        EXPECT_EQ(2, request_proto.referrer_chain().size());
        EXPECT_EQ(kTestUrl, request_proto.referrer_chain().Get(0).url());
        EXPECT_EQ(kTestReferrerUrl,
                  request_proto.referrer_chain().Get(1).url());
        request_validated = true;
      }));
  test_url_loader_factory_.SetInterceptor(interceptor.GetCallback());

  rt_service()->StartLookup(url, response_callback.Get(),
                            base::SequencedTaskRunner::GetCurrentDefault(),
                            SessionID::InvalidValue());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(request_validated);

  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.RT.EventUrlReferrerChainFetchSucceeded",
      /*sample=*/true,
      /*expected_bucket_count=*/1);
}

TEST_F(
    RealTimeUrlLookupServiceTest,
    TestReferrerChain_NoFallbackToEventUrlReferrerChain_AsyncChecksDisabled) {
  EnableRealTimeUrlLookup({}, {kSafeBrowsingAsyncRealTimeCheck});
  GURL url(kTestUrl);
  SetUpRTLookupResponse(RTLookupResponse::ThreatInfo::DANGEROUS,
                        RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING, 60,
                        "example.test/",
                        RTLookupResponse::ThreatInfo::COVERING_MATCH);
  EXPECT_CALL(*referrer_chain_provider_,
              IdentifyReferrerChainByPendingEventURL(
                  url, /*user_gesture_count_limit=*/2, _))
      .WillOnce(Return(ReferrerChainProvider::NAVIGATION_EVENT_NOT_FOUND));
  EXPECT_CALL(*referrer_chain_provider_,
              IdentifyReferrerChainByEventURL(
                  url, _, /*user_gesture_count_limit=*/2, _))
      .Times(0);

  base::MockCallback<RTLookupResponseCallback> response_callback;
  EXPECT_CALL(response_callback, Run(/* is_rt_lookup_successful */ true,
                                     /* is_cached_response */ false, _));
  bool request_validated;
  MustRunInterceptor interceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        RTLookupRequest request_proto;
        ASSERT_TRUE(GetRequestProto(request, &request_proto));
        // Check no referrer chain is attached.
        EXPECT_TRUE(request_proto.referrer_chain().empty());
        request_validated = true;
      }));
  test_url_loader_factory_.SetInterceptor(interceptor.GetCallback());

  rt_service()->StartLookup(url, response_callback.Get(),
                            base::SequencedTaskRunner::GetCurrentDefault(),
                            SessionID::InvalidValue());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(request_validated);
}

TEST_F(RealTimeUrlLookupServiceTest, TestShutdown_CallbackNotPostedOnShutdown) {
  EnableMbb();
  GURL url(kTestUrl);

  base::MockCallback<network::TestURLLoaderFactory::Interceptor>
      request_callback;
  test_url_loader_factory_.SetInterceptor(request_callback.Get());
  EXPECT_CALL(request_callback, Run(_)).Times(1);

  base::MockCallback<RTLookupResponseCallback> response_callback;

  EXPECT_CALL(response_callback, Run(_, _, _)).Times(0);

  rt_service()->StartLookup(url, response_callback.Get(),
                            base::SequencedTaskRunner::GetCurrentDefault(),
                            SessionID::InvalidValue());

  rt_service()->Shutdown();

  task_environment_.RunUntilIdle();
}

TEST_F(RealTimeUrlLookupServiceTest, TestShutdown_CacheManagerReset) {
  // Shutdown and delete depending objects.
  rt_service()->Shutdown();
  cache_manager_.reset();
  content_setting_map_->ShutdownOnUIThread();
  content_setting_map_.reset();

  // Post a task to cache_manager_ to cache the verdict.
  MayBeCacheRealTimeUrlVerdict(RTLookupResponse::ThreatInfo::DANGEROUS,
                               RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING,
                               60, "a.example.test/path1/path2",
                               RTLookupResponse::ThreatInfo::COVERING_MATCH);

  // The task to cache_manager_ should be cancelled and not cause crash.
  task_environment_.RunUntilIdle();
}

TEST_F(RealTimeUrlLookupServiceTest,
       TestShutdown_SendRequestNotCalledOnShutdown) {
  // Never send the request if shutdown is triggered before OnGetAccessToken().
  EnableMbb();
  EnableTokenFetchesInClient();
  GURL url(kTestUrl);

  base::MockCallback<network::TestURLLoaderFactory::Interceptor>
      request_callback;
  testing::StrictMock<base::MockCallback<RTLookupResponseCallback>>
      response_callback;

  test_url_loader_factory_.SetInterceptor(request_callback.Get());
  EXPECT_CALL(request_callback, Run(_)).Times(0);
  EXPECT_CALL(response_callback, Run(_, _, _)).Times(0);

  rt_service()->StartLookup(url, response_callback.Get(),
                            base::SequencedTaskRunner::GetCurrentDefault(),
                            SessionID::InvalidValue());

  rt_service()->Shutdown();

  task_environment_.RunUntilIdle();
}

TEST_F(RealTimeUrlLookupServiceTest, TestSendSampledRequest) {
  EnableMbb();
  // Enabling access token does not affect sending sampled ping.
  EnableTokenFetchesInClient();
  GURL url(kTestUrl);

  MustRunInterceptor interceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        RTLookupRequest request_proto;
        ASSERT_TRUE(GetRequestProto(request, &request_proto));

        EXPECT_EQ(3, request_proto.version());
        EXPECT_EQ(2, request_proto.report_type());
        EXPECT_EQ(1, request_proto.frame_type());
      }));
  test_url_loader_factory_.SetInterceptor(interceptor.GetCallback());

  rt_service()->SendSampledRequest(
      url, base::SequencedTaskRunner::GetCurrentDefault(),
      SessionID::InvalidValue());
  rt_service()->Shutdown();

  task_environment_.RunUntilIdle();
}

TEST_F(RealTimeUrlLookupServiceTest, TestConcurrentSendSampledRequests) {
  EnableMbb();
  // Enabling access token does not affect sending sampled ping.
  EnableTokenFetchesInClient();
  GURL url(kTestUrl);

  // Only one network request should be made.
  base::MockCallback<network::TestURLLoaderFactory::Interceptor>
      request_callback;
  test_url_loader_factory_.SetInterceptor(request_callback.Get());
  EXPECT_CALL(request_callback, Run(_)).Times(1);

  // Send two sampled requests.
  rt_service()->SendSampledRequest(
      url, base::SequencedTaskRunner::GetCurrentDefault(),
      SessionID::InvalidValue());
  rt_service()->SendSampledRequest(
      url, base::SequencedTaskRunner::GetCurrentDefault(),
      SessionID::InvalidValue());
  rt_service()->Shutdown();

  task_environment_.RunUntilIdle();
}

TEST_F(RealTimeUrlLookupServiceTest,
       TestCanSendRTSampleRequest_FeatureEnabled) {
  // When extended reporting is not enabled,
  // sample request will not be sent.
  EXPECT_FALSE(CanSendRTSampleRequest());
  // Enable extended reporting.
  EnableExtendedReporting();
  rt_service()->set_bypass_probability_for_tests(true);
  EXPECT_TRUE(CanSendRTSampleRequest());
}

TEST_F(RealTimeUrlLookupServiceTest, TestBackoffModeSet) {
  EnableMbb();
  auto perform_lookup = [this](bool make_fail) {
    GURL url(kTestUrl);
    SetUpFailureResponse(make_fail ? net::HTTP_INTERNAL_SERVER_ERROR
                                   : net::HTTP_OK);
    base::MockCallback<network::TestURLLoaderFactory::Interceptor>
        request_callback;
    test_url_loader_factory_.SetInterceptor(request_callback.Get());
    EXPECT_CALL(request_callback, Run(_)).Times(1);
    base::MockCallback<RTLookupResponseCallback> response_callback;
    EXPECT_CALL(response_callback, Run(/* is_rt_lookup_successful */ !make_fail,
                                       /* is_cached_response */ false, _));
    rt_service()->StartLookup(url, response_callback.Get(),
                              base::SequencedTaskRunner::GetCurrentDefault(),
                              SessionID::InvalidValue());
    task_environment_.RunUntilIdle();
  };
  auto perform_failing_lookup = [perform_lookup]() { perform_lookup(true); };
  auto perform_successful_lookup = [perform_lookup]() {
    perform_lookup(false);
  };

  // Failing lookups 1 and 2 don't trigger backoff mode. Lookup 3 does.
  perform_failing_lookup();
  EXPECT_FALSE(IsInBackoffMode());
  perform_failing_lookup();
  EXPECT_FALSE(IsInBackoffMode());
  perform_failing_lookup();
  EXPECT_TRUE(IsInBackoffMode());

  // Backoff mode should still be set until 5 minutes later.
  task_environment_.FastForwardBy(base::Seconds(299));
  EXPECT_TRUE(IsInBackoffMode());
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(IsInBackoffMode());

  // Backoff mode only occurs after 3 *consecutive* failures.
  perform_failing_lookup();
  EXPECT_FALSE(IsInBackoffMode());
  perform_failing_lookup();
  EXPECT_FALSE(IsInBackoffMode());
  perform_successful_lookup();
  EXPECT_FALSE(IsInBackoffMode());
  perform_failing_lookup();
  EXPECT_FALSE(IsInBackoffMode());
  perform_failing_lookup();
  EXPECT_FALSE(IsInBackoffMode());
}

TEST_F(RealTimeUrlLookupServiceTest, TestBackoffModeSet_UnparseableResponse) {
  EnableMbb();
  auto perform_failing_lookup = [this]() {
    GURL url(kTestUrl);
    test_url_loader_factory_.AddResponse(kRealTimeLookupUrlPrefix,
                                         "unparseable-response");
    base::MockCallback<network::TestURLLoaderFactory::Interceptor>
        request_callback;
    test_url_loader_factory_.SetInterceptor(request_callback.Get());
    EXPECT_CALL(request_callback, Run(_)).Times(1);

    base::MockCallback<RTLookupResponseCallback> response_callback;
    EXPECT_CALL(response_callback, Run(/* is_rt_lookup_successful */ false,
                                       /* is_cached_response */ false, _));
    rt_service()->StartLookup(url, response_callback.Get(),
                              base::SequencedTaskRunner::GetCurrentDefault(),
                              SessionID::InvalidValue());
    task_environment_.RunUntilIdle();
  };

  perform_failing_lookup();
  perform_failing_lookup();
  EXPECT_FALSE(IsInBackoffMode());
  perform_failing_lookup();
  EXPECT_TRUE(IsInBackoffMode());
}

TEST_F(RealTimeUrlLookupServiceTest, TestBackoffModeRespected_Cached) {
  EnableMbb();

  // Cache a response for |cached_url|.
  GURL cached_url = GURL("https://example.cached.url");
  MayBeCacheRealTimeUrlVerdict(RTLookupResponse::ThreatInfo::DANGEROUS,
                               RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING,
                               60, "example.cached.url/",
                               RTLookupResponse::ThreatInfo::COVERING_MATCH);
  task_environment_.RunUntilIdle();

  // Enable backoff mode by running 3 failing requests.
  GURL url(kTestUrl);
  RunSimpleFailingRequest(url);
  RunSimpleFailingRequest(url);
  RunSimpleFailingRequest(url);
  EXPECT_TRUE(IsInBackoffMode());

  // Run a request for the original |cached_url|. In spite of being in backoff
  // mode, the cached response should apply before the lookup decides to quit
  // due to backoff.
  base::MockCallback<network::TestURLLoaderFactory::Interceptor>
      request_callback;
  base::MockCallback<RTLookupResponseCallback> response_callback;
  EXPECT_CALL(request_callback, Run(_)).Times(0);
  EXPECT_CALL(response_callback, Run(/* is_rt_lookup_successful */ true,
                                     /* is_cached_response */ true, _));
  test_url_loader_factory_.SetInterceptor(request_callback.Get());
  rt_service()->StartLookup(cached_url, response_callback.Get(),
                            base::SequencedTaskRunner::GetCurrentDefault(),
                            SessionID::InvalidValue());

  task_environment_.RunUntilIdle();
}

TEST_F(RealTimeUrlLookupServiceTest, TestBackoffModeRespected_NotCached) {
  EnableMbb();

  // Enable backoff mode by running 3 failing requests.
  GURL url(kTestUrl);
  RunSimpleFailingRequest(url);
  RunSimpleFailingRequest(url);
  RunSimpleFailingRequest(url);
  EXPECT_TRUE(IsInBackoffMode());

  // Since the response is not already cached, the lookup will fail since the
  // service is in backoff mode.
  base::MockCallback<network::TestURLLoaderFactory::Interceptor>
      request_callback;
  base::MockCallback<RTLookupResponseCallback> response_callback;
  EXPECT_CALL(request_callback, Run(_)).Times(0);
  EXPECT_CALL(response_callback, Run(/* is_rt_lookup_successful */ false,
                                     /* is_cached_response */ false, _));
  test_url_loader_factory_.SetInterceptor(request_callback.Get());
  rt_service()->StartLookup(url, response_callback.Get(),
                            base::SequencedTaskRunner::GetCurrentDefault(),
                            SessionID::InvalidValue());

  task_environment_.RunUntilIdle();
}

TEST_F(RealTimeUrlLookupServiceTest, TestRetriableErrors) {
  EnableMbb();
  auto perform_failing_lookup = [this](net::Error net_error) {
    CHECK_NE(net_error, net::OK);
    GURL url(kTestUrl);
    SetUpFailureResponse(net::HTTP_OK, net_error);
    base::MockCallback<network::TestURLLoaderFactory::Interceptor>
        request_callback;
    test_url_loader_factory_.SetInterceptor(request_callback.Get());
    EXPECT_CALL(request_callback, Run(_)).Times(1);

    base::MockCallback<RTLookupResponseCallback> response_callback;
    EXPECT_CALL(response_callback, Run(/* is_rt_lookup_successful */ false,
                                       /* is_cached_response */ false, _));
    rt_service()->StartLookup(url, response_callback.Get(),
                              base::SequencedTaskRunner::GetCurrentDefault(),
                              SessionID::InvalidValue());
    task_environment_.RunUntilIdle();
  };

  // Retriable errors should not trigger backoff mode.
  perform_failing_lookup(net::ERR_INTERNET_DISCONNECTED);
  perform_failing_lookup(net::ERR_NETWORK_CHANGED);
  perform_failing_lookup(net::ERR_INTERNET_DISCONNECTED);
  perform_failing_lookup(net::ERR_NETWORK_CHANGED);
  perform_failing_lookup(net::ERR_INTERNET_DISCONNECTED);
  perform_failing_lookup(net::ERR_NETWORK_CHANGED);
  EXPECT_FALSE(IsInBackoffMode());

  // Retriable errors should not reset the backoff counter back to 0.
  perform_failing_lookup(net::ERR_FAILED);
  perform_failing_lookup(net::ERR_FAILED);
  perform_failing_lookup(net::ERR_INTERNET_DISCONNECTED);
  EXPECT_FALSE(IsInBackoffMode());
  perform_failing_lookup(net::ERR_FAILED);
  EXPECT_TRUE(IsInBackoffMode());
}

}  // namespace safe_browsing
