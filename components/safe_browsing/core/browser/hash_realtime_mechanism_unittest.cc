// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/hash_realtime_mechanism.h"

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/safe_browsing/core/browser/db/test_database_manager.h"
#include "components/safe_browsing/core/browser/db/util.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/hashprefix_realtime/hash_realtime_service.h"
#include "components/safe_browsing/core/browser/url_checker_delegate.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/platform_test.h"

// TODO(crbug.com/1410253): [Also TODO(thefrog)] Delete this whole file when
// deprecating the experiment. Similarly to url_realtime_mechanism_unittest.cc,
// these test cases only exist because the hash real-time mechanism returns
// information used by the experimenter that SafeBrowsingUrlCheckerImpl doesn't
// need, so it does not make sense for that to be tested within
// SafeBrowsingUrlCheckerTest.

namespace safe_browsing {

namespace {

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
    SBThreatType locally_cached_results_threat_type;
    bool should_fail_lookup;
  };

  // |should_complete_lookup| should generally be true, unless you specifically
  // want to test time-sensitive things like timeouts. Setting it to false will
  // avoid calling into |response_callback| in |StartLookup|.
  void SetThreatTypeForUrl(const GURL& gurl,
                           absl::optional<SBThreatType> threat_type,
                           SBThreatType locally_cached_results_threat_type,
                           bool should_fail_lookup) {
    url_details_[gurl.spec()].threat_type = threat_type;
    url_details_[gurl.spec()].locally_cached_results_threat_type =
        locally_cached_results_threat_type;
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
            /*locally_cached_results_threat_type=*/
            url_details_[url].locally_cached_results_threat_type));
  }

 private:
  base::flat_map<std::string, UrlDetail> url_details_;
  base::WeakPtrFactory<MockHashRealTimeService> weak_factory_{this};
};

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
  void CheckUrlForHighConfidenceAllowlist(
      const GURL& gurl,
      const std::string& metric_variation,
      base::OnceCallback<void(bool)> callback) override {
    std::string url = gurl.spec();
    DCHECK(base::Contains(urls_allowlist_match_, url));
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

class HashRealTimeMechanismTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();
    database_manager_ = new MockSafeBrowsingDatabaseManager();
    hash_rt_service_ = std::make_unique<MockHashRealTimeService>();
  }

  std::unique_ptr<HashRealTimeMechanism> CreateHashRealTimeMechanism(
      GURL& url) {
    base::MockCallback<base::RepeatingCallback<content::WebContents*()>>
        mock_web_contents_getter;
    return std::make_unique<HashRealTimeMechanism>(
        url, SBThreatTypeSet({safe_browsing::SB_THREAT_TYPE_URL_PHISHING}),
        database_manager_, base::SequencedTaskRunner::GetCurrentDefault(),
        hash_rt_service_->GetWeakPtr(),
        MechanismExperimentHashDatabaseCache::kNoExperiment,
        /*is_source_lookup_mechanism_experiment=*/false);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<MockSafeBrowsingDatabaseManager> database_manager_;
  std::unique_ptr<MockHashRealTimeService> hash_rt_service_;
};

MATCHER_P6(Matches,
           url,
           threat_type,
           matched_high_confidence_allowlist,
           locally_cached_results_threat_type,
           real_time_request_failed,
           threat_source,
           "") {
  return arg->url.spec() == url.spec() && arg->threat_type == threat_type &&
         arg->matched_high_confidence_allowlist ==
             matched_high_confidence_allowlist &&
         arg->locally_cached_results_threat_type ==
             locally_cached_results_threat_type &&
         arg->real_time_request_failed == real_time_request_failed &&
         arg->threat_source == threat_source &&
         arg->url_real_time_lookup_response == nullptr;
}

TEST_F(HashRealTimeMechanismTest, CheckUrl_HashRealTime_AllowlistMatchSafe) {
  GURL url("https://example.test/");
  auto mechanism = CreateHashRealTimeMechanism(url);
  database_manager_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_SAFE,
                                         /*delayed_callback=*/false);
  database_manager_->SetAllowlistResultForUrl(url, true);
  base::MockCallback<SafeBrowsingLookupMechanism::CompleteCheckResultCallback>
      callback;
  auto result = mechanism->StartCheck(callback.Get());
  EXPECT_EQ(result.did_check_url_real_time_allowlist, false);
  EXPECT_EQ(result.is_safe_synchronously, false);

  EXPECT_CALL(callback,
              Run(Matches(url, SB_THREAT_TYPE_SAFE,
                          /*matched_high_confidence_allowlist*/ true,
                          /*locally_cached_results_threat_type=*/absl::nullopt,
                          /*real_time_request_failed=*/false,
                          /*threat_source=*/absl::nullopt)))
      .Times(1);
  task_environment_.RunUntilIdle();
}

TEST_F(HashRealTimeMechanismTest, CheckUrl_HashRealTime_AllowlistMatchUnsafe) {
  GURL url("https://example.test/");
  auto mechanism = CreateHashRealTimeMechanism(url);
  database_manager_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_URL_PHISHING,
                                         /*delayed_callback=*/false);
  database_manager_->SetAllowlistResultForUrl(url, true);
  base::MockCallback<SafeBrowsingLookupMechanism::CompleteCheckResultCallback>
      callback;
  auto result = mechanism->StartCheck(callback.Get());
  EXPECT_EQ(result.did_check_url_real_time_allowlist, false);
  EXPECT_EQ(result.is_safe_synchronously, false);

  EXPECT_CALL(callback,
              Run(Matches(url, SB_THREAT_TYPE_URL_PHISHING,
                          /*matched_high_confidence_allowlist*/ true,
                          /*locally_cached_results_threat_type=*/absl::nullopt,
                          /*real_time_request_failed=*/false,
                          /*threat_source=*/ThreatSource::UNKNOWN)))
      .Times(1);
  task_environment_.RunUntilIdle();
}

TEST_F(HashRealTimeMechanismTest, CheckUrl_HashRealTime_SafeLookup) {
  GURL url("https://example.test/");
  auto mechanism = CreateHashRealTimeMechanism(url);
  hash_rt_service_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_SAFE,
                                        SB_THREAT_TYPE_SAFE,
                                        /*should_fail_lookup=*/false);
  database_manager_->SetAllowlistResultForUrl(url, false);
  base::MockCallback<SafeBrowsingLookupMechanism::CompleteCheckResultCallback>
      callback;
  auto result = mechanism->StartCheck(callback.Get());
  EXPECT_EQ(result.did_check_url_real_time_allowlist, false);
  EXPECT_EQ(result.is_safe_synchronously, false);

  EXPECT_CALL(
      callback,
      Run(Matches(url, SB_THREAT_TYPE_SAFE,
                  /*matched_high_confidence_allowlist*/ false,
                  /*locally_cached_results_threat_type=*/SB_THREAT_TYPE_SAFE,
                  /*real_time_request_failed=*/false,
                  /*threat_source=*/ThreatSource::NATIVE_PVER5_REAL_TIME)))
      .Times(1);
  task_environment_.RunUntilIdle();
}

TEST_F(HashRealTimeMechanismTest, CheckUrl_HashRealTime_UnsafeLookup) {
  GURL url("https://example.test/");
  auto mechanism = CreateHashRealTimeMechanism(url);
  hash_rt_service_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_URL_PHISHING,
                                        SB_THREAT_TYPE_URL_UNWANTED,
                                        /*should_fail_lookup=*/false);
  database_manager_->SetAllowlistResultForUrl(url, false);
  base::MockCallback<SafeBrowsingLookupMechanism::CompleteCheckResultCallback>
      callback;
  auto result = mechanism->StartCheck(callback.Get());
  EXPECT_EQ(result.did_check_url_real_time_allowlist, false);
  EXPECT_EQ(result.is_safe_synchronously, false);

  EXPECT_CALL(
      callback,
      Run(Matches(
          url, SB_THREAT_TYPE_URL_PHISHING,
          /*matched_high_confidence_allowlist*/ false,
          /*locally_cached_results_threat_type=*/SB_THREAT_TYPE_URL_UNWANTED,
          /*real_time_request_failed=*/false,
          /*threat_source=*/ThreatSource::NATIVE_PVER5_REAL_TIME)))
      .Times(1);
  task_environment_.RunUntilIdle();
}

TEST_F(HashRealTimeMechanismTest, CheckUrl_HashRealTime_MissingService) {
  GURL url("https://example.test/");
  auto mechanism = CreateHashRealTimeMechanism(url);
  hash_rt_service_.reset();
  database_manager_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_URL_PHISHING,
                                         /*delayed_callback=*/false);
  database_manager_->SetAllowlistResultForUrl(url, false);
  base::MockCallback<SafeBrowsingLookupMechanism::CompleteCheckResultCallback>
      callback;
  auto result = mechanism->StartCheck(callback.Get());
  EXPECT_EQ(result.did_check_url_real_time_allowlist, false);
  EXPECT_EQ(result.is_safe_synchronously, false);

  EXPECT_CALL(callback,
              Run(Matches(url, SB_THREAT_TYPE_URL_PHISHING,
                          /*matched_high_confidence_allowlist*/ false,
                          /*locally_cached_results_threat_type=*/absl::nullopt,
                          /*real_time_request_failed=*/true,
                          /*threat_source=*/ThreatSource::UNKNOWN)))
      .Times(1);
  task_environment_.RunUntilIdle();
}

TEST_F(HashRealTimeMechanismTest, CheckUrl_HashRealTime_UnsuccessfulLookup) {
  GURL url("https://example.test/");
  auto mechanism = CreateHashRealTimeMechanism(url);
  hash_rt_service_->SetThreatTypeForUrl(url, absl::nullopt,
                                        SB_THREAT_TYPE_URL_MALWARE,
                                        /*should_fail_lookup=*/true);
  database_manager_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_URL_PHISHING,
                                         /*delayed_callback=*/false);
  database_manager_->SetAllowlistResultForUrl(url, false);
  base::MockCallback<SafeBrowsingLookupMechanism::CompleteCheckResultCallback>
      callback;
  auto result = mechanism->StartCheck(callback.Get());
  EXPECT_EQ(result.did_check_url_real_time_allowlist, false);
  EXPECT_EQ(result.is_safe_synchronously, false);

  EXPECT_CALL(callback,
              Run(Matches(url, SB_THREAT_TYPE_URL_PHISHING,
                          /*matched_high_confidence_allowlist*/ false,
                          /*locally_cached_results_threat_type=*/absl::nullopt,
                          /*real_time_request_failed=*/true,
                          /*threat_source=*/ThreatSource::UNKNOWN)))
      .Times(1);
  task_environment_.RunUntilIdle();
}

}  // namespace safe_browsing
