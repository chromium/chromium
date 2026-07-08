// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/content/content_filter_navigation_observer.h"

#include "base/command_line.h"
#include "base/functional/callback_helpers.h"
#include "base/test/gmock_callback_support.h"
#include "components/multistep_filter/content/content_filter_navigation_observer_test_api.h"
#include "components/multistep_filter/content/filter_initiated_navigation_marker.h"
#include "components/multistep_filter/core/annotation_index/annotation_index_client.h"
#include "components/multistep_filter/core/annotation_index/mock_annotation_index_client.h"
#include "components/multistep_filter/core/extraction/filter_extractor.h"
#include "components/multistep_filter/core/filter_tab_controller_test_api.h"
#include "components/multistep_filter/core/multistep_filter_service.h"
#include "components/multistep_filter/core/multistep_filter_ui_delegate.h"
#include "components/multistep_filter/core/storage/filter_store.h"
#include "components/multistep_filter/core/suggestion/filter_suggestion_generator.h"
#include "components/multistep_filter/core/switches.h"
#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace multistep_filter {

namespace {

using ::testing::_;
using ::testing::Return;

class MockFilterExtractor : public FilterExtractor {
 public:
  MockFilterExtractor(AnnotationIndexClient& annotation_index_client,
                      FilterStore& filter_store)
      : FilterExtractor(annotation_index_client,
                        filter_store,
                        /*log_router=*/nullptr) {}
  ~MockFilterExtractor() override = default;
  MOCK_METHOD(
      void,
      ExtractAnnotationFromUrl,
      (const GURL& url,
       base::OnceCallback<void(std::optional<FilterAnnotation>)> callback,
       int64_t navigation_id),
      (override));
};

class MockFilterSuggestionGenerator : public FilterSuggestionGenerator {
 public:
  MockFilterSuggestionGenerator(AnnotationIndexClient& annotation_index_client,
                                FilterStore& filter_store)
      : FilterSuggestionGenerator(annotation_index_client,
                                  filter_store,
                                  /*log_router=*/nullptr) {}
  ~MockFilterSuggestionGenerator() override = default;
  MOCK_METHOD(
      void,
      GenerateSuggestion,
      (const GURL& url,
       std::vector<std::string> supported_task_types,
       base::OnceCallback<void(std::optional<UrlFilterSuggestion>)> callback,
       int64_t navigation_id),
      (override));
};

class MockMultistepFilterService : public MultistepFilterService {
 public:
  MockMultistepFilterService(
      std::unique_ptr<AnnotationIndexClient> annotation_index_client,
      std::unique_ptr<FilterStore> filter_store)
      : MultistepFilterService([&]() {
          MultistepFilterService::Params params;
          params.annotation_index_client = std::move(annotation_index_client);
          params.filter_store = std::move(filter_store);
          params.identity_manager = nullptr;
          params.consent_helper = nullptr;
          params.log_router = nullptr;
          return params;
        }()) {
    ON_CALL(*this, HasUserProvidedConsent).WillByDefault(Return(true));
  }
  ~MockMultistepFilterService() override = default;

  MOCK_METHOD(bool,
              HasUserProvidedConsent,
              (int64_t navigation_id, std::string_view host),
              (override));
};

class MockUiDelegate : public MultistepFilterUiDelegate {
 public:
  MOCK_METHOD(void, ClearSuggestion, (), (override));
  MOCK_METHOD(void,
              OnSuggestionGenerated,
              (std::optional<UrlFilterSuggestion> suggestion),
              (override));
};

class ContentFilterNavigationObserverTest
    : public content::RenderViewHostTestHarness {
 public:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    auto annotation_client =
        std::make_unique<testing::NiceMock<MockAnnotationIndexClient>>();
    mock_client_ = annotation_client.get();

    // Default support tasks: returns "task1" to keep the cascade going.
    ON_CALL(*mock_client_, GetSupportedTasks)
        .WillByDefault(base::test::RunOnceCallbackRepeatedly<1>(
            std::vector<std::string>{"task1"}));

    mock_service_ =
        std::make_unique<testing::NiceMock<MockMultistepFilterService>>(
            std::move(annotation_client), std::make_unique<FilterStore>());
    auto delegate = std::make_unique<testing::NiceMock<MockUiDelegate>>();
    delegate_ = delegate.get();

    content_filter_navigation_observer_ =
        std::make_unique<ContentFilterNavigationObserver>(
            web_contents(), mock_service_.get(), /*log_router=*/nullptr,
            std::move(delegate));

    auto mock_extractor = std::make_unique<MockFilterExtractor>(
        *mock_client_, *mock_service_->GetFilterStore());
    mock_extractor_ = mock_extractor.get();

    auto mock_generator = std::make_unique<MockFilterSuggestionGenerator>(
        *mock_client_, *mock_service_->GetFilterStore());
    mock_generator_ = mock_generator.get();

    FilterTabController* controller =
        test_api(*content_filter_navigation_observer_).GetTabController();
    test_api(*controller).set_filter_extractor(std::move(mock_extractor));
    test_api(*controller)
        .set_filter_suggestion_generator(std::move(mock_generator));
  }

  void TearDown() override {
    mock_generator_ = nullptr;
    mock_extractor_ = nullptr;
    mock_client_ = nullptr;
    delegate_ = nullptr;
    content_filter_navigation_observer_.reset();
    mock_service_.reset();
    content::RenderViewHostTestHarness::TearDown();
  }

  MockMultistepFilterService& mock_service() { return *mock_service_; }
  MockUiDelegate& delegate() { return *delegate_; }
  MockFilterExtractor& mock_extractor() { return *mock_extractor_; }
  MockFilterSuggestionGenerator& mock_generator() { return *mock_generator_; }
  MockAnnotationIndexClient& mock_client() { return *mock_client_; }

  ContentFilterNavigationObserver* observer() {
    return content_filter_navigation_observer_.get();
  }

 private:
  raw_ptr<MockAnnotationIndexClient> mock_client_ = nullptr;
  raw_ptr<MockFilterExtractor> mock_extractor_ = nullptr;
  raw_ptr<MockFilterSuggestionGenerator> mock_generator_ = nullptr;
  std::unique_ptr<MockMultistepFilterService> mock_service_;
  raw_ptr<MockUiDelegate> delegate_;
  std::unique_ptr<ContentFilterNavigationObserver>
      content_filter_navigation_observer_;
};

// Tests that a valid HTTPS navigation triggers extraction and suggestion
// generation.
TEST_F(ContentFilterNavigationObserverTest, HttpsNavigation) {
  const GURL url("https://www.example.com");
  EXPECT_CALL(delegate(), ClearSuggestion());
  EXPECT_CALL(mock_service(), HasUserProvidedConsent(_, url.GetHost()))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(mock_client(), GetSupportedTasks(url, _, _));
  EXPECT_CALL(mock_extractor(), ExtractAnnotationFromUrl(url, _, _));
  EXPECT_CALL(mock_generator(), GenerateSuggestion(url, _, _, _));
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url);
}

// Tests that same-document navigations preserve suggestions but allow
// extraction.
TEST_F(ContentFilterNavigationObserverTest, SameDocumentNavigation) {
  const GURL url("https://www.example.com");
  EXPECT_CALL(delegate(), ClearSuggestion());
  EXPECT_CALL(mock_service(), HasUserProvidedConsent(_, url.GetHost()))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(mock_client(), GetSupportedTasks(url, _, _));
  EXPECT_CALL(mock_extractor(), ExtractAnnotationFromUrl(url, _, _));
  EXPECT_CALL(mock_generator(), GenerateSuggestion(url, _, _, _));
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url);
  // Reset expectations to test the next navigation.
  testing::Mock::VerifyAndClearExpectations(&delegate());
  testing::Mock::VerifyAndClearExpectations(&mock_service());
  testing::Mock::VerifyAndClearExpectations(&mock_client());
  testing::Mock::VerifyAndClearExpectations(&mock_extractor());
  testing::Mock::VerifyAndClearExpectations(&mock_generator());

  // Same-document navigations (like fragment changes) should NOT clear
  // suggestions, but SHOULD still trigger extraction and suggestion generation.
  const GURL same_doc_url("https://www.example.com/#test");
  EXPECT_CALL(delegate(), ClearSuggestion()).Times(0);
  EXPECT_CALL(mock_service(), HasUserProvidedConsent(_, same_doc_url.GetHost()))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(mock_client(), GetSupportedTasks(same_doc_url, _, _));
  EXPECT_CALL(mock_extractor(), ExtractAnnotationFromUrl(same_doc_url, _, _));
  EXPECT_CALL(mock_generator(), GenerateSuggestion(same_doc_url, _, _, _));
  auto navigation = content::NavigationSimulator::CreateRendererInitiated(
      same_doc_url, main_rfh());
  navigation->CommitSameDocument();
}

// Tests that an uncommitted/aborted navigation does not trigger extraction.
TEST_F(ContentFilterNavigationObserverTest, UncommittedNavigation) {
  const GURL url("https://www.example.com");
  EXPECT_CALL(delegate(), ClearSuggestion()).Times(0);
  EXPECT_CALL(mock_extractor(), ExtractAnnotationFromUrl).Times(0);
  EXPECT_CALL(mock_generator(), GenerateSuggestion).Times(0);
  auto navigation =
      content::NavigationSimulator::CreateBrowserInitiated(url, web_contents());
  navigation->Start();
  navigation->AbortCommit();
}

// Tests that subframe navigations are ignored.
TEST_F(ContentFilterNavigationObserverTest, SubframeNavigation) {
  const GURL url("https://www.example.com");
  EXPECT_CALL(delegate(), ClearSuggestion());
  EXPECT_CALL(mock_service(), HasUserProvidedConsent(_, url.GetHost()))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(mock_client(), GetSupportedTasks(url, _, _));
  EXPECT_CALL(mock_extractor(), ExtractAnnotationFromUrl(url, _, _));
  EXPECT_CALL(mock_generator(), GenerateSuggestion(url, _, _, _));
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url);

  const GURL subframe_url("https://www.example.com/subframe");
  EXPECT_CALL(delegate(), ClearSuggestion()).Times(0);
  EXPECT_CALL(mock_extractor(), ExtractAnnotationFromUrl).Times(0);
  EXPECT_CALL(mock_generator(), GenerateSuggestion).Times(0);
  content::RenderFrameHost* subframe =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("subframe");
  content::NavigationSimulator::NavigateAndCommitFromDocument(subframe_url,
                                                              subframe);
}

// Tests that a render process crash clears existing suggestions.
TEST_F(ContentFilterNavigationObserverTest, PrimaryMainFrameRenderProcessGone) {
  EXPECT_CALL(delegate(), ClearSuggestion());
  observer()->PrimaryMainFrameRenderProcessGone(
      base::TERMINATION_STATUS_PROCESS_CRASHED);
}

}  // namespace

}  // namespace multistep_filter
