// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/filter_tab_controller.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/memory/raw_ptr.h"
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
  MockMultistepFilterUiDelegate() : weak_factory_(this) {}
  ~MockMultistepFilterUiDelegate() override = default;

  MOCK_METHOD(void,
              OnSuggestionGenerated,
              (std::optional<UrlFilterSuggestion>),
              (override));
  MOCK_METHOD(void, ClearSuggestion, (), (override));

  base::WeakPtr<MultistepFilterUiDelegate> GetWeakPtr() override {
    return weak_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockMultistepFilterUiDelegate> weak_factory_;
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
    MultistepFilterService::Params params;
    params.annotation_index_client =
        std::make_unique<MockAnnotationIndexClient>();
    params.filter_store = std::make_unique<FilterStore>();
    mock_service_ = std::make_unique<StrictMock<MockMultistepFilterService>>(
        std::move(params));
    mock_delegate_ =
        std::make_unique<StrictMock<MockMultistepFilterUiDelegate>>();
    controller_ = std::make_unique<FilterTabController>(
        mock_service_.get(), nullptr, mock_delegate_->GetWeakPtr());
    test_api(*controller_).SetObserverForTest(&observer_);
  }

  void TearDown() override {
    controller_.reset();
    mock_delegate_.reset();
    mock_service_.reset();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<StrictMock<MockMultistepFilterService>> mock_service_;
  std::unique_ptr<StrictMock<MockMultistepFilterUiDelegate>> mock_delegate_;
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
// suggestion UI.
TEST_F(FilterTabControllerTest, SameDocumentNavigation) {
  FilterNavigationMetadata metadata;
  metadata.navigation_id = 4;
  metadata.url = GURL("https://example.com/#section1");
  metadata.prev_url = GURL("https://example.com/");
  metadata.is_same_document_navigation = true;
  metadata.is_cryptographic_scheme = true;
  metadata.has_user_gesture = true;

  EXPECT_CALL(*mock_delegate_, ClearSuggestion()).Times(0);
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
  metadata.has_user_gesture = true;

  EXPECT_CALL(*mock_delegate_, ClearSuggestion()).Times(0);
  EXPECT_CALL(*mock_delegate_, OnSuggestionGenerated(Eq(std::nullopt)));

  EXPECT_CALL(observer_, OnExtractionFinishedForTest(Eq(std::nullopt)));
  EXPECT_CALL(observer_, OnSuggestionGeneratedForTest(Eq(std::nullopt)));

  controller_->OnNavigationFinished(metadata);
}

}  // namespace
}  // namespace multistep_filter
