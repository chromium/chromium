// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/content/content_filter_navigation_observer.h"

#include "components/multistep_filter/content/content_filter_navigation_observer_test_api.h"
#include "components/multistep_filter/content/filter_initiated_navigation_marker.h"
#include "components/multistep_filter/core/annotation_index/annotation_index_client.h"
#include "components/multistep_filter/core/annotation_index/mock_annotation_index_client.h"
#include "components/multistep_filter/core/data_models/url_filter_suggestion.h"
#include "components/multistep_filter/core/filter_tab_controller.h"
#include "components/multistep_filter/core/multistep_filter_service.h"
#include "components/multistep_filter/core/multistep_filter_ui_delegate.h"
#include "components/multistep_filter/core/prefs/multistep_filter_retention_prefs.h"
#include "components/multistep_filter/core/storage/filter_store.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "net/base/net_errors.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace multistep_filter {

namespace {

using ::testing::_;
using ::testing::Return;

class MockUiDelegate : public MultistepFilterUiDelegate {
 public:
  MOCK_METHOD(void, ClearSuggestion, (), (override));
  MOCK_METHOD(void,
              ShowSuggestion,
              (std::optional<UrlFilterSuggestion> suggestion,
               SuggestionUiCallbacks callbacks),
              (override));
};

class MockFilterTabController : public FilterTabController {
 public:
  MockFilterTabController(MultistepFilterService* service,
                          MultistepFilterUiDelegate* delegate)
      : FilterTabController(service,
                            /*log_router=*/nullptr,
                            delegate,
                            service->GetFilterStore(),
                            service->GetAnnotationIndexClient()) {}
  ~MockFilterTabController() override = default;

  MOCK_METHOD(void,
              OnNavigationFinished,
              (const FilterNavigationMetadata& metadata),
              (override));
};

class MockMultistepFilterService : public MultistepFilterService {
 public:
  MockMultistepFilterService(
      std::unique_ptr<AnnotationIndexClient> annotation_index_client,
      std::unique_ptr<FilterStore> filter_store,
      PrefService* pref_service,
      signin::IdentityManager* identity_manager)
      : MultistepFilterService([&]() {
          MultistepFilterService::Params params;
          params.annotation_index_client = std::move(annotation_index_client);
          params.filter_store = std::move(filter_store);
          params.identity_manager = identity_manager;
          params.pref_service = pref_service;
          params.consent_helper = nullptr;
          params.log_router = nullptr;
          return params;
        }()) {
    ON_CALL(*this, HasUserProvidedConsent).WillByDefault(Return(true));
    ON_CALL(*this, CanUseModelExecutionFeatures).WillByDefault(Return(true));
  }
  ~MockMultistepFilterService() override = default;

  MOCK_METHOD(bool,
              HasUserProvidedConsent,
              (int64_t navigation_id, std::string_view host),
              (override));
  MOCK_METHOD(bool, CanUseModelExecutionFeatures, (), (const, override));
};

class NavigationTimeCapturer : public content::WebContentsObserver {
 public:
  explicit NavigationTimeCapturer(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}
  ~NavigationTimeCapturer() override = default;

  void DidFinishNavigation(content::NavigationHandle* handle) override {
    if (handle->HasCommitted()) {
      start_time_ = handle->NavigationStart();
      finish_time_ =
          handle->GetNavigationHandleTiming().navigation_did_commit_time;
      navigation_id_ = handle->GetNavigationId();
      ukm_source_id_ = handle->GetNextPageUkmSourceId();
    }
  }

  base::TimeTicks start_time() const { return start_time_; }
  base::TimeTicks finish_time() const { return finish_time_; }
  int64_t navigation_id() const { return navigation_id_; }
  ukm::SourceId ukm_source_id() const { return ukm_source_id_; }

 private:
  base::TimeTicks start_time_;
  base::TimeTicks finish_time_;
  int64_t navigation_id_ = 0;
  ukm::SourceId ukm_source_id_ = ukm::kInvalidSourceId;
};

UrlFilterSuggestion CreateSuggestion(std::string task_type) {
  UrlFilterSuggestion::Params params;
  params.navigation_url = GURL("https://example.com");
  params.source_host = u"source.com";
  params.triggering_host = "trigger.com";
  params.task_type = std::move(task_type);
  return UrlFilterSuggestion(std::move(params));
}

class ContentFilterNavigationObserverTest
    : public content::RenderViewHostTestHarness {
 public:
  ContentFilterNavigationObserverTest() {
    RegisterRetentionProfilePrefs(pref_service_.registry());
  }
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    auto annotation_client =
        std::make_unique<testing::NiceMock<MockAnnotationIndexClient>>();
    mock_client_ = annotation_client.get();

    mock_service_ =
        std::make_unique<testing::NiceMock<MockMultistepFilterService>>(
            std::move(annotation_client), std::make_unique<FilterStore>(),
            &pref_service_, identity_test_env_.identity_manager());
    auto delegate = std::make_unique<testing::NiceMock<MockUiDelegate>>();
    delegate_ = delegate.get();

    content_filter_navigation_observer_ =
        std::make_unique<ContentFilterNavigationObserver>(
            web_contents(), mock_service_.get(), /*log_router=*/nullptr,
            std::move(delegate));

    auto mock_controller =
        std::make_unique<testing::StrictMock<MockFilterTabController>>(
            mock_service_.get(), delegate_);
    mock_controller_ = mock_controller.get();
    test_api(*content_filter_navigation_observer_)
        .SetTabController(std::move(mock_controller));
  }

  void TearDown() override {
    mock_controller_ = nullptr;
    mock_client_ = nullptr;
    delegate_ = nullptr;
    content_filter_navigation_observer_.reset();
    mock_service_.reset();
    content::RenderViewHostTestHarness::TearDown();
  }

  MockMultistepFilterService& mock_service() { return *mock_service_; }
  MockUiDelegate& delegate() { return *delegate_; }
  MockAnnotationIndexClient& mock_client() { return *mock_client_; }
  MockFilterTabController& mock_controller() { return *mock_controller_; }

  ContentFilterNavigationObserver* observer() {
    return content_filter_navigation_observer_.get();
  }

 protected:
  TestingPrefServiceSimple pref_service_;
  signin::IdentityTestEnvironment identity_test_env_;
  raw_ptr<MockAnnotationIndexClient> mock_client_ = nullptr;
  std::unique_ptr<MockMultistepFilterService> mock_service_;
  raw_ptr<MockUiDelegate> delegate_;
  raw_ptr<MockFilterTabController> mock_controller_ = nullptr;
  std::unique_ptr<ContentFilterNavigationObserver>
      content_filter_navigation_observer_;
};

// Tests that a valid HTTPS navigation triggers OnNavigationFinished and
// populates all metadata fields correctly.
TEST_F(ContentFilterNavigationObserverTest, HttpsNavigation) {
  const GURL url("https://www.example.com");
  FilterNavigationMetadata captured_metadata;
  EXPECT_CALL(mock_controller(), OnNavigationFinished)
      .WillOnce(testing::SaveArg<0>(&captured_metadata));

  NavigationTimeCapturer time_capturer(web_contents());

  std::unique_ptr<content::NavigationSimulator> navigation =
      content::NavigationSimulator::CreateBrowserInitiated(url, web_contents());
  navigation->Start();
  navigation->Commit();

  EXPECT_EQ(captured_metadata.navigation_id, time_capturer.navigation_id());
  EXPECT_EQ(captured_metadata.ukm_source_id, time_capturer.ukm_source_id());
  EXPECT_EQ(captured_metadata.navigation_start_time,
            time_capturer.start_time());
  EXPECT_EQ(captured_metadata.navigation_finish_time,
            time_capturer.finish_time());
  EXPECT_EQ(captured_metadata.url, url);
  EXPECT_EQ(captured_metadata.prev_url, GURL());
  EXPECT_TRUE(captured_metadata.is_cryptographic_scheme);
  EXPECT_FALSE(captured_metadata.is_http_allowed_for_testing);
  EXPECT_EQ(captured_metadata.net_error_code, net::OK);
  EXPECT_EQ(captured_metadata.http_response_code, 200);
  EXPECT_FALSE(captured_metadata.is_error_page_navigation);
  EXPECT_TRUE(captured_metadata.has_user_gesture);
  EXPECT_FALSE(captured_metadata.was_filter_initiated_navigation);
  EXPECT_FALSE(captured_metadata.is_same_document_navigation);
}

// Tests that a navigation from the address bar is correctly identified as
// navigation from omnibox or bookmarks.
TEST_F(ContentFilterNavigationObserverTest, NavigationFromAddressBar) {
  const GURL url("https://www.example.com");
  FilterNavigationMetadata captured_metadata;
  EXPECT_CALL(mock_controller(), OnNavigationFinished)
      .WillOnce(testing::SaveArg<0>(&captured_metadata));

  std::unique_ptr<content::NavigationSimulator> navigation =
      content::NavigationSimulator::CreateBrowserInitiated(url, web_contents());
  navigation->SetTransition(ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR));
  navigation->Start();
  navigation->Commit();

  EXPECT_TRUE(captured_metadata.is_navigation_from_omnibox_or_bookmarks);
}

// Tests that a navigation from a bookmark is correctly identified as
// navigation from omnibox or bookmarks.
TEST_F(ContentFilterNavigationObserverTest, NavigationFromBookmark) {
  const GURL url("https://www.example.com");
  FilterNavigationMetadata captured_metadata;
  EXPECT_CALL(mock_controller(), OnNavigationFinished)
      .WillOnce(testing::SaveArg<0>(&captured_metadata));

  std::unique_ptr<content::NavigationSimulator> navigation =
      content::NavigationSimulator::CreateBrowserInitiated(url, web_contents());
  navigation->SetTransition(ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  navigation->Start();
  navigation->Commit();

  EXPECT_TRUE(captured_metadata.is_navigation_from_omnibox_or_bookmarks);
}

// Tests that a standard link click is not identified as user-initiated
// navigation from omnibox or bookmarks.
TEST_F(ContentFilterNavigationObserverTest,
       StandardNavigationNotFromOmniboxOrBookmark) {
  const GURL url("https://www.example.com");
  FilterNavigationMetadata captured_metadata;
  EXPECT_CALL(mock_controller(), OnNavigationFinished)
      .WillOnce(testing::SaveArg<0>(&captured_metadata));

  std::unique_ptr<content::NavigationSimulator> navigation =
      content::NavigationSimulator::CreateRendererInitiated(url, main_rfh());
  navigation->SetTransition(ui::PAGE_TRANSITION_LINK);
  navigation->Start();
  navigation->Commit();

  EXPECT_FALSE(captured_metadata.is_navigation_from_omnibox_or_bookmarks);
}

// Tests that same-document navigations trigger OnNavigationFinished and
// correctly populate the `is_same_document_navigation` metadata flag.
TEST_F(ContentFilterNavigationObserverTest, SameDocumentNavigation) {
  const GURL url("https://www.example.com");
  EXPECT_CALL(mock_controller(), OnNavigationFinished);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url);

  FilterNavigationMetadata captured_metadata;
  EXPECT_CALL(mock_controller(), OnNavigationFinished)
      .WillOnce(testing::SaveArg<0>(&captured_metadata));
  const GURL same_doc_url("https://www.example.com/#test");
  std::unique_ptr<content::NavigationSimulator> navigation =
      content::NavigationSimulator::CreateRendererInitiated(same_doc_url,
                                                            main_rfh());
  navigation->CommitSameDocument();
  EXPECT_TRUE(captured_metadata.is_same_document_navigation);
  EXPECT_EQ(captured_metadata.url, same_doc_url);
}

// Tests that back navigations trigger OnNavigationFinished and correctly
// populate the `is_back_navigation` metadata flag.
TEST_F(ContentFilterNavigationObserverTest, BackNavigation) {
  const GURL url1("https://www.example.com/1");
  const GURL url2("https://www.example.com/2");
  EXPECT_CALL(mock_controller(), OnNavigationFinished);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url1);
  EXPECT_CALL(mock_controller(), OnNavigationFinished);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url2);
  FilterNavigationMetadata captured_metadata;
  EXPECT_CALL(mock_controller(), OnNavigationFinished)
      .WillOnce(testing::SaveArg<0>(&captured_metadata));
  content::NavigationSimulator::GoBack(web_contents());
  EXPECT_TRUE(captured_metadata.is_back_navigation);
  EXPECT_EQ(captured_metadata.url, url1);
}

// Tests that forward navigations trigger OnNavigationFinished and do NOT
// populate the `is_back_navigation` metadata flag.
TEST_F(ContentFilterNavigationObserverTest, ForwardNavigation) {
  const GURL url1("https://www.example.com/1");
  const GURL url2("https://www.example.com/2");
  EXPECT_CALL(mock_controller(), OnNavigationFinished);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url1);
  EXPECT_CALL(mock_controller(), OnNavigationFinished);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url2);
  EXPECT_CALL(mock_controller(), OnNavigationFinished);
  content::NavigationSimulator::GoBack(web_contents());

  FilterNavigationMetadata captured_metadata;
  EXPECT_CALL(mock_controller(), OnNavigationFinished(_))
      .WillOnce(testing::SaveArg<0>(&captured_metadata));
  content::NavigationSimulator::GoForward(web_contents());
  testing::Mock::VerifyAndClearExpectations(&mock_controller());
  EXPECT_FALSE(captured_metadata.is_back_navigation);
  EXPECT_EQ(captured_metadata.url, url2);
}

// Tests that an uncommitted/aborted navigation does not trigger
// OnNavigationFinished.
TEST_F(ContentFilterNavigationObserverTest, UncommittedNavigation) {
  const GURL url("https://www.example.com");
  EXPECT_CALL(mock_controller(), OnNavigationFinished).Times(0);
  std::unique_ptr<content::NavigationSimulator> navigation =
      content::NavigationSimulator::CreateBrowserInitiated(url, web_contents());
  navigation->Start();
  navigation->AbortCommit();
}

// Tests that subframe navigations are ignored.
TEST_F(ContentFilterNavigationObserverTest, SubframeNavigation) {
  const GURL url("https://www.example.com");
  EXPECT_CALL(mock_controller(), OnNavigationFinished);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url);

  EXPECT_CALL(mock_controller(), OnNavigationFinished).Times(0);
  const GURL subframe_url("https://www.example.com/subframe");
  content::RenderFrameHost* const subframe =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("subframe");
  content::NavigationSimulator::NavigateAndCommitFromDocument(subframe_url,
                                                              subframe);
}

// Tests that the observer populates metadata correctly for an error page
// navigation.
TEST_F(ContentFilterNavigationObserverTest, ErrorPage) {
  const GURL url("https://www.example.com");

  FilterNavigationMetadata captured_metadata;
  EXPECT_CALL(mock_controller(), OnNavigationFinished)
      .WillOnce(testing::SaveArg<0>(&captured_metadata));

  std::unique_ptr<content::NavigationSimulator> navigation =
      content::NavigationSimulator::CreateBrowserInitiated(url, web_contents());
  navigation->Fail(net::ERR_CONNECTION_RESET);
  navigation->CommitErrorPage();

  EXPECT_TRUE(captured_metadata.is_error_page_navigation);
  EXPECT_EQ(captured_metadata.net_error_code, net::ERR_CONNECTION_RESET);
}

// Tests that the observer populates metadata correctly for a filter-initiated
// navigation.
TEST_F(ContentFilterNavigationObserverTest, FilterInitiated) {
  const GURL url("https://www.example.com");

  FilterNavigationMetadata captured_metadata;
  EXPECT_CALL(mock_controller(), OnNavigationFinished)
      .WillOnce(testing::SaveArg<0>(&captured_metadata));

  std::unique_ptr<content::NavigationSimulator> navigation =
      content::NavigationSimulator::CreateBrowserInitiated(url, web_contents());
  navigation->Start();

  const UrlFilterSuggestion suggestion = CreateSuggestion("task1");
  FilterInitiatedNavigationMarker::CreateForNavigationHandle(
      *navigation->GetNavigationHandle(), suggestion);

  navigation->Commit();

  EXPECT_TRUE(captured_metadata.was_filter_initiated_navigation);
  ASSERT_TRUE(captured_metadata.applied_suggestion.has_value());
  EXPECT_EQ(captured_metadata.applied_suggestion.value(), suggestion);
}

// Tests that a render process crash clears existing suggestions.
TEST_F(ContentFilterNavigationObserverTest, PrimaryMainFrameRenderProcessGone) {
  EXPECT_CALL(delegate(), ClearSuggestion());
  observer()->PrimaryMainFrameRenderProcessGone(
      base::TERMINATION_STATUS_PROCESS_CRASHED);
}

}  // namespace

}  // namespace multistep_filter
