// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/filter_tab_controller.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "components/multistep_filter/core/annotation_index/mock_annotation_index_client.h"
#include "components/multistep_filter/core/data_models/filter_navigation_metadata.h"
#include "components/multistep_filter/core/filter_tab_controller_test_api.h"
#include "components/multistep_filter/core/logging/multistep_filter_metrics.h"
#include "components/multistep_filter/core/multistep_filter_service.h"
#include "components/multistep_filter/core/multistep_filter_ui_delegate.h"
#include "components/multistep_filter/core/prefs/multistep_filter_retention_prefs.h"
#include "components/multistep_filter/core/storage/filter_store.h"
#include "components/multistep_filter/core/verification/suggestion_application_result.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace multistep_filter {
namespace {

using ::base::Bucket;
using ::base::BucketsAre;
using ::testing::_;
using ::testing::Eq;
using ::testing::Optional;
using ::testing::Return;
using ::testing::StrictMock;

class MockMultistepFilterService : public MultistepFilterService {
 public:
  explicit MockMultistepFilterService(MultistepFilterService::Params params)
      : MultistepFilterService(std::move(params)) {}
  ~MockMultistepFilterService() override = default;

  MOCK_METHOD(bool,
              HasUserProvidedConsent,
              (int64_t, std::string_view),
              (override));
  MOCK_METHOD(void, RecordSuggestionImpression, (), (override));
  MOCK_METHOD(void,
              DeleteAnnotationsForTask,
              (std::string_view, int64_t, std::string_view),
              (override));
  MOCK_METHOD(void,
              RecordUserInteractionWithSuggestion,
              (SuggestionUserDecision),
              (override));
  MOCK_METHOD(RetentionStateSnapshot, GetRetentionState, (), (const, override));
  MOCK_METHOD(bool, CanUseModelExecutionFeatures, (), (const, override));
  MOCK_METHOD(bool, IsSmartSuggestionsEnabled, (), (const, override));
};

class MockMultistepFilterUiDelegate : public MultistepFilterUiDelegate {
 public:
  MockMultistepFilterUiDelegate() = default;
  ~MockMultistepFilterUiDelegate() override = default;

  MOCK_METHOD(void,
              ShowSuggestion,
              (std::optional<UrlFilterSuggestion>, SuggestionUiCallbacks),
              (override));
  MOCK_METHOD(void, ClearSuggestion, (), (override));
};

class MockFilterExtractor : public FilterExtractor {
 public:
  MockFilterExtractor(AnnotationIndexClient& client,
                      FilterStore& store,
                      MultistepFilterLogRouter* router)
      : FilterExtractor(client, store, router) {}
  ~MockFilterExtractor() override = default;

  MOCK_METHOD(void,
              ExtractAnnotationFromUrl,
              (const GURL&,
               base::OnceCallback<void(std::optional<FilterAnnotation>)>,
               int64_t),
              (override));
};

class MockFilterSuggestionGenerator : public FilterSuggestionGenerator {
 public:
  MockFilterSuggestionGenerator(AnnotationIndexClient& client,
                                FilterStore& store,
                                MultistepFilterLogRouter* router)
      : FilterSuggestionGenerator(client, store, router) {}
  ~MockFilterSuggestionGenerator() override = default;

  MOCK_METHOD(void,
              GenerateSuggestion,
              (const GURL&,
               std::vector<std::string>,
               base::OnceCallback<void(std::optional<UrlFilterSuggestion>)>,
               int64_t),
              (override));
};

class MockObserver : public FilterTabController::ObserverForTest {
 public:
  MockObserver() = default;
  ~MockObserver() override = default;

  MOCK_METHOD(void,
              OnExtractionFinishedForTest,
              (std::optional<base::Uuid>),
              (override));
  MOCK_METHOD(void,
              OnSuggestionGeneratedForTest,
              (std::optional<UrlFilterSuggestion>),
              (override));
};

class FilterTabControllerTest : public testing::Test {
 public:
  FilterTabControllerTest() {
    RegisterRetentionProfilePrefs(pref_service_.registry());
  }
  ~FilterTabControllerTest() override = default;

  void SetUp() override {
    auto mock_client =
        std::make_unique<StrictMock<MockAnnotationIndexClient>>();
    mock_annotation_client_ = mock_client.get();
    auto filter_store = std::make_unique<FilterStore>();
    filter_store_ = filter_store.get();

    MultistepFilterService::Params params;
    params.annotation_index_client = std::move(mock_client);
    params.filter_store = std::move(filter_store);
    params.pref_service = &pref_service_;
    params.identity_manager = identity_test_env_.identity_manager();
    params.consent_helper =
        unified_consent::UrlKeyedDataCollectionConsentHelper::
            NewAnonymizedDataCollectionConsentHelper(&pref_service_);
    mock_service_ = std::make_unique<StrictMock<MockMultistepFilterService>>(
        std::move(params));
    EXPECT_CALL(*mock_service_, CanUseModelExecutionFeatures())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_service_, IsSmartSuggestionsEnabled())
        .WillRepeatedly(Return(true));
    mock_delegate_ =
        std::make_unique<StrictMock<MockMultistepFilterUiDelegate>>();
    controller_ = std::make_unique<FilterTabController>(
        mock_service_.get(), /*log_router=*/nullptr, mock_delegate_.get(),
        filter_store_, mock_annotation_client_);
    test_api(*controller_).SetObserverForTest(&observer_);

    auto mock_extractor = std::make_unique<StrictMock<MockFilterExtractor>>(
        *mock_annotation_client_, *filter_store_, /*log_router=*/nullptr);
    mock_extractor_ = mock_extractor.get();
    test_api(*controller_).set_filter_extractor(std::move(mock_extractor));

    auto mock_generator =
        std::make_unique<StrictMock<MockFilterSuggestionGenerator>>(
            *mock_annotation_client_, *filter_store_, /*log_router=*/nullptr);
    mock_generator_ = mock_generator.get();
    test_api(*controller_)
        .set_filter_suggestion_generator(std::move(mock_generator));
  }

  void RunNavigationAndShowSuggestion(
      const FilterNavigationMetadata& metadata,
      const UrlFilterSuggestion& suggestion,
      const base::Uuid& annotation_id,
      const RetentionStateSnapshot& snapshot,
      MultistepFilterUiDelegate::SuggestionUiCallbacks& out_callbacks) {
    EXPECT_CALL(*mock_delegate_, ClearSuggestion());
    EXPECT_CALL(*mock_service_, HasUserProvidedConsent(metadata.navigation_id,
                                                       metadata.url.GetHost()))
        .WillOnce(Return(true));

    std::vector<std::string> supported_tasks = {suggestion.task_type};
    EXPECT_CALL(*mock_annotation_client(),
                GetSupportedTasks(metadata.url, _, metadata.navigation_id))
        .WillOnce(base::test::RunOnceCallback<1>(supported_tasks));

    FilterAnnotation annotation(annotation_id, suggestion.task_type,
                                metadata.url.GetHost(), base::Time::Now(), {});
    EXPECT_CALL(*mock_extractor_, ExtractAnnotationFromUrl(
                                      metadata.url, _, metadata.navigation_id))
        .WillOnce(base::test::RunOnceCallback<1>(annotation));

    EXPECT_CALL(*mock_generator_,
                GenerateSuggestion(metadata.url, supported_tasks, _,
                                   metadata.navigation_id))
        .WillOnce(base::test::RunOnceCallback<2>(suggestion));

    EXPECT_CALL(*mock_delegate_, ShowSuggestion(std::optional(suggestion), _))
        .WillOnce(testing::SaveArgByMove<1>(&out_callbacks));

    EXPECT_CALL(observer_,
                OnExtractionFinishedForTest(std::optional(annotation_id)));
    EXPECT_CALL(observer_,
                OnSuggestionGeneratedForTest(std::optional(suggestion)));

    controller_->OnNavigationFinished(metadata);

    // Trigger OnSuggestionShown
    EXPECT_CALL(*mock_service_, GetRetentionState()).WillOnce(Return(snapshot));
    EXPECT_CALL(*mock_service_, RecordSuggestionImpression()).Times(1);
    EXPECT_CALL(
        *mock_service_,
        DeleteAnnotationsForTask(suggestion.task_type, metadata.navigation_id,
                                 metadata.url.GetHost()))
        .Times(1);
    ASSERT_FALSE(out_callbacks.on_suggestion_shown.is_null());
    std::move(out_callbacks.on_suggestion_shown).Run();
  }

  void RunSuggestionApplicationFlow(
      const FilterNavigationMetadata& metadata,
      const FilterAnnotation& extraction_annotation,
      const std::string& expected_task_type = "Task1") {
    EXPECT_CALL(*mock_delegate_, ClearSuggestion());
    EXPECT_CALL(*mock_service_, HasUserProvidedConsent(metadata.navigation_id,
                                                       metadata.url.GetHost()))
        .WillOnce(Return(true));

    std::vector<std::string> supported_tasks = {expected_task_type};
    EXPECT_CALL(*mock_annotation_client(),
                GetSupportedTasks(metadata.url, _, metadata.navigation_id))
        .WillOnce(base::test::RunOnceCallback<1>(supported_tasks));

    EXPECT_CALL(*mock_extractor_, ExtractAnnotationFromUrl(
                                      metadata.url, _, metadata.navigation_id))
        .WillOnce(base::test::RunOnceCallback<1>(extraction_annotation));

    EXPECT_CALL(*mock_generator_, GenerateSuggestion)
        .WillOnce(base::test::RunOnceCallback<2>(std::nullopt));
    EXPECT_CALL(*mock_delegate_, ShowSuggestion(Eq(std::nullopt), _));
    EXPECT_CALL(observer_, OnExtractionFinishedForTest(
                               std::optional(extraction_annotation.id)));
    EXPECT_CALL(observer_, OnSuggestionGeneratedForTest(Eq(std::nullopt)));

    controller_->OnNavigationFinished(metadata);
  }

  void VerifyApplicationOutcomeRetentionHistograms(
      const base::HistogramTester& histogram_tester,
      base::span<const std::string_view> expected_slices,
      SuggestionApplicationResult expected_outcome,
      int expected_count = 1) {
    for (const auto& slice : kAllRetentionSlices) {
      std::string name = base::StrCat(
          {"MultistepFilter.ApplicationOutcome.ByRetention.", slice});
      if (std::ranges::contains(expected_slices, slice)) {
        histogram_tester.ExpectUniqueSample(name, expected_outcome,
                                            expected_count);
      } else {
        histogram_tester.ExpectTotalCount(name, 0);
      }
    }
  }

  void VerifyAcceptanceRetentionHistograms(
      const base::HistogramTester& histogram_tester,
      base::span<const std::string_view> expected_slices,
      SuggestionUserDecision expected_decision,
      int expected_count = 1) {
    for (const auto& slice : kAllRetentionSlices) {
      std::string initial_name = base::StrCat(
          {"MultistepFilter.Acceptance.InitialCue.ByRetention.", slice});
      std::string overall_name =
          base::StrCat({"MultistepFilter.Acceptance.ByRetention.", slice});
      if (std::ranges::contains(expected_slices, slice)) {
        histogram_tester.ExpectUniqueSample(initial_name, expected_decision,
                                            expected_count);
        histogram_tester.ExpectUniqueSample(overall_name, expected_decision,
                                            expected_count);
      } else {
        histogram_tester.ExpectTotalCount(initial_name, 0);
        histogram_tester.ExpectTotalCount(overall_name, 0);
      }
    }
  }

  void DestroyController() {
    mock_generator_ = nullptr;
    mock_extractor_ = nullptr;
    controller_.reset();
  }

  void TearDown() override {
    DestroyController();
    mock_delegate_.reset();
    filter_store_ = nullptr;
    mock_annotation_client_ = nullptr;
    mock_service_.reset();
  }

  StrictMock<MockAnnotationIndexClient>* mock_annotation_client() {
    return mock_annotation_client_;
  }

  FilterNavigationMetadata CreateMetadata(
      int64_t navigation_id,
      const GURL& url,
      bool is_cryptographic = true,
      bool is_error_page = false,
      int net_error_code = 0,
      std::optional<UrlFilterSuggestion> applied_suggestion = std::nullopt) {
    FilterNavigationMetadata metadata;
    metadata.navigation_id = navigation_id;
    metadata.navigation_start_time = base::TimeTicks::Now();
    metadata.navigation_finish_time = base::TimeTicks::Now();
    metadata.url = url;
    metadata.is_cryptographic_scheme = is_cryptographic;
    metadata.is_error_page_navigation = is_error_page;
    metadata.net_error_code = net_error_code;
    metadata.applied_suggestion = std::move(applied_suggestion);
    return metadata;
  }

  void InitializeTrackerWithNavigation() {
    FilterNavigationMetadata metadata =
        CreateMetadata(1, GURL("https://example.com"));
    metadata.has_user_gesture = true;
    EXPECT_CALL(*mock_delegate_, ClearSuggestion());
    EXPECT_CALL(*mock_service_, HasUserProvidedConsent(metadata.navigation_id,
                                                       metadata.url.GetHost()))
        .WillOnce(Return(true));
    EXPECT_CALL(*mock_annotation_client(),
                GetSupportedTasks(metadata.url, _, metadata.navigation_id))
        .WillOnce(base::test::RunOnceCallback<1>(std::vector<std::string>()));
    EXPECT_CALL(*mock_delegate_, ShowSuggestion(Eq(std::nullopt), _));
    EXPECT_CALL(observer_, OnExtractionFinishedForTest(Eq(std::nullopt)));
    EXPECT_CALL(observer_, OnSuggestionGeneratedForTest(Eq(std::nullopt)));

    controller_->OnNavigationFinished(metadata);
  }

  UrlFilterSuggestion CreateDefaultSuggestion(
      int64_t triggering_navigation_id,
      const GURL& url = GURL("https://example.com/")) {
    return UrlFilterSuggestion(UrlFilterSuggestion::Params{
        .navigation_url = url,
        .triggering_navigation_id = triggering_navigation_id,
        .triggering_host = "example.com",
        .task_type = "Task1"});
  }

  UrlFilterSuggestion CreateSuggestionWithFacet(
      const std::string& key,
      const std::u16string& label,
      const std::string& value,
      int64_t triggering_navigation_id,
      const GURL& url = GURL("https://example.com/")) {
    UrlFilterSuggestion::Params params;
    params.navigation_url = url;
    params.triggering_navigation_id = triggering_navigation_id;
    params.triggering_host = "example.com";
    params.task_type = "Task1";
    params.attribute_ui_labels.emplace_back(
        FilterSuggestionCandidateAttribute(key, label),
        FilterAttribute(key, value));
    return UrlFilterSuggestion(std::move(params));
  }

  void ExpectNoExtractionOrSuggestion() {
    EXPECT_CALL(*mock_delegate_, ClearSuggestion());
    EXPECT_CALL(*mock_delegate_, ShowSuggestion(Eq(std::nullopt), _));
    EXPECT_CALL(observer_, OnExtractionFinishedForTest(Eq(std::nullopt)));
    EXPECT_CALL(observer_, OnSuggestionGeneratedForTest(Eq(std::nullopt)));
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  signin::IdentityTestEnvironment identity_test_env_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<StrictMock<MockMultistepFilterService>> mock_service_;
  std::unique_ptr<StrictMock<MockMultistepFilterUiDelegate>> mock_delegate_;
  raw_ptr<StrictMock<MockFilterExtractor>> mock_extractor_ = nullptr;
  raw_ptr<StrictMock<MockFilterSuggestionGenerator>> mock_generator_ = nullptr;
  raw_ptr<StrictMock<MockAnnotationIndexClient>> mock_annotation_client_ =
      nullptr;
  raw_ptr<FilterStore> filter_store_ = nullptr;
  StrictMock<MockObserver> observer_;
  std::unique_ptr<FilterTabController> controller_;
};

// Tests that non-cryptographic insecure schemes (HTTP) instantly trigger early
// fail-safes.
TEST_F(FilterTabControllerTest, HttpNavigation) {
  FilterNavigationMetadata metadata = CreateMetadata(
      /*navigation_id=*/1, GURL("http://example.com"),
      /*is_cryptographic=*/false);
  metadata.is_http_allowed_for_testing = false;

  ExpectNoExtractionOrSuggestion();

  controller_->OnNavigationFinished(metadata);
}

// Tests that network HTTP 4xx/5xx errors instantly trigger early fail-safes.
TEST_F(FilterTabControllerTest, ErrorPageNavigation) {
  FilterNavigationMetadata metadata = CreateMetadata(
      /*navigation_id=*/2, GURL("https://example.com"),
      /*is_cryptographic=*/true, /*is_error_page=*/true,
      /*net_error_code=*/-106);

  ExpectNoExtractionOrSuggestion();

  controller_->OnNavigationFinished(metadata);
}

// Tests that FilterTabController aborts immediately when consent is false.
TEST_F(FilterTabControllerTest, SuppressExtractionAndGenerationOnConsentFalse) {
  FilterNavigationMetadata metadata =
      CreateMetadata(3, GURL("https://example.com"));
  metadata.prev_url = GURL("https://different.com");
  metadata.has_user_gesture = true;

  ExpectNoExtractionOrSuggestion();

  EXPECT_CALL(*mock_service_, HasUserProvidedConsent(metadata.navigation_id,
                                                     metadata.url.GetHost()))
      .WillOnce(Return(false));

  controller_->OnNavigationFinished(metadata);
}

// Tests that FilterTabController aborts immediately when model execution is
// disabled.
TEST_F(FilterTabControllerTest,
       SuppressExtractionAndGenerationOnModelExecutionDisabled) {
  FilterNavigationMetadata metadata =
      CreateMetadata(3, GURL("https://example.com"));
  metadata.prev_url = GURL("https://different.com");
  metadata.has_user_gesture = true;

  ExpectNoExtractionOrSuggestion();

  EXPECT_CALL(*mock_service_, CanUseModelExecutionFeatures())
      .WillOnce(Return(false));

  controller_->OnNavigationFinished(metadata);
}

// Tests that FilterTabController aborts immediately when smart suggestions are
// disabled in settings.
TEST_F(FilterTabControllerTest,
       SuppressExtractionAndGenerationOnSmartSuggestionsDisabled) {
  FilterNavigationMetadata metadata =
      CreateMetadata(3, GURL("https://example.com"));
  metadata.prev_url = GURL("https://different.com");
  metadata.has_user_gesture = true;

  ExpectNoExtractionOrSuggestion();

  EXPECT_CALL(*mock_service_, IsSmartSuggestionsEnabled())
      .WillOnce(Return(false));

  controller_->OnNavigationFinished(metadata);
}

// Tests that SPA (Single Page Application) fragment routing preserves existing
// suggestion UI, but aborts early when consent is false.
TEST_F(FilterTabControllerTest, SameDocumentNavigationConsentFalse) {
  FilterNavigationMetadata metadata =
      CreateMetadata(4, GURL("https://example.com/#section1"));
  metadata.prev_url = GURL("https://example.com/");
  metadata.is_same_document_navigation = true;
  metadata.has_user_gesture = true;

  EXPECT_CALL(*mock_delegate_, ClearSuggestion).Times(0);
  EXPECT_CALL(*mock_delegate_, ShowSuggestion(Eq(std::nullopt), _));

  EXPECT_CALL(*mock_service_, HasUserProvidedConsent(metadata.navigation_id,
                                                     metadata.url.GetHost()))
      .WillOnce(Return(false));

  EXPECT_CALL(observer_, OnExtractionFinishedForTest(Eq(std::nullopt)));
  EXPECT_CALL(observer_, OnSuggestionGeneratedForTest(Eq(std::nullopt)));

  controller_->OnNavigationFinished(metadata);
}

// Tests that explicit user page reloads suppress redundant UI triggers while
// cleanly preserving the current suggestion.
TEST_F(FilterTabControllerTest, SameUrlReCommitNavigation) {
  FilterNavigationMetadata metadata =
      CreateMetadata(5, GURL("https://example.com/"));
  metadata.prev_url = GURL("https://example.com/");
  metadata.has_user_gesture = true;

  EXPECT_CALL(*mock_delegate_, ClearSuggestion).Times(0);
  EXPECT_CALL(*mock_delegate_, ShowSuggestion).Times(0);

  controller_->OnNavigationFinished(metadata);
}

// Tests that a navigation without user gesture to the same host and path
// preserves the existing suggestion UI.
TEST_F(FilterTabControllerTest,
       NavigationWithoutUserGestureSamePathPreservesSuggestion) {
  FilterNavigationMetadata metadata =
      CreateMetadata(1, GURL("https://example.com/"));
  metadata.has_user_gesture = true;

  UrlFilterSuggestion suggestion =
      CreateDefaultSuggestion(metadata.navigation_id, metadata.url);
  base::Uuid annotation_id = base::Uuid::GenerateRandomV4();
  RetentionStateSnapshot snapshot;
  MultistepFilterUiDelegate::SuggestionUiCallbacks callbacks;

  RunNavigationAndShowSuggestion(metadata, suggestion, annotation_id, snapshot,
                                 callbacks);

  FilterNavigationMetadata second_metadata =
      CreateMetadata(2, GURL("https://example.com/?query=1"));
  second_metadata.prev_url = GURL("https://example.com/");
  second_metadata.has_user_gesture = false;

  EXPECT_CALL(*mock_delegate_, ClearSuggestion).Times(0);
  EXPECT_CALL(*mock_delegate_, ShowSuggestion).Times(0);
  EXPECT_CALL(observer_, OnExtractionFinishedForTest).Times(0);
  EXPECT_CALL(observer_, OnSuggestionGeneratedForTest).Times(0);

  controller_->OnNavigationFinished(second_metadata);
}

// Tests that a navigation without user gesture to the same host but different
// path clears the existing suggestion UI.
TEST_F(FilterTabControllerTest,
       NavigationWithoutUserGestureDifferentPathClearsSuggestion) {
  FilterNavigationMetadata metadata =
      CreateMetadata(1, GURL("https://example.com/"));
  metadata.has_user_gesture = true;

  UrlFilterSuggestion suggestion =
      CreateDefaultSuggestion(metadata.navigation_id, metadata.url);
  base::Uuid annotation_id = base::Uuid::GenerateRandomV4();
  RetentionStateSnapshot snapshot;
  MultistepFilterUiDelegate::SuggestionUiCallbacks callbacks;

  RunNavigationAndShowSuggestion(metadata, suggestion, annotation_id, snapshot,
                                 callbacks);

  FilterNavigationMetadata second_metadata =
      CreateMetadata(2, GURL("https://example.com/different_page"));
  second_metadata.prev_url = GURL("https://example.com/");
  second_metadata.has_user_gesture = false;

  EXPECT_CALL(*mock_delegate_, ClearSuggestion()).Times(1);
  EXPECT_CALL(*mock_delegate_, ShowSuggestion(Eq(std::nullopt), _));
  EXPECT_CALL(observer_, OnExtractionFinishedForTest(Eq(std::nullopt)));
  EXPECT_CALL(observer_, OnSuggestionGeneratedForTest(Eq(std::nullopt)));

  controller_->OnNavigationFinished(second_metadata);
}

// Tests that a navigation without user gesture to a different host clears the
// existing suggestion UI.
TEST_F(FilterTabControllerTest,
       NavigationWithoutUserGestureDifferentHostClearsSuggestion) {
  FilterNavigationMetadata metadata =
      CreateMetadata(1, GURL("https://example.com/"));
  metadata.has_user_gesture = true;

  UrlFilterSuggestion suggestion =
      CreateDefaultSuggestion(metadata.navigation_id, metadata.url);
  base::Uuid annotation_id = base::Uuid::GenerateRandomV4();
  RetentionStateSnapshot snapshot;
  MultistepFilterUiDelegate::SuggestionUiCallbacks callbacks;

  RunNavigationAndShowSuggestion(metadata, suggestion, annotation_id, snapshot,
                                 callbacks);

  FilterNavigationMetadata second_metadata =
      CreateMetadata(2, GURL("https://different.com/"));
  second_metadata.prev_url = GURL("https://example.com/");
  second_metadata.has_user_gesture = false;

  EXPECT_CALL(*mock_delegate_, ClearSuggestion()).Times(1);
  EXPECT_CALL(*mock_delegate_, ShowSuggestion(Eq(std::nullopt), _));
  EXPECT_CALL(observer_, OnExtractionFinishedForTest(Eq(std::nullopt)));
  EXPECT_CALL(observer_, OnSuggestionGeneratedForTest(Eq(std::nullopt)));

  controller_->OnNavigationFinished(second_metadata);
}

// Tests that a background redirect on the same page does not interrupt an
// ongoing extraction flow from the previous navigation.
TEST_F(FilterTabControllerTest, BackgroundRedirectDoesNotInterruptOngoingFlow) {
  FilterNavigationMetadata metadata1 =
      CreateMetadata(1, GURL("https://example.com/"));
  metadata1.has_user_gesture = true;

  base::OnceCallback<void(std::vector<std::string>)> tasks_callback;
  EXPECT_CALL(*mock_delegate_, ClearSuggestion());
  EXPECT_CALL(*mock_service_, HasUserProvidedConsent(metadata1.navigation_id,
                                                     metadata1.url.GetHost()))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_annotation_client(),
              GetSupportedTasks(metadata1.url, _, metadata1.navigation_id))
      .WillOnce(testing::SaveArgByMove<1>(&tasks_callback));

  controller_->OnNavigationFinished(metadata1);
  ASSERT_FALSE(tasks_callback.is_null());

  FilterNavigationMetadata metadata2 =
      CreateMetadata(2, GURL("https://example.com/?query=1"));
  metadata2.prev_url = GURL("https://example.com/");
  metadata2.has_user_gesture = false;

  EXPECT_CALL(*mock_delegate_, ClearSuggestion).Times(0);
  EXPECT_CALL(*mock_delegate_, ShowSuggestion).Times(0);

  controller_->OnNavigationFinished(metadata2);

  std::vector<std::string> supported_tasks = {"Task1"};
  base::Uuid expected_id = base::Uuid::GenerateRandomV4();
  FilterAnnotation annotation(expected_id, "Task1", "example.com",
                              base::Time::Now(), {});
  UrlFilterSuggestion expected_suggestion =
      CreateDefaultSuggestion(metadata1.navigation_id, metadata1.url);

  EXPECT_CALL(*mock_extractor_, ExtractAnnotationFromUrl(
                                    metadata1.url, _, metadata1.navigation_id))
      .WillOnce(base::test::RunOnceCallback<1>(annotation));
  EXPECT_CALL(*mock_generator_,
              GenerateSuggestion(metadata1.url, supported_tasks, _,
                                 metadata1.navigation_id))
      .WillOnce(base::test::RunOnceCallback<2>(expected_suggestion));
  EXPECT_CALL(*mock_delegate_,
              ShowSuggestion(std::optional(expected_suggestion), _));
  EXPECT_CALL(observer_,
              OnExtractionFinishedForTest(std::optional(expected_id)));
  EXPECT_CALL(observer_,
              OnSuggestionGeneratedForTest(std::optional(expected_suggestion)));

  std::move(tasks_callback).Run(supported_tasks);
}

// Tests that a background redirect on the same page does not reset the
// latency base (navigation finish time) in the metrics tracker.
TEST_F(FilterTabControllerTest, BackgroundRedirectDoesNotResetLatencyBase) {
  base::HistogramTester histogram_tester;

  FilterNavigationMetadata metadata1 =
      CreateMetadata(1, GURL("https://example.com/"));
  metadata1.has_user_gesture = true;

  base::OnceCallback<void(std::vector<std::string>)> tasks_callback;
  EXPECT_CALL(*mock_delegate_, ClearSuggestion());
  EXPECT_CALL(*mock_service_, HasUserProvidedConsent(metadata1.navigation_id,
                                                     metadata1.url.GetHost()))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_annotation_client(),
              GetSupportedTasks(metadata1.url, _, metadata1.navigation_id))
      .WillOnce(testing::SaveArgByMove<1>(&tasks_callback));

  controller_->OnNavigationFinished(metadata1);
  ASSERT_FALSE(tasks_callback.is_null());

  task_environment_.FastForwardBy(base::Seconds(2));

  FilterNavigationMetadata metadata2 =
      CreateMetadata(2, GURL("https://example.com/?query=1"));
  metadata2.prev_url = GURL("https://example.com/");
  metadata2.has_user_gesture = false;

  controller_->OnNavigationFinished(metadata2);

  task_environment_.FastForwardBy(base::Seconds(3));

  std::vector<std::string> supported_tasks = {"Task1"};
  base::Uuid expected_id = base::Uuid::GenerateRandomV4();
  FilterAnnotation annotation(expected_id, "Task1", "example.com",
                              base::Time::Now(), {});
  UrlFilterSuggestion expected_suggestion =
      CreateDefaultSuggestion(metadata1.navigation_id, metadata1.url);

  EXPECT_CALL(*mock_extractor_, ExtractAnnotationFromUrl(
                                    metadata1.url, _, metadata1.navigation_id))
      .WillOnce(base::test::RunOnceCallback<1>(annotation));
  EXPECT_CALL(*mock_generator_,
              GenerateSuggestion(metadata1.url, supported_tasks, _,
                                 metadata1.navigation_id))
      .WillOnce(base::test::RunOnceCallback<2>(expected_suggestion));

  MultistepFilterUiDelegate::SuggestionUiCallbacks captured_callbacks;
  EXPECT_CALL(*mock_delegate_,
              ShowSuggestion(std::optional(expected_suggestion), _))
      .WillOnce(testing::SaveArgByMove<1>(&captured_callbacks));

  EXPECT_CALL(observer_,
              OnExtractionFinishedForTest(std::optional(expected_id)));
  EXPECT_CALL(observer_,
              OnSuggestionGeneratedForTest(std::optional(expected_suggestion)));

  std::move(tasks_callback).Run(supported_tasks);

  EXPECT_CALL(*mock_service_, GetRetentionState())
      .WillOnce(Return(RetentionStateSnapshot()));
  EXPECT_CALL(*mock_service_, RecordSuggestionImpression()).Times(1);
  EXPECT_CALL(*mock_service_,
              DeleteAnnotationsForTask("Task1", metadata1.navigation_id, _))
      .Times(1);

  ASSERT_FALSE(captured_callbacks.on_suggestion_shown.is_null());
  std::move(captured_callbacks.on_suggestion_shown).Run();
  DestroyController();

  histogram_tester.ExpectUniqueTimeSample(
      kMultistepFilterTimeNavigationToSuggestionShownHistogram,
      base::Seconds(5), 1);
}

// Tests that SPA (Single Page Application) fragment routing preserves existing
// suggestion UI, but correctly cascades to new extractions and suggestions on
// success.
TEST_F(FilterTabControllerTest, SameDocumentNavigationSuccess) {
  FilterNavigationMetadata metadata =
      CreateMetadata(6, GURL("https://example.com/#section1"));
  metadata.prev_url = GURL("https://example.com/");
  metadata.is_same_document_navigation = true;
  metadata.has_user_gesture = true;

  EXPECT_CALL(*mock_delegate_, ClearSuggestion).Times(0);
  EXPECT_CALL(*mock_service_, HasUserProvidedConsent(metadata.navigation_id,
                                                     metadata.url.GetHost()))
      .WillOnce(Return(true));

  std::vector<std::string> supported_tasks = {"Task1"};
  EXPECT_CALL(*mock_annotation_client(),
              GetSupportedTasks(metadata.url, _, metadata.navigation_id))
      .WillOnce(base::test::RunOnceCallback<1>(supported_tasks));

  base::Uuid expected_id = base::Uuid::GenerateRandomV4();
  FilterAnnotation annotation(expected_id, "Task1", "example.com",
                              base::Time::Now(), {});

  EXPECT_CALL(*mock_extractor_,
              ExtractAnnotationFromUrl(metadata.url, _, metadata.navigation_id))
      .WillOnce(base::test::RunOnceCallback<1>(annotation));

  UrlFilterSuggestion expected_suggestion =
      CreateDefaultSuggestion(metadata.navigation_id, metadata.url);

  EXPECT_CALL(*mock_generator_,
              GenerateSuggestion(metadata.url, supported_tasks, _,
                                 metadata.navigation_id))
      .WillOnce(base::test::RunOnceCallback<2>(expected_suggestion));

  EXPECT_CALL(*mock_delegate_,
              ShowSuggestion(std::optional(expected_suggestion), _));
  EXPECT_CALL(observer_,
              OnExtractionFinishedForTest(std::optional(expected_id)));
  EXPECT_CALL(observer_,
              OnSuggestionGeneratedForTest(std::optional(expected_suggestion)));

  controller_->OnNavigationFinished(metadata);
}

// Tests that if the background RPC returns zero valid tasks for the domain,
// generation aborts gracefully.
TEST_F(FilterTabControllerTest,
       SuppressExtractionAndGenerationOnEmptySupportedTasks) {
  FilterNavigationMetadata metadata =
      CreateMetadata(7, GURL("https://example.com/"));
  metadata.has_user_gesture = true;

  EXPECT_CALL(*mock_delegate_, ClearSuggestion());
  EXPECT_CALL(*mock_delegate_, ShowSuggestion(Eq(std::nullopt), _));

  EXPECT_CALL(*mock_service_, HasUserProvidedConsent(metadata.navigation_id,
                                                     metadata.url.GetHost()))
      .WillOnce(Return(true));

  std::vector<std::string> empty_tasks;
  EXPECT_CALL(*mock_annotation_client(),
              GetSupportedTasks(metadata.url, _, metadata.navigation_id))
      .WillOnce(base::test::RunOnceCallback<1>(empty_tasks));

  EXPECT_CALL(observer_, OnExtractionFinishedForTest(Eq(std::nullopt)));
  EXPECT_CALL(observer_, OnSuggestionGeneratedForTest(Eq(std::nullopt)));

  controller_->OnNavigationFinished(metadata);
}

// Tests that clicking Accept and executing the resulting filter navigation
// correctly prevents generating a cyclical suggestion on the landing page.
TEST_F(FilterTabControllerTest, SuppressGenerationOnFilterInitiatedNavigation) {
  FilterNavigationMetadata metadata =
      CreateMetadata(8, GURL("https://example.com/filtered"));
  metadata.has_user_gesture = true;
  metadata.was_filter_initiated_navigation = true;

  EXPECT_CALL(*mock_delegate_, ClearSuggestion());
  // Suggestion failsafe will still trigger for the generator since we don't
  // start it.
  EXPECT_CALL(*mock_delegate_, ShowSuggestion(Eq(std::nullopt), _));

  EXPECT_CALL(*mock_service_, HasUserProvidedConsent(metadata.navigation_id,
                                                     metadata.url.GetHost()))
      .WillOnce(Return(true));

  std::vector<std::string> supported_tasks = {"Task1"};
  EXPECT_CALL(*mock_annotation_client(),
              GetSupportedTasks(metadata.url, _, metadata.navigation_id))
      .WillOnce(base::test::RunOnceCallback<1>(supported_tasks));

  base::Uuid expected_id = base::Uuid::GenerateRandomV4();
  FilterAnnotation annotation(expected_id, "Task1", "example.com",
                              base::Time::Now(), {});

  EXPECT_CALL(*mock_extractor_,
              ExtractAnnotationFromUrl(metadata.url, _, metadata.navigation_id))
      .WillOnce(base::test::RunOnceCallback<1>(annotation));

  // Generator is NOT called.
  EXPECT_CALL(*mock_generator_, GenerateSuggestion).Times(0);

  EXPECT_CALL(observer_,
              OnExtractionFinishedForTest(std::optional(expected_id)));
  EXPECT_CALL(observer_, OnSuggestionGeneratedForTest(Eq(std::nullopt)));

  controller_->OnNavigationFinished(metadata);
}

// Tests the complete end-to-end operational cascade successfully executing
// extraction and suggestion generation.
TEST_F(FilterTabControllerTest, SuccessfulExtractionAndGenerationCascade) {
  FilterNavigationMetadata metadata =
      CreateMetadata(9, GURL("https://example.com/"));
  metadata.has_user_gesture = true;

  EXPECT_CALL(*mock_delegate_, ClearSuggestion());

  EXPECT_CALL(*mock_service_, HasUserProvidedConsent(metadata.navigation_id,
                                                     metadata.url.GetHost()))
      .WillOnce(Return(true));

  std::vector<std::string> supported_tasks = {"Task1"};
  EXPECT_CALL(*mock_annotation_client(),
              GetSupportedTasks(metadata.url, _, metadata.navigation_id))
      .WillOnce(base::test::RunOnceCallback<1>(supported_tasks));

  base::Uuid expected_id = base::Uuid::GenerateRandomV4();
  FilterAnnotation annotation(expected_id, "Task1", "example.com",
                              base::Time::Now(), {});

  EXPECT_CALL(*mock_extractor_,
              ExtractAnnotationFromUrl(metadata.url, _, metadata.navigation_id))
      .WillOnce(base::test::RunOnceCallback<1>(annotation));

  UrlFilterSuggestion expected_suggestion =
      CreateDefaultSuggestion(metadata.navigation_id, metadata.url);

  EXPECT_CALL(*mock_generator_,
              GenerateSuggestion(metadata.url, supported_tasks, _,
                                 metadata.navigation_id))
      .WillOnce(base::test::RunOnceCallback<2>(expected_suggestion));

  EXPECT_CALL(*mock_delegate_,
              ShowSuggestion(std::optional(expected_suggestion), _));

  EXPECT_CALL(observer_,
              OnExtractionFinishedForTest(std::optional(expected_id)));
  EXPECT_CALL(observer_,
              OnSuggestionGeneratedForTest(std::optional(expected_suggestion)));

  controller_->OnNavigationFinished(metadata);
}

// Tests that non-cryptographic HTTP navigation successfully triggers extraction
// and generation cascade when the testing switch is explicitly allowed.
TEST_F(FilterTabControllerTest, HttpNavigationWithTestingSwitch) {
  FilterNavigationMetadata metadata = CreateMetadata(
      /*navigation_id=*/10, GURL("http://example.com/"),
      /*is_cryptographic=*/false);
  metadata.is_http_allowed_for_testing = true;
  metadata.has_user_gesture = true;

  EXPECT_CALL(*mock_delegate_, ClearSuggestion());

  EXPECT_CALL(*mock_service_, HasUserProvidedConsent(metadata.navigation_id,
                                                     metadata.url.GetHost()))
      .WillOnce(Return(true));

  std::vector<std::string> supported_tasks = {"Task1"};
  EXPECT_CALL(*mock_annotation_client(),
              GetSupportedTasks(metadata.url, _, metadata.navigation_id))
      .WillOnce(base::test::RunOnceCallback<1>(supported_tasks));

  base::Uuid expected_id = base::Uuid::GenerateRandomV4();
  FilterAnnotation annotation(expected_id, "Task1", "example.com",
                              base::Time::Now(), {});

  EXPECT_CALL(*mock_extractor_,
              ExtractAnnotationFromUrl(metadata.url, _, metadata.navigation_id))
      .WillOnce(base::test::RunOnceCallback<1>(annotation));

  UrlFilterSuggestion expected_suggestion =
      CreateDefaultSuggestion(metadata.navigation_id, metadata.url);

  EXPECT_CALL(*mock_generator_,
              GenerateSuggestion(metadata.url, supported_tasks, _,
                                 metadata.navigation_id))
      .WillOnce(base::test::RunOnceCallback<2>(expected_suggestion));

  EXPECT_CALL(*mock_delegate_,
              ShowSuggestion(std::optional(expected_suggestion), _));

  EXPECT_CALL(observer_,
              OnExtractionFinishedForTest(std::optional(expected_id)));
  EXPECT_CALL(observer_,
              OnSuggestionGeneratedForTest(std::optional(expected_suggestion)));

  controller_->OnNavigationFinished(metadata);
}

// Tests that FilterTabController aborts immediately when there is no user
// gesture on a new navigation.
TEST_F(FilterTabControllerTest,
       SuppressExtractionAndGenerationOnNoUserGesture) {
  FilterNavigationMetadata metadata =
      CreateMetadata(11, GURL("https://example.com/"));
  metadata.has_user_gesture = false;

  EXPECT_CALL(*mock_delegate_, ClearSuggestion());
  EXPECT_CALL(*mock_delegate_, ShowSuggestion(Eq(std::nullopt), _));

  EXPECT_CALL(observer_, OnExtractionFinishedForTest(Eq(std::nullopt)));
  EXPECT_CALL(observer_, OnSuggestionGeneratedForTest(Eq(std::nullopt)));

  controller_->OnNavigationFinished(metadata);
}

// Tests that FilterTabController notifies the service when a suggestion is
// shown.
TEST_F(FilterTabControllerTest, OnSuggestionShownNotifiesService) {
  InitializeTrackerWithNavigation();
  UrlFilterSuggestion suggestion(
      UrlFilterSuggestion::Params{.triggering_navigation_id = 42,
                                  .triggering_host = "example.com",
                                  .task_type = "task_type_1"});

  EXPECT_CALL(*mock_service_, GetRetentionState())
      .WillOnce(Return(RetentionStateSnapshot()));
  EXPECT_CALL(*mock_service_, RecordSuggestionImpression()).Times(1);
  EXPECT_CALL(*mock_service_,
              DeleteAnnotationsForTask("task_type_1", 42, "example.com"))
      .Times(1);

  controller_->OnSuggestionShown(suggestion);
}

// Tests that FilterTabController notifies the service when the user makes a
// decision on a suggestion.
TEST_F(FilterTabControllerTest, OnUserDecisionNotifiesService) {
  InitializeTrackerWithNavigation();
  UrlFilterSuggestion suggestion(
      UrlFilterSuggestion::Params{.navigation_url = GURL("https://example.com"),
                                  .triggering_navigation_id = 42,
                                  .triggering_host = "example.com",
                                  .task_type = "task_type_1"});

  EXPECT_CALL(*mock_service_, GetRetentionState())
      .WillOnce(Return(RetentionStateSnapshot()));
  EXPECT_CALL(*mock_service_, RecordSuggestionImpression()).Times(1);
  EXPECT_CALL(*mock_service_,
              DeleteAnnotationsForTask("task_type_1", 42, "example.com"))
      .Times(1);

  controller_->OnSuggestionShown(suggestion);

  EXPECT_CALL(*mock_service_, RecordUserInteractionWithSuggestion(
                                  SuggestionUserDecision::kAccepted))
      .Times(1);

  controller_->OnUserDecision(SuggestionUserDecision::kAccepted);
}

// Tests that FilterTabController wires the suggestion callbacks correctly.
TEST_F(FilterTabControllerTest, SuggestionCallbacksWiredCorrectly) {
  FilterNavigationMetadata metadata =
      CreateMetadata(9, GURL("https://example.com/"));
  metadata.has_user_gesture = true;

  EXPECT_CALL(*mock_delegate_, ClearSuggestion());

  EXPECT_CALL(*mock_service_, HasUserProvidedConsent(metadata.navigation_id,
                                                     metadata.url.GetHost()))
      .WillOnce(Return(true));

  std::vector<std::string> supported_tasks = {"Task1"};
  EXPECT_CALL(*mock_annotation_client(),
              GetSupportedTasks(metadata.url, _, metadata.navigation_id))
      .WillOnce(base::test::RunOnceCallback<1>(supported_tasks));

  base::Uuid expected_id = base::Uuid::GenerateRandomV4();
  FilterAnnotation annotation(expected_id, "Task1", "example.com",
                              base::Time::Now(), {});

  EXPECT_CALL(*mock_extractor_,
              ExtractAnnotationFromUrl(metadata.url, _, metadata.navigation_id))
      .WillOnce(base::test::RunOnceCallback<1>(annotation));

  UrlFilterSuggestion expected_suggestion =
      CreateDefaultSuggestion(metadata.navigation_id, metadata.url);

  EXPECT_CALL(*mock_generator_,
              GenerateSuggestion(metadata.url, supported_tasks, _,
                                 metadata.navigation_id))
      .WillOnce(base::test::RunOnceCallback<2>(expected_suggestion));

  MultistepFilterUiDelegate::SuggestionUiCallbacks captured_callbacks;
  EXPECT_CALL(*mock_delegate_,
              ShowSuggestion(std::optional(expected_suggestion), _))
      .WillOnce(testing::SaveArgByMove<1>(&captured_callbacks));

  EXPECT_CALL(observer_,
              OnExtractionFinishedForTest(std::optional(expected_id)));
  EXPECT_CALL(observer_,
              OnSuggestionGeneratedForTest(std::optional(expected_suggestion)));

  controller_->OnNavigationFinished(metadata);

  // 1. Verify on_suggestion_shown callback.
  EXPECT_CALL(*mock_service_, GetRetentionState())
      .WillOnce(Return(RetentionStateSnapshot()));
  EXPECT_CALL(*mock_service_, RecordSuggestionImpression()).Times(1);
  EXPECT_CALL(
      *mock_service_,
      DeleteAnnotationsForTask("Task1", metadata.navigation_id, "example.com"))
      .Times(1);
  ASSERT_FALSE(captured_callbacks.on_suggestion_shown.is_null());
  std::move(captured_callbacks.on_suggestion_shown).Run();

  // 2. Verify on_user_interaction callback.
  EXPECT_CALL(*mock_service_, RecordUserInteractionWithSuggestion(
                                  SuggestionUserDecision::kAccepted))
      .Times(1);
  ASSERT_FALSE(captured_callbacks.on_user_interaction.is_null());
  std::move(captured_callbacks.on_user_interaction)
      .Run(SuggestionUserDecision::kAccepted);
}

// Tests that FilterTabController logs the correct histograms when the initial
// cue is accepted.
TEST_F(FilterTabControllerTest, HistogramLoggingInitialCueAccepted) {
  base::HistogramTester histogram_tester;

  FilterNavigationMetadata metadata =
      CreateMetadata(1, GURL("https://example.com"));
  metadata.has_user_gesture = true;

  base::Uuid expected_id = base::Uuid::GenerateRandomV4();
  UrlFilterSuggestion expected_suggestion = CreateSuggestionWithFacet(
      "key1", u"Label1", "value1", metadata.navigation_id, metadata.url);

  MultistepFilterUiDelegate::SuggestionUiCallbacks captured_callbacks;
  RunNavigationAndShowSuggestion(metadata, expected_suggestion, expected_id,
                                 RetentionStateSnapshot(), captured_callbacks);

  // When on_user_interaction(kAccepted) is run:
  EXPECT_CALL(*mock_service_, RecordUserInteractionWithSuggestion(
                                  SuggestionUserDecision::kAccepted))
      .Times(1);
  ASSERT_FALSE(captured_callbacks.on_user_interaction.is_null());
  std::move(captured_callbacks.on_user_interaction)
      .Run(SuggestionUserDecision::kAccepted);

  histogram_tester.ExpectUniqueSample(
      kMultistepFilterAcceptanceInitialCueHistogram,
      SuggestionUserDecision::kAccepted, 1);
  histogram_tester.ExpectUniqueSample(kMultistepFilterAcceptanceHistogram,
                                      SuggestionUserDecision::kAccepted, 1);
  histogram_tester.ExpectTotalCount(
      kMultistepFilterAcceptanceReopenedCueHistogram, 0);
  histogram_tester.ExpectUniqueSample(
      kMultistepFilterNumberOfFacetsShownHistogram, 1, 1);
  histogram_tester.ExpectUniqueSample(
      "MultistepFilter.NumberOfFacetsShown.ByTask.Task1", 1, 1);

  // Default snapshot is FirstImpression.
  histogram_tester.ExpectUniqueSample(
      "MultistepFilter.Acceptance.InitialCue.ByRetention.FirstImpression",
      SuggestionUserDecision::kAccepted, 1);
  histogram_tester.ExpectUniqueSample(
      "MultistepFilter.Acceptance.ByRetention.FirstImpression",
      SuggestionUserDecision::kAccepted, 1);
}

// Tests that FilterTabController logs the correct histograms when the reopened
// cue is ignored on navigation.
TEST_F(FilterTabControllerTest,
       HistogramLoggingReopenedCueIgnoredOnNavigation) {
  base::HistogramTester histogram_tester;

  FilterNavigationMetadata metadata =
      CreateMetadata(1, GURL("https://example.com"));
  metadata.has_user_gesture = true;

  base::Uuid expected_id = base::Uuid::GenerateRandomV4();
  UrlFilterSuggestion expected_suggestion =
      CreateDefaultSuggestion(metadata.navigation_id, metadata.url);

  MultistepFilterUiDelegate::SuggestionUiCallbacks captured_callbacks;
  RunNavigationAndShowSuggestion(metadata, expected_suggestion, expected_id,
                                 RetentionStateSnapshot(), captured_callbacks);

  // 2. User reopens from omnibox.
  ASSERT_FALSE(captured_callbacks.on_suggestion_reopened.is_null());
  std::move(captured_callbacks.on_suggestion_reopened).Run();

  // 3. Navigate away (simulates clearing suggestion with kIgnored).
  FilterNavigationMetadata new_metadata =
      CreateMetadata(2, GURL("https://different-example.com"));
  new_metadata.has_user_gesture = true;

  EXPECT_CALL(*mock_delegate_, ClearSuggestion()).Times(1);
  EXPECT_CALL(*mock_delegate_, ShowSuggestion(Eq(std::nullopt), _)).Times(1);
  EXPECT_CALL(*mock_service_,
              HasUserProvidedConsent(new_metadata.navigation_id,
                                     new_metadata.url.GetHost()))
      .WillOnce(Return(false));

  EXPECT_CALL(observer_, OnExtractionFinishedForTest(Eq(std::nullopt)));
  EXPECT_CALL(observer_, OnSuggestionGeneratedForTest(Eq(std::nullopt)));

  controller_->OnNavigationFinished(new_metadata);

  histogram_tester.ExpectUniqueSample(
      kMultistepFilterAcceptanceInitialCueHistogram,
      SuggestionUserDecision::kIgnored, 1);
  histogram_tester.ExpectUniqueSample(kMultistepFilterAcceptanceHistogram,
                                      SuggestionUserDecision::kIgnored, 1);
  histogram_tester.ExpectUniqueSample(
      kMultistepFilterAcceptanceReopenedCueHistogram,
      SuggestionUserDecision::kIgnored, 1);
}

// Tests that FilterTabController logs the correct histograms when the reopened
// cue is ignored on destruction.
TEST_F(FilterTabControllerTest,
       HistogramLoggingReopenedCueIgnoredOnDestruction) {
  base::HistogramTester histogram_tester;

  FilterNavigationMetadata metadata =
      CreateMetadata(1, GURL("https://example.com"));
  metadata.has_user_gesture = true;

  base::Uuid expected_id = base::Uuid::GenerateRandomV4();
  UrlFilterSuggestion expected_suggestion =
      CreateDefaultSuggestion(metadata.navigation_id, metadata.url);

  MultistepFilterUiDelegate::SuggestionUiCallbacks captured_callbacks;
  RunNavigationAndShowSuggestion(metadata, expected_suggestion, expected_id,
                                 RetentionStateSnapshot(), captured_callbacks);

  // 2. User reopens from omnibox.
  ASSERT_FALSE(captured_callbacks.on_suggestion_reopened.is_null());
  std::move(captured_callbacks.on_suggestion_reopened).Run();

  // 3. Destroy controller (simulates tab closure).
  DestroyController();

  histogram_tester.ExpectUniqueSample(
      kMultistepFilterAcceptanceInitialCueHistogram,
      SuggestionUserDecision::kIgnored, 1);
  histogram_tester.ExpectUniqueSample(kMultistepFilterAcceptanceHistogram,
                                      SuggestionUserDecision::kIgnored, 1);
  histogram_tester.ExpectUniqueSample(
      kMultistepFilterAcceptanceReopenedCueHistogram,
      SuggestionUserDecision::kIgnored, 1);
}

// Tests that same-document navigations do not prematurely log the active UI
// suggestion as ignored.
TEST_F(FilterTabControllerTest, SameDocumentNavigationDoesNotLogIgnored) {
  base::HistogramTester histogram_tester;

  FilterNavigationMetadata metadata =
      CreateMetadata(1, GURL("https://example.com"));
  metadata.has_user_gesture = true;

  base::Uuid expected_id = base::Uuid::GenerateRandomV4();
  UrlFilterSuggestion expected_suggestion =
      CreateDefaultSuggestion(metadata.navigation_id, metadata.url);

  MultistepFilterUiDelegate::SuggestionUiCallbacks captured_callbacks;
  RunNavigationAndShowSuggestion(metadata, expected_suggestion, expected_id,
                                 RetentionStateSnapshot(), captured_callbacks);

  // Trigger same-document navigation
  FilterNavigationMetadata same_doc_metadata =
      CreateMetadata(2, GURL("https://example.com/#section1"));
  same_doc_metadata.prev_url = GURL("https://example.com/");
  same_doc_metadata.is_same_document_navigation = true;
  same_doc_metadata.has_user_gesture = true;

  EXPECT_CALL(*mock_delegate_, ClearSuggestion()).Times(0);
  EXPECT_CALL(*mock_service_,
              HasUserProvidedConsent(same_doc_metadata.navigation_id,
                                     same_doc_metadata.url.GetHost()))
      .WillOnce(Return(true));

  std::vector<std::string> supported_tasks = {"Task1"};
  EXPECT_CALL(*mock_annotation_client(),
              GetSupportedTasks(same_doc_metadata.url, _,
                                same_doc_metadata.navigation_id))
      .WillOnce(base::test::RunOnceCallback<1>(supported_tasks));

  // Mock extractor to do nothing.
  EXPECT_CALL(*mock_extractor_,
              ExtractAnnotationFromUrl(same_doc_metadata.url, _,
                                       same_doc_metadata.navigation_id))
      .Times(1);

  // Mock generator to do nothing (simulating async delay).
  EXPECT_CALL(*mock_generator_,
              GenerateSuggestion(same_doc_metadata.url, supported_tasks, _,
                                 same_doc_metadata.navigation_id))
      .Times(1);

  controller_->OnNavigationFinished(same_doc_metadata);

  // Verify that NO acceptance histograms were logged yet.
  histogram_tester.ExpectTotalCount(kMultistepFilterAcceptanceHistogram, 0);
}

// Tests that same-document navigations log the active UI suggestion as ignored
// if the new suggestion generation fails.
TEST_F(FilterTabControllerTest, SameDocumentNavigationFailureLogsIgnored) {
  base::HistogramTester histogram_tester;

  FilterNavigationMetadata metadata =
      CreateMetadata(1, GURL("https://example.com"));
  metadata.has_user_gesture = true;

  base::Uuid expected_id = base::Uuid::GenerateRandomV4();
  UrlFilterSuggestion expected_suggestion =
      CreateDefaultSuggestion(metadata.navigation_id, metadata.url);

  MultistepFilterUiDelegate::SuggestionUiCallbacks captured_callbacks;
  RunNavigationAndShowSuggestion(metadata, expected_suggestion, expected_id,
                                 RetentionStateSnapshot(), captured_callbacks);

  // Trigger same-document navigation
  FilterNavigationMetadata same_doc_metadata =
      CreateMetadata(2, GURL("https://example.com/#section1"));
  same_doc_metadata.prev_url = GURL("https://example.com/");
  same_doc_metadata.is_same_document_navigation = true;
  same_doc_metadata.has_user_gesture = true;

  EXPECT_CALL(*mock_delegate_, ClearSuggestion()).Times(0);
  EXPECT_CALL(*mock_service_,
              HasUserProvidedConsent(same_doc_metadata.navigation_id,
                                     same_doc_metadata.url.GetHost()))
      .WillOnce(Return(true));

  std::vector<std::string> supported_tasks = {"Task1"};
  EXPECT_CALL(*mock_annotation_client(),
              GetSupportedTasks(same_doc_metadata.url, _,
                                same_doc_metadata.navigation_id))
      .WillOnce(base::test::RunOnceCallback<1>(supported_tasks));

  // Mock extractor to do nothing.
  EXPECT_CALL(*mock_extractor_,
              ExtractAnnotationFromUrl(same_doc_metadata.url, _,
                                       same_doc_metadata.navigation_id))
      .Times(1);

  // Mock generator to return nullopt.
  EXPECT_CALL(*mock_generator_,
              GenerateSuggestion(same_doc_metadata.url, supported_tasks, _,
                                 same_doc_metadata.navigation_id))
      .WillOnce(base::test::RunOnceCallback<2>(std::nullopt));

  EXPECT_CALL(*mock_delegate_, ShowSuggestion(Eq(std::nullopt), _));
  EXPECT_CALL(observer_, OnSuggestionGeneratedForTest(Eq(std::nullopt)));

  controller_->OnNavigationFinished(same_doc_metadata);

  // Verify that kIgnored was logged because the suggestion was cleared.
  histogram_tester.ExpectUniqueSample(kMultistepFilterAcceptanceHistogram,
                                      SuggestionUserDecision::kIgnored, 1);
}

// Tests that when navigation finishes with an error page, and there was an
// applied suggestion, the application outcome is recorded as failure.
TEST_F(FilterTabControllerTest, NavigationErrorLogsApplicationFailure) {
  base::HistogramTester histogram_tester;
  FilterNavigationMetadata metadata = CreateMetadata(
      /*navigation_id=*/10, GURL("https://example.com/error"),
      /*is_cryptographic=*/true, /*is_error_page=*/true,
      /*net_error_code=*/-106, CreateDefaultSuggestion(9));

  ExpectNoExtractionOrSuggestion();

  controller_->OnNavigationFinished(metadata);

  histogram_tester.ExpectUniqueSample(
      kMultistepFilterApplicationOutcomeHistogram,
      SuggestionApplicationResult::kFailedErrorPage, 1);
  histogram_tester.ExpectUniqueSample(
      "MultistepFilter.ApplicationOutcome.ByTask.Task1",
      SuggestionApplicationResult::kFailedErrorPage, 1);
  VerifyApplicationOutcomeRetentionHistograms(
      histogram_tester, {kRetentionSliceFirstImpression},
      SuggestionApplicationResult::kFailedErrorPage);
}

// Tests that when navigation finishes with an insecure scheme (HTTP), and
// there was an applied suggestion, the application outcome is recorded as
// failure.
TEST_F(FilterTabControllerTest, HttpNavigationLogsApplicationFailure) {
  base::HistogramTester histogram_tester;
  FilterNavigationMetadata metadata = CreateMetadata(
      /*navigation_id=*/12, GURL("http://example.com/error"),
      /*is_cryptographic=*/false, /*is_error_page=*/false,
      /*net_error_code=*/0, CreateDefaultSuggestion(9));
  metadata.is_http_allowed_for_testing = false;

  ExpectNoExtractionOrSuggestion();

  controller_->OnNavigationFinished(metadata);

  histogram_tester.ExpectUniqueSample(
      kMultistepFilterApplicationOutcomeHistogram,
      SuggestionApplicationResult::kFailedErrorPage, 1);
  histogram_tester.ExpectUniqueSample(
      "MultistepFilter.ApplicationOutcome.ByTask.Task1",
      SuggestionApplicationResult::kFailedErrorPage, 1);
  VerifyApplicationOutcomeRetentionHistograms(
      histogram_tester, {kRetentionSliceFirstImpression},
      SuggestionApplicationResult::kFailedErrorPage);
}

// Tests that successful extraction and application verification logs success.
TEST_F(FilterTabControllerTest, SuccessfulApplicationLogsSuccess) {
  base::HistogramTester histogram_tester;
  UrlFilterSuggestion suggestion = CreateSuggestionWithFacet(
      "key1", u"Label1", "value1", /*triggering_navigation_id=*/10);

  FilterNavigationMetadata metadata = CreateMetadata(
      /*navigation_id=*/11, GURL("https://example.com/applied"),
      /*is_cryptographic=*/true, /*is_error_page=*/false,
      /*net_error_code=*/0, std::move(suggestion));
  metadata.has_user_gesture = true;

  base::Uuid expected_id = base::Uuid::GenerateRandomV4();
  FilterAttribute attr("key1", "value1");
  FilterAnnotation annotation(expected_id, "Task1", "example.com",
                              base::Time::Now(), {attr});

  RunSuggestionApplicationFlow(metadata, annotation);
  DestroyController();

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          kMultistepFilterApplicationOutcomeHistogram),
      BucketsAre(Bucket(SuggestionApplicationResult::kAllFiltersApplied, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "MultistepFilter.ApplicationOutcome.ByTask.Task1"),
      BucketsAre(Bucket(SuggestionApplicationResult::kAllFiltersApplied, 1)));

  histogram_tester.ExpectUniqueSample(
      kMultistepFilterNumberOfFacetsSuccessfullyAppliedHistogram, 1, 1);
  histogram_tester.ExpectUniqueSample(
      "MultistepFilter.NumberOfFacetsSuccessfullyApplied.ByTask.Task1", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "MultistepFilter.ApplicationOutcome.ByTask.Task1.ByFacet.key1", true, 1);
  VerifyApplicationOutcomeRetentionHistograms(
      histogram_tester, {kRetentionSliceFirstImpression},
      SuggestionApplicationResult::kAllFiltersApplied);
}

// Tests that failed extraction and application verification logs failure and
// per-facet failure.
TEST_F(FilterTabControllerTest,
       FailedApplicationLogsFailureAndPerFacetOutcomes) {
  base::HistogramTester histogram_tester;
  UrlFilterSuggestion suggestion = CreateSuggestionWithFacet(
      "key1", u"Label1", "value1", /*triggering_navigation_id=*/11);

  FilterNavigationMetadata metadata = CreateMetadata(
      /*navigation_id=*/12, GURL("https://example.com/applied"),
      /*is_cryptographic=*/true, /*is_error_page=*/false,
      /*net_error_code=*/0, std::move(suggestion));
  metadata.has_user_gesture = true;

  // Set up MISMATCHING annotation (different value or key)
  base::Uuid expected_id = base::Uuid::GenerateRandomV4();
  FilterAttribute attr("key1", "value_mismatch");
  FilterAnnotation annotation(expected_id, "Task1", "example.com",
                              base::Time::Now(), {attr});

  RunSuggestionApplicationFlow(metadata, annotation);

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  kMultistepFilterApplicationOutcomeHistogram),
              BucketsAre(Bucket(
                  SuggestionApplicationResult::kFailedAttributeMismatch, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "MultistepFilter.ApplicationOutcome.ByTask.Task1"),
              BucketsAre(Bucket(
                  SuggestionApplicationResult::kFailedAttributeMismatch, 1)));

  histogram_tester.ExpectTotalCount(
      kMultistepFilterNumberOfFacetsSuccessfullyAppliedHistogram, 0);
  histogram_tester.ExpectUniqueSample(
      "MultistepFilter.ApplicationOutcome.ByTask.Task1.ByFacet.key1", false, 1);

  VerifyApplicationOutcomeRetentionHistograms(
      histogram_tester, {kRetentionSliceFirstImpression},
      SuggestionApplicationResult::kFailedAttributeMismatch);
}

// Tests that the acceptance retention histograms are logged correctly when the
// retention state is default.
TEST_F(FilterTabControllerTest,
       HistogramLoggingWithRetentionState_FirstImpression) {
  base::HistogramTester histogram_tester;
  FilterNavigationMetadata metadata =
      CreateMetadata(1, GURL("https://example.com"));
  metadata.has_user_gesture = true;
  base::Uuid expected_id = base::Uuid::GenerateRandomV4();
  UrlFilterSuggestion expected_suggestion = CreateSuggestionWithFacet(
      "key1", u"Label1", "value1", metadata.navigation_id, metadata.url);

  MultistepFilterUiDelegate::SuggestionUiCallbacks captured_callbacks;
  RunNavigationAndShowSuggestion(metadata, expected_suggestion, expected_id,
                                 RetentionStateSnapshot(), captured_callbacks);

  EXPECT_CALL(*mock_service_, RecordUserInteractionWithSuggestion(
                                  SuggestionUserDecision::kDismissed))
      .Times(1);
  ASSERT_FALSE(captured_callbacks.on_user_interaction.is_null());
  std::move(captured_callbacks.on_user_interaction)
      .Run(SuggestionUserDecision::kDismissed);

  // Base histograms:
  histogram_tester.ExpectUniqueSample(
      kMultistepFilterAcceptanceInitialCueHistogram,
      SuggestionUserDecision::kDismissed, 1);
  histogram_tester.ExpectUniqueSample(kMultistepFilterAcceptanceHistogram,
                                      SuggestionUserDecision::kDismissed, 1);

  VerifyAcceptanceRetentionHistograms(histogram_tester,
                                      {kRetentionSliceFirstImpression},
                                      SuggestionUserDecision::kDismissed);
}

// Tests that the acceptance retention histograms are logged correctly when the
// retention state is accepted last time.
TEST_F(FilterTabControllerTest,
       HistogramLoggingWithRetentionState_AcceptedLastTime) {
  base::HistogramTester histogram_tester;
  FilterNavigationMetadata metadata =
      CreateMetadata(1, GURL("https://example.com"));
  metadata.has_user_gesture = true;
  base::Uuid expected_id = base::Uuid::GenerateRandomV4();
  UrlFilterSuggestion expected_suggestion = CreateSuggestionWithFacet(
      "key1", u"Label1", "value1", metadata.navigation_id, metadata.url);

  RetentionStateSnapshot snapshot;
  snapshot.suggestion_impressions = 1;
  snapshot.suggestion_acceptances = 1;
  snapshot.is_last_suggestion_accepted = true;

  MultistepFilterUiDelegate::SuggestionUiCallbacks captured_callbacks;
  RunNavigationAndShowSuggestion(metadata, expected_suggestion, expected_id,
                                 snapshot, captured_callbacks);

  EXPECT_CALL(*mock_service_, RecordUserInteractionWithSuggestion(
                                  SuggestionUserDecision::kDismissed))
      .Times(1);
  ASSERT_FALSE(captured_callbacks.on_user_interaction.is_null());
  std::move(captured_callbacks.on_user_interaction)
      .Run(SuggestionUserDecision::kDismissed);

  // Base histograms:
  histogram_tester.ExpectUniqueSample(
      kMultistepFilterAcceptanceInitialCueHistogram,
      SuggestionUserDecision::kDismissed, 1);
  histogram_tester.ExpectUniqueSample(kMultistepFilterAcceptanceHistogram,
                                      SuggestionUserDecision::kDismissed, 1);

  VerifyAcceptanceRetentionHistograms(
      histogram_tester,
      {kRetentionSliceAcceptedLastTime, kRetentionSliceAcceptedAtLeastOnce},
      SuggestionUserDecision::kDismissed);
}

// Tests that the acceptance retention histograms are logged correctly when the
// retention state is rejected last time and accepted at least once.
TEST_F(
    FilterTabControllerTest,
    HistogramLoggingWithRetentionState_RejectedLastTime_AcceptedAtLeastOnce) {
  base::HistogramTester histogram_tester;
  FilterNavigationMetadata metadata =
      CreateMetadata(1, GURL("https://example.com"));
  metadata.has_user_gesture = true;
  base::Uuid expected_id = base::Uuid::GenerateRandomV4();
  UrlFilterSuggestion expected_suggestion = CreateSuggestionWithFacet(
      "key1", u"Label1", "value1", metadata.navigation_id, metadata.url);

  RetentionStateSnapshot snapshot;
  snapshot.suggestion_impressions = 2;
  snapshot.suggestion_acceptances = 1;
  snapshot.is_last_suggestion_accepted = false;

  MultistepFilterUiDelegate::SuggestionUiCallbacks captured_callbacks;
  RunNavigationAndShowSuggestion(metadata, expected_suggestion, expected_id,
                                 snapshot, captured_callbacks);

  EXPECT_CALL(*mock_service_, RecordUserInteractionWithSuggestion(
                                  SuggestionUserDecision::kDismissed))
      .Times(1);
  ASSERT_FALSE(captured_callbacks.on_user_interaction.is_null());
  std::move(captured_callbacks.on_user_interaction)
      .Run(SuggestionUserDecision::kDismissed);

  // Base histograms:
  histogram_tester.ExpectUniqueSample(
      kMultistepFilterAcceptanceInitialCueHistogram,
      SuggestionUserDecision::kDismissed, 1);
  histogram_tester.ExpectUniqueSample(kMultistepFilterAcceptanceHistogram,
                                      SuggestionUserDecision::kDismissed, 1);

  VerifyAcceptanceRetentionHistograms(
      histogram_tester,
      {kRetentionSliceRejectedLastTime, kRetentionSliceAcceptedAtLeastOnce},
      SuggestionUserDecision::kDismissed);
}

// Tests that the acceptance retention histograms are logged correctly when the
// retention state is rejected last time and saw cues but never accepted.
TEST_F(
    FilterTabControllerTest,
    HistogramLoggingWithRetentionState_RejectedLastTime_SawCuesButNeverAccepted) {
  base::HistogramTester histogram_tester;
  FilterNavigationMetadata metadata =
      CreateMetadata(1, GURL("https://example.com"));
  metadata.has_user_gesture = true;
  base::Uuid expected_id = base::Uuid::GenerateRandomV4();
  UrlFilterSuggestion expected_suggestion = CreateSuggestionWithFacet(
      "key1", u"Label1", "value1", metadata.navigation_id, metadata.url);

  RetentionStateSnapshot snapshot;
  snapshot.suggestion_impressions = 1;
  snapshot.suggestion_acceptances = 0;
  snapshot.is_last_suggestion_accepted = false;

  MultistepFilterUiDelegate::SuggestionUiCallbacks captured_callbacks;
  RunNavigationAndShowSuggestion(metadata, expected_suggestion, expected_id,
                                 snapshot, captured_callbacks);

  EXPECT_CALL(*mock_service_, RecordUserInteractionWithSuggestion(
                                  SuggestionUserDecision::kDismissed))
      .Times(1);
  ASSERT_FALSE(captured_callbacks.on_user_interaction.is_null());
  std::move(captured_callbacks.on_user_interaction)
      .Run(SuggestionUserDecision::kDismissed);

  // Base histograms:
  histogram_tester.ExpectUniqueSample(
      kMultistepFilterAcceptanceInitialCueHistogram,
      SuggestionUserDecision::kDismissed, 1);
  histogram_tester.ExpectUniqueSample(kMultistepFilterAcceptanceHistogram,
                                      SuggestionUserDecision::kDismissed, 1);

  VerifyAcceptanceRetentionHistograms(
      histogram_tester,
      {kRetentionSliceRejectedLastTime, kRetentionSliceSawCuesButNeverAccepted},
      SuggestionUserDecision::kDismissed);
}

// Tests that the application outcome retention histograms are logged correctly
// when the retention state is accepted last time.
TEST_F(FilterTabControllerTest,
       SuccessfulApplicationLogsSuccess_WithRetentionState) {
  base::HistogramTester histogram_tester;

  // 1. Initial navigation and show suggestion with non-default retention state.
  FilterNavigationMetadata metadata =
      CreateMetadata(1, GURL("https://example.com"));
  metadata.has_user_gesture = true;
  base::Uuid expected_id = base::Uuid::GenerateRandomV4();
  UrlFilterSuggestion expected_suggestion = CreateSuggestionWithFacet(
      "key1", u"Label1", "value1", metadata.navigation_id, metadata.url);

  RetentionStateSnapshot snapshot;
  snapshot.suggestion_impressions = 1;
  snapshot.suggestion_acceptances = 1;
  snapshot.is_last_suggestion_accepted = true;

  MultistepFilterUiDelegate::SuggestionUiCallbacks captured_callbacks;
  RunNavigationAndShowSuggestion(metadata, expected_suggestion, expected_id,
                                 snapshot, captured_callbacks);

  // 2. User accepts suggestion.
  EXPECT_CALL(*mock_service_, RecordUserInteractionWithSuggestion(
                                  SuggestionUserDecision::kAccepted))
      .Times(1);
  ASSERT_FALSE(captured_callbacks.on_user_interaction.is_null());
  std::move(captured_callbacks.on_user_interaction)
      .Run(SuggestionUserDecision::kAccepted);

  // 3. Second navigation (applying the suggestion).
  FilterNavigationMetadata apply_metadata = CreateMetadata(
      /*navigation_id=*/2, GURL("https://example.com/applied"),
      /*is_cryptographic=*/true, /*is_error_page=*/false,
      /*net_error_code=*/0, expected_suggestion);
  apply_metadata.has_user_gesture = true;

  // Set up matching annotation
  base::Uuid expected_apply_id = base::Uuid::GenerateRandomV4();
  FilterAttribute attr("key1", "value1");
  FilterAnnotation annotation(expected_apply_id, "Task1", "example.com",
                              base::Time::Now(), {attr});

  RunSuggestionApplicationFlow(apply_metadata, annotation);
  DestroyController();

  // Verify application outcome histograms:
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          kMultistepFilterApplicationOutcomeHistogram),
      BucketsAre(Bucket(SuggestionApplicationResult::kAllFiltersApplied, 1)));

  VerifyApplicationOutcomeRetentionHistograms(
      histogram_tester,
      {kRetentionSliceAcceptedLastTime, kRetentionSliceAcceptedAtLeastOnce},
      SuggestionApplicationResult::kAllFiltersApplied);
}

// Tests that if a new navigation finishes while the controller is waiting for
// extraction of a previously applied suggestion, the application session is
// aborted and logged as a failure.
TEST_F(FilterTabControllerTest, ApplicationInterruptedByNewNavigation) {
  base::HistogramTester histogram_tester;

  // 1. Suggestion accepted, starting landing navigation.
  UrlFilterSuggestion suggestion = CreateSuggestionWithFacet(
      "key1", u"Label1", "value1", /*triggering_navigation_id=*/10);

  FilterNavigationMetadata landing_metadata = CreateMetadata(
      /*navigation_id=*/11, GURL("https://example.com/applied"),
      /*is_cryptographic=*/true, /*is_error_page=*/false,
      /*net_error_code=*/0, std::move(suggestion));
  landing_metadata.has_user_gesture = true;

  EXPECT_CALL(*mock_delegate_, ClearSuggestion());
  EXPECT_CALL(*mock_service_,
              HasUserProvidedConsent(landing_metadata.navigation_id,
                                     landing_metadata.url.GetHost()))
      .WillOnce(Return(true));

  std::vector<std::string> supported_tasks = {"Task1"};
  EXPECT_CALL(*mock_annotation_client(),
              GetSupportedTasks(landing_metadata.url, _,
                                landing_metadata.navigation_id))
      .WillOnce(base::test::RunOnceCallback<1>(supported_tasks));

  // Mock extractor to do nothing (simulate async delay).
  base::OnceCallback<void(std::optional<FilterAnnotation>)> extraction_callback;
  EXPECT_CALL(*mock_extractor_,
              ExtractAnnotationFromUrl(landing_metadata.url, _,
                                       landing_metadata.navigation_id))
      .WillOnce(testing::WithArg<1>(
          [&extraction_callback](
              base::OnceCallback<void(std::optional<FilterAnnotation>)> cb) {
            extraction_callback = std::move(cb);
          }));

  // Mock generator to do nothing.
  EXPECT_CALL(*mock_generator_, GenerateSuggestion).WillOnce(Return());

  // Trigger landing navigation finish.
  controller_->OnNavigationFinished(landing_metadata);

  // No application outcome should be logged yet.
  histogram_tester.ExpectTotalCount(kMultistepFilterApplicationOutcomeHistogram,
                                    0);

  // 2. A new navigation finishes before extraction completes.
  FilterNavigationMetadata interrupt_metadata = CreateMetadata(
      /*navigation_id=*/12, GURL("https://example.com/different"),
      /*is_cryptographic=*/true, /*is_error_page=*/false,
      /*net_error_code=*/0);
  interrupt_metadata.has_user_gesture = true;

  // Mock for the new navigation.
  EXPECT_CALL(*mock_delegate_, ClearSuggestion());
  EXPECT_CALL(*mock_service_,
              HasUserProvidedConsent(interrupt_metadata.navigation_id,
                                     interrupt_metadata.url.GetHost()))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_annotation_client(),
              GetSupportedTasks(interrupt_metadata.url, _,
                                interrupt_metadata.navigation_id))
      .WillOnce(base::test::RunOnceCallback<1>(supported_tasks));
  // Extraction and generation for new navigation will be triggered.
  EXPECT_CALL(*mock_extractor_,
              ExtractAnnotationFromUrl(interrupt_metadata.url, _,
                                       interrupt_metadata.navigation_id))
      .Times(1);
  EXPECT_CALL(*mock_generator_, GenerateSuggestion).Times(1);

  // Trigger interrupt navigation finish.
  controller_->OnNavigationFinished(interrupt_metadata);

  // Verify that the application outcome was logged as abandoned.
  histogram_tester.ExpectUniqueSample(
      kMultistepFilterApplicationOutcomeHistogram,
      SuggestionApplicationResult::kAbandonedBeforeVerification, 1);
}

}  // namespace
}  // namespace multistep_filter
