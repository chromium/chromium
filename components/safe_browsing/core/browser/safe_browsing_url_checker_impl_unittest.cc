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
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/db/test_database_manager.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/realtime/fake_url_lookup_service.h"
#include "components/safe_browsing/core/browser/realtime/url_lookup_service.h"
#include "components/safe_browsing/core/browser/safe_browsing_token_fetcher.h"
#include "components/safe_browsing/core/browser/url_checker_delegate.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/hashprefix_realtime/hash_realtime_utils.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/security_interstitials/core/unsafe_resource.h"
#include "components/sessions/core/session_id.h"
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
            base::SequencedTaskRunner::GetCurrentDefault()) {}
  // SafeBrowsingDatabaseManager implementation.
  // Checks the threat type of |gurl| previously set by |SetThreatTypeForUrl|.
  // It crashes if the threat type of |gurl| is not set in advance.
  bool CheckBrowseUrl(
      const GURL& gurl,
      const safe_browsing::SBThreatTypeSet& threat_types,
      Client* client,
      CheckBrowseUrlType check_type) override {
    std::string url = gurl.spec();
    DCHECK(base::Contains(urls_threat_type_, url));
    DCHECK(base::Contains(urls_delayed_callback_, url));
    EXPECT_EQ(check_type, expected_check_type_);
    if (urls_threat_type_[url] == SBThreatType::SB_THREAT_TYPE_SAFE) {
      return true;
    }
    if (!urls_delayed_callback_[url]) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(&MockSafeBrowsingDatabaseManager::OnCheckBrowseURLDone,
                         this, gurl, client->GetWeakPtr()));
    } else {
      // If delayed callback is set to true, store the client in |urls_client_|.
      // The callback can be triggered by |RestartDelayedCallback|.
      urls_client_[url] = client;
    }
    return false;
  }

  ThreatSource GetBrowseUrlThreatSource(
      CheckBrowseUrlType check_type) const override {
    return ThreatSource::UNKNOWN;
  }

  ThreatSource GetNonBrowseUrlThreatSource() const override {
    return ThreatSource::UNKNOWN;
  }

  // Calls the callback with the allowlist match result previously set by
  // |SetAllowlistLookupDetailsForUrl|. Returns the logging details previously
  // set by |SetAllowlistLookupDetailsForUrl|. It crashes if either of the
  // allowlist match result or the logging details are not set in advance for
  // the |gurl|.
  void CheckUrlForHighConfidenceAllowlist(
      const GURL& gurl,
      CheckUrlForHighConfidenceAllowlistCallback callback) override {
    std::string url = gurl.spec();
    DCHECK(base::Contains(urls_allowlist_match_, url));
    DCHECK(base::Contains(urls_allowlist_logging_details_, url));

    ui_task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), urls_allowlist_match_[url],
                       urls_allowlist_logging_details_[url]));
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
                       this, gurl, urls_client_[url]->GetWeakPtr()));
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
      std::optional<SafeBrowsingDatabaseManager::
                        HighConfidenceAllowlistCheckLoggingDetails>
          logging_details) {
    std::string url = gurl.spec();
    urls_allowlist_match_[url] = match;
    urls_allowlist_logging_details_[url] = logging_details;
  }

  void SetExpectedCheckBrowseUrlType(CheckBrowseUrlType check_type) {
    expected_check_type_ = check_type;
  }

  void CancelCheck(Client* client) override { called_cancel_check_ = true; }

  bool HasCalledCancelCheck() { return called_cancel_check_; }

 protected:
  ~MockSafeBrowsingDatabaseManager() override = default;

 private:
  void OnCheckBrowseURLDone(const GURL& gurl, base::WeakPtr<Client> client) {
    if (called_cancel_check_) {
      return;
    }
    CHECK(client);
    std::string url = gurl.spec();
    DCHECK(base::Contains(urls_threat_type_, url));
    ThreatMetadata metadata;
    client->OnCheckBrowseUrlResult(gurl, urls_threat_type_[url], metadata);
  }
  base::flat_map<std::string, SBThreatType> urls_threat_type_;
  base::flat_map<std::string, bool> urls_delayed_callback_;
  base::flat_map<std::string, raw_ptr<Client, CtnExperimental>> urls_client_;
  base::flat_map<std::string, bool> urls_allowlist_match_;
  base::flat_map<std::string,
                 std::optional<SafeBrowsingDatabaseManager::
                                   HighConfidenceAllowlistCheckLoggingDetails>>
      urls_allowlist_logging_details_;
  CheckBrowseUrlType expected_check_type_ = CheckBrowseUrlType::kHashDatabase;

  bool called_cancel_check_ = false;
};

class MockUrlCheckerDelegate : public UrlCheckerDelegate {
 public:
  explicit MockUrlCheckerDelegate(SafeBrowsingDatabaseManager* database_manager)
      : database_manager_(database_manager),
        threat_types_(SBThreatTypeSet(
            {safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_PHISHING})) {}

  MOCK_METHOD1(MaybeDestroyNoStatePrefetchContents,
               void(base::OnceCallback<content::WebContents*()>));
  MOCK_METHOD4(StartDisplayingBlockingPageHelper,
               void(const security_interstitials::UnsafeResource&,
                    const std::string&,
                    const net::HttpRequestHeaders&,
                    bool));
  MOCK_METHOD1(StartObservingInteractionsForDelayedBlockingPageHelper,
               void(const security_interstitials::UnsafeResource&));
  MOCK_METHOD5(ShouldSkipRequestCheck,
               bool(const GURL&,
                    int,
                    int,
                    base::optional_ref<const base::UnguessableToken>,
                    bool));
  MOCK_METHOD1(NotifySuspiciousSiteDetected,
               void(const base::RepeatingCallback<content::WebContents*()>&));
  MOCK_METHOD0(GetUIManager, BaseUIManager*());
  MOCK_METHOD2(SendUrlRealTimeAndHashRealTimeDiscrepancyReport,
               void(std::unique_ptr<ClientSafeBrowsingReportRequest>,
                    const base::RepeatingCallback<content::WebContents*()>&));

  bool IsUrlAllowlisted(const GURL& url) override {
    return url.spec() == kAllowlistedUrl;
  }
  void SetPolicyAllowlistDomains(
      const std::vector<std::string>& allowlist_domains) override {}
  const SBThreatTypeSet& GetThreatTypes() override { return threat_types_; }
  SafeBrowsingDatabaseManager* GetDatabaseManager() override {
    return database_manager_;
  }

  bool AreBackgroundHashRealTimeSampleLookupsAllowed(
      const base::RepeatingCallback<content::WebContents*()>&) override {
    return are_background_hprt_lookups_allowed_;
  }

  void SetAllowHashRealTimeSampleLookups(
      bool are_background_hprt_lookups_allowed) {
    are_background_hprt_lookups_allowed_ = are_background_hprt_lookups_allowed;
  }

 protected:
  ~MockUrlCheckerDelegate() override = default;

 private:
  raw_ptr<SafeBrowsingDatabaseManager> database_manager_;
  SBThreatTypeSet threat_types_;
  bool are_background_hprt_lookups_allowed_ = true;
};

class FakeRealTimeUrlLookupService
    : public testing::FakeRealTimeUrlLookupService {
 public:
  FakeRealTimeUrlLookupService() = default;

  // Returns the threat type previously set by |SetThreatTypeForUrl|. It crashes
  // if the threat type for the |gurl| is not set in advance.
  void StartLookup(
      const GURL& gurl,
      RTLookupResponseCallback response_callback,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      SessionID tab_id) override {
    using enum SBThreatType;

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
      case SB_THREAT_TYPE_SUSPICIOUS_SITE:
        threat_type = RTLookupResponse::ThreatInfo::THREAT_TYPE_UNSPECIFIED;
        verdict_type = RTLookupResponse::ThreatInfo::SUSPICIOUS;
        break;
      default:
        NOTREACHED_IN_MIGRATION();
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

 private:
  struct UrlDetail {
    SBThreatType threat_type;
    bool should_complete_lookup;
  };

  base::flat_map<std::string, UrlDetail> url_details_;
  bool is_cached_response_ = false;
};

class MockHashRealTimeService : public HashRealTimeService {
 public:
  MockHashRealTimeService()
      : HashRealTimeService(
            /*get_network_context=*/base::NullCallback(),
            /*cache_manager=*/nullptr,
            /*ohttp_key_service=*/nullptr,
            /*webui_delegate=*/nullptr) {}
  base::WeakPtr<MockHashRealTimeService> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  struct UrlDetail {
    std::optional<SBThreatType> threat_type;
    bool should_fail_lookup;
    bool should_delay_lookup;
  };

  // |should_complete_lookup| should generally be true, unless you specifically
  // want to test time-sensitive things like timeouts. Setting it to false will
  // avoid calling into |response_callback| in |StartLookup|.
  void SetThreatTypeForUrl(const GURL& gurl,
                           std::optional<SBThreatType> threat_type,
                           bool should_fail_lookup,
                           bool should_delay_lookup) {
    url_details_[gurl.spec()].threat_type = threat_type;
    url_details_[gurl.spec()].should_fail_lookup = should_fail_lookup;
    url_details_[gurl.spec()].should_delay_lookup = should_delay_lookup;
  }

  void StartLookup(
      const GURL& gurl,
      HPRTLookupResponseCallback response_callback,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner) override {
    std::string url = gurl.spec();
    ASSERT_TRUE(base::Contains(url_details_, url));
    if (url_details_[gurl.spec()].should_delay_lookup) {
      callback_task_runner->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(
              std::move(response_callback),
              /*is_lookup_successful=*/!url_details_[url].should_fail_lookup,
              /*threat_type=*/url_details_[url].threat_type),
          base::Milliseconds(1));
    } else {
      callback_task_runner->PostTask(
          FROM_HERE,
          base::BindOnce(
              std::move(response_callback),
              /*is_lookup_successful=*/!url_details_[url].should_fail_lookup,
              /*threat_type=*/url_details_[url].threat_type));
    }
  }

 private:
  base::flat_map<std::string, UrlDetail> url_details_;
  base::WeakPtrFactory<MockHashRealTimeService> weak_factory_{this};
};

struct CreateSafeBrowsingUrlCheckerOptionalArgs {
  std::string url_lookup_service_metric_suffix = ".Enterprise";
  bool check_allowlist_before_hash_database = false;
};

}  // namespace

class SafeBrowsingUrlCheckerTest : public PlatformTest {
 public:
  SafeBrowsingUrlCheckerTest() = default;

  void SetUp() override {
    PlatformTest::SetUp();
    database_manager_ = new MockSafeBrowsingDatabaseManager();
    url_checker_delegate_ = new MockUrlCheckerDelegate(database_manager_.get());
    url_lookup_service_ = std::make_unique<FakeRealTimeUrlLookupService>();
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
    return std::make_unique<SafeBrowsingUrlCheckerImpl>(
        net::HttpRequestHeaders(), /*load_flags=*/0,
        /*has_user_gesture=*/false, url_checker_delegate_,
        mock_web_contents_getter.Get(), /*weak_web_state=*/nullptr,
        UnsafeResource::kNoRenderProcessId, std::nullopt,
        UnsafeResource::kNoFrameTreeNodeId, /*navigation_id=*/std::nullopt,
        url_real_time_lookup_enabled, can_check_safe_browsing_db,
        /*can_check_high_confidence_allowlist=*/true,
        /*url_lookup_service_metric_suffix=*/
        optional_args.url_lookup_service_metric_suffix,
        base::SequencedTaskRunner::GetCurrentDefault(),
        url_real_time_lookup_enabled ? url_lookup_service_->GetWeakPtr()
                                     : nullptr,
        /*hash_realtime_service=*/hash_realtime_service_->GetWeakPtr(),
        hash_real_time_selection,
        /*is_async_check=*/false,
        optional_args.check_allowlist_before_hash_database,
        SessionID::InvalidValue());
  }

 protected:
  using enum SBThreatType;

  void CheckHashRealTimeMetrics(std::optional<bool> expected_local_match_result,
                                std::optional<bool> expected_is_service_found,
                                bool expected_can_check_reputation) {
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
    histogram_tester_.ExpectUniqueSample(
        /*name=*/"SafeBrowsing.HPRT.CanGetReputationOfUrl",
        /*sample=*/expected_can_check_reputation,
        /*expected_bucket_count=*/1);
  }
  void CheckUrlRealTimeLocalMatchMetrics(
      std::optional<bool> expected_local_match_result,
      std::optional<bool> expect_url_lookup_service_metric_suffix) {
    ASSERT_EQ(expected_local_match_result.has_value(),
              expect_url_lookup_service_metric_suffix.has_value());
    if (!expected_local_match_result.has_value()) {
      histogram_tester_.ExpectTotalCount(
          /*name=*/"SafeBrowsing.RT.LocalMatch.Result", /*expected_count=*/0);
    } else {
      AsyncMatch expected_local_match_result_value =
          expected_local_match_result.value() ? AsyncMatch::MATCH
                                              : AsyncMatch::NO_MATCH;
      std::string expected_base_histogram = "SafeBrowsing.RT.LocalMatch.Result";
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
  void ValidateCheckUrlTimeTakenMetrics(int expected_hprt_log_count,
                                        int expected_urt_log_count,
                                        int expected_hpd_log_count) {
    histogram_tester_.ExpectTotalCount(
        /*name=*/"SafeBrowsing.CheckUrl.TimeTaken.HashRealTime",
        /*expected_count=*/expected_hprt_log_count);
    histogram_tester_.ExpectTotalCount(
        /*name=*/"SafeBrowsing.CheckUrl.TimeTaken.UrlRealTime",
        /*expected_count=*/expected_urt_log_count);
    histogram_tester_.ExpectTotalCount(
        /*name=*/"SafeBrowsing.CheckUrl.TimeTaken.HashDatabase",
        /*expected_count=*/expected_hpd_log_count);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  scoped_refptr<MockSafeBrowsingDatabaseManager> database_manager_;
  scoped_refptr<MockUrlCheckerDelegate> url_checker_delegate_;
  std::unique_ptr<FakeRealTimeUrlLookupService> url_lookup_service_;
  std::unique_ptr<MockHashRealTimeService> hash_realtime_service_;
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
  database_manager_->SetThreatTypeForUrl(url, SBThreatType::SB_THREAT_TYPE_SAFE,
                                         /*delayed_callback=*/false);
  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  EXPECT_CALL(
      callback,
      Run(/*proceed=*/true, /*showed_interstitial=*/false,
          /*has_post_commit_interstitial_skipped=*/false,
          SafeBrowsingUrlCheckerImpl::PerformedCheck::kHashDatabaseCheck));
  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(_, _, _, _))
      .Times(0);

  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());
  task_environment_.RunUntilIdle();
  histogram_tester_.ExpectUniqueSample("SafeBrowsing.CheckUrl.Timeout",
                                       /*sample=*/false,
                                       /*expected_bucket_count=*/1);
  ValidateCheckUrlTimeTakenMetrics(/*expected_hprt_log_count=*/0,
                                   /*expected_urt_log_count=*/0,
                                   /*expected_hpd_log_count=*/1);
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
  EXPECT_CALL(callback, Run(_, _, _, _)).Times(0);
  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(
                  IsSameThreatSource(ThreatSource::UNKNOWN), _, _, _))
      .Times(1);
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());
  task_environment_.RunUntilIdle();
  ValidateCheckUrlTimeTakenMetrics(/*expected_hprt_log_count=*/0,
                                   /*expected_urt_log_count=*/0,
                                   /*expected_hpd_log_count=*/1);
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
      Run(/*proceed=*/true, /*showed_interstitial=*/false,
          /*has_post_commit_interstitial_skipped=*/false,
          SafeBrowsingUrlCheckerImpl::PerformedCheck::kHashDatabaseCheck));
  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(_, _, _, _))
      .Times(0);
  safe_browsing_url_checker->CheckUrl(origin_url, "GET", origin_callback.Get());

  GURL redirect_url("https://example.redirect.test/");
  database_manager_->SetThreatTypeForUrl(redirect_url, SB_THREAT_TYPE_SAFE,
                                         /*delayed_callback=*/false);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      redirect_callback;
  EXPECT_CALL(
      redirect_callback,
      Run(/*proceed=*/true, /*showed_interstitial=*/false,
          /*has_post_commit_interstitial_skipped=*/false,
          SafeBrowsingUrlCheckerImpl::PerformedCheck::kHashDatabaseCheck));
  safe_browsing_url_checker->CheckUrl(redirect_url, "GET",
                                      redirect_callback.Get());

  task_environment_.RunUntilIdle();
  ValidateCheckUrlTimeTakenMetrics(/*expected_hprt_log_count=*/0,
                                   /*expected_urt_log_count=*/0,
                                   /*expected_hpd_log_count=*/2);
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
  EXPECT_CALL(origin_callback, Run(_, _, _, _)).Times(0);
  // Not displayed yet, because the callback is not returned.
  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(_, _, _, _))
      .Times(0);
  safe_browsing_url_checker->CheckUrl(origin_url, "GET", origin_callback.Get());

  task_environment_.RunUntilIdle();

  GURL redirect_url("https://example.redirect.test/");
  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      redirect_callback;
  // Not called because it is blocked by the first URL.
  EXPECT_CALL(redirect_callback, Run(_, _, _, _)).Times(0);
  safe_browsing_url_checker->CheckUrl(redirect_url, "GET",
                                      redirect_callback.Get());

  // The blocking page should be displayed when the origin callback is returned.
  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(_, _, _, _))
      .Times(1);
  database_manager_->RestartDelayedCallback(origin_url);
  task_environment_.RunUntilIdle();
  ValidateCheckUrlTimeTakenMetrics(/*expected_hprt_log_count=*/0,
                                   /*expected_urt_log_count=*/0,
                                   /*expected_hpd_log_count=*/1);
}

TEST_F(SafeBrowsingUrlCheckerTest,
       CheckUrl_CheckAllowlistBeforeHashDatabaseCheck_OnAllowlistUnsafe) {
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*url_real_time_lookup_enabled=*/false,
      /*can_check_safe_browsing_db=*/true,
      /*hash_real_time_selection=*/
      hash_realtime_utils::HashRealTimeSelection::kNone,
      /*optional_args=*/{.check_allowlist_before_hash_database = true});

  GURL url("https://example.test/");
  database_manager_->SetAllowlistLookupDetailsForUrl(
      url, /*match=*/true,
      /*logging_details=*/std::nullopt);
  // Although we mark the URL as phishing in the hash database, the callback
  // should return safe because it is on the allowlist.
  database_manager_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_URL_PHISHING,
                                         /*delayed_callback=*/false);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  EXPECT_CALL(
      callback,
      Run(/*proceed=*/true, /*showed_interstitial=*/false,
          /*has_post_commit_interstitial_skipped=*/false,
          SafeBrowsingUrlCheckerImpl::PerformedCheck::kHashDatabaseCheck));

  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());
  task_environment_.RunUntilIdle();
  ValidateCheckUrlTimeTakenMetrics(/*expected_hprt_log_count=*/0,
                                   /*expected_urt_log_count=*/0,
                                   /*expected_hpd_log_count=*/1);
}

TEST_F(SafeBrowsingUrlCheckerTest,
       CheckUrl_CheckAllowlistBeforeHashDatabaseCheck_NotOnAllowlistSafe) {
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*url_real_time_lookup_enabled=*/false,
      /*can_check_safe_browsing_db=*/true,
      /*hash_real_time_selection=*/
      hash_realtime_utils::HashRealTimeSelection::kNone,
      /*optional_args=*/{.check_allowlist_before_hash_database = true});

  GURL url("https://example.test/");
  database_manager_->SetAllowlistLookupDetailsForUrl(
      url, /*match=*/false,
      /*logging_details=*/std::nullopt);
  database_manager_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_SAFE,
                                         /*delayed_callback=*/false);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  EXPECT_CALL(
      callback,
      Run(/*proceed=*/true, /*showed_interstitial=*/false,
          /*has_post_commit_interstitial_skipped=*/false,
          SafeBrowsingUrlCheckerImpl::PerformedCheck::kHashDatabaseCheck));

  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());
  task_environment_.RunUntilIdle();
  ValidateCheckUrlTimeTakenMetrics(/*expected_hprt_log_count=*/0,
                                   /*expected_urt_log_count=*/0,
                                   /*expected_hpd_log_count=*/1);
}

TEST_F(SafeBrowsingUrlCheckerTest,
       CheckUrl_CheckAllowlistBeforeHashDatabaseCheck_NotOnAllowlistUnsafe) {
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*url_real_time_lookup_enabled=*/false,
      /*can_check_safe_browsing_db=*/true,
      /*hash_real_time_selection=*/
      hash_realtime_utils::HashRealTimeSelection::kNone,
      /*optional_args=*/{.check_allowlist_before_hash_database = true});

  GURL url("https://example.test/");
  database_manager_->SetAllowlistLookupDetailsForUrl(
      url, /*match=*/false,
      /*logging_details=*/std::nullopt);
  database_manager_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_URL_PHISHING,
                                         /*delayed_callback=*/false);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  EXPECT_CALL(callback, Run(_, _, _, _)).Times(0);
  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(
                  IsSameThreatSource(ThreatSource::UNKNOWN), _, _, _))
      .Times(1);

  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());
  task_environment_.RunUntilIdle();
  ValidateCheckUrlTimeTakenMetrics(/*expected_hprt_log_count=*/0,
                                   /*expected_urt_log_count=*/0,
                                   /*expected_hpd_log_count=*/1);
}

TEST_F(SafeBrowsingUrlCheckerTest,
       CheckUrl_NotCheckAllowlistBeforeHashDatabaseCheck) {
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*url_real_time_lookup_enabled=*/false,
      /*can_check_safe_browsing_db=*/true,
      /*hash_real_time_selection=*/
      hash_realtime_utils::HashRealTimeSelection::kNone,
      /*optional_args=*/{.check_allowlist_before_hash_database = false});

  GURL url("https://example.test/");
  // Although we mark the URL matching the allowlist, the URL should still be
  // flagged by the hash database check because
  // check_allowlist_before_hash_database is set to false.
  database_manager_->SetAllowlistLookupDetailsForUrl(
      url, /*match=*/true,
      /*logging_details=*/std::nullopt);
  database_manager_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_URL_PHISHING,
                                         /*delayed_callback=*/false);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  EXPECT_CALL(callback, Run(_, _, _, _)).Times(0);
  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(
                  IsSameThreatSource(ThreatSource::UNKNOWN), _, _, _))
      .Times(1);

  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());
  task_environment_.RunUntilIdle();
  ValidateCheckUrlTimeTakenMetrics(/*expected_hprt_log_count=*/0,
                                   /*expected_urt_log_count=*/0,
                                   /*expected_hpd_log_count=*/1);
}

TEST_F(SafeBrowsingUrlCheckerTest, CheckUrl_UrlRealTimeEnabledAllowlistMatch) {
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*url_real_time_lookup_enabled=*/true,
      /*can_check_safe_browsing_db=*/true,
      /*hash_real_time_selection=*/
      hash_realtime_utils::HashRealTimeSelection::kNone);

  GURL url("https://example.test/");
  database_manager_->SetAllowlistLookupDetailsForUrl(
      url, /*match=*/true,
      /*logging_details=*/std::nullopt);
  // To make sure hash based check is not skipped when the URL is in the
  // allowlist, set threat type to phishing for hash based check.
  database_manager_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_URL_PHISHING,
                                         /*delayed_callback=*/false);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  // Note that the callback is not called, because resource fetch is not blocked
  // while we perform a real-time check.
  EXPECT_CALL(callback, Run(_, _, _, _)).Times(0);
  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(
                  IsSameThreatSource(ThreatSource::UNKNOWN), _, _, _))
      .Times(1);
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());

  task_environment_.RunUntilIdle();
  CheckUrlRealTimeLocalMatchMetrics(
      /*expected_local_match_result=*/true,
      /*expect_url_lookup_service_metric_suffix=*/true);
  ValidateCheckUrlTimeTakenMetrics(/*expected_hprt_log_count=*/0,
                                   /*expected_urt_log_count=*/1,
                                   /*expected_hpd_log_count=*/0);
  histogram_tester_.ExpectUniqueSample(
      /*name=*/
      "SafeBrowsing.RT.HashDatabaseFallbackThreatType.AllowlistMatch",
      /*sample=*/SB_THREAT_TYPE_URL_PHISHING,
      /*expected_bucket_count=*/1);
}

TEST_F(SafeBrowsingUrlCheckerTest, CheckUrl_UrlRealTimeEnabledSafeUrl) {
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*url_real_time_lookup_enabled=*/true,
      /*can_check_safe_browsing_db=*/true,
      /*hash_real_time_selection=*/
      hash_realtime_utils::HashRealTimeSelection::kNone);

  GURL url("https://example.test/");
  database_manager_->SetAllowlistLookupDetailsForUrl(
      url, /*match=*/false,
      /*logging_details=*/std::nullopt);
  url_lookup_service_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_SAFE,
                                           /*should_complete_lookup=*/true);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  EXPECT_CALL(
      callback,
      Run(/*proceed=*/true, /*showed_interstitial=*/false,
          /*has_post_commit_interstitial_skipped=*/false,
          SafeBrowsingUrlCheckerImpl::PerformedCheck::kUrlRealTimeCheck));
  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(_, _, _, _))
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
      /*expected_local_match_result=*/false,
      /*expect_url_lookup_service_metric_suffix=*/true);
  ValidateCheckUrlTimeTakenMetrics(/*expected_hprt_log_count=*/0,
                                   /*expected_urt_log_count=*/1,
                                   /*expected_hpd_log_count=*/0);
}

TEST_F(SafeBrowsingUrlCheckerTest,
       CheckUrl_UrlRealTimeEnabledSafeUrl_EmptyUrlLookupServiceMetricSuffix) {
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*url_real_time_lookup_enabled=*/true,
      /*can_check_safe_browsing_db=*/true,
      /*hash_real_time_selection=*/
      hash_realtime_utils::HashRealTimeSelection::kNone,
      /*optional_args=*/{.url_lookup_service_metric_suffix = ""});

  GURL url("https://example.test/");
  database_manager_->SetAllowlistLookupDetailsForUrl(
      url, /*match=*/false,
      /*logging_details=*/std::nullopt);
  url_lookup_service_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_SAFE,
                                           /*should_complete_lookup=*/true);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  EXPECT_CALL(
      callback,
      Run(/*proceed=*/true, /*showed_interstitial=*/false,
          /*has_post_commit_interstitial_skipped=*/false,
          SafeBrowsingUrlCheckerImpl::PerformedCheck::kUrlRealTimeCheck));
  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(_, _, _, _))
      .Times(0);
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());

  task_environment_.RunUntilIdle();
  histogram_tester_.ExpectUniqueSample("SafeBrowsing.CheckUrl.Timeout",
                                       /*sample=*/false,
                                       /*expected_bucket_count=*/1);

  CheckUrlRealTimeLocalMatchMetrics(
      /*expected_local_match_result=*/false,
      /*expect_url_lookup_service_metric_suffix=*/false);
  ValidateCheckUrlTimeTakenMetrics(/*expected_hprt_log_count=*/0,
                                   /*expected_urt_log_count=*/1,
                                   /*expected_hpd_log_count=*/0);
}

TEST_F(SafeBrowsingUrlCheckerTest,
       CheckUrl_UrlRealTimeEnabledSafeUrlFromCache) {
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*url_real_time_lookup_enabled=*/true,
      /*can_check_safe_browsing_db=*/true,
      /*hash_real_time_selection=*/
      hash_realtime_utils::HashRealTimeSelection::kNone);

  GURL url("https://example.test/");
  database_manager_->SetAllowlistLookupDetailsForUrl(
      url, /*match=*/false,
      /*logging_details=*/std::nullopt);
  database_manager_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_SAFE,
                                         /*delayed_callback=*/false);
  url_lookup_service_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_SAFE,
                                           /*should_complete_lookup=*/true);
  url_lookup_service_->SetIsCachedResponse(true);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  EXPECT_CALL(
      callback,
      Run(/*proceed=*/true, /*showed_interstitial=*/false,
          /*has_post_commit_interstitial_skipped=*/false,
          SafeBrowsingUrlCheckerImpl::PerformedCheck::kUrlRealTimeCheck));
  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(_, _, _, _))
      .Times(0);
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());

  task_environment_.RunUntilIdle();

  histogram_tester_.ExpectUniqueSample(
      "SafeBrowsing.RT.GetCache.FallbackThreatType",
      /*sample=*/SB_THREAT_TYPE_SAFE,
      /*expected_bucket_count=*/1);
  CheckUrlRealTimeLocalMatchMetrics(
      /*expected_local_match_result=*/false,
      /*expect_url_lookup_service_metric_suffix=*/true);
  ValidateCheckUrlTimeTakenMetrics(/*expected_hprt_log_count=*/0,
                                   /*expected_urt_log_count=*/1,
                                   /*expected_hpd_log_count=*/0);
  histogram_tester_.ExpectUniqueSample(
      /*name=*/
      "SafeBrowsing.RT.HashDatabaseFallbackThreatType.CacheMatch",
      /*sample=*/SB_THREAT_TYPE_SAFE,
      /*expected_bucket_count=*/1);
}

TEST_F(SafeBrowsingUrlCheckerTest,
       CheckUrl_UrlRealTimeEnabledSafeUrlFromCacheFalsePositive) {
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*url_real_time_lookup_enabled=*/true,
      /*can_check_safe_browsing_db=*/true,
      /*hash_real_time_selection=*/
      hash_realtime_utils::HashRealTimeSelection::kNone);

  GURL url("https://example.test/");
  database_manager_->SetAllowlistLookupDetailsForUrl(
      url, /*match=*/false,
      /*logging_details=*/std::nullopt);
  database_manager_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_URL_PHISHING,
                                         /*delayed_callback=*/false);
  url_lookup_service_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_SAFE,
                                           /*should_complete_lookup=*/true);
  url_lookup_service_->SetIsCachedResponse(true);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(
                  IsSameThreatSource(ThreatSource::UNKNOWN), _, _, _))
      .Times(1);
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());

  task_environment_.RunUntilIdle();

  histogram_tester_.ExpectUniqueSample(
      "SafeBrowsing.RT.GetCache.FallbackThreatType",
      /*sample=*/SB_THREAT_TYPE_URL_PHISHING,
      /*expected_bucket_count=*/1);
  CheckUrlRealTimeLocalMatchMetrics(
      /*expected_local_match_result=*/false,
      /*expect_url_lookup_service_metric_suffix=*/true);
  ValidateCheckUrlTimeTakenMetrics(/*expected_hprt_log_count=*/0,
                                   /*expected_urt_log_count=*/1,
                                   /*expected_hpd_log_count=*/0);
  histogram_tester_.ExpectUniqueSample(
      /*name=*/
      "SafeBrowsing.RT.HashDatabaseFallbackThreatType.CacheMatch",
      /*sample=*/SB_THREAT_TYPE_URL_PHISHING,
      /*expected_bucket_count=*/1);
}

TEST_F(SafeBrowsingUrlCheckerTest,
       CheckUrl_UrlRealTimeEnabled_SuspiciousSiteDetection) {
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*url_real_time_lookup_enabled=*/true,
      /*can_check_safe_browsing_db=*/true,
      /*hash_real_time_selection=*/
      hash_realtime_utils::HashRealTimeSelection::kNone);

  GURL url("https://example.test/");
  url_lookup_service_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_SUSPICIOUS_SITE,
                                           /*should_complete_lookup=*/true);
  database_manager_->SetAllowlistLookupDetailsForUrl(
      url, /*match=*/false,
      /*logging_details=*/std::nullopt);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  EXPECT_CALL(
      callback,
      Run(/*proceed=*/true, /*showed_interstitial=*/false,
          /*has_post_commit_interstitial_skipped=*/false,
          SafeBrowsingUrlCheckerImpl::PerformedCheck::kUrlRealTimeCheck))
      .Times(1);
  // Suspicious site detection should happen for URL real time lookups.
  EXPECT_CALL(*url_checker_delegate_, NotifySuspiciousSiteDetected(_)).Times(1);
  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(_, _, _, _))
      .Times(0);
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());

  task_environment_.RunUntilIdle();
  CheckUrlRealTimeLocalMatchMetrics(
      /*expected_local_match_result=*/false,
      /*expect_url_lookup_service_metric_suffix=*/true);
  ValidateCheckUrlTimeTakenMetrics(/*expected_hprt_log_count=*/0,
                                   /*expected_urt_log_count=*/1,
                                   /*expected_hpd_log_count=*/0);
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
          IsSameThreatSource(ThreatSource::URL_REAL_TIME_CHECK), _, _, _))
      .Times(1);
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());

  task_environment_.RunUntilIdle();
  CheckUrlRealTimeLocalMatchMetrics(
      /*expected_local_match_result=*/false,
      /*expect_url_lookup_service_metric_suffix=*/true);
  ValidateCheckUrlTimeTakenMetrics(/*expected_hprt_log_count=*/0,
                                   /*expected_urt_log_count=*/1,
                                   /*expected_hpd_log_count=*/0);
  // Since the allowlist is not checked when SB is disabled, these metrics
  // should not be logged.
  histogram_tester_.ExpectTotalCount(
      /*name=*/"SafeBrowsing.RT.AllStoresAvailable", /*expected_count=*/0);
  histogram_tester_.ExpectTotalCount(
      /*name=*/"SafeBrowsing.RT.AllowlistSizeTooSmall", /*expected_count=*/0);
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
          IsSameThreatSource(ThreatSource::URL_REAL_TIME_CHECK), _, _, _))
      .Times(1);
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());

  task_environment_.RunUntilIdle();
  CheckUrlRealTimeLocalMatchMetrics(
      /*expected_local_match_result=*/false,
      /*expect_url_lookup_service_metric_suffix=*/true);
  ValidateCheckUrlTimeTakenMetrics(/*expected_hprt_log_count=*/0,
                                   /*expected_urt_log_count=*/1,
                                   /*expected_hpd_log_count=*/0);
  // Since the allowlist is not checked when SB is disabled, these metrics
  // should not be logged.
  histogram_tester_.ExpectTotalCount(
      /*name=*/"SafeBrowsing.RT.AllStoresAvailable", /*expected_count=*/0);
  histogram_tester_.ExpectTotalCount(
      /*name=*/"SafeBrowsing.RT.AllowlistSizeTooSmall", /*expected_count=*/0);
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
          IsSameThreatSource(ThreatSource::URL_REAL_TIME_CHECK), _, _, _))
      .Times(1);
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());

  task_environment_.RunUntilIdle();
  CheckUrlRealTimeLocalMatchMetrics(
      /*expected_local_match_result=*/false,
      /*expect_url_lookup_service_metric_suffix=*/true);
  ValidateCheckUrlTimeTakenMetrics(/*expected_hprt_log_count=*/0,
                                   /*expected_urt_log_count=*/1,
                                   /*expected_hpd_log_count=*/0);
  // Since the allowlist is not checked when SB is disabled, these metrics
  // should not be logged.
  histogram_tester_.ExpectTotalCount(
      /*name=*/"SafeBrowsing.RT.AllStoresAvailable", /*expected_count=*/0);
  histogram_tester_.ExpectTotalCount(
      /*name=*/"SafeBrowsing.RT.AllowlistSizeTooSmall", /*expected_count=*/0);
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
  EXPECT_CALL(
      callback,
      Run(/*proceed=*/true, /*showed_interstitial=*/false,
          /*has_post_commit_interstitial_skipped=*/false,
          SafeBrowsingUrlCheckerImpl::PerformedCheck::kUrlRealTimeCheck));
  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(_, _, _, _))
      .Times(0);
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());

  task_environment_.RunUntilIdle();
  CheckUrlRealTimeLocalMatchMetrics(
      /*expected_local_match_result=*/false,
      /*expect_url_lookup_service_metric_suffix=*/true);
  ValidateCheckUrlTimeTakenMetrics(/*expected_hprt_log_count=*/0,
                                   /*expected_urt_log_count=*/1,
                                   /*expected_hpd_log_count=*/0);
  // Since the allowlist is not checked when SB is disabled, these metrics
  // should not be logged.
  histogram_tester_.ExpectTotalCount(
      /*name=*/"SafeBrowsing.RT.AllStoresAvailable", /*expected_count=*/0);
  histogram_tester_.ExpectTotalCount(
      /*name=*/"SafeBrowsing.RT.AllowlistSizeTooSmall", /*expected_count=*/0);
}

TEST_F(SafeBrowsingUrlCheckerTest,
       CheckUrl_UrlRealTimeEnabledRedirectUrlsSafe) {
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*url_real_time_lookup_enabled=*/true,
      /*can_check_safe_browsing_db=*/true,
      /*hash_real_time_selection=*/
      hash_realtime_utils::HashRealTimeSelection::kNone);

  GURL origin_url("https://example.test/");
  database_manager_->SetAllowlistLookupDetailsForUrl(
      origin_url,
      /*match=*/false, /*logging_details=*/std::nullopt);
  url_lookup_service_->SetThreatTypeForUrl(origin_url, SB_THREAT_TYPE_SAFE,
                                           /*should_complete_lookup=*/true);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      origin_callback;
  EXPECT_CALL(
      origin_callback,
      Run(/*proceed=*/true, /*showed_interstitial=*/false,
          /*has_post_commit_interstitial_skipped=*/false,
          SafeBrowsingUrlCheckerImpl::PerformedCheck::kUrlRealTimeCheck));
  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(_, _, _, _))
      .Times(0);
  safe_browsing_url_checker->CheckUrl(origin_url, "GET", origin_callback.Get());

  GURL redirect_url("https://example.redirect.test/");
  database_manager_->SetAllowlistLookupDetailsForUrl(
      redirect_url,
      /*match=*/false, /*logging_details=*/std::nullopt);
  url_lookup_service_->SetThreatTypeForUrl(redirect_url, SB_THREAT_TYPE_SAFE,
                                           /*should_complete_lookup=*/true);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      redirect_callback;
  EXPECT_CALL(
      redirect_callback,
      Run(/*proceed=*/true, /*showed_interstitial=*/false,
          /*has_post_commit_interstitial_skipped=*/false,
          SafeBrowsingUrlCheckerImpl::PerformedCheck::kUrlRealTimeCheck));
  safe_browsing_url_checker->CheckUrl(redirect_url, "GET",
                                      redirect_callback.Get());

  task_environment_.RunUntilIdle();
  ValidateCheckUrlTimeTakenMetrics(/*expected_hprt_log_count=*/0,
                                   /*expected_urt_log_count=*/2,
                                   /*expected_hpd_log_count=*/0);
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
    database_manager_->SetAllowlistLookupDetailsForUrl(
        url, /*match=*/false,
        /*logging_details=*/std::nullopt);
    url_lookup_service_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_URL_PHISHING,
                                             /*should_complete_lookup=*/true);

    base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback> cb;
    safe_browsing_url_checker->CheckUrl(url, "GET", cb.Get());
    EXPECT_FALSE(database_manager_->HasCalledCancelCheck());
    safe_browsing_url_checker.reset();
    EXPECT_FALSE(database_manager_->HasCalledCancelCheck());

    task_environment_.RunUntilIdle();
    ValidateCheckUrlTimeTakenMetrics(/*expected_hprt_log_count=*/0,
                                     /*expected_urt_log_count=*/0,
                                     /*expected_hpd_log_count=*/0);
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
    EXPECT_CALL(cb, Run(_, _, _, _)).Times(0);
    safe_browsing_url_checker->CheckUrl(url, "GET", cb.Get());
    EXPECT_FALSE(database_manager_->HasCalledCancelCheck());
    safe_browsing_url_checker.reset();
    EXPECT_TRUE(database_manager_->HasCalledCancelCheck());

    task_environment_.RunUntilIdle();
    ValidateCheckUrlTimeTakenMetrics(/*expected_hprt_log_count=*/0,
                                     /*expected_urt_log_count=*/0,
                                     /*expected_hpd_log_count=*/0);
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
    database_manager_->SetAllowlistLookupDetailsForUrl(
        url, /*match=*/false,
        /*logging_details=*/std::nullopt);
    url_lookup_service_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_URL_PHISHING,
                                             /*should_complete_lookup=*/false);
    base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback> cb;
    EXPECT_CALL(
        cb, Run(/*proceed=*/true,
                /*showed_interstitial=*/false,
                /*has_post_commit_interstitial_skipped=*/false,
                SafeBrowsingUrlCheckerImpl::PerformedCheck::kUrlRealTimeCheck));
    safe_browsing_url_checker->CheckUrl(url, "GET", cb.Get());
    EXPECT_FALSE(database_manager_->HasCalledCancelCheck());
    task_environment_.FastForwardBy(base::Seconds(5));
    EXPECT_FALSE(database_manager_->HasCalledCancelCheck());
    task_environment_.RunUntilIdle();
    histograms.ExpectUniqueSample("SafeBrowsing.CheckUrl.Timeout",
                                  /*sample=*/true,
                                  /*expected_bucket_count=*/1);
    histograms.ExpectUniqueTimeSample(
        "SafeBrowsing.CheckUrl.TimeTaken.UrlRealTime",
        /*sample=*/base::Seconds(5),
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

    base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
        callback;
    EXPECT_CALL(
        callback,
        Run(/*proceed=*/true, /*showed_interstitial=*/false,
            /*has_post_commit_interstitial_skipped=*/false,
            SafeBrowsingUrlCheckerImpl::PerformedCheck::kHashDatabaseCheck));
    safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());
    EXPECT_FALSE(database_manager_->HasCalledCancelCheck());
    task_environment_.FastForwardBy(base::Seconds(5));
    EXPECT_TRUE(database_manager_->HasCalledCancelCheck());
    task_environment_.RunUntilIdle();
    histograms.ExpectUniqueSample("SafeBrowsing.CheckUrl.Timeout",
                                  /*sample=*/true,
                                  /*expected_bucket_count=*/1);
    histograms.ExpectUniqueTimeSample(
        "SafeBrowsing.CheckUrl.TimeTaken.HashDatabase",
        /*sample=*/base::Seconds(5),
        /*expected_bucket_count=*/1);
  }
}

TEST_F(SafeBrowsingUrlCheckerTest, CheckUrl_HashRealTimeService_InvalidUrl) {
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*url_real_time_lookup_enabled=*/false,
      /*can_check_safe_browsing_db=*/true,
      /*hash_real_time_selection=*/
      hash_realtime_utils::HashRealTimeSelection::kHashRealTimeService);

  GURL url("http://localhost");
  database_manager_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_SAFE,
                                         /*delayed_callback=*/false);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  EXPECT_CALL(
      callback,
      Run(/*proceed=*/true, /*showed_interstitial=*/false,
          /*has_post_commit_interstitial_skipped=*/false,
          SafeBrowsingUrlCheckerImpl::PerformedCheck::kHashDatabaseCheck))
      .Times(1);
  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(
                  IsSameThreatSource(ThreatSource::UNKNOWN), _, _, _))
      .Times(0);
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());
  task_environment_.RunUntilIdle();
  CheckHashRealTimeMetrics(/*expected_local_match_result=*/std::nullopt,
                           /*expected_is_service_found=*/std::nullopt,
                           /*expected_can_check_reputation=*/false);
  ValidateCheckUrlTimeTakenMetrics(/*expected_hprt_log_count=*/0,
                                   /*expected_urt_log_count=*/0,
                                   /*expected_hpd_log_count=*/1);
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
  database_manager_->SetAllowlistLookupDetailsForUrl(
      url, /*match=*/true,
      /*logging_details=*/std::nullopt);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  EXPECT_CALL(
      callback,
      Run(/*proceed=*/true, /*showed_interstitial=*/false,
          /*has_post_commit_interstitial_skipped=*/false,
          SafeBrowsingUrlCheckerImpl::PerformedCheck::kHashRealTimeCheck))
      .Times(1);
  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(
                  IsSameThreatSource(ThreatSource::UNKNOWN), _, _, _))
      .Times(0);
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());
  task_environment_.RunUntilIdle();
  CheckHashRealTimeMetrics(/*expected_local_match_result=*/true,
                           /*expected_is_service_found=*/std::nullopt,
                           /*expected_can_check_reputation=*/true);
  ValidateCheckUrlTimeTakenMetrics(/*expected_hprt_log_count=*/1,
                                   /*expected_urt_log_count=*/0,
                                   /*expected_hpd_log_count=*/0);
  histogram_tester_.ExpectUniqueSample(
      /*name=*/
      "SafeBrowsing.HPRT.HashDatabaseFallbackThreatType.AllowlistMatch",
      /*sample=*/SB_THREAT_TYPE_SAFE,
      /*expected_bucket_count=*/1);
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
  database_manager_->SetAllowlistLookupDetailsForUrl(
      url, /*match=*/true,
      /*logging_details=*/std::nullopt);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  // Note that the callback is not called, because resource fetch is not blocked
  // while we perform a real-time check.
  EXPECT_CALL(callback, Run(_, _, _, _)).Times(0);
  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(
                  IsSameThreatSource(ThreatSource::UNKNOWN), _, _, _))
      .Times(1);
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());
  task_environment_.RunUntilIdle();
  CheckHashRealTimeMetrics(/*expected_local_match_result=*/true,
                           /*expected_is_service_found=*/std::nullopt,
                           /*expected_can_check_reputation=*/true);
  ValidateCheckUrlTimeTakenMetrics(/*expected_hprt_log_count=*/1,
                                   /*expected_urt_log_count=*/0,
                                   /*expected_hpd_log_count=*/0);
  histogram_tester_.ExpectUniqueSample(
      /*name=*/
      "SafeBrowsing.HPRT.HashDatabaseFallbackThreatType.AllowlistMatch",
      /*sample=*/SB_THREAT_TYPE_URL_PHISHING,
      /*expected_bucket_count=*/1);
}

TEST_F(SafeBrowsingUrlCheckerTest, CheckUrl_HashRealTimeService_SafeLookup) {
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*url_real_time_lookup_enabled=*/false,
      /*can_check_safe_browsing_db=*/true,
      /*hash_real_time_selection=*/
      hash_realtime_utils::HashRealTimeSelection::kHashRealTimeService);

  GURL url("https://example.test/");
  hash_realtime_service_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_SAFE,
                                              /*should_fail_lookup=*/false,
                                              /*should_delay_lookup=*/false);
  database_manager_->SetAllowlistLookupDetailsForUrl(
      url, /*match=*/false,
      /*logging_details=*/std::nullopt);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  EXPECT_CALL(
      callback,
      Run(/*proceed=*/true, /*showed_interstitial=*/false,
          /*has_post_commit_interstitial_skipped=*/false,
          SafeBrowsingUrlCheckerImpl::PerformedCheck::kHashRealTimeCheck))
      .Times(1);
  EXPECT_CALL(
      *url_checker_delegate_,
      StartDisplayingBlockingPageHelper(
          IsSameThreatSource(ThreatSource::NATIVE_PVER5_REAL_TIME), _, _, _))
      .Times(0);
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());
  task_environment_.RunUntilIdle();
  CheckHashRealTimeMetrics(/*expected_local_match_result=*/false,
                           /*expected_is_service_found=*/true,
                           /*expected_can_check_reputation=*/true);
  ValidateCheckUrlTimeTakenMetrics(/*expected_hprt_log_count=*/1,
                                   /*expected_urt_log_count=*/0,
                                   /*expected_hpd_log_count=*/0);
}

TEST_F(SafeBrowsingUrlCheckerTest, CheckUrl_HashRealTimeService_UnsafeLookup) {
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*url_real_time_lookup_enabled=*/false,
      /*can_check_safe_browsing_db=*/true,
      /*hash_real_time_selection=*/
      hash_realtime_utils::HashRealTimeSelection::kHashRealTimeService);

  GURL url("https://example.test/");
  hash_realtime_service_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_URL_PHISHING,
                                              /*should_fail_lookup=*/false,
                                              /*should_delay_lookup=*/false);
  database_manager_->SetAllowlistLookupDetailsForUrl(
      url, /*match=*/false,
      /*logging_details=*/std::nullopt);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  // Note that the callback is not called, because resource fetch is not blocked
  // while we perform a real-time check.
  EXPECT_CALL(callback, Run(_, _, _, _)).Times(0);
  EXPECT_CALL(
      *url_checker_delegate_,
      StartDisplayingBlockingPageHelper(
          IsSameThreatSource(ThreatSource::NATIVE_PVER5_REAL_TIME), _, _, _))
      .Times(1);
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());
  task_environment_.RunUntilIdle();
  CheckHashRealTimeMetrics(/*expected_local_match_result=*/false,
                           /*expected_is_service_found=*/true,
                           /*expected_can_check_reputation=*/true);
  ValidateCheckUrlTimeTakenMetrics(/*expected_hprt_log_count=*/1,
                                   /*expected_urt_log_count=*/0,
                                   /*expected_hpd_log_count=*/0);
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
  database_manager_->SetAllowlistLookupDetailsForUrl(
      url, /*match=*/false,
      /*logging_details=*/std::nullopt);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  // Note that the callback is not called, because resource fetch is not blocked
  // while we perform a real-time check.
  EXPECT_CALL(callback, Run(_, _, _, _)).Times(0);
  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(
                  IsSameThreatSource(ThreatSource::UNKNOWN), _, _, _))
      .Times(1);
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());
  task_environment_.RunUntilIdle();
  CheckHashRealTimeMetrics(/*expected_local_match_result=*/false,
                           /*expected_is_service_found=*/false,
                           /*expected_can_check_reputation=*/true);
  ValidateCheckUrlTimeTakenMetrics(/*expected_hprt_log_count=*/1,
                                   /*expected_urt_log_count=*/0,
                                   /*expected_hpd_log_count=*/0);
}

TEST_F(SafeBrowsingUrlCheckerTest,
       CheckUrl_HashRealTimeService_UnsuccessfulLookup) {
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*url_real_time_lookup_enabled=*/false,
      /*can_check_safe_browsing_db=*/true,
      /*hash_real_time_selection=*/
      hash_realtime_utils::HashRealTimeSelection::kHashRealTimeService);

  GURL url("https://example.test/");
  hash_realtime_service_->SetThreatTypeForUrl(url, std::nullopt,
                                              /*should_fail_lookup=*/true,
                                              /*should_delay_lookup=*/false);
  database_manager_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_URL_PHISHING,
                                         /*delayed_callback=*/false);
  database_manager_->SetAllowlistLookupDetailsForUrl(
      url, /*match=*/false,
      /*logging_details=*/std::nullopt);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  // Note that the callback is not called, because resource fetch is not blocked
  // while we perform a real-time check.
  EXPECT_CALL(callback, Run(_, _, _, _)).Times(0);
  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(
                  IsSameThreatSource(ThreatSource::UNKNOWN), _, _, _))
      .Times(1);
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());
  task_environment_.RunUntilIdle();
  CheckHashRealTimeMetrics(/*expected_local_match_result=*/false,
                           /*expected_is_service_found=*/true,
                           /*expected_can_check_reputation=*/true);
  ValidateCheckUrlTimeTakenMetrics(/*expected_hprt_log_count=*/1,
                                   /*expected_urt_log_count=*/0,
                                   /*expected_hpd_log_count=*/0);
  histogram_tester_.ExpectUniqueSample(
      /*name=*/
      "SafeBrowsing.HPRT.HashDatabaseFallbackThreatType.OriginalCheckFailed",
      /*sample=*/SB_THREAT_TYPE_URL_PHISHING,
      /*expected_bucket_count=*/1);
}

TEST_F(SafeBrowsingUrlCheckerTest,
       CheckUrl_HashRealTimeServiceAndUrlRealTimeBothEnabled) {
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*url_real_time_lookup_enabled=*/true,
      /*can_check_safe_browsing_db=*/true,
      /*hash_real_time_selection=*/
      hash_realtime_utils::HashRealTimeSelection::kHashRealTimeService);

  GURL url("https://example.test/");
  database_manager_->SetAllowlistLookupDetailsForUrl(
      url, /*match=*/false,
      /*logging_details=*/std::nullopt);
  url_lookup_service_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_URL_PHISHING,
                                           /*should_complete_lookup=*/true);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  // Should perform the URL real-time check if both that and the hash real-time
  // check are enabled.
  EXPECT_CALL(
      *url_checker_delegate_,
      StartDisplayingBlockingPageHelper(
          IsSameThreatSource(ThreatSource::URL_REAL_TIME_CHECK), _, _, _))
      .Times(1);
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());

  task_environment_.RunUntilIdle();
  ValidateCheckUrlTimeTakenMetrics(/*expected_hprt_log_count=*/0,
                                   /*expected_urt_log_count=*/1,
                                   /*expected_hpd_log_count=*/0);
}
MATCHER_P4(DiscrepancyReportMatches,
           url,
           report_type,
           url_realtime_threat_type,
           hash_realtime_threat_type,
           "") {
  return arg->url() == url && arg->type() == report_type &&
         arg->url_real_time_and_hash_real_time_discrepancy_info()
                 .url_realtime_threat_type() == url_realtime_threat_type &&
         arg->url_real_time_and_hash_real_time_discrepancy_info()
                 .hash_realtime_threat_type() == hash_realtime_threat_type;
}
TEST_F(SafeBrowsingUrlCheckerTest,
       CheckUrl_BackgroundHashRealTimeLookups_SampleRateIsZero) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      safe_browsing::kHashPrefixRealTimeLookupsSamplePing,
      {{"HashPrefixRealTimeLookupsSampleRate",
        /** Override to 0% to make sure it will not be sampled. */
        "0"}});
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*url_real_time_lookup_enabled=*/true,
      /*can_check_safe_browsing_db=*/true,
      /*hash_real_time_selection=*/
      hash_realtime_utils::HashRealTimeSelection::kHashRealTimeService);

  GURL url("https://example.test/");
  database_manager_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_URL_PHISHING,
                                         /*delayed_callback=*/false);
  database_manager_->SetAllowlistLookupDetailsForUrl(
      url, /*match=*/true,
      /*logging_details=*/std::nullopt);
  url_lookup_service_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_URL_PHISHING,
                                           /*should_complete_lookup=*/true);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());

  // The SendUrlRealTimeAndHashRealTimeDiscrepancyReport method should not be
  // called because the sample rate is zero.
  EXPECT_CALL(*url_checker_delegate_,
              SendUrlRealTimeAndHashRealTimeDiscrepancyReport(_, _))
      .Times(0);

  task_environment_.RunUntilIdle();

  histogram_tester_.ExpectBucketCount(
      /*name=*/
      "SafeBrowsing.CheckUrl."
      "UrlRealTimeWithBackgroundHashRealTimeMechanismTriggered",
      /*sample=*/1,
      /*expected_count=*/0);

  // The normal URT lookup should be performed.
  ValidateCheckUrlTimeTakenMetrics(/*expected_hprt_log_count=*/0,
                                   /*expected_urt_log_count=*/1,
                                   /*expected_hpd_log_count=*/0);
}

TEST_F(SafeBrowsingUrlCheckerTest,
       CheckUrl_BackgroundHashRealTimeLookups_NoHashRealTimeSelection) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      safe_browsing::kHashPrefixRealTimeLookupsSamplePing,
      {{"HashPrefixRealTimeLookupsSampleRate",
        /** Override to 100% to make sure it will always be sampled. */
        "100"}});
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*url_real_time_lookup_enabled=*/true,
      /*can_check_safe_browsing_db=*/true,
      /*hash_real_time_selection=*/
      hash_realtime_utils::HashRealTimeSelection::kNone);

  GURL url("https://example.test/");
  database_manager_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_URL_PHISHING,
                                         /*delayed_callback=*/false);
  database_manager_->SetAllowlistLookupDetailsForUrl(
      url, /*match=*/true,
      /*logging_details=*/std::nullopt);
  url_lookup_service_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_URL_PHISHING,
                                           /*should_complete_lookup=*/true);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());

  // The SendUrlRealTimeAndHashRealTimeDiscrepancyReport method should not be
  // called because hash_real_time_selection is none.
  EXPECT_CALL(*url_checker_delegate_,
              SendUrlRealTimeAndHashRealTimeDiscrepancyReport(_, _))
      .Times(0);

  task_environment_.RunUntilIdle();

  histogram_tester_.ExpectBucketCount(
      /*name=*/
      "SafeBrowsing.CheckUrl."
      "UrlRealTimeWithBackgroundHashRealTimeMechanismTriggered",
      /*sample=*/1,
      /*expected_count=*/0);
}

TEST_F(SafeBrowsingUrlCheckerTest,
       CheckUrl_BackgroundHashRealTimeLookups_HashRealTimeService) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      safe_browsing::kHashPrefixRealTimeLookupsSamplePing,
      {{"HashPrefixRealTimeLookupsSampleRate",
        /** Override to 100% to make sure it will always be sampled. */
        "100"}});
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*url_real_time_lookup_enabled=*/true,
      /*can_check_safe_browsing_db=*/true,
      /*hash_real_time_selection=*/
      hash_realtime_utils::HashRealTimeSelection::kHashRealTimeService);

  GURL url("https://example.test/");
  hash_realtime_service_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_URL_PHISHING,
                                              /*should_fail_lookup=*/false,
                                              /*should_delay_lookup=*/false);
  url_lookup_service_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_SAFE,
                                           /*should_complete_lookup=*/true);
  database_manager_->SetAllowlistLookupDetailsForUrl(
      url, /*match=*/false,
      /*logging_details=*/std::nullopt);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  EXPECT_CALL(
      callback,
      Run(/*proceed=*/true, /*showed_interstitial=*/false,
          /*has_post_commit_interstitial_skipped=*/false,
          SafeBrowsingUrlCheckerImpl::PerformedCheck::kUrlRealTimeCheck));
  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(_, _, _, _))
      .Times(0);
  // The SendUrlRealTimeAndHashRealTimeDiscrepancyReport method should be called
  // because there is an discrepancy in the verdicts from URL real-time lookup
  // and hash real-time lookup, which means a CSBRR was sent.
  EXPECT_CALL(
      *url_checker_delegate_,
      SendUrlRealTimeAndHashRealTimeDiscrepancyReport(
          DiscrepancyReportMatches(
              "https://example.test/",
              ClientSafeBrowsingReportRequest::
                  URL_REALTIME_AND_HASH_REALTIME_DISCREPANCY,
              ClientSafeBrowsingReportRequest::
                  UrlRealTimeAndHashRealTimeDiscrepancyInfo::SAFE_OR_OTHER,
              ClientSafeBrowsingReportRequest::
                  UrlRealTimeAndHashRealTimeDiscrepancyInfo::PHISHING),
          _))
      .Times(1);
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());

  task_environment_.RunUntilIdle();

  histogram_tester_.ExpectBucketCount(
      /*name=*/
      "SafeBrowsing.URTAndBackgroundHPRT.Result",
      /*sample=*/3,  // UrtSafeAndHprtUnsafe
      /*expected_count=*/1);

  histogram_tester_.ExpectBucketCount(
      /*name=*/
      "SafeBrowsing.CheckUrl."
      "UrlRealTimeWithBackgroundHashRealTimeMechanismTriggered",
      /*sample=*/1,
      /*expected_count=*/1);
}

TEST_F(SafeBrowsingUrlCheckerTest,
       CheckUrl_BackgroundHashRealTimeLookups_Android) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      safe_browsing::kHashPrefixRealTimeLookupsSamplePing,
      {{"HashPrefixRealTimeLookupsSampleRate",
        /** Override to 100% to make sure it will always be sampled. */
        "100"}});
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*url_real_time_lookup_enabled=*/true,
      /*can_check_safe_browsing_db=*/true,
      /*hash_real_time_selection=*/
      hash_realtime_utils::HashRealTimeSelection::kDatabaseManager);

  GURL url("https://example.test/");
  database_manager_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_SAFE,
                                         /*delayed_callback=*/false);
  database_manager_->SetExpectedCheckBrowseUrlType(
      CheckBrowseUrlType::kHashRealTime);
  url_lookup_service_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_URL_PHISHING,
                                           /*should_complete_lookup=*/true);
  database_manager_->SetAllowlistLookupDetailsForUrl(
      url, /*match=*/false,
      /*logging_details=*/std::nullopt);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  EXPECT_CALL(
      *url_checker_delegate_,
      StartDisplayingBlockingPageHelper(
          IsSameThreatSource(ThreatSource::URL_REAL_TIME_CHECK), _, _, _))
      .Times(1);
  // The SendUrlRealTimeAndHashRealTimeDiscrepancyReport method should be called
  // because there is an discrepancy in the verdicts from URL real-time lookup
  // and hash real-time lookup, which means a CSBRR was sent.
  EXPECT_CALL(
      *url_checker_delegate_,
      SendUrlRealTimeAndHashRealTimeDiscrepancyReport(
          DiscrepancyReportMatches(
              "https://example.test/",
              ClientSafeBrowsingReportRequest::
                  URL_REALTIME_AND_HASH_REALTIME_DISCREPANCY,
              ClientSafeBrowsingReportRequest::
                  UrlRealTimeAndHashRealTimeDiscrepancyInfo::PHISHING,
              ClientSafeBrowsingReportRequest::
                  UrlRealTimeAndHashRealTimeDiscrepancyInfo::SAFE_OR_OTHER),
          _))
      .Times(1);
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());

  task_environment_.RunUntilIdle();

  histogram_tester_.ExpectBucketCount(
      /*name=*/
      "SafeBrowsing.URTAndBackgroundHPRT.Result",
      /*sample=*/4,  // UrtUnsafeAndHprtSafe
      /*expected_count=*/1);

  histogram_tester_.ExpectBucketCount(
      /*name=*/
      "SafeBrowsing.CheckUrl."
      "UrlRealTimeWithBackgroundHashRealTimeMechanismTriggered",
      /*sample=*/1,
      /*expected_count=*/1);
}

TEST_F(SafeBrowsingUrlCheckerTest,
       CheckUrl_BackgroundHashRealTimeLookups_NoDiscrepancy) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      safe_browsing::kHashPrefixRealTimeLookupsSamplePing,
      {{"HashPrefixRealTimeLookupsSampleRate",
        /** Override to 100% to make sure it will always be sampled. */
        "100"}});
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*url_real_time_lookup_enabled=*/true,
      /*can_check_safe_browsing_db=*/true,
      /*hash_real_time_selection=*/
      hash_realtime_utils::HashRealTimeSelection::kHashRealTimeService);

  GURL url("https://example.test/");
  hash_realtime_service_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_URL_PHISHING,
                                              /*should_fail_lookup=*/false,
                                              /*should_delay_lookup=*/false);
  url_lookup_service_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_URL_PHISHING,
                                           /*should_complete_lookup=*/true);
  database_manager_->SetAllowlistLookupDetailsForUrl(
      url, /*match=*/false,
      /*logging_details=*/std::nullopt);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());

  // The SendUrlRealTimeAndHashRealTimeDiscrepancyReport method should not be
  // called because there is no verdict discrepancy.
  EXPECT_CALL(*url_checker_delegate_,
              SendUrlRealTimeAndHashRealTimeDiscrepancyReport(_, _))
      .Times(0);

  task_environment_.RunUntilIdle();

  // The SendUrlRealTimeAndHashRealTimeDiscrepancyReport method should not be
  // called because there is no discrepancy in the verdicts from URL real-time
  // lookup and hash real-time lookup.
  EXPECT_CALL(*url_checker_delegate_,
              SendUrlRealTimeAndHashRealTimeDiscrepancyReport(_, _))
      .Times(0);
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());

  task_environment_.RunUntilIdle();

  histogram_tester_.ExpectBucketCount(
      /*name=*/
      "SafeBrowsing.URTAndBackgroundHPRT.Result",
      /*sample=*/5,  // UrtUnsafeAndHprtUnsafe
      /*expected_count=*/1);

  histogram_tester_.ExpectBucketCount(
      /*name=*/
      "SafeBrowsing.CheckUrl."
      "UrlRealTimeWithBackgroundHashRealTimeMechanismTriggered",
      /*sample=*/1,
      /*expected_count=*/1);
}

TEST_F(SafeBrowsingUrlCheckerTest,
       CheckUrl_BackgroundHashRealTimeLookups_HashRealTimeNotComplete) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      safe_browsing::kHashPrefixRealTimeLookupsSamplePing,
      {{"HashPrefixRealTimeLookupsSampleRate",
        /** Override to 100% to make sure it will always be sampled. */
        "100"}});
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*url_real_time_lookup_enabled=*/true,
      /*can_check_safe_browsing_db=*/true,
      /*hash_real_time_selection=*/
      hash_realtime_utils::HashRealTimeSelection::kHashRealTimeService);

  GURL url("https://example.test/");
  hash_realtime_service_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_URL_PHISHING,
                                              /*should_fail_lookup=*/false,
                                              /*should_delay_lookup=*/true);
  url_lookup_service_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_SAFE,
                                           /*should_complete_lookup=*/true);
  database_manager_->SetAllowlistLookupDetailsForUrl(
      url, /*match=*/false,
      /*logging_details=*/std::nullopt);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  EXPECT_CALL(
      callback,
      Run(/*proceed=*/true, /*showed_interstitial=*/false,
          /*has_post_commit_interstitial_skipped=*/false,
          SafeBrowsingUrlCheckerImpl::PerformedCheck::kUrlRealTimeCheck));
  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(_, _, _, _))
      .Times(0);
  // The SendUrlRealTimeAndHashRealTimeDiscrepancyReport method should be not be
  // called because the hash real-time lookup result hasn't come back when URL
  // real-time lookup completes.
  EXPECT_CALL(*url_checker_delegate_,
              SendUrlRealTimeAndHashRealTimeDiscrepancyReport(_, _))
      .Times(0);
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());

  task_environment_.RunUntilIdle();

  histogram_tester_.ExpectBucketCount(
      /*name=*/
      "SafeBrowsing.URTAndBackgroundHPRT.Result",
      /*sample=*/0,  // UrtSafeAndHprtUnfinished
      /*expected_count=*/1);

  histogram_tester_.ExpectBucketCount(
      /*name=*/
      "SafeBrowsing.CheckUrl."
      "UrlRealTimeWithBackgroundHashRealTimeMechanismTriggered",
      /*sample=*/1,
      /*expected_count=*/1);
}

TEST_F(SafeBrowsingUrlCheckerTest,
       CheckUrl_BackgroundHashRealTimeLookups_EnhancedProtectionDisabled) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      safe_browsing::kHashPrefixRealTimeLookupsSamplePing,
      {{"HashPrefixRealTimeLookupsSampleRate",
        /** Override to 0% to make sure it will not be sampled. */
        "100"}});
  auto safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*url_real_time_lookup_enabled=*/true,
      /*can_check_safe_browsing_db=*/true,
      /*hash_real_time_selection=*/
      hash_realtime_utils::HashRealTimeSelection::kHashRealTimeService);

  GURL url("https://example.test/");
  url_checker_delegate_->SetAllowHashRealTimeSampleLookups(false);
  hash_realtime_service_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_URL_PHISHING,
                                              /*should_fail_lookup=*/false,
                                              /*should_delay_lookup=*/false);
  database_manager_->SetAllowlistLookupDetailsForUrl(
      url, /*match=*/false,
      /*logging_details=*/std::nullopt);
  url_lookup_service_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_SAFE,
                                           /*should_complete_lookup=*/true);

  base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
      callback;
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());

  // The SendUrlRealTimeAndHashRealTimeDiscrepancyReport method should not be
  // called because the user has enhanced protection off.
  EXPECT_CALL(*url_checker_delegate_,
              SendUrlRealTimeAndHashRealTimeDiscrepancyReport(_, _))
      .Times(0);

  task_environment_.RunUntilIdle();

  histogram_tester_.ExpectBucketCount(
      /*name=*/
      "SafeBrowsing.CheckUrl."
      "UrlRealTimeWithBackgroundHashRealTimeMechanismTriggered",
      /*sample=*/1,
      /*expected_count=*/0);

  // The normal URT lookup should be performed.
  ValidateCheckUrlTimeTakenMetrics(/*expected_hprt_log_count=*/0,
                                   /*expected_urt_log_count=*/1,
                                   /*expected_hpd_log_count=*/0);
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
  EXPECT_CALL(callback, Run(_, _, _, _)).Times(0);
  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(
                  IsSameThreatSource(ThreatSource::UNKNOWN), _, _, _))
      .Times(1);
  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());

  task_environment_.RunUntilIdle();
  CheckHashRealTimeMetrics(/*expected_local_match_result=*/std::nullopt,
                           /*expected_is_service_found=*/std::nullopt,
                           /*expected_can_check_reputation=*/true);
  ValidateCheckUrlTimeTakenMetrics(/*expected_hprt_log_count=*/1,
                                   /*expected_urt_log_count=*/0,
                                   /*expected_hpd_log_count=*/0);
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
              Run(/*proceed=*/true, /*showed_interstitial=*/false,
                  /*has_post_commit_interstitial_skipped=*/false,
                  SafeBrowsingUrlCheckerImpl::PerformedCheck::kCheckSkipped));
  EXPECT_CALL(*url_checker_delegate_,
              StartDisplayingBlockingPageHelper(_, _, _, _))
      .Times(0);

  safe_browsing_url_checker->CheckUrl(url, "GET", callback.Get());
  task_environment_.RunUntilIdle();
  ValidateCheckUrlTimeTakenMetrics(/*expected_hprt_log_count=*/0,
                                   /*expected_urt_log_count=*/0,
                                   /*expected_hpd_log_count=*/0);
}

TEST_F(SafeBrowsingUrlCheckerTest, CheckUrl_AllowlistCheckLoggingDetails) {
  auto urt_safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*url_real_time_lookup_enabled=*/true,
      /*can_check_safe_browsing_db=*/true,
      /*hash_real_time_selection=*/
      hash_realtime_utils::HashRealTimeSelection::kNone);
  auto hprt_safe_browsing_url_checker = CreateSafeBrowsingUrlChecker(
      /*url_real_time_lookup_enabled=*/false,
      /*can_check_safe_browsing_db=*/true,
      /*hash_real_time_selection=*/
      hash_realtime_utils::HashRealTimeSelection::kHashRealTimeService);

  struct TestCase {
    std::string url;
    std::optional<
        SafeBrowsingDatabaseManager::HighConfidenceAllowlistCheckLoggingDetails>
        logging_details;
    std::optional<bool> expected_all_stores_available_log;
    std::optional<bool> expected_allowlist_size_too_small_log;
  } test_cases[] = {
      {"https://example.test/no_logging_details", std::nullopt, std::nullopt,
       std::nullopt},
      {"https://example.test/stores_available__allowlist_too_small",
       SafeBrowsingDatabaseManager::HighConfidenceAllowlistCheckLoggingDetails(
           true, true),
       true, true},
      {"https://example.test/stores_available__allowlist_not_too_small",
       SafeBrowsingDatabaseManager::HighConfidenceAllowlistCheckLoggingDetails(
           true, false),
       true, false},
      {"https://example.test/stores_unavailable__allowlist_too_small",
       SafeBrowsingDatabaseManager::HighConfidenceAllowlistCheckLoggingDetails(
           false, true),
       false, true},
      {"https://example.test/stores_unavailable__allowlist_not_too_small",
       SafeBrowsingDatabaseManager::HighConfidenceAllowlistCheckLoggingDetails(
           false, false),
       false, false}};

  for (const auto& test_case : test_cases) {
    GURL url(test_case.url);
    database_manager_->SetAllowlistLookupDetailsForUrl(
        url, /*match=*/false, test_case.logging_details);

    // Check URT logging.
    {
      url_lookup_service_->SetThreatTypeForUrl(url, SB_THREAT_TYPE_SAFE,
                                               /*should_complete_lookup=*/true);
      base::HistogramTester urt_histogram_tester;
      base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
          urt_callback;
      urt_safe_browsing_url_checker->CheckUrl(url, "GET", urt_callback.Get());
      task_environment_.RunUntilIdle();
      if (test_case.expected_all_stores_available_log.has_value()) {
        urt_histogram_tester.ExpectUniqueSample(
            "SafeBrowsing.RT.AllStoresAvailable",
            /*sample=*/test_case.expected_all_stores_available_log.value(),
            /*expected_bucket_count=*/1);
      } else {
        urt_histogram_tester.ExpectTotalCount(
            /*name=*/"SafeBrowsing.RT.AllStoresAvailable",
            /*expected_count=*/0);
      }
      if (test_case.expected_allowlist_size_too_small_log.has_value()) {
        urt_histogram_tester.ExpectUniqueSample(
            "SafeBrowsing.RT.AllowlistSizeTooSmall",
            /*sample=*/test_case.expected_allowlist_size_too_small_log.value(),
            /*expected_bucket_count=*/1);
      } else {
        urt_histogram_tester.ExpectTotalCount(
            /*name=*/"SafeBrowsing.RT.AllowlistSizeTooSmall",
            /*expected_count=*/0);
      }
    }

    // Check HPRT logging.
    {
      hash_realtime_service_->SetThreatTypeForUrl(
          url, SB_THREAT_TYPE_SAFE,
          /*should_fail_lookup=*/false, /*should_delay_lookup=*/false);
      base::HistogramTester hprt_histogram_tester;
      base::MockCallback<SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback>
          hprt_callback;
      hprt_safe_browsing_url_checker->CheckUrl(url, "GET", hprt_callback.Get());
      task_environment_.RunUntilIdle();
      if (test_case.expected_all_stores_available_log.has_value()) {
        hprt_histogram_tester.ExpectUniqueSample(
            "SafeBrowsing.HPRT.AllStoresAvailable",
            /*sample=*/test_case.expected_all_stores_available_log.value(),
            /*expected_bucket_count=*/1);
      } else {
        hprt_histogram_tester.ExpectTotalCount(
            /*name=*/"SafeBrowsing.HPRT.AllStoresAvailable",
            /*expected_count=*/0);
      }
      if (test_case.expected_allowlist_size_too_small_log.has_value()) {
        hprt_histogram_tester.ExpectUniqueSample(
            "SafeBrowsing.HPRT.AllowlistSizeTooSmall",
            /*sample=*/test_case.expected_allowlist_size_too_small_log.value(),
            /*expected_bucket_count=*/1);
      } else {
        hprt_histogram_tester.ExpectTotalCount(
            /*name=*/"SafeBrowsing.HPRT.AllowlistSizeTooSmall",
            /*expected_count=*/0);
      }
    }
  }
}

}  // namespace safe_browsing
