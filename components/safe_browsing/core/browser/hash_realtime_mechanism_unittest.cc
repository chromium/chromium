// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/hash_realtime_mechanism.h"

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/safe_browsing/core/browser/db/test_database_manager.h"
#include "components/safe_browsing/core/browser/db/util.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/hashprefix_realtime/hash_realtime_service.h"
#include "components/safe_browsing/core/browser/url_checker_delegate.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/platform_test.h"

// TODO(crbug.com/1392143): [Also TODO(thefrog)] Migrate these tests to
// safe_browsing_url_checker_impl_unittest.cc once SafeBrowsingUrlCheckerImpl
// supports hash-prefix real-time lookups.

namespace safe_browsing {

namespace {

class MockHashRealTimeService : public HashRealTimeService {
 public:
  MockHashRealTimeService()
      : HashRealTimeService(
            /*url_loader_factory=*/nullptr,
            /*cache_manager=*/nullptr,
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
            /*threat_type=*/url_details_[url].threat_type));
  }

  bool IsInBackoffMode() const override { return is_in_backoff_mode_; }
  void EnableBackoffMode() { is_in_backoff_mode_ = true; }

 private:
  base::flat_map<std::string, UrlDetail> url_details_;
  bool is_in_backoff_mode_ = false;
  base::WeakPtrFactory<MockHashRealTimeService> weak_factory_{this};
};

// This is copied from safe_browsing_url_checker_impl_unittest.cc. The tests in
// this file will be added to that file soon, once SafeBrowsingUrlCheckerImpl
// supports hash-prefix real-time lookups, at which point this duplicate code
// will be deleted.
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

class HashRealTimeMechanismTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();
    database_manager_ = new MockSafeBrowsingDatabaseManager();
    hash_rt_service_ = std::make_unique<MockHashRealTimeService>();
  }

  std::unique_ptr<HashRealTimeMechanism> CreateHashRealTimeMechanism(
      GURL& url,
      bool can_check_db) {
    base::MockCallback<base::RepeatingCallback<content::WebContents*()>>
        mock_web_contents_getter;
    return std::make_unique<HashRealTimeMechanism>(
        url, SBThreatTypeSet({safe_browsing::SB_THREAT_TYPE_URL_PHISHING}),
        database_manager_, can_check_db,
        base::SequencedTaskRunner::GetCurrentDefault(),
        hash_rt_service_->GetWeakPtr(),
        MechanismExperimentHashDatabaseCache::kNoExperiment);
  }

  void CheckHashRealTimeMetrics(
      absl::optional<bool> expected_local_match_result,
      absl::optional<bool> expected_is_service_available) {
    if (!expected_local_match_result.has_value()) {
      histogram_tester_->ExpectTotalCount(
          /*name=*/"SafeBrowsing.HPRT.LocalMatch.Result", /*expected_count=*/0);
    } else {
      histogram_tester_->ExpectUniqueSample(
          /*name=*/"SafeBrowsing.HPRT.LocalMatch.Result",
          /*sample=*/expected_local_match_result.value() ? AsyncMatch::MATCH
                                                         : AsyncMatch::NO_MATCH,
          /*expected_bucket_count=*/1);
    }
    if (!expected_is_service_available.has_value()) {
      histogram_tester_->ExpectTotalCount(
          /*name=*/"SafeBrowsing.HPRT.IsLookupServiceAvailable",
          /*expected_count=*/0);
    } else {
      histogram_tester_->ExpectUniqueSample(
          /*name=*/"SafeBrowsing.HPRT.IsLookupServiceAvailable",
          /*sample=*/expected_is_service_available.value(),
          /*expected_bucket_count=*/1);
    }
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<MockSafeBrowsingDatabaseManager> database_manager_;
  std::unique_ptr<MockHashRealTimeService> hash_rt_service_;
  std::unique_ptr<base::HistogramTester> histogram_tester_ =
      std::make_unique<base::HistogramTester>();
};

MATCHER_P2(Matches, url, threat_type, "") {
  return arg->url.spec() == url.spec() && arg->threat_type == threat_type &&
         !arg->is_from_url_real_time_check &&
         arg->url_real_time_lookup_response == nullptr;
}

TEST_F(HashRealTimeMechanismTest, CanCheckUrl_HashRealTime) {
  auto can_check_url =
      [](std::string url,
         network::mojom::RequestDestination request_destination =
             network::mojom::RequestDestination::kDocument) {
        EXPECT_TRUE(GURL(url).is_valid());
        return HashRealTimeMechanism::CanCheckUrl(GURL(url),
                                                  request_destination);
      };
  // Yes: HTTPS and main-frame URL.
  EXPECT_TRUE(can_check_url("https://example.test/path"));
  // Yes: HTTP and main-frame URL.
  EXPECT_TRUE(can_check_url("http://example.test/path"));
  // No: It's not a mainframe URL.
  EXPECT_FALSE(can_check_url("https://example.test/path",
                             network::mojom::RequestDestination::kFrame));
  // No: The URL scheme is not HTTP/HTTPS.
  EXPECT_FALSE(can_check_url("ftp://example.test/path"));
  // No: It's localhost.
  EXPECT_FALSE(can_check_url("http://localhost/path"));
  // No: The host is an IP address, but is not publicly routable.
  EXPECT_FALSE(can_check_url("http://0.0.0.0"));
  // Yes: The host is an IP address and is publicly routable.
  EXPECT_TRUE(can_check_url("http://1.0.0.0"));
  // No: Hostname does not have at least 1 dot.
  EXPECT_FALSE(can_check_url("https://example/path"));
  // No: Hostname does not have at least 3 characters.
  EXPECT_FALSE(can_check_url("https://e./path"));
}

TEST_F(HashRealTimeMechanismTest, CheckUrl_HashRealTime_CantCheckDb) {
  GURL url("https://example.test/");
  auto mechanism = CreateHashRealTimeMechanism(url, /*can_check_db=*/false);
  base::MockCallback<SafeBrowsingLookupMechanism::CompleteCheckResultCallback>
      callback;
  auto result = mechanism->StartCheck(callback.Get());
  EXPECT_CALL(callback, Run(testing::_)).Times(0);
  EXPECT_EQ(result.did_check_url_real_time_allowlist, false);
  EXPECT_EQ(result.is_safe_synchronously, true);

  task_environment_.RunUntilIdle();
  CheckHashRealTimeMetrics(/*expected_local_match_result=*/absl::nullopt,
                           /*expected_is_service_available=*/absl::nullopt);
}

TEST_F(HashRealTimeMechanismTest, CheckUrl_HashRealTime_AllowlistMatchSafe) {
  GURL url("https://example.test/");
  auto mechanism = CreateHashRealTimeMechanism(url, /*can_check_db=*/true);
  database_manager_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_SAFE,
                                         /*delayed_callback=*/false);
  database_manager_->SetAllowlistResultForUrl(url, true);
  base::MockCallback<SafeBrowsingLookupMechanism::CompleteCheckResultCallback>
      callback;
  auto result = mechanism->StartCheck(callback.Get());
  EXPECT_EQ(result.did_check_url_real_time_allowlist, false);
  EXPECT_EQ(result.is_safe_synchronously, false);

  EXPECT_CALL(callback, Run(Matches(url, SB_THREAT_TYPE_SAFE))).Times(1);
  task_environment_.RunUntilIdle();
  CheckHashRealTimeMetrics(/*expected_local_match_result=*/true,
                           /*expected_is_service_available=*/absl::nullopt);
}

TEST_F(HashRealTimeMechanismTest, CheckUrl_HashRealTime_AllowlistMatchUnsafe) {
  GURL url("https://example.test/");
  auto mechanism = CreateHashRealTimeMechanism(url, /*can_check_db=*/true);
  database_manager_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_URL_PHISHING,
                                         /*delayed_callback=*/false);
  database_manager_->SetAllowlistResultForUrl(url, true);
  base::MockCallback<SafeBrowsingLookupMechanism::CompleteCheckResultCallback>
      callback;
  auto result = mechanism->StartCheck(callback.Get());
  EXPECT_EQ(result.did_check_url_real_time_allowlist, false);
  EXPECT_EQ(result.is_safe_synchronously, false);

  EXPECT_CALL(callback, Run(Matches(url, SB_THREAT_TYPE_URL_PHISHING)))
      .Times(1);
  task_environment_.RunUntilIdle();
  CheckHashRealTimeMetrics(/*expected_local_match_result=*/true,
                           /*expected_is_service_available=*/absl::nullopt);
}

TEST_F(HashRealTimeMechanismTest, CheckUrl_HashRealTime_SafeLookup) {
  GURL url("https://example.test/");
  auto mechanism = CreateHashRealTimeMechanism(url, /*can_check_db=*/true);
  hash_rt_service_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_SAFE,
                                        /*should_fail_lookup=*/false);
  database_manager_->SetAllowlistResultForUrl(url, false);
  base::MockCallback<SafeBrowsingLookupMechanism::CompleteCheckResultCallback>
      callback;
  auto result = mechanism->StartCheck(callback.Get());
  EXPECT_EQ(result.did_check_url_real_time_allowlist, false);
  EXPECT_EQ(result.is_safe_synchronously, false);

  EXPECT_CALL(callback, Run(Matches(url, SB_THREAT_TYPE_SAFE))).Times(1);
  task_environment_.RunUntilIdle();
  CheckHashRealTimeMetrics(/*expected_local_match_result=*/false,
                           /*expected_is_service_available=*/true);
}

TEST_F(HashRealTimeMechanismTest, CheckUrl_HashRealTime_UnsafeLookup) {
  GURL url("https://example.test/");
  auto mechanism = CreateHashRealTimeMechanism(url, /*can_check_db=*/true);
  hash_rt_service_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_URL_PHISHING,
                                        /*should_fail_lookup=*/false);
  database_manager_->SetAllowlistResultForUrl(url, false);
  base::MockCallback<SafeBrowsingLookupMechanism::CompleteCheckResultCallback>
      callback;
  auto result = mechanism->StartCheck(callback.Get());
  EXPECT_EQ(result.did_check_url_real_time_allowlist, false);
  EXPECT_EQ(result.is_safe_synchronously, false);

  EXPECT_CALL(callback, Run(Matches(url, SB_THREAT_TYPE_URL_PHISHING)))
      .Times(1);
  task_environment_.RunUntilIdle();
  CheckHashRealTimeMetrics(/*expected_local_match_result=*/false,
                           /*expected_is_service_available=*/true);
}

TEST_F(HashRealTimeMechanismTest, CheckUrl_HashRealTime_BackoffMode) {
  GURL url("https://example.test/");
  auto mechanism = CreateHashRealTimeMechanism(url, /*can_check_db=*/true);
  hash_rt_service_->EnableBackoffMode();
  database_manager_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_URL_PHISHING,
                                         /*delayed_callback=*/false);
  database_manager_->SetAllowlistResultForUrl(url, false);
  base::MockCallback<SafeBrowsingLookupMechanism::CompleteCheckResultCallback>
      callback;
  auto result = mechanism->StartCheck(callback.Get());
  EXPECT_EQ(result.did_check_url_real_time_allowlist, false);
  EXPECT_EQ(result.is_safe_synchronously, false);

  EXPECT_CALL(callback, Run(Matches(url, SB_THREAT_TYPE_URL_PHISHING)))
      .Times(1);
  task_environment_.RunUntilIdle();
  CheckHashRealTimeMetrics(/*expected_local_match_result=*/false,
                           /*expected_is_service_available=*/false);
}

TEST_F(HashRealTimeMechanismTest, CheckUrl_HashRealTime_UnsuccessfulLookup) {
  GURL url("https://example.test/");
  auto mechanism = CreateHashRealTimeMechanism(url, /*can_check_db=*/true);
  hash_rt_service_->SetThreatTypeForUrl(url, absl::nullopt,
                                        /*should_fail_lookup=*/true);
  database_manager_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_URL_PHISHING,
                                         /*delayed_callback=*/false);
  database_manager_->SetAllowlistResultForUrl(url, false);
  base::MockCallback<SafeBrowsingLookupMechanism::CompleteCheckResultCallback>
      callback;
  auto result = mechanism->StartCheck(callback.Get());
  EXPECT_EQ(result.did_check_url_real_time_allowlist, false);
  EXPECT_EQ(result.is_safe_synchronously, false);

  EXPECT_CALL(callback, Run(Matches(url, SB_THREAT_TYPE_URL_PHISHING)))
      .Times(1);
  task_environment_.RunUntilIdle();
  CheckHashRealTimeMetrics(/*expected_local_match_result=*/false,
                           /*expected_is_service_available=*/true);
}

}  // namespace safe_browsing
