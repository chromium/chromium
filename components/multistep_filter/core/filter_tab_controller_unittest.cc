// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/filter_tab_controller.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/memory/raw_ptr.h"
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
#include "components/multistep_filter/core/storage/filter_store.h"
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
};

class MockMultistepFilterUiDelegate : public MultistepFilterUiDelegate {
 public:
  MockMultistepFilterUiDelegate() = default;
  ~MockMultistepFilterUiDelegate() override = default;

  MOCK_METHOD(void,
              OnSuggestionGenerated,
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
  FilterTabControllerTest() = default;
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
    mock_service_ = std::make_unique<StrictMock<MockMultistepFilterService>>(
        std::move(params));
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

  void TearDown() override {
    mock_generator_ = nullptr;
    mock_extractor_ = nullptr;
    controller_.reset();
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
    metadata.url = url;
    metadata.is_cryptographic_scheme = is_cryptographic;
    metadata.is_error_page_navigation = is_error_page;
    metadata.net_error_code = net_error_code;
    metadata.applied_suggestion = std::move(applied_suggestion);
    return metadata;
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
    EXPECT_CALL(*mock_delegate_,
                OnSuggestionGenerated(testing::Eq(std::nullopt), _));
    EXPECT_CALL(observer_,
                OnExtractionFinishedForTest(testing::Eq(std::nullopt)));
    EXPECT_CALL(observer_,
                OnSuggestionGeneratedForTest(testing::Eq(std::nullopt)));
  }

 protected:
  base::test::TaskEnvironment task_environment_;
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

// Tests that SPA (Single Page Application) fragment routing preserves existing
// suggestion UI, but aborts early when consent is false.
TEST_F(FilterTabControllerTest, SameDocumentNavigationConsentFalse) {
  FilterNavigationMetadata metadata =
      CreateMetadata(4, GURL("https://example.com/#section1"));
  metadata.prev_url = GURL("https://example.com/");
  metadata.is_same_document_navigation = true;
  metadata.has_user_gesture = true;

  EXPECT_CALL(*mock_delegate_, ClearSuggestion).Times(0);
  EXPECT_CALL(*mock_delegate_, OnSuggestionGenerated(Eq(std::nullopt), _));

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
  EXPECT_CALL(*mock_delegate_, OnSuggestionGenerated).Times(0);

  controller_->OnNavigationFinished(metadata);
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
              OnSuggestionGenerated(std::optional(expected_suggestion), _));
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
  EXPECT_CALL(*mock_delegate_, OnSuggestionGenerated(Eq(std::nullopt), _));

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
  EXPECT_CALL(*mock_delegate_, OnSuggestionGenerated(Eq(std::nullopt), _));

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
              OnSuggestionGenerated(std::optional(expected_suggestion), _));

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
              OnSuggestionGenerated(std::optional(expected_suggestion), _));

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
  FilterNavigationMetadata metadata;
  metadata.navigation_id = 11;
  metadata.url = GURL("https://example.com/");
  metadata.is_cryptographic_scheme = true;
  metadata.has_user_gesture = false;

  EXPECT_CALL(*mock_delegate_, ClearSuggestion());
  EXPECT_CALL(*mock_delegate_,
              OnSuggestionGenerated(testing::Eq(std::nullopt), _));

  EXPECT_CALL(observer_,
              OnExtractionFinishedForTest(testing::Eq(std::nullopt)));
  EXPECT_CALL(observer_,
              OnSuggestionGeneratedForTest(testing::Eq(std::nullopt)));

  controller_->OnNavigationFinished(metadata);
}

// Tests that FilterTabController notifies the service when a suggestion is
// shown.
TEST_F(FilterTabControllerTest, OnSuggestionShownNotifiesService) {
  UrlFilterSuggestion suggestion(
      UrlFilterSuggestion::Params{.triggering_navigation_id = 42,
                                  .triggering_host = "example.com",
                                  .task_type = "task_type_1"});

  EXPECT_CALL(*mock_service_, RecordSuggestionImpression()).Times(1);
  EXPECT_CALL(*mock_service_, DeleteAnnotationsForTask(
                                  testing::Eq("task_type_1"), testing::Eq(42),
                                  testing::Eq("example.com")))
      .Times(1);

  controller_->OnSuggestionShown(suggestion);
}

// Tests that FilterTabController notifies the service when the user makes a
// decision on a suggestion.
TEST_F(FilterTabControllerTest, OnUserDecisionNotifiesService) {
  UrlFilterSuggestion suggestion(
      UrlFilterSuggestion::Params{.navigation_url = GURL("https://example.com"),
                                  .triggering_navigation_id = 42,
                                  .triggering_host = "example.com",
                                  .task_type = "task_type_1"});

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
              OnSuggestionGenerated(std::optional(expected_suggestion), _))
      .WillOnce(testing::SaveArgByMove<1>(&captured_callbacks));

  EXPECT_CALL(observer_,
              OnExtractionFinishedForTest(std::optional(expected_id)));
  EXPECT_CALL(observer_,
              OnSuggestionGeneratedForTest(std::optional(expected_suggestion)));

  controller_->OnNavigationFinished(metadata);

  // 1. Verify on_suggestion_shown callback.
  EXPECT_CALL(*mock_service_, RecordSuggestionImpression()).Times(1);
  EXPECT_CALL(*mock_service_,
              DeleteAnnotationsForTask(testing::Eq("Task1"),
                                       testing::Eq(metadata.navigation_id),
                                       testing::Eq("example.com")))
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

  EXPECT_CALL(*mock_delegate_, ClearSuggestion());

  EXPECT_CALL(*mock_service_, HasUserProvidedConsent(metadata.navigation_id,
                                                     metadata.url.GetHost()))
      .WillOnce(testing::Return(true));

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

  UrlFilterSuggestion expected_suggestion = CreateSuggestionWithFacet(
      "key1", u"Label1", "value1", metadata.navigation_id, metadata.url);

  EXPECT_CALL(*mock_generator_,
              GenerateSuggestion(metadata.url, supported_tasks, _,
                                 metadata.navigation_id))
      .WillOnce(base::test::RunOnceCallback<2>(expected_suggestion));

  MultistepFilterUiDelegate::SuggestionUiCallbacks captured_callbacks;
  EXPECT_CALL(*mock_delegate_,
              OnSuggestionGenerated(std::optional(expected_suggestion), _))
      .WillOnce(testing::SaveArgByMove<1>(&captured_callbacks));

  EXPECT_CALL(observer_,
              OnExtractionFinishedForTest(std::optional(expected_id)));
  EXPECT_CALL(observer_,
              OnSuggestionGeneratedForTest(std::optional(expected_suggestion)));

  controller_->OnNavigationFinished(metadata);

  // When on_suggestion_shown is run:
  EXPECT_CALL(*mock_service_, RecordSuggestionImpression()).Times(1);
  EXPECT_CALL(*mock_service_,
              DeleteAnnotationsForTask(testing::Eq("Task1"),
                                       testing::Eq(metadata.navigation_id),
                                       testing::Eq("example.com")))
      .Times(1);
  ASSERT_FALSE(captured_callbacks.on_suggestion_shown.is_null());
  std::move(captured_callbacks.on_suggestion_shown).Run();

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
}

// Tests that FilterTabController logs the correct histograms when the reopened
// cue is ignored on navigation.
TEST_F(FilterTabControllerTest,
       HistogramLoggingReopenedCueIgnoredOnNavigation) {
  base::HistogramTester histogram_tester;

  FilterNavigationMetadata metadata =
      CreateMetadata(1, GURL("https://example.com"));
  metadata.has_user_gesture = true;

  EXPECT_CALL(*mock_delegate_, ClearSuggestion());

  EXPECT_CALL(*mock_service_, HasUserProvidedConsent(metadata.navigation_id,
                                                     metadata.url.GetHost()))
      .WillOnce(testing::Return(true));

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
              OnSuggestionGenerated(std::optional(expected_suggestion), _))
      .WillOnce(testing::SaveArgByMove<1>(&captured_callbacks));

  EXPECT_CALL(observer_,
              OnExtractionFinishedForTest(std::optional(expected_id)));
  EXPECT_CALL(observer_,
              OnSuggestionGeneratedForTest(std::optional(expected_suggestion)));

  controller_->OnNavigationFinished(metadata);

  // 1. Initial cue shown.
  EXPECT_CALL(*mock_service_, RecordSuggestionImpression()).Times(1);
  EXPECT_CALL(*mock_service_,
              DeleteAnnotationsForTask(testing::Eq("Task1"),
                                       testing::Eq(metadata.navigation_id),
                                       testing::Eq("example.com")))
      .Times(1);
  ASSERT_FALSE(captured_callbacks.on_suggestion_shown.is_null());
  std::move(captured_callbacks.on_suggestion_shown).Run();

  // 2. User reopens from omnibox.
  ASSERT_FALSE(captured_callbacks.on_suggestion_reopened.is_null());
  std::move(captured_callbacks.on_suggestion_reopened).Run();

  // 3. Navigate away (simulates clearing suggestion with kIgnored).
  FilterNavigationMetadata new_metadata =
      CreateMetadata(2, GURL("https://different-example.com"));
  new_metadata.has_user_gesture = true;

  EXPECT_CALL(*mock_delegate_, ClearSuggestion()).Times(1);
  EXPECT_CALL(*mock_delegate_,
              OnSuggestionGenerated(testing::Eq(std::nullopt), _))
      .Times(1);
  EXPECT_CALL(*mock_service_,
              HasUserProvidedConsent(new_metadata.navigation_id,
                                     new_metadata.url.GetHost()))
      .WillOnce(testing::Return(false));

  EXPECT_CALL(observer_,
              OnExtractionFinishedForTest(testing::Eq(std::nullopt)));
  EXPECT_CALL(observer_,
              OnSuggestionGeneratedForTest(testing::Eq(std::nullopt)));

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

  EXPECT_CALL(*mock_delegate_, ClearSuggestion());

  EXPECT_CALL(*mock_service_, HasUserProvidedConsent(metadata.navigation_id,
                                                     metadata.url.GetHost()))
      .WillOnce(testing::Return(true));

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
              OnSuggestionGenerated(std::optional(expected_suggestion), _))
      .WillOnce(testing::SaveArgByMove<1>(&captured_callbacks));

  EXPECT_CALL(observer_,
              OnExtractionFinishedForTest(std::optional(expected_id)));
  EXPECT_CALL(observer_,
              OnSuggestionGeneratedForTest(std::optional(expected_suggestion)));

  controller_->OnNavigationFinished(metadata);

  // 1. Initial cue shown.
  EXPECT_CALL(*mock_service_, RecordSuggestionImpression()).Times(1);
  EXPECT_CALL(*mock_service_,
              DeleteAnnotationsForTask(testing::Eq("Task1"),
                                       testing::Eq(metadata.navigation_id),
                                       testing::Eq("example.com")))
      .Times(1);
  ASSERT_FALSE(captured_callbacks.on_suggestion_shown.is_null());
  std::move(captured_callbacks.on_suggestion_shown).Run();

  // 2. User reopens from omnibox.
  ASSERT_FALSE(captured_callbacks.on_suggestion_reopened.is_null());
  std::move(captured_callbacks.on_suggestion_reopened).Run();

  // 3. Destroy controller (simulates tab closure).
  mock_extractor_ = nullptr;
  mock_generator_ = nullptr;
  controller_.reset();

  histogram_tester.ExpectUniqueSample(
      kMultistepFilterAcceptanceInitialCueHistogram,
      SuggestionUserDecision::kIgnored, 1);
  histogram_tester.ExpectUniqueSample(kMultistepFilterAcceptanceHistogram,
                                      SuggestionUserDecision::kIgnored, 1);
  histogram_tester.ExpectUniqueSample(
      kMultistepFilterAcceptanceReopenedCueHistogram,
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
      MultistepFilterApplicationOutcome::kNotAllFiltersApplied, 1);
  histogram_tester.ExpectUniqueSample(
      "MultistepFilter.ApplicationOutcome.ByTask.Task1",
      MultistepFilterApplicationOutcome::kNotAllFiltersApplied, 1);
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
      MultistepFilterApplicationOutcome::kNotAllFiltersApplied, 1);
  histogram_tester.ExpectUniqueSample(
      "MultistepFilter.ApplicationOutcome.ByTask.Task1",
      MultistepFilterApplicationOutcome::kNotAllFiltersApplied, 1);
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

  EXPECT_CALL(*mock_delegate_, ClearSuggestion());
  EXPECT_CALL(*mock_service_, HasUserProvidedConsent(metadata.navigation_id,
                                                     metadata.url.GetHost()))
      .WillOnce(Return(true));

  std::vector<std::string> supported_tasks = {"Task1"};
  EXPECT_CALL(*mock_annotation_client(),
              GetSupportedTasks(metadata.url, _, metadata.navigation_id))
      .WillOnce(base::test::RunOnceCallback<1>(supported_tasks));

  // Set up matching annotation
  base::Uuid expected_id = base::Uuid::GenerateRandomV4();
  FilterAttribute attr("key1", "value1");
  FilterAnnotation annotation(expected_id, "Task1", "example.com",
                              base::Time::Now(), {attr});

  EXPECT_CALL(*mock_extractor_,
              ExtractAnnotationFromUrl(metadata.url, _, metadata.navigation_id))
      .WillOnce(base::test::RunOnceCallback<1>(annotation));

  EXPECT_CALL(*mock_generator_, GenerateSuggestion)
      .WillOnce(base::test::RunOnceCallback<2>(std::nullopt));
  EXPECT_CALL(*mock_delegate_, OnSuggestionGenerated(Eq(std::nullopt), _));
  EXPECT_CALL(observer_,
              OnExtractionFinishedForTest(std::optional(expected_id)));
  EXPECT_CALL(observer_, OnSuggestionGeneratedForTest(Eq(std::nullopt)));

  controller_->OnNavigationFinished(metadata);

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  kMultistepFilterApplicationOutcomeHistogram),
              BucketsAre(Bucket(
                  MultistepFilterApplicationOutcome::kAllFiltersApplied, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "MultistepFilter.ApplicationOutcome.ByTask.Task1"),
              BucketsAre(Bucket(
                  MultistepFilterApplicationOutcome::kAllFiltersApplied, 1)));

  histogram_tester.ExpectUniqueSample(
      kMultistepFilterNumberOfFacetsSuccessfullyAppliedHistogram, 1, 1);
  histogram_tester.ExpectUniqueSample(
      "MultistepFilter.NumberOfFacetsSuccessfullyApplied.ByTask.Task1", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "MultistepFilter.ApplicationOutcome.ByTask.Task1.ByFacet.key1", true, 1);
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

  EXPECT_CALL(*mock_delegate_, ClearSuggestion());
  EXPECT_CALL(*mock_service_, HasUserProvidedConsent(metadata.navigation_id,
                                                     metadata.url.GetHost()))
      .WillOnce(Return(true));

  std::vector<std::string> supported_tasks = {"Task1"};
  EXPECT_CALL(*mock_annotation_client(),
              GetSupportedTasks(metadata.url, _, metadata.navigation_id))
      .WillOnce(base::test::RunOnceCallback<1>(supported_tasks));

  // Set up MISMATCHING annotation (different value or key)
  base::Uuid expected_id = base::Uuid::GenerateRandomV4();
  FilterAttribute attr("key1", "value_mismatch");
  FilterAnnotation annotation(expected_id, "Task1", "example.com",
                              base::Time::Now(), {attr});

  EXPECT_CALL(*mock_extractor_,
              ExtractAnnotationFromUrl(metadata.url, _, metadata.navigation_id))
      .WillOnce(base::test::RunOnceCallback<1>(annotation));

  EXPECT_CALL(*mock_generator_, GenerateSuggestion)
      .WillOnce(base::test::RunOnceCallback<2>(std::nullopt));
  EXPECT_CALL(*mock_delegate_, OnSuggestionGenerated(Eq(std::nullopt), _));

  EXPECT_CALL(observer_,
              OnExtractionFinishedForTest(std::optional(expected_id)));
  EXPECT_CALL(observer_, OnSuggestionGeneratedForTest(Eq(std::nullopt)));

  controller_->OnNavigationFinished(metadata);

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          kMultistepFilterApplicationOutcomeHistogram),
      BucketsAre(
          Bucket(MultistepFilterApplicationOutcome::kNotAllFiltersApplied, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "MultistepFilter.ApplicationOutcome.ByTask.Task1"),
      BucketsAre(
          Bucket(MultistepFilterApplicationOutcome::kNotAllFiltersApplied, 1)));

  histogram_tester.ExpectTotalCount(
      kMultistepFilterNumberOfFacetsSuccessfullyAppliedHistogram, 0);
  histogram_tester.ExpectUniqueSample(
      "MultistepFilter.ApplicationOutcome.ByTask.Task1.ByFacet.key1", false, 1);
}

}  // namespace
}  // namespace multistep_filter
