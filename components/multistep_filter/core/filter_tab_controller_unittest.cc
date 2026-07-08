// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/filter_tab_controller.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "components/multistep_filter/core/annotation_index/mock_annotation_index_client.h"
#include "components/multistep_filter/core/data_models/filter_navigation_metadata.h"
#include "components/multistep_filter/core/filter_tab_controller_test_api.h"
#include "components/multistep_filter/core/multistep_filter_service.h"
#include "components/multistep_filter/core/multistep_filter_ui_delegate.h"
#include "components/multistep_filter/core/storage/filter_store.h"
#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace multistep_filter {
namespace {

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
};

class MockMultistepFilterUiDelegate : public MultistepFilterUiDelegate {
 public:
  MockMultistepFilterUiDelegate() = default;
  ~MockMultistepFilterUiDelegate() override = default;

  MOCK_METHOD(void,
              OnSuggestionGenerated,
              (std::optional<UrlFilterSuggestion>),
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
  FilterNavigationMetadata metadata;
  metadata.navigation_id = 1;
  metadata.url = GURL("http://example.com");
  metadata.is_cryptographic_scheme = false;
  metadata.is_http_allowed_for_testing = false;

  EXPECT_CALL(*mock_delegate_, ClearSuggestion());
  EXPECT_CALL(*mock_delegate_, OnSuggestionGenerated(Eq(std::nullopt)));
  EXPECT_CALL(observer_, OnExtractionFinishedForTest(Eq(std::nullopt)));
  EXPECT_CALL(observer_, OnSuggestionGeneratedForTest(Eq(std::nullopt)));

  controller_->OnNavigationFinished(metadata);
}

// Tests that network HTTP 4xx/5xx errors instantly trigger early fail-safes.
TEST_F(FilterTabControllerTest, ErrorPageNavigation) {
  FilterNavigationMetadata metadata;
  metadata.navigation_id = 2;
  metadata.url = GURL("https://example.com");
  metadata.is_cryptographic_scheme = true;
  metadata.is_error_page_navigation = true;
  metadata.net_error_code = -106;

  EXPECT_CALL(*mock_delegate_, ClearSuggestion());
  EXPECT_CALL(*mock_delegate_, OnSuggestionGenerated(Eq(std::nullopt)));
  EXPECT_CALL(observer_, OnExtractionFinishedForTest(Eq(std::nullopt)));
  EXPECT_CALL(observer_, OnSuggestionGeneratedForTest(Eq(std::nullopt)));

  controller_->OnNavigationFinished(metadata);
}

// Tests that FilterTabController aborts immediately when consent is false.
TEST_F(FilterTabControllerTest, SuppressExtractionAndGenerationOnConsentFalse) {
  FilterNavigationMetadata metadata;
  metadata.navigation_id = 3;
  metadata.url = GURL("https://example.com");
  metadata.prev_url = GURL("https://different.com");
  metadata.is_cryptographic_scheme = true;
  metadata.is_error_page_navigation = false;
  metadata.is_same_document_navigation = false;
  metadata.has_user_gesture = true;

  EXPECT_CALL(*mock_delegate_, ClearSuggestion());
  EXPECT_CALL(*mock_delegate_, OnSuggestionGenerated(Eq(std::nullopt)));

  EXPECT_CALL(*mock_service_, HasUserProvidedConsent(metadata.navigation_id,
                                                     metadata.url.GetHost()))
      .WillOnce(Return(false));

  EXPECT_CALL(observer_, OnExtractionFinishedForTest(Eq(std::nullopt)));
  EXPECT_CALL(observer_, OnSuggestionGeneratedForTest(Eq(std::nullopt)));

  controller_->OnNavigationFinished(metadata);
}

// Tests that SPA (Single Page Application) fragment routing preserves existing
// suggestion UI, but aborts early when consent is false.
TEST_F(FilterTabControllerTest, SameDocumentNavigationConsentFalse) {
  FilterNavigationMetadata metadata;
  metadata.navigation_id = 4;
  metadata.url = GURL("https://example.com/#section1");
  metadata.prev_url = GURL("https://example.com/");
  metadata.is_same_document_navigation = true;
  metadata.is_cryptographic_scheme = true;
  metadata.has_user_gesture = true;

  EXPECT_CALL(*mock_delegate_, ClearSuggestion).Times(0);
  EXPECT_CALL(*mock_delegate_, OnSuggestionGenerated(Eq(std::nullopt)));

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
  FilterNavigationMetadata metadata;
  metadata.navigation_id = 5;
  metadata.url = GURL("https://example.com/");
  metadata.prev_url = GURL("https://example.com/");
  metadata.is_same_document_navigation = false;
  metadata.is_cryptographic_scheme = true;
  metadata.has_user_gesture = true;

  EXPECT_CALL(*mock_delegate_, ClearSuggestion).Times(0);
  EXPECT_CALL(*mock_delegate_, OnSuggestionGenerated).Times(0);

  controller_->OnNavigationFinished(metadata);
}

// Tests that SPA (Single Page Application) fragment routing preserves existing
// suggestion UI, but correctly cascades to new extractions and suggestions on
// success.
TEST_F(FilterTabControllerTest, SameDocumentNavigationSuccess) {
  FilterNavigationMetadata metadata;
  metadata.navigation_id = 6;
  metadata.url = GURL("https://example.com/#section1");
  metadata.prev_url = GURL("https://example.com/");
  metadata.is_same_document_navigation = true;
  metadata.is_cryptographic_scheme = true;
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

  UrlFilterSuggestion expected_suggestion(UrlFilterSuggestion::Params{
      .navigation_url = metadata.url,
      .triggering_navigation_id = metadata.navigation_id,
      .task_type = "Task1"});

  EXPECT_CALL(*mock_generator_,
              GenerateSuggestion(metadata.url, supported_tasks, _,
                                 metadata.navigation_id))
      .WillOnce(base::test::RunOnceCallback<2>(expected_suggestion));

  EXPECT_CALL(*mock_delegate_,
              OnSuggestionGenerated(std::optional(expected_suggestion)));
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
  FilterNavigationMetadata metadata;
  metadata.navigation_id = 7;
  metadata.url = GURL("https://example.com/");
  metadata.is_cryptographic_scheme = true;
  metadata.has_user_gesture = true;

  EXPECT_CALL(*mock_delegate_, ClearSuggestion());
  EXPECT_CALL(*mock_delegate_, OnSuggestionGenerated(Eq(std::nullopt)));

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
  FilterNavigationMetadata metadata;
  metadata.navigation_id = 8;
  metadata.url = GURL("https://example.com/filtered");
  metadata.is_cryptographic_scheme = true;
  metadata.has_user_gesture = true;
  metadata.was_filter_initiated_navigation = true;

  EXPECT_CALL(*mock_delegate_, ClearSuggestion());
  // Suggestion failsafe will still trigger for the generator since we don't
  // start it.
  EXPECT_CALL(*mock_delegate_, OnSuggestionGenerated(Eq(std::nullopt)));

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
  FilterNavigationMetadata metadata;
  metadata.navigation_id = 9;
  metadata.url = GURL("https://example.com/");
  metadata.is_cryptographic_scheme = true;
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

  UrlFilterSuggestion expected_suggestion(UrlFilterSuggestion::Params{
      .navigation_url = metadata.url,
      .triggering_navigation_id = metadata.navigation_id,
      .task_type = "Task1"});

  EXPECT_CALL(*mock_generator_,
              GenerateSuggestion(metadata.url, supported_tasks, _,
                                 metadata.navigation_id))
      .WillOnce(base::test::RunOnceCallback<2>(expected_suggestion));

  EXPECT_CALL(*mock_delegate_,
              OnSuggestionGenerated(std::optional(expected_suggestion)));

  EXPECT_CALL(observer_,
              OnExtractionFinishedForTest(std::optional(expected_id)));
  EXPECT_CALL(observer_,
              OnSuggestionGeneratedForTest(std::optional(expected_suggestion)));

  controller_->OnNavigationFinished(metadata);
}

// Tests that non-cryptographic HTTP navigation successfully triggers extraction
// and generation cascade when the testing switch is explicitly allowed.
TEST_F(FilterTabControllerTest, HttpNavigationWithTestingSwitch) {
  FilterNavigationMetadata metadata;
  metadata.navigation_id = 10;
  metadata.url = GURL("http://example.com/");
  metadata.is_cryptographic_scheme = false;
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

  UrlFilterSuggestion expected_suggestion(UrlFilterSuggestion::Params{
      .navigation_url = metadata.url,
      .triggering_navigation_id = metadata.navigation_id,
      .task_type = "Task1"});

  EXPECT_CALL(*mock_generator_,
              GenerateSuggestion(metadata.url, supported_tasks, _,
                                 metadata.navigation_id))
      .WillOnce(base::test::RunOnceCallback<2>(expected_suggestion));

  EXPECT_CALL(*mock_delegate_,
              OnSuggestionGenerated(std::optional(expected_suggestion)));

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
              OnSuggestionGenerated(testing::Eq(std::nullopt)));

  EXPECT_CALL(observer_,
              OnExtractionFinishedForTest(testing::Eq(std::nullopt)));
  EXPECT_CALL(observer_,
              OnSuggestionGeneratedForTest(testing::Eq(std::nullopt)));

  controller_->OnNavigationFinished(metadata);
}

}  // namespace
}  // namespace multistep_filter
