// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/safe_browsing_url_checker_impl.h"
#include <memory>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/timer/mock_timer.h"
#include "components/safe_browsing/core/browser/db/test_database_manager.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/realtime/url_lookup_service.h"
#include "components/safe_browsing/core/browser/safe_browsing_token_fetcher.h"
#include "components/safe_browsing/core/browser/url_checker_delegate.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/security_interstitials/core/unsafe_resource.h"
#include "net/http/http_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using security_interstitials::UnsafeResource;
using ::testing::_;

namespace safe_browsing {

namespace {

// A matcher for threat source in UnsafeResource.
MATCHER_P(IsSameThreatSource, threatSource, "") {
  return arg.threat_source == threatSource;
}

class MockSafeBrowsingDatabaseManager : public TestSafeBrowsingDatabaseManager {
 public:
  MockSafeBrowsingDatabaseManager()
      : TestSafeBrowsingDatabaseManager(
            base::SequencedTaskRunner::GetCurrentDefault(),
            base::SequencedTaskRunner::GetCurrentDefault()) {}
  // SafeBrowsingDatabaseManager implementation.
  // Checks the threat type of |gurl| previously set by |SetThreatTypeForUrl|.
  // It crashes if the threat type of |gurl| is not set in advance.
  bool CheckBrowseUrl(const GURL& gurl,
                      const safe_browsing::SBThreatTypeSet& threat_types,
                      Client* client,
                      MechanismExperimentHashDatabaseCache
                          experiment_cache_selection) override {
    std::string url = gurl.spec();
    DCHECK(base::Contains(urls_threat_type_, url));
    DCHECK(base::Contains(urls_delayed_callback_, url));
    EXPECT_TRUE(base::Contains(acceptable_cache_selections_,
                               experiment_cache_selection));
    if (urls_threat_type_[url] == SB_THREAT_TYPE_SAFE) {
      return true;
    }
    if (!urls_delayed_callback_[url]) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(&MockSafeBrowsingDatabaseManager::OnCheckBrowseURLDone,
                         this, gurl, client));
    } else {
      // If delayed callback is set to true, store the client in |urls_client_|.
      // The callback can be triggered by |RestartDelayedCallback|.
      urls_client_[url] = client;
    }
    return false;
  }

  bool CanCheckRequestDestination(
      network::mojom::RequestDestination request_destination) const override {
    return true;
  }

  bool ChecksAreAlwaysAsync() const override { return false; }

  ThreatSource GetThreatSource() const override {
    return ThreatSource::UNKNOWN;
  }

  // Returns the allowlist match result previously set by
  // |SetAllowlistResultForUrl|. It crashes if the allowlist match result for
  // the |gurl| is not set in advance.
  bool CheckUrlForHighConfidenceAllowlist(
      const GURL& gurl,
      const std::string& metric_variation) override {
    std::string url = gurl.spec();
    DCHECK(base::Contains(urls_allowlist_match_, url));
    return urls_allowlist_match_[url];
  }

  // Helper functions.
  // Restart the previous delayed callback for |gurl|. This is useful to test
  // the asynchronous URL check, i.e. the database manager is still checking the
  // previous URL and the new redirect URL arrives.
  void RestartDelayedCallback(const GURL& gurl) {
    std::string url = gurl.spec();
    DCHECK(base::Contains(urls_delayed_callback_, url));
    DCHECK_EQ(true, urls_delayed_callback_[url]);
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&MockSafeBrowsingDatabaseManager::OnCheckBrowseURLDone,
                       this, gurl, urls_client_[url]));
  }

  void SetThreatTypeForUrl(const GURL& gurl,
                           SBThreatType threat_type,
                           bool delayed_callback) {
    std::string url = gurl.spec();
    urls_threat_type_[url] = threat_type;
    urls_delayed_callback_[url] = delayed_callback;
  }

  void SetAllowlistResultForUrl(const GURL& gurl, bool match) {
    std::string url = gurl.spec();
    urls_allowlist_match_[url] = match;
  }

  void SetAcceptableExperimentCacheSelections(
      std::set<MechanismExperimentHashDatabaseCache>
          acceptable_cache_selections) {
    acceptable_cache_selections_ = acceptable_cache_selections;
  }

  void CancelCheck(Client* client) override { called_cancel_check_ = true; }

  bool HasCalledCancelCheck() { return called_cancel_check_; }

 protected:
  ~MockSafeBrowsingDatabaseManager() override = default;

 private:
  void OnCheckBrowseURLDone(const GURL& gurl, Client* client) {
    if (called_cancel_check_) {
      return;
    }

    std::string url = gurl.spec();
    DCHECK(base::Contains(urls_threat_type_, url));
    ThreatMetadata metadata;
    client->OnCheckBrowseUrlResult(gurl, urls_threat_type_[url], metadata);
  }
  base::flat_map<std::string, SBThreatType> urls_threat_type_;
  base::flat_map<std::string, bool> urls_delayed_callback_;
  base::flat_map<std::string, Client*> urls_client_;
  base::flat_map<std::string, bool> urls_allowlist_match_;
  std::set<MechanismExperimentHashDatabaseCache> acceptable_cache_selections_ =
      {MechanismExperimentHashDatabaseCache::kNoExperiment};

  bool called_cancel_check_ = false;
};

class MockUrlCheckerDelegate : public UrlCheckerDelegate {
 public:
  explicit MockUrlCheckerDelegate(SafeBrowsingDatabaseManager* database_manager)
      : database_manager_(database_manager),
        threat_types_(
            SBThreatTypeSet({safe_browsing::SB_THREAT_TYPE_URL_PHISHING})) {}

  MOCK_METHOD1(MaybeDestroyNoStatePrefetchContents,
               void(base::OnceCallback<content::WebContents*()>));
  MOCK_METHOD5(StartDisplayingBlockingPageHelper,
               void(const security_interstitials::UnsafeResource&,
                    const std::string&,
                    const net::HttpRequestHeaders&,
                    bool,
                    bool));
  MOCK_METHOD2(StartObservingInteractionsForDelayedBlockingPageHelper,
               void(const security_interstitials::UnsafeResource&, bool));
  MOCK_METHOD5(ShouldSkipRequestCheck, bool(const GURL&, int, int, int, bool));
  MOCK_METHOD1(NotifySuspiciousSiteDetected,
               void(const base::RepeatingCallback<content::WebContents*()>&));
  MOCK_METHOD0(GetUIManager, BaseUIManager*());

  bool IsUrlAllowlisted(const GURL& url) override { return false; }
  void SetPolicyAllowlistDomains(
      const std::vector<std::string>& allowlist_domains) override {}
  const SBThreatTypeSet& GetThreatTypes() override { return threat_types_; }
  SafeBrowsingDatabaseManager* GetDatabaseManager() override {
    return database_manager_;
  }
  // TODO(crbug.com/1410253): delete these 6 methods upon experiment completion.
  void CheckLookupMechanismExperimentEligibility(
      const security_interstitials::UnsafeResource& resource,
      base::OnceCallback<void(bool)> callback,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner) override {
    num_check_experiment_eligibility_calls_++;
    CheckLookupMechanismExperimentEligibilityInternal(
        resource, std::move(callback), callback_task_runner);
  }
  void CheckLookupMechanismExperimentEligibilityInternal(
      const security_interstitials::UnsafeResource& resource,
      base::OnceCallback<void(bool)> callback,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner) {
    std::string url = resource.url.spec();
    DCHECK(base::Contains(expected_experiment_eligibility_, url));
    callback_task_runner->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  expected_experiment_eligibility_[url]));
  }
  int GetNumCheckExperimentEligibilityCalls() {
    return num_check_experiment_eligibility_calls_;
  }
  void CheckExperimentEligibilityAndStartBlockingPage(
      const security_interstitials::UnsafeResource& resource,
      base::OnceCallback<void(bool)> callback,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner) override {
    num_check_experiment_eligibility_and_block_page_calls_++;
    CheckLookupMechanismExperimentEligibilityInternal(
        resource, std::move(callback), callback_task_runner);
    std::string method;
    net::HttpRequestHeaders headers;
    StartDisplayingBlockingPageHelper(resource, method, headers, false, false);
  }
  int GetNumCheckExperimentEligibilityAndStartBlockingPageCalls() {
    return num_check_experiment_eligibility_and_block_page_calls_;
  }
  void SetLookupMechanismExperimentEligibility(const GURL& url,
                                               bool eligibility) {
    DCHECK(!base::Contains(expected_experiment_eligibility_, url.spec()));
    expected_experiment_eligibility_[url.spec()] = eligibility;
  }

 protected:
  ~MockUrlCheckerDelegate() override = default;

 private:
  raw_ptr<SafeBrowsingDatabaseManager> database_manager_;
  SBThreatTypeSet threat_types_;
  // TODO(crbug.com/1410253): delete these 3 fields upon experiment completion.
  base::flat_map<std::string, bool> expected_experiment_eligibility_;
  int num_check_experiment_eligibility_calls_ = 0;
  int num_check_experiment_eligibility_and_block_page_calls_ = 0;
};

class MockRealTimeUrlLookupService : public RealTimeUrlLookupServiceBase {
 public:
  MockRealTimeUrlLookupService()
      : RealTimeUrlLookupServiceBase(
            /*url_loader_factory=*/nullptr,
            /*cache_manager=*/nullptr,
            /*get_user_population_callback=*/base::BindRepeating([]() {
              return ChromeUserPopulation();
            }),
            /*referrer_chain_provider=*/nullptr) {}
  // Returns the threat type previously set by |SetThreatTypeForUrl|. It crashes
  // if the threat type for the |gurl| is not set in advance.
  void StartLookup(
      const GURL& gurl,
      const GURL& last_committed_url,
      bool is_mainframe,
      RTLookupRequestCallback request_callback,
      RTLookupResponseCallback response_callback,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner) override {
    std::string url = gurl.spec();
    DCHECK(base::Contains(url_details_, url));
    auto response = std::make_unique<RTLookupResponse>();
    RTLookupResponse::ThreatInfo* new_threat_info = response->add_threat_info();
    RTLookupResponse::ThreatInfo threat_info;
    RTLookupResponse::ThreatInfo::ThreatType threat_type;
    RTLookupResponse::ThreatInfo::VerdictType verdict_type;
    SBThreatType sb_threat_type = url_details_[url].threat_type;
    switch (sb_threat_type) {
      case SB_THREAT_TYPE_URL_PHISHING:
        threat_type = RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING;
        verdict_type = RTLookupResponse::ThreatInfo::DANGEROUS;
        break;
      case SB_THREAT_TYPE_SAFE:
        threat_type = RTLookupResponse::ThreatInfo::THREAT_TYPE_UNSPECIFIED;
        verdict_type = RTLookupResponse::ThreatInfo::SAFE;
        break;
      case SB_THREAT_TYPE_MANAGED_POLICY_BLOCK:
        threat_type = RTLookupResponse::ThreatInfo::MANAGED_POLICY;
        verdict_type = RTLookupResponse::ThreatInfo::DANGEROUS;
        break;
      case SB_THREAT_TYPE_MANAGED_POLICY_WARN:
        threat_type = RTLookupResponse::ThreatInfo::MANAGED_POLICY;
        verdict_type = RTLookupResponse::ThreatInfo::WARN;
        break;
      default:
        NOTREACHED();
        threat_type = RTLookupResponse::ThreatInfo::THREAT_TYPE_UNSPECIFIED;
        verdict_type = RTLookupResponse::ThreatInfo::SAFE;
    }
    threat_info.set_threat_type(threat_type);
    threat_info.set_verdict_type(verdict_type);
    *new_threat_info = threat_info;
    if (url_details_[url].should_complete_lookup) {
      callback_task_runner->PostTask(
          FROM_HERE, base::BindOnce(std::move(response_callback),
                                    /*is_rt_lookup_successful=*/true,
                                    /*is_cached_response=*/is_cached_response_,
                                    std::move(response)));
    }
  }

  void SendSampledRequest(
      const GURL& gurl,
      const GURL& last_committed_url,
      bool is_mainframe,
      RTLookupRequestCallback request_callback,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner) override {}

  // |should_complete_lookup| should generally be true, unless you specifically
  // want to test time-sensitive things like timeouts. Setting it to false will
  // avoid calling into |response_callback| in |StartLookup|.
  void SetThreatTypeForUrl(const GURL& gurl,
                           SBThreatType threat_type,
                           bool should_complete_lookup) {
    url_details_[gurl.spec()].threat_type = threat_type;
    url_details_[gurl.spec()].should_complete_lookup = should_complete_lookup;
  }

  void SetIsCachedResponse(bool is_cached_response) {
    is_cached_response_ = is_cached_response;
  }

  // RealTimeUrlLookupServiceBase:
  bool CanPerformFullURLLookup() const override { return true; }
  bool CanCheckSubresourceURL() const override { return false; }
  bool CanCheckSafeBrowsingDb() const override { return true; }
  bool CanCheckSafeBrowsingHighConfidenceAllowlist() const override {
    return true;
  }
  bool CanSendRTSampleRequest() const override { return true; }

 private:
  struct UrlDetail {
    SBThreatType threat_type;
    bool should_complete_lookup;
  };

  // RealTimeUrlLookupServiceBase:
  GURL GetRealTimeLookupUrl() const override { return GURL(); }
  net::NetworkTrafficAnnotationTag GetTrafficAnnotationTag() const override {
    return TRAFFIC_ANNOTATION_FOR_TESTS;
  }
  bool CanPerformFullURLLookupWithToken() const override { return false; }
  int GetReferrerUserGestureLimit() const override { return 0; }
  bool CanSendPageLoadToken() const override { return false; }
  void GetAccessToken(
      const GURL& url,
      const GURL& last_committed_url,
      bool is_mainframe,
      RTLookupRequestCallback request_callback,
      RTLookupResponseCallback response_callback,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner) override {}
  absl::optional<std::string> GetDMTokenString() const override {
    return absl::nullopt;
  }
  std::string GetMetricSuffix() const override { return ""; }
  bool ShouldIncludeCredentials() const override { return false; }
  double GetMinAllowedTimestampForReferrerChains() const override { return 0; }

  base::flat_map<std::string, UrlDetail> url_details_;
  bool is_cached_response_ = false;
};

class MockHashRealTimeService : public HashRealTimeService {
 public:
  MockHashRealTimeService()
      : HashRealTimeService(
            /*url_loader_factory=*/nullptr,
            /*get_network_context=*/base::NullCallback(),
            /*cache_manager=*/nullptr,
            /*ohttp_key_service=*/nullptr,
            /*get_is_enhanced_protection_enabled=*/base::NullCallback()) {}
  base::WeakPtr<MockHashRealTimeService> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  struct UrlDetail {
    absl::optional<SBThreatType> threat_type;
    bool should_fail_lookup;
  };

  // |should_complete_lookup| should generally be true, unless you specifically
  // want to test time-sensitive things like timeouts. Setting it to false will
  // avoid calling into |response_callback| in |StartLookup|.
  void SetThreatTypeForUrl(const GURL& gurl,
                           absl::optional<SBThreatType> threat_type,
                           bool should_fail_lookup) {
    url_details_[gurl.spec()].threat_type = threat_type;
    url_details_[gurl.spec()].should_fail_lookup = should_fail_lookup;
  }

  void StartLookup(
      const GURL& gurl,
      HPRTLookupResponseCallback response_callback,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner) override {
    std::string url = gurl.spec();
    ASSERT_TRUE(base::Contains(url_details_, url));
    callback_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(response_callback),
            /*is_lookup_successful=*/!url_details_[url].should_fail_lookup,
            /*threat_type=*/url_details_[url].threat_type,
            /*locally_cached_results_threat_type=*/SB_THREAT_TYPE_SAFE));
  }

 private:
  base::flat_map<std::string, UrlDetail> url_details_;
  base::WeakPtrFactory<MockHashRealTimeService> weak_factory_{this};
};

}  // namespace

class SafeBrowsingUrlCheckerTest : public PlatformTest {
 public:
  SafeBrowsingUrlCheckerTest() = default;

  void SetUp() override {
    PlatformTest::SetUp();
    database_manager_ = new MockSafeBrowsingDatabaseManager();
    url_checker_delegate_ = new MockUrlCheckerDelegate(database_manager_.get());
    url_lookup_service_ = std::make_unique<MockRealTimeUrlLookupService>();
    hash_realtime_service_ = std::make_unique<MockHashRealTimeService>();
  }

  std::unique_ptr<SafeBrowsingUrlCheckerImpl> CreateSafeBrowsingUrlChecker(
      bool real_time_lookup_enabled,
      bool can_check_safe_browsing_db,
      bool is_lookup_mechanism_experiment_enabled = false) {
    base::MockCallback<base::RepeatingCallback<content::WebContents*()>>
        mock_web_contents_getter;
    scoped_refptr<SafeBrowsingLookupMechanismExperimenter>
        mechanism_experimenter = nullptr;
    if (is_lookup_mechanism_experiment_enabled) {
      mechanism_experimenter = base::MakeRefCounted<
          SafeBrowsingLookupMechanismExperimenter>(
          /*is_prefetch=*/false, /*ping_manager_on_ui=*/nullptr,
          /*ui_task_runner=*/base::SequencedTaskRunner::GetCurrentDefault());
      // Tell the experimenter that WillProcessResponse has been reached so that
      // once the mechanisms complete, the experiment concludes and all memory
      // is cleaned up. Otherwise, this will cause memory leaks in the test.
      mechanism_experimenter->OnWillProcessResponseReached(
          base::TimeTicks::Now());
    }
    return std::make_unique<SafeBrowsingUrlCheckerImpl>(
        net::HttpRequestHeaders(), /*load_flags=*/0,
        network::mojom::RequestDestination::kDocument,
        /*has_user_gesture=*/false, url_checker_delegate_,
        mock_web_contents_getter.Get(), UnsafeResource::kNoRenderProcessId,
        UnsafeResource::kNoRenderFrameId, UnsafeResource::kNoFrameTreeNodeId,
        real_time_lookup_enabled,
        /*can_rt_check_subresource_url=*/false, can_check_safe_browsing_db,
        /*can_check_high_confidence_allowlist=*/true,
        /*url_lookup_service_metric_suffix=*/
        real_time_lookup_enabled ? ".Enterprise" : ".None",
        /*last_committed_url=*/GURL(),
        base::SequencedTaskRunner::GetCurrentDefault(),
        real_time_lookup_enabled ? url_lookup_service_->GetWeakPtr() : nullptr,
        /*webui_delegate_=*/nullptr,
        /*hash_realtime_service=*/hash_realtime_service_->GetWeakPtr(),
        /*mechanism_experimenter=*/mechanism_experimenter,
        is_lookup_mechanism_experiment_enabled);
  }

  // This can be used as the CheckUrl callback in cases where it's a local check
  // but it is not safe synchronously. Since the database manager is mocked out,
  // this is only relevant for unsafe URL checks that are interrupted by a
  // timeout, which ends up making them conclude the check is safe and call the
  // slow check notifier callback.
  void OnCheckUrlCallbackSettingSlowCheckNotifier(
      SafeBrowsingUrlCheckerImpl::NativeUrlCheckNotifier* slow_check_notifier,
      bool proceed,
      bool showed_interstitial,
      bool did_perform_real_time_check,
      bool did_check_allowlist) {
    *slow_check_notifier = slow_check_notifier_callback_.Get();
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  scoped_refptr<MockSafeBrowsingDatabaseManager> database_manager_;
  scoped_refptr<MockUrlCheckerDelegate> url_checker_delegate_;
  std::unique_ptr<MockRealTimeUrlLookupService> url_lookup_service_;
  std::unique_ptr<MockHashRealTimeService> hash_realtime_service_;
  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeUrlCheckNotifier>
      slow_check_notifier_callback_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(SafeBrowsingUrlCheckerTest, CheckUrl_SafeUrl) {
  base::HistogramTester histograms;
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*real_time_lookup_enabled=*/false, /*can_check_safe_browsing_db=*/true);

  GURL url("https://example.test/");
  database_manager_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_SAFE,
                                         /*delayed_callback=*/false);
  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  EXPECT_CALL(callback,
              Run(nullptr, /*proceed=*/true, /*showed_interstitial=*/false,
                  /*did_perform_real_time_check=*/false,
                  /*did_check_allowlist=*/false));
  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(_, _, _, _, _))
      .Times(0);

  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());
  task_environment_.RunUntilIdle();
  histograms.ExpectUniqueSample("SafeBrowsing.CheckUrl.Timeout",
                                /*sample=*/false,
                                /*expected_bucket_count=*/1);
}

TEST_F(SafeBrowsingUrlCheckerTest, CheckUrl_DangerousUrl) {
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*real_time_lookup_enabled=*/false, /*can_check_safe_browsing_db=*/true);

  GURL url("https://example.test/");
  database_manager_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_URL_PHISHING,
                                         /*delayed_callback=*/false);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  EXPECT_CALL(callback, Run(_, /*proceed=*/false, /*showed_interstitial=*/false,
                            /*did_perform_real_time_check=*/false,
                            /*did_check_allowlist=*/false));
  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(
                  IsSameThreatSource(ThreatSource::UNKNOWN), _, _, _, _))
      .Times(1);
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());
  task_environment_.RunUntilIdle();
}

TEST_F(SafeBrowsingUrlCheckerTest, CheckUrl_RedirectUrlsSafe) {
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*real_time_lookup_enabled=*/false, /*can_check_safe_browsing_db=*/true);

  GURL origin_url("https://example.test/");
  database_manager_->SetThreatTypeForUrl(origin_url, SB_THREAT_TYPE_SAFE,
                                         /*delayed_callback=*/false);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      origin_callback;
  EXPECT_CALL(origin_callback,
              Run(_, /*proceed=*/true, /*showed_interstitial=*/false,
                  /*did_perform_real_time_check=*/false,
                  /*did_check_allowlist=*/false));
  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(_, _, _, _, _))
      .Times(0);
  safe_browsing_url_checker->CheckUrl(origin_url, "GET", origin_callback.Get());

  GURL redirect_url("https://example.redirect.test/");
  database_manager_->SetThreatTypeForUrl(redirect_url, SB_THREAT_TYPE_SAFE,
                                         /*delayed_callback=*/false);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      redirect_callback;
  EXPECT_CALL(redirect_callback,
              Run(_, /*proceed=*/true, /*showed_interstitial=*/false,
                  /*did_perform_real_time_check=*/false,
                  /*did_check_allowlist=*/false));
  safe_browsing_url_checker->CheckUrl(redirect_url, "GET",
                                      redirect_callback.Get());

  task_environment_.RunUntilIdle();
}

TEST_F(SafeBrowsingUrlCheckerTest,
       CheckUrl_RedirectUrlsOriginDangerousRedirectSafe) {
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*real_time_lookup_enabled=*/false, /*can_check_safe_browsing_db=*/true);

  GURL origin_url("https://example.test/");
  database_manager_->SetThreatTypeForUrl(
      origin_url, SB_THREAT_TYPE_URL_PHISHING, /*delayed_callback=*/true);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      origin_callback;
  EXPECT_CALL(origin_callback,
              Run(_, /*proceed=*/false, /*showed_interstitial=*/false,
                  /*did_perform_real_time_check=*/false,
                  /*did_check_allowlist=*/false));
  // Not displayed yet, because the callback is not returned.
  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(_, _, _, _, _))
      .Times(0);
  safe_browsing_url_checker->CheckUrl(origin_url, "GET", origin_callback.Get());

  task_environment_.RunUntilIdle();

  GURL redirect_url("https://example.redirect.test/");
  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      redirect_callback;
  // Not called because it is blocked by the first URL.
  EXPECT_CALL(redirect_callback, Run(_, _, _, _, _)).Times(0);
  safe_browsing_url_checker->CheckUrl(redirect_url, "GET",
                                      redirect_callback.Get());

  // The blocking page should be displayed when the origin callback is returned.
  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(_, _, _, _, _))
      .Times(1);
  database_manager_->RestartDelayedCallback(origin_url);
  task_environment_.RunUntilIdle();
}

TEST_F(SafeBrowsingUrlCheckerTest, CheckUrl_RealTimeEnabledAllowlistMatch) {
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*real_time_lookup_enabled=*/true, /*can_check_safe_browsing_db=*/true);

  GURL url("https://example.test/");
  database_manager_->SetAllowlistResultForUrl(url, true);
  // To make sure hash based check is not skipped when the URL is in the
  // allowlist, set threat type to phishing for hash based check.
  database_manager_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_URL_PHISHING,
                                         /*delayed_callback=*/false);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  // Note that the callback is not called, because resource fetch is not blocked
  // while we perform a real time URL check.
  EXPECT_CALL(callback, Run(_, _, _, _, _)).Times(0);
  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(
                  IsSameThreatSource(ThreatSource::UNKNOWN), _, _, _, _))
      .Times(1);
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());

  task_environment_.RunUntilIdle();
}

TEST_F(SafeBrowsingUrlCheckerTest, CheckUrl_RealTimeEnabledSafeUrl) {
  base::HistogramTester histograms;
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*real_time_lookup_enabled=*/true, /*can_check_safe_browsing_db=*/true);

  GURL url("https://example.test/");
  database_manager_->SetAllowlistResultForUrl(url, false);
  url_lookup_service_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_SAFE,
                                           /*should_complete_lookup=*/true);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  EXPECT_CALL(callback, Run(_, /*proceed=*/true, /*showed_interstitial=*/false,
                            /*did_perform_real_time_check=*/true,
                            /*did_check_allowlist=*/true));
  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(_, _, _, _, _))
      .Times(0);
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());

  task_environment_.RunUntilIdle();
  histograms.ExpectUniqueSample("SafeBrowsing.CheckUrl.Timeout",
                                /*sample=*/false,
                                /*expected_bucket_count=*/1);

  // The false positive metric should not be logged, because the
  // verdict is not from cache.
  histograms.ExpectTotalCount("SafeBrowsing.RT.GetCache.FallbackThreatType",
                              /*expected_count=*/0);
}

TEST_F(SafeBrowsingUrlCheckerTest, CheckUrl_RealTimeEnabledSafeUrlFromCache) {
  base::HistogramTester histograms;
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*real_time_lookup_enabled=*/true, /*can_check_safe_browsing_db=*/true);

  GURL url("https://example.test/");
  database_manager_->SetAllowlistResultForUrl(url, false);
  database_manager_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_SAFE,
                                         /*delayed_callback=*/false);
  url_lookup_service_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_SAFE,
                                           /*should_complete_lookup=*/true);
  url_lookup_service_->SetIsCachedResponse(true);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  EXPECT_CALL(callback, Run(_, /*proceed=*/true, /*showed_interstitial=*/false,
                            /*did_perform_real_time_check=*/true,
                            /*did_check_allowlist=*/true));
  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(_, _, _, _, _))
      .Times(0);
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());

  task_environment_.RunUntilIdle();

  histograms.ExpectUniqueSample("SafeBrowsing.RT.GetCache.FallbackThreatType",
                                /*sample=*/SB_THREAT_TYPE_SAFE,
                                /*expected_bucket_count=*/1);
}

TEST_F(SafeBrowsingUrlCheckerTest,
       CheckUrl_RealTimeEnabledSafeUrlFromCacheFalsePositive) {
  base::HistogramTester histograms;
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*real_time_lookup_enabled=*/true, /*can_check_safe_browsing_db=*/true);

  GURL url("https://example.test/");
  database_manager_->SetAllowlistResultForUrl(url, false);
  database_manager_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_URL_PHISHING,
                                         /*delayed_callback=*/false);
  url_lookup_service_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_SAFE,
                                           /*should_complete_lookup=*/true);
  url_lookup_service_->SetIsCachedResponse(true);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(
                  IsSameThreatSource(ThreatSource::UNKNOWN), _, _, _, _))
      .Times(1);
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());

  task_environment_.RunUntilIdle();

  histograms.ExpectUniqueSample("SafeBrowsing.RT.GetCache.FallbackThreatType",
                                /*sample=*/SB_THREAT_TYPE_URL_PHISHING,
                                /*expected_bucket_count=*/1);
}

TEST_F(SafeBrowsingUrlCheckerTest,
       CheckUrl_RealTimeEnabledSafeBrowsingDisabled_ManagedWarn) {
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*real_time_lookup_enabled=*/true, /*can_check_safe_browsing_db=*/false);

  GURL url("https://example.test/");
  url_lookup_service_->SetThreatTypeForUrl(
      url, SB_THREAT_TYPE_MANAGED_POLICY_WARN, /*should_complete_lookup=*/true);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  // Should still show warning page because real time lookup is enabled.
  EXPECT_CALL(
      *url_checker_delegate_,
      StartDisplayingBlockingPageHelper(
          IsSameThreatSource(ThreatSource::REAL_TIME_CHECK), _, _, _, _))
      .Times(1);
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());

  task_environment_.RunUntilIdle();
}

TEST_F(SafeBrowsingUrlCheckerTest,
       CheckUrl_RealTimeEnabledSafeBrowsingDisabled_ManagedBlock) {
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*real_time_lookup_enabled=*/true, /*can_check_safe_browsing_db=*/false);

  GURL url("https://example.test/");
  url_lookup_service_->SetThreatTypeForUrl(url,
                                           SB_THREAT_TYPE_MANAGED_POLICY_BLOCK,
                                           /*should_complete_lookup=*/true);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  // Should still show blocking page because real time lookup is enabled.
  EXPECT_CALL(
      *url_checker_delegate_,
      StartDisplayingBlockingPageHelper(
          IsSameThreatSource(ThreatSource::REAL_TIME_CHECK), _, _, _, _))
      .Times(1);
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());

  task_environment_.RunUntilIdle();
}

TEST_F(SafeBrowsingUrlCheckerTest,
       CheckUrl_RealTimeEnabledSafeBrowsingDisabled_Dangerous) {
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*real_time_lookup_enabled=*/true, /*can_check_safe_browsing_db=*/false);

  GURL url("https://example.test/");
  url_lookup_service_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_URL_PHISHING,
                                           /*should_complete_lookup=*/true);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  // Should still show blocking page because real time lookup is enabled.
  EXPECT_CALL(
      *url_checker_delegate_,
      StartDisplayingBlockingPageHelper(
          IsSameThreatSource(ThreatSource::REAL_TIME_CHECK), _, _, _, _))
      .Times(1);
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());

  task_environment_.RunUntilIdle();
}

TEST_F(SafeBrowsingUrlCheckerTest,
       CheckUrl_RealTimeEnabledSafeBrowsingDisabled_Safe) {
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*real_time_lookup_enabled=*/true, /*can_check_safe_browsing_db=*/false);

  GURL url("https://example.test/");
  url_lookup_service_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_SAFE,
                                           /*should_complete_lookup=*/true);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  EXPECT_CALL(callback, Run(_, /*proceed=*/true, /*showed_interstitial=*/false,
                            /*did_perform_real_time_check=*/true,
                            /*did_check_allowlist=*/false));
  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(_, _, _, _, _))
      .Times(0);
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());

  task_environment_.RunUntilIdle();
}

TEST_F(SafeBrowsingUrlCheckerTest, CheckUrl_RealTimeEnabledRedirectUrlsSafe) {
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*real_time_lookup_enabled=*/true,
      /*can_check_safe_browsing_db=*/true);

  GURL origin_url("https://example.test/");
  database_manager_->SetAllowlistResultForUrl(origin_url, false);
  url_lookup_service_->SetThreatTypeForUrl(origin_url, SB_THREAT_TYPE_SAFE,
                                           /*should_complete_lookup=*/true);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      origin_callback;
  EXPECT_CALL(origin_callback,
              Run(_, /*proceed=*/true, /*showed_interstitial=*/false,
                  /*did_perform_real_time_check=*/true,
                  /*did_check_allowlist=*/true));
  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(_, _, _, _, _))
      .Times(0);
  safe_browsing_url_checker->CheckUrl(origin_url, "GET", origin_callback.Get());

  GURL redirect_url("https://example.redirect.test/");
  database_manager_->SetAllowlistResultForUrl(redirect_url, false);
  url_lookup_service_->SetThreatTypeForUrl(redirect_url, SB_THREAT_TYPE_SAFE,
                                           /*should_complete_lookup=*/true);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      redirect_callback;
  EXPECT_CALL(redirect_callback,
              Run(_, /*proceed=*/true, /*showed_interstitial=*/false,
                  /*did_perform_real_time_check=*/true,
                  /*did_check_allowlist=*/true));
  safe_browsing_url_checker->CheckUrl(redirect_url, "GET",
                                      redirect_callback.Get());

  task_environment_.RunUntilIdle();
}

TEST_F(SafeBrowsingUrlCheckerTest, CheckUrl_CancelCheckOnDestruct) {
  // Do not cancel check for real-time checks.
  {
    auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
        /*real_time_lookup_enabled=*/true, /*can_check_safe_browsing_db=*/true);

    GURL url("https://example.test/");
    database_manager_->SetAllowlistResultForUrl(url, false);
    url_lookup_service_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_URL_PHISHING,
                                             /*should_complete_lookup=*/true);

    base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback> cb;
    safe_browsing_url_checker->CheckUrl(url, "GET", cb.Get());
    EXPECT_FALSE(database_manager_->HasCalledCancelCheck());
    safe_browsing_url_checker.reset();
    EXPECT_FALSE(database_manager_->HasCalledCancelCheck());

    task_environment_.RunUntilIdle();
  }
  // Do cancel check for local checks.
  {
    auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
        /*real_time_lookup_enabled=*/false,
        /*can_check_safe_browsing_db=*/true);

    GURL url("https://example.test/");
    database_manager_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_URL_PHISHING,
                                           /*delayed_callback=*/false);

    base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback> cb;
    EXPECT_CALL(cb, Run(_, /*proceed=*/false, /*showed_interstitial=*/false,
                        /*did_perform_real_time_check=*/false,
                        /*did_check_allowlist=*/false));
    safe_browsing_url_checker->CheckUrl(url, "GET", cb.Get());
    EXPECT_FALSE(database_manager_->HasCalledCancelCheck());
    safe_browsing_url_checker.reset();
    EXPECT_TRUE(database_manager_->HasCalledCancelCheck());

    task_environment_.RunUntilIdle();
  }
}

TEST_F(SafeBrowsingUrlCheckerTest, CheckUrl_CancelCheckOnTimeout) {
  // Timeout for real-time checks should not cancel the database check.
  {
    base::HistogramTester histograms;
    auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
        /*real_time_lookup_enabled=*/true,
        /*can_check_safe_browsing_db=*/true);

    GURL url("https://example.test/");
    database_manager_->SetAllowlistResultForUrl(url, false);
    url_lookup_service_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_URL_PHISHING,
                                             /*should_complete_lookup=*/false);
    base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback> cb;
    EXPECT_CALL(cb, Run(_, /*proceed=*/true,
                        /*showed_interstitial=*/false,
                        /*did_perform_real_time_check=*/true,
                        /*did_check_allowlist=*/true));
    safe_browsing_url_checker->CheckUrl(url, "GET", cb.Get());
    EXPECT_FALSE(database_manager_->HasCalledCancelCheck());
    task_environment_.FastForwardBy(base::Seconds(5));
    EXPECT_FALSE(database_manager_->HasCalledCancelCheck());
    task_environment_.RunUntilIdle();
    histograms.ExpectUniqueSample("SafeBrowsing.CheckUrl.Timeout",
                                  /*sample=*/true,
                                  /*expected_bucket_count=*/1);
  }
  // Timeout for local checks should cancel the database check.
  {
    base::HistogramTester histograms;
    auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
        /*real_time_lookup_enabled=*/false,
        /*can_check_safe_browsing_db=*/true);

    GURL url("https://example.test/");
    database_manager_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_URL_PHISHING,
                                           /*delayed_callback=*/true);

    SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback callback =
        base::BindOnce(&SafeBrowsingUrlCheckerTest::
                           OnCheckUrlCallbackSettingSlowCheckNotifier,
                       base::Unretained(this));
    EXPECT_CALL(slow_check_notifier_callback_,
                Run(/*proceed=*/true, /*showed_interstitial=*/false,
                    /*did_perform_real_time_check=*/false,
                    /*did_check_allowlist=*/false));
    safe_browsing_url_checker->CheckUrl(url, "GET", std::move(callback));
    EXPECT_FALSE(database_manager_->HasCalledCancelCheck());
    task_environment_.FastForwardBy(base::Seconds(5));
    EXPECT_TRUE(database_manager_->HasCalledCancelCheck());
    task_environment_.RunUntilIdle();
    histograms.ExpectUniqueSample("SafeBrowsing.CheckUrl.Timeout",
                                  /*sample=*/true,
                                  /*expected_bucket_count=*/1);
  }
}

// Same as CheckUrl_SafeUrl but with the lookup mechanism experiment enabled.
TEST_F(SafeBrowsingUrlCheckerTest, CheckUrl_SafeUrl_LookupMechanismExperiment) {
  base::HistogramTester histograms;
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*real_time_lookup_enabled=*/false, /*can_check_safe_browsing_db=*/true,
      /*is_lookup_mechanism_experiment_enabled=*/true);

  GURL url("https://example.test/");
  database_manager_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_SAFE,
                                         /*delayed_callback=*/false);
  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  EXPECT_CALL(callback,
              Run(nullptr, /*proceed=*/true, /*showed_interstitial=*/false,
                  /*did_perform_real_time_check=*/false,
                  /*did_check_allowlist=*/false));
  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(_, _, _, _, _))
      .Times(0);

  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());
  task_environment_.RunUntilIdle();
  histograms.ExpectUniqueSample("SafeBrowsing.CheckUrl.Timeout",
                                /*sample=*/false,
                                /*expected_bucket_count=*/1);

  EXPECT_EQ(url_checker_delegate_->GetNumCheckExperimentEligibilityCalls(), 0);
  EXPECT_EQ(url_checker_delegate_
                ->GetNumCheckExperimentEligibilityAndStartBlockingPageCalls(),
            0);
  // Make sure the experiment ended with no logged results.
  histograms.ExpectTotalCount("SafeBrowsing.HPRTExperiment.WarningsResult", 0);
}

// Same as CheckUrl_RealTimeEnabledAllowlistMatch but with the lookup mechanism
// experiment enabled.
TEST_F(SafeBrowsingUrlCheckerTest,
       CheckUrl_RealTimeEnabledAllowlistMatch_LookupMechanismExperiment) {
  base::HistogramTester histograms;
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*real_time_lookup_enabled=*/true, /*can_check_safe_browsing_db=*/true,
      /*is_lookup_mechanism_experiment_enabled=*/true);

  GURL url("https://example.test/");
  database_manager_->SetAllowlistResultForUrl(url, true);
  // To make sure hash based check is not skipped when the URL is in the
  // allowlist, set threat type to phishing for hash based check.
  database_manager_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_URL_PHISHING,
                                         /*delayed_callback=*/false);
  std::set<MechanismExperimentHashDatabaseCache> cache_selections = {
      MechanismExperimentHashDatabaseCache::kUrlRealTimeOnly,
      MechanismExperimentHashDatabaseCache::kHashRealTimeOnly,
      MechanismExperimentHashDatabaseCache::kHashDatabaseOnly,
  };
  database_manager_->SetAcceptableExperimentCacheSelections(cache_selections);
  url_checker_delegate_->SetLookupMechanismExperimentEligibility(
      url, /*eligibility=*/true);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  // Note that the callback is not called, because resource fetch is not blocked
  // while we perform a real time URL check.
  EXPECT_CALL(callback, Run(_, _, _, _, _)).Times(0);
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());
  task_environment_.RunUntilIdle();

  EXPECT_EQ(url_checker_delegate_->GetNumCheckExperimentEligibilityCalls(), 1);
  EXPECT_EQ(url_checker_delegate_
                ->GetNumCheckExperimentEligibilityAndStartBlockingPageCalls(),
            1);
  // Make sure the experiment ended and logged results.
  histograms.ExpectTotalCount("SafeBrowsing.HPRTExperiment.WarningsResult", 1);
}

// Same as CheckUrl_RealTimeEnabledAllowlistMatch_LookupMechanismExperiment but
// the check is not eligible for the experiment so it should log no results.
TEST_F(
    SafeBrowsingUrlCheckerTest,
    CheckUrl_RealTimeEnabledAllowlistMatch_LookupMechanismExperimentNoEligibility) {
  base::HistogramTester histograms;
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*real_time_lookup_enabled=*/true, /*can_check_safe_browsing_db=*/true,
      /*is_lookup_mechanism_experiment_enabled=*/true);

  GURL url("https://example.test/");
  database_manager_->SetAllowlistResultForUrl(url, true);
  // To make sure hash based check is not skipped when the URL is in the
  // allowlist, set threat type to phishing for hash based check.
  database_manager_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_URL_PHISHING,
                                         /*delayed_callback=*/false);
  std::set<MechanismExperimentHashDatabaseCache> cache_selections = {
      MechanismExperimentHashDatabaseCache::kUrlRealTimeOnly,
      MechanismExperimentHashDatabaseCache::kHashRealTimeOnly,
      MechanismExperimentHashDatabaseCache::kHashDatabaseOnly,
  };
  database_manager_->SetAcceptableExperimentCacheSelections(cache_selections);
  url_checker_delegate_->SetLookupMechanismExperimentEligibility(
      url, /*eligibility=*/false);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  // Note that the callback is not called, because resource fetch is not blocked
  // while we perform a real time URL check.
  EXPECT_CALL(callback, Run(_, _, _, _, _)).Times(0);
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());

  task_environment_.RunUntilIdle();

  EXPECT_EQ(url_checker_delegate_->GetNumCheckExperimentEligibilityCalls(), 1);
  EXPECT_EQ(url_checker_delegate_
                ->GetNumCheckExperimentEligibilityAndStartBlockingPageCalls(),
            1);
  // Make sure the experiment did not log results.
  histograms.ExpectTotalCount("SafeBrowsing.HPRTExperiment.WarningsResult", 0);
}

// Same as CheckUrl_RealTimeEnabledSafeUrl but with the lookup mechanism
// experiment enabled.
TEST_F(SafeBrowsingUrlCheckerTest,
       CheckUrl_RealTimeEnabledSafeUrl_LookupMechanismExperiment) {
  base::HistogramTester histograms;
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*real_time_lookup_enabled=*/true, /*can_check_safe_browsing_db=*/true,
      /*is_lookup_mechanism_experiment_enabled=*/true);

  GURL url("https://example.test/");
  database_manager_->SetAllowlistResultForUrl(url, false);
  url_lookup_service_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_SAFE,
                                           /*should_complete_lookup=*/true);
  database_manager_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_URL_PHISHING,
                                         /*delayed_callback=*/false);
  std::set<MechanismExperimentHashDatabaseCache> cache_selections = {
      MechanismExperimentHashDatabaseCache::kHashDatabaseOnly};
  database_manager_->SetAcceptableExperimentCacheSelections(cache_selections);
  hash_realtime_service_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_URL_PHISHING,
                                              /*should_fail_lookup=*/false);
  url_checker_delegate_->SetLookupMechanismExperimentEligibility(
      url, /*eligibility=*/true);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  EXPECT_CALL(callback, Run(_, /*proceed=*/true, /*showed_interstitial=*/false,
                            /*did_perform_real_time_check=*/true,
                            /*did_check_allowlist=*/true));
  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(_, _, _, _, _))
      .Times(0);
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());

  task_environment_.RunUntilIdle();
  histograms.ExpectUniqueSample("SafeBrowsing.CheckUrl.Timeout",
                                /*sample=*/false,
                                /*expected_bucket_count=*/1);

  // The false positive metric should not be logged, because the
  // verdict is not from cache.
  histograms.ExpectTotalCount("SafeBrowsing.RT.GetCache.FallbackThreatType",
                              /*expected_count=*/0);

  EXPECT_EQ(url_checker_delegate_->GetNumCheckExperimentEligibilityCalls(), 1);
  EXPECT_EQ(url_checker_delegate_
                ->GetNumCheckExperimentEligibilityAndStartBlockingPageCalls(),
            0);
  // Make sure the experiment ended and logged results.
  histograms.ExpectTotalCount("SafeBrowsing.HPRTExperiment.WarningsResult", 1);
}

// Same as CheckUrl_RealTimeEnabledRedirectUrlsSafe but with the lookup
// mechanism experiment enabled.
TEST_F(SafeBrowsingUrlCheckerTest,
       CheckUrl_RealTimeEnabledRedirectUrlsSafe_LookupMechanismExperiment) {
  base::HistogramTester histograms;
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*real_time_lookup_enabled=*/true,
      /*can_check_safe_browsing_db=*/true,
      /*is_lookup_mechanism_experiment_enabled=*/true);

  std::set<MechanismExperimentHashDatabaseCache> cache_selections = {
      MechanismExperimentHashDatabaseCache::kHashDatabaseOnly};
  database_manager_->SetAcceptableExperimentCacheSelections(cache_selections);

  GURL origin_url("https://example.test/");
  // Sanity check only the URL real-time result is used by setting the other
  // mechanism responses to phishing.
  database_manager_->SetThreatTypeForUrl(origin_url,
                                         SB_THREAT_TYPE_URL_PHISHING,
                                         /*delayed_callback=*/false);
  url_lookup_service_->SetThreatTypeForUrl(origin_url, SB_THREAT_TYPE_SAFE,
                                           /*should_complete_lookup=*/true);
  hash_realtime_service_->SetThreatTypeForUrl(origin_url,
                                              SB_THREAT_TYPE_URL_PHISHING,
                                              /*should_fail_lookup=*/false);
  database_manager_->SetAllowlistResultForUrl(origin_url, false);
  url_checker_delegate_->SetLookupMechanismExperimentEligibility(
      origin_url, /*eligibility=*/true);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      origin_callback;
  EXPECT_CALL(origin_callback,
              Run(_, /*proceed=*/true, /*showed_interstitial=*/false,
                  /*did_perform_real_time_check=*/true,
                  /*did_check_allowlist=*/true));
  safe_browsing_url_checker->CheckUrl(origin_url, "GET", origin_callback.Get());

  GURL redirect_url("https://example.redirect.test/");
  // Sanity check only the URL real-time result is used by setting the other
  // mechanism responses to phishing.
  database_manager_->SetThreatTypeForUrl(redirect_url,
                                         SB_THREAT_TYPE_URL_PHISHING,
                                         /*delayed_callback=*/false);
  url_lookup_service_->SetThreatTypeForUrl(redirect_url, SB_THREAT_TYPE_SAFE,
                                           /*should_complete_lookup=*/true);
  hash_realtime_service_->SetThreatTypeForUrl(redirect_url,
                                              SB_THREAT_TYPE_URL_PHISHING,
                                              /*should_fail_lookup=*/false);
  database_manager_->SetAllowlistResultForUrl(redirect_url, false);
  url_checker_delegate_->SetLookupMechanismExperimentEligibility(
      redirect_url, /*eligibility=*/true);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      redirect_callback;
  EXPECT_CALL(redirect_callback,
              Run(_, /*proceed=*/true, /*showed_interstitial=*/false,
                  /*did_perform_real_time_check=*/true,
                  /*did_check_allowlist=*/true));
  safe_browsing_url_checker->CheckUrl(redirect_url, "GET",
                                      redirect_callback.Get());

  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(_, _, _, _, _))
      .Times(0);

  task_environment_.RunUntilIdle();

  EXPECT_EQ(url_checker_delegate_->GetNumCheckExperimentEligibilityCalls(), 2);
  EXPECT_EQ(url_checker_delegate_
                ->GetNumCheckExperimentEligibilityAndStartBlockingPageCalls(),
            0);
  // Make sure the experiment ended and logged results.
  histograms.ExpectTotalCount(
      "SafeBrowsing.HPRTExperiment.Redirects.WarningsResult", 1);
}

}  // namespace safe_browsing
