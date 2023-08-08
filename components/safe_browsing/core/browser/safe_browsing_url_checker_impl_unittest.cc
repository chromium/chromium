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
#include "components/safe_browsing/core/browser/hashprefix_realtime/hash_realtime_utils.h"
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

constexpr char kAllowlistedUrl[] = "https://allowlisted.url/";

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
  bool CheckBrowseUrl(
      const GURL& gurl,
      const safe_browsing::SBThreatTypeSet& threat_types,
      Client* client,
      MechanismExperimentHashDatabaseCache experiment_cache_selection,
      CheckBrowseUrlType check_type) override {
    std::string url = gurl.spec();
    DCHECK(base::Contains(urls_threat_type_, url));
    DCHECK(base::Contains(urls_delayed_callback_, url));
    EXPECT_TRUE(base::Contains(acceptable_cache_selections_,
                               experiment_cache_selection));
    EXPECT_EQ(check_type, expected_check_type_);
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
    // Chosen to match specific test case CheckUrl_InvalidRequestDestination.
    return request_destination != network::mojom::RequestDestination::kAudio;
  }

  bool ChecksAreAlwaysAsync() const override { return false; }

  ThreatSource GetThreatSource() const override {
    return ThreatSource::UNKNOWN;
  }

  // Returns the allowlist match result previously set by
  // |SetAllowlistLookupDetailsForUrl|. It also checks whether the
  // |metric_variation| parameter passed through is an expected value. It
  // crashes if either of the allowlist match result or the allowed metric
  // variations are not set in advance for the |gurl|.
  void CheckUrlForHighConfidenceAllowlist(
      const GURL& gurl,
      const std::string& metric_variation,
      base::OnceCallback<void(bool)> callback) override {
    std::string url = gurl.spec();
    DCHECK(base::Contains(urls_allowlist_match_, url));
    DCHECK(base::Contains(urls_allowlist_metric_variation_, url));
    EXPECT_TRUE(base::Contains(urls_allowlist_metric_variation_[url],
                               metric_variation));
    sb_task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), urls_allowlist_match_[url]));
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

  void SetAllowlistLookupDetailsForUrl(
      const GURL& gurl,
      bool match,
      const std::set<std::string>& metric_variations) {
    std::string url = gurl.spec();
    urls_allowlist_match_[url] = match;
    urls_allowlist_metric_variation_[url] = metric_variations;
  }

  void SetAcceptableExperimentCacheSelections(
      std::set<MechanismExperimentHashDatabaseCache>
          acceptable_cache_selections) {
    acceptable_cache_selections_ = acceptable_cache_selections;
  }

  void SetExpectedCheckBrowseUrlType(CheckBrowseUrlType check_type) {
    expected_check_type_ = check_type;
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
  base::flat_map<std::string, std::set<std::string>>
      urls_allowlist_metric_variation_;
  std::set<MechanismExperimentHashDatabaseCache> acceptable_cache_selections_ =
      {MechanismExperimentHashDatabaseCache::kNoExperiment};
  CheckBrowseUrlType expected_check_type_ = CheckBrowseUrlType::kHashDatabase;

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

  bool IsUrlAllowlisted(const GURL& url) override {
    return url.spec() == kAllowlistedUrl;
  }
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
            /*referrer_chain_provider=*/nullptr,
            /*pref_service=*/nullptr) {}
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
            /*get_is_enhanced_protection_enabled=*/base::NullCallback(),
            /*webui_delegate=*/nullptr) {}
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
      bool is_source_lookup_mechanism_experiment,
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

struct CreateSafeBrowsingUrlCheckerOptionalArgs {
  network::mojom::RequestDestination request_destination =
      network::mojom::RequestDestination::kDocument;
  bool can_urt_check_subresource_url = false;
  std::string url_lookup_service_metric_suffix = ".Enterprise";
  bool is_lookup_mechanism_experiment_enabled = false;
};
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
      bool url_real_time_lookup_enabled,
      bool can_check_safe_browsing_db,
      hash_realtime_utils::HashRealTimeSelection hash_real_time_selection,
      CreateSafeBrowsingUrlCheckerOptionalArgs optional_args =
          CreateSafeBrowsingUrlCheckerOptionalArgs()) {
    base::MockCallback<base::RepeatingCallback<content::WebContents*()>>
        mock_web_contents_getter;
    scoped_refptr<SafeBrowsingLookupMechanismExperimenter>
        mechanism_experimenter = nullptr;
    if (optional_args.is_lookup_mechanism_experiment_enabled) {
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
        optional_args.request_destination,
        /*has_user_gesture=*/false, url_checker_delegate_,
        mock_web_contents_getter.Get(), UnsafeResource::kNoRenderProcessId,
        UnsafeResource::kNoRenderFrameId, UnsafeResource::kNoFrameTreeNodeId,
        url_real_time_lookup_enabled,
        optional_args.can_urt_check_subresource_url, can_check_safe_browsing_db,
        /*can_check_high_confidence_allowlist=*/true,
        /*url_lookup_service_metric_suffix=*/
        optional_args.url_lookup_service_metric_suffix,
        /*last_committed_url=*/GURL(),
        base::SequencedTaskRunner::GetCurrentDefault(),
        url_real_time_lookup_enabled ? url_lookup_service_->GetWeakPtr()
                                     : nullptr,
        /*webui_delegate_=*/nullptr,
        /*hash_realtime_service=*/hash_realtime_service_->GetWeakPtr(),
        /*mechanism_experimenter=*/mechanism_experimenter,
        optional_args.is_lookup_mechanism_experiment_enabled,
        hash_real_time_selection);
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
      SafeBrowsingUrlCheckerImpl::PerformedCheck performed_check,
      bool did_check_url_real_time_allowlist) {
    *slow_check_notifier = slow_check_notifier_callback_.Get();
  }

 protected:
  void CheckHashRealTimeMetrics(
      absl::optional<bool> expected_local_match_result,
      absl::optional<bool> expected_is_service_found) {
    if (!expected_local_match_result.has_value()) {
      histogram_tester_.ExpectTotalCount(
          /*name=*/"SafeBrowsing.HPRT.LocalMatch.Result", /*expected_count=*/0);
    } else {
      histogram_tester_.ExpectUniqueSample(
          /*name=*/"SafeBrowsing.HPRT.LocalMatch.Result",
          /*sample=*/expected_local_match_result.value() ? AsyncMatch::MATCH
                                                         : AsyncMatch::NO_MATCH,
          /*expected_bucket_count=*/1);
    }
    if (!expected_is_service_found.has_value()) {
      histogram_tester_.ExpectTotalCount(
          /*name=*/"SafeBrowsing.HPRT.IsLookupServiceFound",
          /*expected_count=*/0);
    } else {
      histogram_tester_.ExpectUniqueSample(
          /*name=*/"SafeBrowsing.HPRT.IsLookupServiceFound",
          /*sample=*/expected_is_service_found.value(),
          /*expected_bucket_count=*/1);
    }
  }
  void CheckUrlRealTimeLocalMatchMetrics(
      absl::optional<bool> expected_local_match_result,
      absl::optional<bool> expected_mainframe_log,
      absl::optional<bool> expect_url_lookup_service_metric_suffix) {
    ASSERT_EQ(expected_local_match_result.has_value(),
              expected_mainframe_log.has_value());
    ASSERT_EQ(expected_local_match_result.has_value(),
              expect_url_lookup_service_metric_suffix.has_value());
    if (!expected_local_match_result.has_value()) {
      histogram_tester_.ExpectTotalCount(
          /*name=*/"SafeBrowsing.RT.LocalMatch.Result", /*expected_count=*/0);
      histogram_tester_.ExpectTotalCount(
          /*name=*/"SafeBrowsing.RT.LocalMatch.Result.Mainframe",
          /*expected_count=*/0);
      histogram_tester_.ExpectTotalCount(
          /*name=*/"SafeBrowsing.RT.LocalMatch.Result.NonMainframe",
          /*expected_count=*/0);
    } else {
      AsyncMatch expected_local_match_result_value =
          expected_local_match_result.value() ? AsyncMatch::MATCH
                                              : AsyncMatch::NO_MATCH;
      histogram_tester_.ExpectUniqueSample(
          /*name=*/"SafeBrowsing.RT.LocalMatch.Result",
          /*sample=*/expected_local_match_result_value,
          /*expected_bucket_count=*/1);
      std::string expected_base_histogram =
          expected_mainframe_log.value()
              ? "SafeBrowsing.RT.LocalMatch.Result.Mainframe"
              : "SafeBrowsing.RT.LocalMatch.Result.NonMainframe";
      histogram_tester_.ExpectUniqueSample(
          /*name=*/expected_base_histogram,
          /*sample=*/expected_local_match_result_value,
          /*expected_bucket_count=*/1);
      histogram_tester_.ExpectUniqueSample(
          /*name=*/expected_base_histogram + ".Enterprise",
          /*sample=*/expected_local_match_result_value,
          /*expected_bucket_count=*/
          expect_url_lookup_service_metric_suffix.value() ? 1 : 0);
    }
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  scoped_refptr<MockSafeBrowsingDatabaseManager> database_manager_;
  scoped_refptr<MockUrlCheckerDelegate> url_checker_delegate_;
  std::unique_ptr<MockRealTimeUrlLookupService> url_lookup_service_;
  std::unique_ptr<MockHashRealTimeService> hash_realtime_service_;
  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeUrlCheckNotifier>
      slow_check_notifier_callback_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_tester_;
};

TEST_F(SafeBrowsingUrlCheckerTest, CheckUrl_SafeUrl) {
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*url_real_time_lookup_enabled=*/false,
      /*can_check_safe_browsing_db=*/true,
      /*hash_real_time_selection=*/
      hash_realtime_utils::HashRealTimeSelection::kNone);

  GURL url("https://example.test/");
  database_manager_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_SAFE,
                                         /*delayed_callback=*/false);
  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  EXPECT_CALL(
      callback,
      Run(nullptr, /*proceed=*/true, /*showed_interstitial=*/false,
          SafeBrowsingUrlCheckerImpl::PerformedCheck::kHashDatabaseCheck,
          /*did_check_url_real_time_allowlist=*/false));
  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(_, _, _, _, _))
      .Times(0);

  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());
  task_environment_.RunUntilIdle();
  histogram_tester_.ExpectUniqueSample("SafeBrowsing.CheckUrl.Timeout",
                                       /*sample=*/false,
                                       /*expected_bucket_count=*/1);
}

TEST_F(SafeBrowsingUrlCheckerTest, CheckUrl_DangerousUrl) {
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*url_real_time_lookup_enabled=*/false,
      /*can_check_safe_browsing_db=*/true,
      /*hash_real_time_selection=*/
      hash_realtime_utils::HashRealTimeSelection::kNone);

  GURL url("https://example.test/");
  database_manager_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_URL_PHISHING,
                                         /*delayed_callback=*/false);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  EXPECT_CALL(
      callback,
      Run(_, /*proceed=*/false, /*showed_interstitial=*/false,
          SafeBrowsingUrlCheckerImpl::PerformedCheck::kHashDatabaseCheck,
          /*did_check_url_real_time_allowlist=*/false));
  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(
                  IsSameThreatSource(ThreatSource::UNKNOWN), _, _, _, _))
      .Times(1);
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());
  task_environment_.RunUntilIdle();
}

TEST_F(SafeBrowsingUrlCheckerTest, CheckUrl_RedirectUrlsSafe) {
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*url_real_time_lookup_enabled=*/false,
      /*can_check_safe_browsing_db=*/true,
      /*hash_real_time_selection=*/
      hash_realtime_utils::HashRealTimeSelection::kNone);

  GURL origin_url("https://example.test/");
  database_manager_->SetThreatTypeForUrl(origin_url, SB_THREAT_TYPE_SAFE,
                                         /*delayed_callback=*/false);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      origin_callback;
  EXPECT_CALL(
      origin_callback,
      Run(_, /*proceed=*/true, /*showed_interstitial=*/false,
          SafeBrowsingUrlCheckerImpl::PerformedCheck::kHashDatabaseCheck,
          /*did_check_url_real_time_allowlist=*/false));
  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(_, _, _, _, _))
      .Times(0);
  safe_browsing_url_checker->CheckUrl(origin_url, "GET", origin_callback.Get());

  GURL redirect_url("https://example.redirect.test/");
  database_manager_->SetThreatTypeForUrl(redirect_url, SB_THREAT_TYPE_SAFE,
                                         /*delayed_callback=*/false);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      redirect_callback;
  EXPECT_CALL(
      redirect_callback,
      Run(_, /*proceed=*/true, /*showed_interstitial=*/false,
          SafeBrowsingUrlCheckerImpl::PerformedCheck::kHashDatabaseCheck,
          /*did_check_url_real_time_allowlist=*/false));
  safe_browsing_url_checker->CheckUrl(redirect_url, "GET",
                                      redirect_callback.Get());

  task_environment_.RunUntilIdle();
}

TEST_F(SafeBrowsingUrlCheckerTest,
       CheckUrl_RedirectUrlsOriginDangerousRedirectSafe) {
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*url_real_time_lookup_enabled=*/false,
      /*can_check_safe_browsing_db=*/true,
      /*hash_real_time_selection=*/
      hash_realtime_utils::HashRealTimeSelection::kNone);

  GURL origin_url("https://example.test/");
  database_manager_->SetThreatTypeForUrl(
      origin_url, SB_THREAT_TYPE_URL_PHISHING, /*delayed_callback=*/true);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      origin_callback;
  EXPECT_CALL(
      origin_callback,
      Run(_, /*proceed=*/false, /*showed_interstitial=*/false,
          SafeBrowsingUrlCheckerImpl::PerformedCheck::kHashDatabaseCheck,
          /*did_check_url_real_time_allowlist=*/false));
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

TEST_F(SafeBrowsingUrlCheckerTest, CheckUrl_UrlRealTimeEnabledAllowlistMatch) {
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*url_real_time_lookup_enabled=*/true,
      /*can_check_safe_browsing_db=*/true,
      /*hash_real_time_selection=*/
      hash_realtime_utils::HashRealTimeSelection::kNone);

  GURL url("https://example.test/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url, /*match=*/true,
                                                     {"RT"});
  // To make sure hash based check is not skipped when the URL is in the
  // allowlist, set threat type to phishing for hash based check.
  database_manager_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_URL_PHISHING,
                                         /*delayed_callback=*/false);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  // Note that the callback is not called, because resource fetch is not blocked
  // while we perform a real-time check.
  EXPECT_CALL(callback, Run(_, _, _, _, _)).Times(0);
  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(
                  IsSameThreatSource(ThreatSource::UNKNOWN), _, _, _, _))
      .Times(1);
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());

  task_environment_.RunUntilIdle();
  CheckUrlRealTimeLocalMatchMetrics(
      /*expected_local_match_result=*/true, /*expected_mainframe_log=*/true,
      /*expect_url_lookup_service_metric_suffix=*/true);
}

TEST_F(SafeBrowsingUrlCheckerTest, CheckUrl_UrlRealTimeEnabledSafeUrl) {
  base::HistogramTester histograms;
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*url_real_time_lookup_enabled=*/true,
      /*can_check_safe_browsing_db=*/true,
      /*hash_real_time_selection=*/
      hash_realtime_utils::HashRealTimeSelection::kNone);

  GURL url("https://example.test/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url, /*match=*/false,
                                                     {"RT"});
  url_lookup_service_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_SAFE,
                                           /*should_complete_lookup=*/true);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  EXPECT_CALL(callback,
              Run(_, /*proceed=*/true, /*showed_interstitial=*/false,
                  SafeBrowsingUrlCheckerImpl::PerformedCheck::kUrlRealTimeCheck,
                  /*did_check_url_real_time_allowlist=*/true));
  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(_, _, _, _, _))
      .Times(0);
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());

  task_environment_.RunUntilIdle();
  histogram_tester_.ExpectUniqueSample("SafeBrowsing.CheckUrl.Timeout",
                                       /*sample=*/false,
                                       /*expected_bucket_count=*/1);

  // The false positive metric should not be logged, because the
  // verdict is not from cache.
  histogram_tester_.ExpectTotalCount(
      "SafeBrowsing.RT.GetCache.FallbackThreatType",
      /*expected_count=*/0);
  CheckUrlRealTimeLocalMatchMetrics(
      /*expected_local_match_result=*/false, /*expected_mainframe_log=*/true,
      /*expect_url_lookup_service_metric_suffix=*/true);
}

TEST_F(SafeBrowsingUrlCheckerTest,
       CheckUrl_UrlRealTimeEnabledSafeUrl_NonMainframe) {
  base::HistogramTester histograms;
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*url_real_time_lookup_enabled=*/true,
      /*can_check_safe_browsing_db=*/true,
      /*hash_real_time_selection=*/
      hash_realtime_utils::HashRealTimeSelection::kNone,
      /*optional_args=*/
      {.request_destination = network::mojom::RequestDestination::kFrame,
       .can_urt_check_subresource_url = true});

  GURL url("https://example.test/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url, /*match=*/false,
                                                     {"RT"});
  url_lookup_service_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_SAFE,
                                           /*should_complete_lookup=*/true);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  EXPECT_CALL(callback,
              Run(_, /*proceed=*/true, /*showed_interstitial=*/false,
                  SafeBrowsingUrlCheckerImpl::PerformedCheck::kUrlRealTimeCheck,
                  /*did_check_url_real_time_allowlist=*/true));
  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(_, _, _, _, _))
      .Times(0);
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());

  task_environment_.RunUntilIdle();
  histogram_tester_.ExpectUniqueSample("SafeBrowsing.CheckUrl.Timeout",
                                       /*sample=*/false,
                                       /*expected_bucket_count=*/1);

  CheckUrlRealTimeLocalMatchMetrics(
      /*expected_local_match_result=*/false, /*expected_mainframe_log=*/false,
      /*expect_url_lookup_service_metric_suffix=*/true);
}

TEST_F(SafeBrowsingUrlCheckerTest,
       CheckUrl_UrlRealTimeEnabledSafeUrl_EmptyUrlLookupServiceMetricSuffix) {
  base::HistogramTester histograms;
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*url_real_time_lookup_enabled=*/true,
      /*can_check_safe_browsing_db=*/true,
      /*hash_real_time_selection=*/
      hash_realtime_utils::HashRealTimeSelection::kNone,
      /*optional_args=*/{.url_lookup_service_metric_suffix = ""});

  GURL url("https://example.test/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url, /*match=*/false,
                                                     {"RT"});
  url_lookup_service_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_SAFE,
                                           /*should_complete_lookup=*/true);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  EXPECT_CALL(callback,
              Run(_, /*proceed=*/true, /*showed_interstitial=*/false,
                  SafeBrowsingUrlCheckerImpl::PerformedCheck::kUrlRealTimeCheck,
                  /*did_check_url_real_time_allowlist=*/true));
  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(_, _, _, _, _))
      .Times(0);
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());

  task_environment_.RunUntilIdle();
  histogram_tester_.ExpectUniqueSample("SafeBrowsing.CheckUrl.Timeout",
                                       /*sample=*/false,
                                       /*expected_bucket_count=*/1);

  CheckUrlRealTimeLocalMatchMetrics(
      /*expected_local_match_result=*/false, /*expected_mainframe_log=*/true,
      /*expect_url_lookup_service_metric_suffix=*/false);
}

TEST_F(SafeBrowsingUrlCheckerTest,
       CheckUrl_UrlRealTimeEnabledSafeUrlFromCache) {
  base::HistogramTester histograms;
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*url_real_time_lookup_enabled=*/true,
      /*can_check_safe_browsing_db=*/true,
      /*hash_real_time_selection=*/
      hash_realtime_utils::HashRealTimeSelection::kNone);

  GURL url("https://example.test/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url, /*match=*/false,
                                                     {"RT"});
  database_manager_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_SAFE,
                                         /*delayed_callback=*/false);
  url_lookup_service_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_SAFE,
                                           /*should_complete_lookup=*/true);
  url_lookup_service_->SetIsCachedResponse(true);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  EXPECT_CALL(callback,
              Run(_, /*proceed=*/true, /*showed_interstitial=*/false,
                  SafeBrowsingUrlCheckerImpl::PerformedCheck::kUrlRealTimeCheck,
                  /*did_check_url_real_time_allowlist=*/true));
  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(_, _, _, _, _))
      .Times(0);
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());

  task_environment_.RunUntilIdle();

  histogram_tester_.ExpectUniqueSample(
      "SafeBrowsing.RT.GetCache.FallbackThreatType",
      /*sample=*/SB_THREAT_TYPE_SAFE,
      /*expected_bucket_count=*/1);
  CheckUrlRealTimeLocalMatchMetrics(
      /*expected_local_match_result=*/false, /*expected_mainframe_log=*/true,
      /*expect_url_lookup_service_metric_suffix=*/true);
}

TEST_F(SafeBrowsingUrlCheckerTest,
       CheckUrl_UrlRealTimeEnabledSafeUrlFromCacheFalsePositive) {
  base::HistogramTester histograms;
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*url_real_time_lookup_enabled=*/true,
      /*can_check_safe_browsing_db=*/true,
      /*hash_real_time_selection=*/
      hash_realtime_utils::HashRealTimeSelection::kNone);

  GURL url("https://example.test/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url, /*match=*/false,
                                                     {"RT"});
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

  histogram_tester_.ExpectUniqueSample(
      "SafeBrowsing.RT.GetCache.FallbackThreatType",
      /*sample=*/SB_THREAT_TYPE_URL_PHISHING,
      /*expected_bucket_count=*/1);
  CheckUrlRealTimeLocalMatchMetrics(
      /*expected_local_match_result=*/false, /*expected_mainframe_log=*/true,
      /*expect_url_lookup_service_metric_suffix=*/true);
}

TEST_F(SafeBrowsingUrlCheckerTest,
       CheckUrl_UrlRealTimeEnabledSafeBrowsingDisabled_ManagedWarn) {
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*url_real_time_lookup_enabled=*/true,
      /*can_check_safe_browsing_db=*/false,
      /*hash_real_time_selection=*/
      hash_realtime_utils::HashRealTimeSelection::kNone);

  GURL url("https://example.test/");
  url_lookup_service_->SetThreatTypeForUrl(
      url, SB_THREAT_TYPE_MANAGED_POLICY_WARN, /*should_complete_lookup=*/true);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  // Should still show warning page because real time URL lookup is enabled.
  EXPECT_CALL(
      *url_checker_delegate_,
      StartDisplayingBlockingPageHelper(
          IsSameThreatSource(ThreatSource::URL_REAL_TIME_CHECK), _, _, _, _))
      .Times(1);
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());

  task_environment_.RunUntilIdle();
  CheckUrlRealTimeLocalMatchMetrics(
      /*expected_local_match_result=*/false, /*expected_mainframe_log=*/true,
      /*expect_url_lookup_service_metric_suffix=*/true);
}

TEST_F(SafeBrowsingUrlCheckerTest,
       CheckUrl_UrlRealTimeEnabledSafeBrowsingDisabled_ManagedBlock) {
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*url_real_time_lookup_enabled=*/true,
      /*can_check_safe_browsing_db=*/false,
      /*hash_real_time_selection=*/
      hash_realtime_utils::HashRealTimeSelection::kNone);

  GURL url("https://example.test/");
  url_lookup_service_->SetThreatTypeForUrl(url,
                                           SB_THREAT_TYPE_MANAGED_POLICY_BLOCK,
                                           /*should_complete_lookup=*/true);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  // Should still show blocking page because real time URL lookup is enabled.
  EXPECT_CALL(
      *url_checker_delegate_,
      StartDisplayingBlockingPageHelper(
          IsSameThreatSource(ThreatSource::URL_REAL_TIME_CHECK), _, _, _, _))
      .Times(1);
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());

  task_environment_.RunUntilIdle();
  CheckUrlRealTimeLocalMatchMetrics(
      /*expected_local_match_result=*/false, /*expected_mainframe_log=*/true,
      /*expect_url_lookup_service_metric_suffix=*/true);
}

TEST_F(SafeBrowsingUrlCheckerTest,
       CheckUrl_UrlRealTimeEnabledSafeBrowsingDisabled_Dangerous) {
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*url_real_time_lookup_enabled=*/true,
      /*can_check_safe_browsing_db=*/false,
      /*hash_real_time_selection=*/
      hash_realtime_utils::HashRealTimeSelection::kNone);

  GURL url("https://example.test/");
  url_lookup_service_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_URL_PHISHING,
                                           /*should_complete_lookup=*/true);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  // Should still show blocking page because real time URL lookup is enabled.
  EXPECT_CALL(
      *url_checker_delegate_,
      StartDisplayingBlockingPageHelper(
          IsSameThreatSource(ThreatSource::URL_REAL_TIME_CHECK), _, _, _, _))
      .Times(1);
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());

  task_environment_.RunUntilIdle();
  CheckUrlRealTimeLocalMatchMetrics(
      /*expected_local_match_result=*/false, /*expected_mainframe_log=*/true,
      /*expect_url_lookup_service_metric_suffix=*/true);
}

TEST_F(SafeBrowsingUrlCheckerTest,
       CheckUrl_UrlRealTimeEnabledSafeBrowsingDisabled_Safe) {
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*url_real_time_lookup_enabled=*/true,
      /*can_check_safe_browsing_db=*/false,
      /*hash_real_time_selection=*/
      hash_realtime_utils::HashRealTimeSelection::kNone);

  GURL url("https://example.test/");
  url_lookup_service_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_SAFE,
                                           /*should_complete_lookup=*/true);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  EXPECT_CALL(callback,
              Run(_, /*proceed=*/true, /*showed_interstitial=*/false,
                  SafeBrowsingUrlCheckerImpl::PerformedCheck::kUrlRealTimeCheck,
                  /*did_check_url_real_time_allowlist=*/false));
  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(_, _, _, _, _))
      .Times(0);
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());

  task_environment_.RunUntilIdle();
  CheckUrlRealTimeLocalMatchMetrics(
      /*expected_local_match_result=*/false, /*expected_mainframe_log=*/true,
      /*expect_url_lookup_service_metric_suffix=*/true);
}

TEST_F(SafeBrowsingUrlCheckerTest,
       CheckUrl_UrlRealTimeEnabledSafeBrowsingDisabled_Subresource) {
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*url_real_time_lookup_enabled=*/true,
      /*can_check_safe_browsing_db=*/false,
      /*hash_real_time_selection=*/
      hash_realtime_utils::HashRealTimeSelection::kNone,
      /*optional_args=*/
      {.request_destination = network::mojom::RequestDestination::kScript});

  GURL url("https://example.test/");

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  EXPECT_CALL(callback,
              Run(_, /*proceed=*/true, /*showed_interstitial=*/false,
                  SafeBrowsingUrlCheckerImpl::PerformedCheck::kCheckSkipped,
                  /*did_check_url_real_time_allowlist=*/false));
  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(_, _, _, _, _))
      .Times(0);
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());

  task_environment_.RunUntilIdle();
  CheckUrlRealTimeLocalMatchMetrics(
      /*expected_local_match_result=*/absl::nullopt,
      /*expected_mainframe_log=*/absl::nullopt,
      /*expect_url_lookup_service_metric_suffix=*/absl::nullopt);
}

TEST_F(SafeBrowsingUrlCheckerTest,
       CheckUrl_UrlRealTimeEnabledRedirectUrlsSafe) {
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*url_real_time_lookup_enabled=*/true,
      /*can_check_safe_browsing_db=*/true,
      /*hash_real_time_selection=*/
      hash_realtime_utils::HashRealTimeSelection::kNone);

  GURL origin_url("https://example.test/");
  database_manager_->SetAllowlistLookupDetailsForUrl(origin_url,
                                                     /*match=*/false, {"RT"});
  url_lookup_service_->SetThreatTypeForUrl(origin_url, SB_THREAT_TYPE_SAFE,
                                           /*should_complete_lookup=*/true);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      origin_callback;
  EXPECT_CALL(origin_callback,
              Run(_, /*proceed=*/true, /*showed_interstitial=*/false,
                  SafeBrowsingUrlCheckerImpl::PerformedCheck::kUrlRealTimeCheck,
                  /*did_check_url_real_time_allowlist=*/true));
  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(_, _, _, _, _))
      .Times(0);
  safe_browsing_url_checker->CheckUrl(origin_url, "GET", origin_callback.Get());

  GURL redirect_url("https://example.redirect.test/");
  database_manager_->SetAllowlistLookupDetailsForUrl(redirect_url,
                                                     /*match=*/false, {"RT"});
  url_lookup_service_->SetThreatTypeForUrl(redirect_url, SB_THREAT_TYPE_SAFE,
                                           /*should_complete_lookup=*/true);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      redirect_callback;
  EXPECT_CALL(redirect_callback,
              Run(_, /*proceed=*/true, /*showed_interstitial=*/false,
                  SafeBrowsingUrlCheckerImpl::PerformedCheck::kUrlRealTimeCheck,
                  /*did_check_url_real_time_allowlist=*/true));
  safe_browsing_url_checker->CheckUrl(redirect_url, "GET",
                                      redirect_callback.Get());

  task_environment_.RunUntilIdle();
}

TEST_F(SafeBrowsingUrlCheckerTest, CheckUrl_CancelCheckOnDestruct) {
  // Do not cancel check for real-time URL checks.
  {
    auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
        /*url_real_time_lookup_enabled=*/true,
        /*can_check_safe_browsing_db=*/true,
        /*hash_real_time_selection=*/
        hash_realtime_utils::HashRealTimeSelection::kNone);

    GURL url("https://example.test/");
    database_manager_->SetAllowlistLookupDetailsForUrl(url, /*match=*/false,
                                                       {"RT"});
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
        /*url_real_time_lookup_enabled=*/false,
        /*can_check_safe_browsing_db=*/true,
        /*hash_real_time_selection=*/
        hash_realtime_utils::HashRealTimeSelection::kNone);

    GURL url("https://example.test/");
    database_manager_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_URL_PHISHING,
                                           /*delayed_callback=*/false);

    base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback> cb;
    EXPECT_CALL(
        cb, Run(_, /*proceed=*/false, /*showed_interstitial=*/false,
                SafeBrowsingUrlCheckerImpl::PerformedCheck::kHashDatabaseCheck,
                /*did_check_url_real_time_allowlist=*/false));
    safe_browsing_url_checker->CheckUrl(url, "GET", cb.Get());
    EXPECT_FALSE(database_manager_->HasCalledCancelCheck());
    safe_browsing_url_checker.reset();
    EXPECT_TRUE(database_manager_->HasCalledCancelCheck());

    task_environment_.RunUntilIdle();
  }
}

TEST_F(SafeBrowsingUrlCheckerTest, CheckUrl_CancelCheckOnTimeout) {
  // Timeout for real-time URL checks should not cancel the database check.
  {
    base::HistogramTester histograms;
    auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
        /*url_real_time_lookup_enabled=*/true,
        /*can_check_safe_browsing_db=*/true,
        /*hash_real_time_selection=*/
        hash_realtime_utils::HashRealTimeSelection::kNone);

    GURL url("https://example.test/");
    database_manager_->SetAllowlistLookupDetailsForUrl(url, /*match=*/false,
                                                       {"RT"});
    url_lookup_service_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_URL_PHISHING,
                                             /*should_complete_lookup=*/false);
    base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback> cb;
    EXPECT_CALL(
        cb, Run(_, /*proceed=*/true,
                /*showed_interstitial=*/false,
                SafeBrowsingUrlCheckerImpl::PerformedCheck::kUrlRealTimeCheck,
                /*did_check_url_real_time_allowlist=*/true));
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
        /*url_real_time_lookup_enabled=*/false,
        /*can_check_safe_browsing_db=*/true,
        /*hash_real_time_selection=*/
        hash_realtime_utils::HashRealTimeSelection::kNone);

    GURL url("https://example.test/");
    database_manager_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_URL_PHISHING,
                                           /*delayed_callback=*/true);

    SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback callback =
        base::BindOnce(&SafeBrowsingUrlCheckerTest::
                           OnCheckUrlCallbackSettingSlowCheckNotifier,
                       base::Unretained(this));
    EXPECT_CALL(
        slow_check_notifier_callback_,
        Run(/*proceed=*/true, /*showed_interstitial=*/false,
            SafeBrowsingUrlCheckerImpl::PerformedCheck::kHashDatabaseCheck,
            /*did_check_url_real_time_allowlist=*/false));
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
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*url_real_time_lookup_enabled=*/false,
      /*can_check_safe_browsing_db=*/true,
      /*hash_real_time_selection=*/
      hash_realtime_utils::HashRealTimeSelection::kNone,
      /*optional_args=*/{.is_lookup_mechanism_experiment_enabled = true});

  GURL url("https://example.test/");
  database_manager_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_SAFE,
                                         /*delayed_callback=*/false);
  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  EXPECT_CALL(
      callback,
      Run(nullptr, /*proceed=*/true, /*showed_interstitial=*/false,
          SafeBrowsingUrlCheckerImpl::PerformedCheck::kHashDatabaseCheck,
          /*did_check_url_real_time_allowlist=*/false));
  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(_, _, _, _, _))
      .Times(0);

  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());
  task_environment_.RunUntilIdle();
  histogram_tester_.ExpectUniqueSample("SafeBrowsing.CheckUrl.Timeout",
                                       /*sample=*/false,
                                       /*expected_bucket_count=*/1);

  EXPECT_EQ(url_checker_delegate_->GetNumCheckExperimentEligibilityCalls(), 0);
  EXPECT_EQ(url_checker_delegate_
                ->GetNumCheckExperimentEligibilityAndStartBlockingPageCalls(),
            0);
  // Make sure the experiment ended with no logged results.
  histogram_tester_.ExpectTotalCount(
      "SafeBrowsing.HPRTExperiment.WarningsResult", 0);
}

// Same as CheckUrl_UrlRealTimeEnabledAllowlistMatch but with the lookup
// mechanism experiment enabled.
TEST_F(SafeBrowsingUrlCheckerTest,
       CheckUrl_UrlRealTimeEnabledAllowlistMatch_LookupMechanismExperiment) {
  base::HistogramTester histograms;
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*url_real_time_lookup_enabled=*/true,
      /*can_check_safe_browsing_db=*/true,
      /*hash_real_time_selection=*/
      hash_realtime_utils::HashRealTimeSelection::kNone,
      /*optional_args=*/{.is_lookup_mechanism_experiment_enabled = true});

  GURL url("https://example.test/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url, /*match=*/true,
                                                     {"RT", "HPRT"});
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
  // while we perform a real-time check.
  EXPECT_CALL(callback, Run(_, _, _, _, _)).Times(0);
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());
  task_environment_.RunUntilIdle();

  EXPECT_EQ(url_checker_delegate_->GetNumCheckExperimentEligibilityCalls(), 1);
  EXPECT_EQ(url_checker_delegate_
                ->GetNumCheckExperimentEligibilityAndStartBlockingPageCalls(),
            1);
  // Make sure the experiment ended and logged results.
  histogram_tester_.ExpectTotalCount(
      "SafeBrowsing.HPRTExperiment.WarningsResult", 1);
}

// Same as CheckUrl_UrlRealTimeEnabledAllowlistMatch_LookupMechanismExperiment
// but the check is not eligible for the experiment so it should log no results.
TEST_F(
    SafeBrowsingUrlCheckerTest,
    CheckUrl_UrlRealTimeEnabledAllowlistMatch_LookupMechanismExperimentNoEligibility) {
  base::HistogramTester histograms;
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*url_real_time_lookup_enabled=*/true,
      /*can_check_safe_browsing_db=*/true,
      /*hash_real_time_selection=*/
      hash_realtime_utils::HashRealTimeSelection::kNone,
      /*optional_args=*/{.is_lookup_mechanism_experiment_enabled = true});

  GURL url("https://example.test/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url, /*match=*/true,
                                                     {"RT", "HPRT"});
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
  // while we perform a real-time check.
  EXPECT_CALL(callback, Run(_, _, _, _, _)).Times(0);
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());

  task_environment_.RunUntilIdle();

  EXPECT_EQ(url_checker_delegate_->GetNumCheckExperimentEligibilityCalls(), 1);
  EXPECT_EQ(url_checker_delegate_
                ->GetNumCheckExperimentEligibilityAndStartBlockingPageCalls(),
            1);
  // Make sure the experiment did not log results.
  histogram_tester_.ExpectTotalCount(
      "SafeBrowsing.HPRTExperiment.WarningsResult", 0);
}

// Same as CheckUrl_UrlRealTimeEnabledSafeUrl but with the lookup mechanism
// experiment enabled.
TEST_F(SafeBrowsingUrlCheckerTest,
       CheckUrl_UrlRealTimeEnabledSafeUrl_LookupMechanismExperiment) {
  base::HistogramTester histograms;
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*url_real_time_lookup_enabled=*/true,
      /*can_check_safe_browsing_db=*/true,
      /*hash_real_time_selection=*/
      hash_realtime_utils::HashRealTimeSelection::kNone,
      /*optional_args=*/{.is_lookup_mechanism_experiment_enabled = true});

  GURL url("https://example.test/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url, /*match=*/false,
                                                     {"RT", "HPRT"});
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
  EXPECT_CALL(callback,
              Run(_, /*proceed=*/true, /*showed_interstitial=*/false,
                  SafeBrowsingUrlCheckerImpl::PerformedCheck::kUrlRealTimeCheck,
                  /*did_check_url_real_time_allowlist=*/true));
  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(_, _, _, _, _))
      .Times(0);
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());

  task_environment_.RunUntilIdle();
  histogram_tester_.ExpectUniqueSample("SafeBrowsing.CheckUrl.Timeout",
                                       /*sample=*/false,
                                       /*expected_bucket_count=*/1);

  // The false positive metric should not be logged, because the
  // verdict is not from cache.
  histogram_tester_.ExpectTotalCount(
      "SafeBrowsing.RT.GetCache.FallbackThreatType",
      /*expected_count=*/0);

  EXPECT_EQ(url_checker_delegate_->GetNumCheckExperimentEligibilityCalls(), 1);
  EXPECT_EQ(url_checker_delegate_
                ->GetNumCheckExperimentEligibilityAndStartBlockingPageCalls(),
            0);
  // Make sure the experiment ended and logged results.
  histogram_tester_.ExpectTotalCount(
      "SafeBrowsing.HPRTExperiment.WarningsResult", 1);
}

// Same as CheckUrl_UrlRealTimeEnabledRedirectUrlsSafe but with the lookup
// mechanism experiment enabled.
TEST_F(SafeBrowsingUrlCheckerTest,
       CheckUrl_UrlRealTimeEnabledRedirectUrlsSafe_LookupMechanismExperiment) {
  base::HistogramTester histograms;
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*url_real_time_lookup_enabled=*/true,
      /*can_check_safe_browsing_db=*/true,
      /*hash_real_time_selection=*/
      hash_realtime_utils::HashRealTimeSelection::kNone,
      /*optional_args=*/{.is_lookup_mechanism_experiment_enabled = true});

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
  database_manager_->SetAllowlistLookupDetailsForUrl(
      origin_url, /*match=*/false, {"RT", "HPRT"});
  url_checker_delegate_->SetLookupMechanismExperimentEligibility(
      origin_url, /*eligibility=*/true);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      origin_callback;
  EXPECT_CALL(origin_callback,
              Run(_, /*proceed=*/true, /*showed_interstitial=*/false,
                  SafeBrowsingUrlCheckerImpl::PerformedCheck::kUrlRealTimeCheck,
                  /*did_check_url_real_time_allowlist=*/true));
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
  database_manager_->SetAllowlistLookupDetailsForUrl(
      redirect_url, /*match=*/false, {"RT", "HPRT"});
  url_checker_delegate_->SetLookupMechanismExperimentEligibility(
      redirect_url, /*eligibility=*/true);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      redirect_callback;
  EXPECT_CALL(redirect_callback,
              Run(_, /*proceed=*/true, /*showed_interstitial=*/false,
                  SafeBrowsingUrlCheckerImpl::PerformedCheck::kUrlRealTimeCheck,
                  /*did_check_url_real_time_allowlist=*/true));
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
  histogram_tester_.ExpectTotalCount(
      "SafeBrowsing.HPRTExperiment.Redirects.WarningsResult", 1);
}

TEST_F(SafeBrowsingUrlCheckerTest,
       CheckUrl_HashRealTimeService_AllowlistMatchSafe) {
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*url_real_time_lookup_enabled=*/false,
      /*can_check_safe_browsing_db=*/true,
      /*hash_real_time_selection=*/
      hash_realtime_utils::HashRealTimeSelection::kHashRealTimeService);

  GURL url("https://example.test/");
  database_manager_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_SAFE,
                                         /*delayed_callback=*/false);
  database_manager_->SetAllowlistLookupDetailsForUrl(url, /*match=*/true,
                                                     {"HPRT"});

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  EXPECT_CALL(
      callback,
      Run(_, /*proceed=*/true, /*showed_interstitial=*/false,
          SafeBrowsingUrlCheckerImpl::PerformedCheck::kHashRealTimeCheck,
          /*did_check_allowlist=*/false))
      .Times(1);
  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(
                  IsSameThreatSource(ThreatSource::UNKNOWN), _, _, _, _))
      .Times(0);
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());
  task_environment_.RunUntilIdle();
  CheckHashRealTimeMetrics(/*expected_local_match_result=*/true,
                           /*expected_is_service_found=*/absl::nullopt);
}

TEST_F(SafeBrowsingUrlCheckerTest,
       CheckUrl_HashRealTimeService_AllowlistMatchUnsafe) {
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*url_real_time_lookup_enabled=*/false,
      /*can_check_safe_browsing_db=*/true,
      /*hash_real_time_selection=*/
      hash_realtime_utils::HashRealTimeSelection::kHashRealTimeService);

  GURL url("https://example.test/");
  database_manager_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_URL_PHISHING,
                                         /*delayed_callback=*/false);
  database_manager_->SetAllowlistLookupDetailsForUrl(url, /*match=*/true,
                                                     {"HPRT"});

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  // Note that the callback is not called, because resource fetch is not blocked
  // while we perform a real-time check.
  EXPECT_CALL(callback, Run(_, _, _, _, _)).Times(0);
  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(
                  IsSameThreatSource(ThreatSource::UNKNOWN), _, _, _, _))
      .Times(1);
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());
  task_environment_.RunUntilIdle();
  CheckHashRealTimeMetrics(/*expected_local_match_result=*/true,
                           /*expected_is_service_found=*/absl::nullopt);
}

TEST_F(SafeBrowsingUrlCheckerTest, CheckUrl_HashRealTimeService_SafeLookup) {
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*url_real_time_lookup_enabled=*/false,
      /*can_check_safe_browsing_db=*/true,
      /*hash_real_time_selection=*/
      hash_realtime_utils::HashRealTimeSelection::kHashRealTimeService);

  GURL url("https://example.test/");
  hash_realtime_service_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_SAFE,
                                              /*should_fail_lookup=*/false);
  database_manager_->SetAllowlistLookupDetailsForUrl(url, /*match=*/false,
                                                     {"HPRT"});

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  EXPECT_CALL(
      callback,
      Run(_, /*proceed=*/true, /*showed_interstitial=*/false,
          SafeBrowsingUrlCheckerImpl::PerformedCheck::kHashRealTimeCheck,
          /*did_check_allowlist=*/false))
      .Times(1);
  EXPECT_CALL(
      *url_checker_delegate_,
      StartDisplayingBlockingPageHelper(
          IsSameThreatSource(ThreatSource::NATIVE_PVER5_REAL_TIME), _, _, _, _))
      .Times(0);
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());
  task_environment_.RunUntilIdle();
  CheckHashRealTimeMetrics(/*expected_local_match_result=*/false,
                           /*expected_is_service_found=*/true);
}

TEST_F(SafeBrowsingUrlCheckerTest, CheckUrl_HashRealTimeService_UnsafeLookup) {
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*url_real_time_lookup_enabled=*/false,
      /*can_check_safe_browsing_db=*/true,
      /*hash_real_time_selection=*/
      hash_realtime_utils::HashRealTimeSelection::kHashRealTimeService);

  GURL url("https://example.test/");
  hash_realtime_service_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_URL_PHISHING,
                                              /*should_fail_lookup=*/false);
  database_manager_->SetAllowlistLookupDetailsForUrl(url, /*match=*/false,
                                                     {"HPRT"});

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  // Note that the callback is not called, because resource fetch is not blocked
  // while we perform a real-time check.
  EXPECT_CALL(callback, Run(_, _, _, _, _)).Times(0);
  EXPECT_CALL(
      *url_checker_delegate_,
      StartDisplayingBlockingPageHelper(
          IsSameThreatSource(ThreatSource::NATIVE_PVER5_REAL_TIME), _, _, _, _))
      .Times(1);
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());
  task_environment_.RunUntilIdle();
  CheckHashRealTimeMetrics(/*expected_local_match_result=*/false,
                           /*expected_is_service_found=*/true);
}

TEST_F(SafeBrowsingUrlCheckerTest,
       CheckUrl_HashRealTimeService_MissingService) {
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*url_real_time_lookup_enabled=*/false,
      /*can_check_safe_browsing_db=*/true,
      /*hash_real_time_selection=*/
      hash_realtime_utils::HashRealTimeSelection::kHashRealTimeService);
  hash_realtime_service_.reset();

  GURL url("https://example.test/");
  database_manager_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_URL_PHISHING,
                                         /*delayed_callback=*/false);
  database_manager_->SetAllowlistLookupDetailsForUrl(url, /*match=*/false,
                                                     {"HPRT"});

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  // Note that the callback is not called, because resource fetch is not blocked
  // while we perform a real-time check.
  EXPECT_CALL(callback, Run(_, _, _, _, _)).Times(0);
  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(
                  IsSameThreatSource(ThreatSource::UNKNOWN), _, _, _, _))
      .Times(1);
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());
  task_environment_.RunUntilIdle();
  CheckHashRealTimeMetrics(/*expected_local_match_result=*/false,
                           /*expected_is_service_found=*/false);
}

TEST_F(SafeBrowsingUrlCheckerTest,
       CheckUrl_HashRealTimeService_UnsuccessfulLookup) {
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*url_real_time_lookup_enabled=*/false,
      /*can_check_safe_browsing_db=*/true,
      /*hash_real_time_selection=*/
      hash_realtime_utils::HashRealTimeSelection::kHashRealTimeService);

  GURL url("https://example.test/");
  hash_realtime_service_->SetThreatTypeForUrl(url, absl::nullopt,
                                              /*should_fail_lookup=*/true);
  database_manager_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_URL_PHISHING,
                                         /*delayed_callback=*/false);
  database_manager_->SetAllowlistLookupDetailsForUrl(url, /*match=*/false,
                                                     {"HPRT"});

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  // Note that the callback is not called, because resource fetch is not blocked
  // while we perform a real-time check.
  EXPECT_CALL(callback, Run(_, _, _, _, _)).Times(0);
  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(
                  IsSameThreatSource(ThreatSource::UNKNOWN), _, _, _, _))
      .Times(1);
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());
  task_environment_.RunUntilIdle();
  CheckHashRealTimeMetrics(/*expected_local_match_result=*/false,
                           /*expected_is_service_found=*/true);
}

TEST_F(SafeBrowsingUrlCheckerTest,
       CheckUrl_HashRealTimeServiceAndUrlRealTimeBothEnabled) {
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*url_real_time_lookup_enabled=*/true,
      /*can_check_safe_browsing_db=*/true,
      /*hash_real_time_selection=*/
      hash_realtime_utils::HashRealTimeSelection::kHashRealTimeService);

  GURL url("https://example.test/");
  database_manager_->SetAllowlistLookupDetailsForUrl(url, /*match=*/false,
                                                     {"RT"});
  url_lookup_service_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_URL_PHISHING,
                                           /*should_complete_lookup=*/true);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  // Should perform the URL real-time check if both that and the hash real-time
  // check are enabled.
  EXPECT_CALL(
      *url_checker_delegate_,
      StartDisplayingBlockingPageHelper(
          IsSameThreatSource(ThreatSource::URL_REAL_TIME_CHECK), _, _, _, _))
      .Times(1);
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());

  task_environment_.RunUntilIdle();
}

TEST_F(SafeBrowsingUrlCheckerTest,
       CheckUrl_HashRealTimeEnabledThroughDatabaseManager) {
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*url_real_time_lookup_enabled=*/false,
      /*can_check_safe_browsing_db=*/true,
      /*hash_real_time_selection=*/
      hash_realtime_utils::HashRealTimeSelection::kDatabaseManager);

  GURL url("https://example.test/");
  database_manager_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_URL_PHISHING,
                                         /*delayed_callback=*/false);
  database_manager_->SetExpectedCheckBrowseUrlType(
      CheckBrowseUrlType::kHashRealTime);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  // Note that the callback is not called, because resource fetch is not blocked
  // while we perform a real-time check.
  EXPECT_CALL(callback, Run(_, _, _, _, _)).Times(0);
  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(
                  IsSameThreatSource(ThreatSource::UNKNOWN), _, _, _, _))
      .Times(1);
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());

  task_environment_.RunUntilIdle();
}

TEST_F(SafeBrowsingUrlCheckerTest, CheckUrl_InvalidRequestDestination) {
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*url_real_time_lookup_enabled=*/false,
      /*can_check_safe_browsing_db=*/true,
      /*hash_real_time_selection=*/
      hash_realtime_utils::HashRealTimeSelection::kNone,
      /*optional_args=*/
      {.request_destination = network::mojom::RequestDestination::kAudio});
  GURL url("https://example.test/");

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  EXPECT_CALL(callback,
              Run(nullptr, /*proceed=*/true, /*showed_interstitial=*/false,
                  SafeBrowsingUrlCheckerImpl::PerformedCheck::kCheckSkipped,
                  /*did_check_url_real_time_allowlist=*/false));
  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(_, _, _, _, _))
      .Times(0);

  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());
  task_environment_.RunUntilIdle();

  histogram_tester_.ExpectUniqueSample(
      /*name=*/"SB2.RequestDestination.Skipped",
      /*sample=*/network::mojom::RequestDestination::kAudio,
      /*expected_bucket_count=*/1);
}

TEST_F(SafeBrowsingUrlCheckerTest, CheckUrl_AllowlistedUrl) {
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*url_real_time_lookup_enabled=*/false,
      /*can_check_safe_browsing_db=*/true,
      /*hash_real_time_selection=*/
      hash_realtime_utils::HashRealTimeSelection::kNone);
  GURL url(kAllowlistedUrl);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  EXPECT_CALL(callback,
              Run(nullptr, /*proceed=*/true, /*showed_interstitial=*/false,
                  SafeBrowsingUrlCheckerImpl::PerformedCheck::kCheckSkipped,
                  /*did_check_url_real_time_allowlist=*/false));
  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(_, _, _, _, _))
      .Times(0);

  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());
  task_environment_.RunUntilIdle();
}

}  // namespace safe_browsing
