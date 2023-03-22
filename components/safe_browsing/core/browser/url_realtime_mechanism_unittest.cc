// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/url_realtime_mechanism.h"

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/safe_browsing/core/browser/db/test_database_manager.h"
#include "components/safe_browsing/core/browser/db/util.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/url_checker_delegate.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/platform_test.h"

// TODO(crbug.com/1410253): [Also TODO(thefrog)] Delete this whole file when
// deprecating the experiment. This file was only added because the URL
// real-time mechanism was returning information used by the experimenter that
// SafeBrowsingUrlCheckerImpl didn't need, and thus didn't make sense to be
// tested within SafeBrowsingUrlCheckerTest.

namespace safe_browsing {

namespace {

// This is copied from safe_browsing_url_checker_impl_unittest.cc. This whole
// file will be deleted soon so the duplicate code will be gone. One change to
// this class from the original is that this adds a |should_fail_lookup| option.
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
      case SB_THREAT_TYPE_URL_MALWARE:
        threat_type = RTLookupResponse::ThreatInfo::WEB_MALWARE;
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
          FROM_HERE,
          base::BindOnce(
              std::move(response_callback),
              /*is_rt_lookup_successful=*/!url_details_[url].should_fail_lookup,
              /*is_cached_response=*/is_cached_response_, std::move(response)));
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
                           bool should_complete_lookup,
                           bool should_fail_lookup = false) {
    url_details_[gurl.spec()].threat_type = threat_type;
    url_details_[gurl.spec()].should_complete_lookup = should_complete_lookup;
    url_details_[gurl.spec()].should_fail_lookup = should_fail_lookup;
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
    bool should_fail_lookup;
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

// This is copied from safe_browsing_url_checker_impl_unittest.cc. This whole
// file will be deleted soon so the duplicate code will be gone.
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

}  // namespace

class UrlRealTimeMechanismTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();
    database_manager_ = new MockSafeBrowsingDatabaseManager();
    url_lookup_service_ = std::make_unique<MockRealTimeUrlLookupService>();
  }

  std::unique_ptr<UrlRealTimeMechanism> CreateUrlRealTimeMechanism(
      GURL& url,
      bool can_check_db) {
    base::MockCallback<base::RepeatingCallback<content::WebContents*()>>
        mock_web_contents_getter;
    return std::make_unique<UrlRealTimeMechanism>(
        url, SBThreatTypeSet({safe_browsing::SB_THREAT_TYPE_URL_PHISHING}),
        network::mojom::RequestDestination::kDocument, database_manager_,
        can_check_db,
        /*can_check_high_confidence_allowlist=*/true,
        /*url_lookup_service_metric_suffix=*/"",
        /*last_committed_url=*/GURL(),
        base::SequencedTaskRunner::GetCurrentDefault(),
        url_lookup_service_->GetWeakPtr(),
        /*webui_delegate_=*/nullptr,
        MechanismExperimentHashDatabaseCache::kNoExperiment);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<MockSafeBrowsingDatabaseManager> database_manager_;
  std::unique_ptr<MockRealTimeUrlLookupService> url_lookup_service_;
};

MATCHER_P2(Matches,
           locally_cached_results_threat_type,
           real_time_request_failed,
           "") {
  return arg->locally_cached_results_threat_type ==
             locally_cached_results_threat_type &&
         arg->real_time_request_failed == real_time_request_failed;
}

TEST_F(UrlRealTimeMechanismTest, CheckUrl_UrlRealTime_AllowlistMatchSafe) {
  GURL url("https://example.test/");
  auto mechanism = CreateUrlRealTimeMechanism(url, /*can_check_db=*/true);
  database_manager_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_SAFE,
                                         /*delayed_callback=*/false);
  database_manager_->SetAllowlistResultForUrl(url, true);
  base::MockCallback<SafeBrowsingLookupMechanism::CompleteCheckResultCallback>
      callback;
  auto result = mechanism->StartCheck(callback.Get());
  EXPECT_EQ(result.matched_high_confidence_allowlist, true);

  EXPECT_CALL(callback,
              Run(Matches(/*locally_cached_results_threat_type=*/absl::nullopt,
                          /*real_time_request_failed=*/false)))
      .Times(1);
  task_environment_.RunUntilIdle();
}

TEST_F(UrlRealTimeMechanismTest, CheckUrl_UrlRealTime_AllowlistMatchUnsafe) {
  GURL url("https://example.test/");
  auto mechanism = CreateUrlRealTimeMechanism(url, /*can_check_db=*/true);
  database_manager_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_URL_PHISHING,
                                         /*delayed_callback=*/false);
  database_manager_->SetAllowlistResultForUrl(url, true);
  base::MockCallback<SafeBrowsingLookupMechanism::CompleteCheckResultCallback>
      callback;
  auto result = mechanism->StartCheck(callback.Get());
  EXPECT_EQ(result.matched_high_confidence_allowlist, true);

  EXPECT_CALL(callback,
              Run(Matches(/*locally_cached_results_threat_type=*/absl::nullopt,
                          /*real_time_request_failed=*/false)))
      .Times(1);
  task_environment_.RunUntilIdle();
}

TEST_F(UrlRealTimeMechanismTest, CheckUrl_UrlRealTime_UnsafeLookup) {
  GURL url("https://example.test/");
  auto mechanism = CreateUrlRealTimeMechanism(url, /*can_check_db=*/true);
  url_lookup_service_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_URL_PHISHING,
                                           /*should_complete_lookup=*/true);
  database_manager_->SetAllowlistResultForUrl(url, false);
  base::MockCallback<SafeBrowsingLookupMechanism::CompleteCheckResultCallback>
      callback;
  auto result = mechanism->StartCheck(callback.Get());
  EXPECT_EQ(result.matched_high_confidence_allowlist, false);

  EXPECT_CALL(callback,
              Run(Matches(
                  /*locally_cached_results_threat_type=*/SB_THREAT_TYPE_SAFE,
                  /*real_time_request_failed=*/false)))
      .Times(1);
  task_environment_.RunUntilIdle();
}

TEST_F(UrlRealTimeMechanismTest, CheckUrl_UrlRealTime_UnsafeFromCache) {
  GURL url("https://example.test/");
  auto mechanism = CreateUrlRealTimeMechanism(url, /*can_check_db=*/true);
  database_manager_->SetAllowlistResultForUrl(url, false);
  database_manager_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_SAFE,
                                         /*delayed_callback=*/false);
  url_lookup_service_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_URL_MALWARE,
                                           /*should_complete_lookup=*/true);
  url_lookup_service_->SetIsCachedResponse(true);

  base::MockCallback<SafeBrowsingLookupMechanism::CompleteCheckResultCallback>
      callback;
  auto result = mechanism->StartCheck(callback.Get());
  EXPECT_EQ(result.matched_high_confidence_allowlist, false);

  EXPECT_CALL(
      callback,
      Run(Matches(
          /*locally_cached_results_threat_type=*/SB_THREAT_TYPE_URL_MALWARE,
          /*real_time_request_failed=*/false)))
      .Times(1);
  task_environment_.RunUntilIdle();
}

TEST_F(UrlRealTimeMechanismTest, CheckUrl_UrlRealTime_SafeFromCache) {
  GURL url("https://example.test/");
  auto mechanism = CreateUrlRealTimeMechanism(url, /*can_check_db=*/true);
  database_manager_->SetAllowlistResultForUrl(url, false);
  database_manager_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_SAFE,
                                         /*delayed_callback=*/false);
  url_lookup_service_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_SAFE,
                                           /*should_complete_lookup=*/true);
  url_lookup_service_->SetIsCachedResponse(true);

  base::MockCallback<SafeBrowsingLookupMechanism::CompleteCheckResultCallback>
      callback;
  auto result = mechanism->StartCheck(callback.Get());
  EXPECT_EQ(result.matched_high_confidence_allowlist, false);

  EXPECT_CALL(callback,
              Run(Matches(
                  /*locally_cached_results_threat_type=*/SB_THREAT_TYPE_SAFE,
                  /*real_time_request_failed=*/false)))
      .Times(1);
  task_environment_.RunUntilIdle();
}

TEST_F(UrlRealTimeMechanismTest,
       CheckUrl_UrlRealTime_SafeFromCacheFalsePositive) {
  GURL url("https://example.test/");
  auto mechanism = CreateUrlRealTimeMechanism(url, /*can_check_db=*/true);
  database_manager_->SetAllowlistResultForUrl(url, false);
  database_manager_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_URL_PHISHING,
                                         /*delayed_callback=*/false);
  url_lookup_service_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_SAFE,
                                           /*should_complete_lookup=*/true);
  url_lookup_service_->SetIsCachedResponse(true);

  base::MockCallback<SafeBrowsingLookupMechanism::CompleteCheckResultCallback>
      callback;
  auto result = mechanism->StartCheck(callback.Get());
  EXPECT_EQ(result.matched_high_confidence_allowlist, false);

  EXPECT_CALL(callback,
              Run(Matches(
                  /*locally_cached_results_threat_type=*/SB_THREAT_TYPE_SAFE,
                  /*real_time_request_failed=*/false)))
      .Times(1);
  task_environment_.RunUntilIdle();
}

TEST_F(UrlRealTimeMechanismTest, CheckUrl_UrlRealTime_MissingService) {
  GURL url("https://example.test/");
  auto mechanism = CreateUrlRealTimeMechanism(url, /*can_check_db=*/true);
  url_lookup_service_.reset();
  database_manager_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_URL_PHISHING,
                                         /*delayed_callback=*/false);
  database_manager_->SetAllowlistResultForUrl(url, false);
  base::MockCallback<SafeBrowsingLookupMechanism::CompleteCheckResultCallback>
      callback;
  auto result = mechanism->StartCheck(callback.Get());
  EXPECT_EQ(result.matched_high_confidence_allowlist, false);

  EXPECT_CALL(callback,
              Run(Matches(/*locally_cached_results_threat_type=*/absl::nullopt,
                          /*real_time_request_failed=*/true)))
      .Times(1);
  task_environment_.RunUntilIdle();
}

TEST_F(UrlRealTimeMechanismTest, CheckUrl_UrlRealTime_UnsuccessfulLookup) {
  GURL url("https://example.test/");
  auto mechanism = CreateUrlRealTimeMechanism(url, /*can_check_db=*/true);
  url_lookup_service_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_URL_MALWARE,
                                           /*should_complete_lookup=*/true,
                                           /*should_fail_lookup=*/true);
  database_manager_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_URL_PHISHING,
                                         /*delayed_callback=*/false);
  database_manager_->SetAllowlistResultForUrl(url, false);
  base::MockCallback<SafeBrowsingLookupMechanism::CompleteCheckResultCallback>
      callback;
  auto result = mechanism->StartCheck(callback.Get());
  EXPECT_EQ(result.matched_high_confidence_allowlist, false);

  EXPECT_CALL(callback,
              Run(Matches(/*locally_cached_results_threat_type=*/absl::nullopt,
                          /*real_time_request_failed=*/true)))
      .Times(1);
  task_environment_.RunUntilIdle();
}

}  // namespace safe_browsing
