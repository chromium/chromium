// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/content/filter_navigation_observer.h"

#include "base/functional/callback_helpers.h"
#include "components/multistep_filter/content/filter_initiated_navigation_marker.h"
#include "components/multistep_filter/core/annotation_index/annotation_index_client.h"
#include "components/multistep_filter/core/annotation_index/mock_annotation_index_client.h"
#include "components/multistep_filter/core/multistep_filter_service.h"
#include "components/multistep_filter/core/multistep_filter_ui_delegate.h"
#include "components/multistep_filter/core/storage/filter_store.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace multistep_filter {

namespace {

using ::testing::_;

class MockMultistepFilterService : public MultistepFilterService {
 public:
  MockMultistepFilterService(
      std::unique_ptr<AnnotationIndexClient> annotation_index_client,
      std::unique_ptr<FilterStore> filter_store)
      : MultistepFilterService(std::move(annotation_index_client),
                               std::move(filter_store),
                               /*identity_manager=*/nullptr) {
    ON_CALL(*this, GenerateFilterSuggestions)
        .WillByDefault(
            [](const GURL& url,
               base::OnceCallback<void(std::optional<UrlFilterSuggestion>)>
                   callback) {
              if (callback) {
                std::move(callback).Run(std::nullopt);
              }
            });
  }
  ~MockMultistepFilterService() override = default;

  MOCK_METHOD(void, ExtractAnnotation, (const GURL& url), (override));
  MOCK_METHOD(
      void,
      GenerateFilterSuggestions,
      (const GURL& url,
       base::OnceCallback<void(std::optional<UrlFilterSuggestion>)> callback),
      (override));
};

class MockUiDelegate : public MultistepFilterUiDelegate {
 public:
  MOCK_METHOD(void, ClearSuggestion, (), (override));
  MOCK_METHOD(void,
              OnSuggestionGenerated,
              (std::optional<UrlFilterSuggestion> suggestion),
              (override));
  MOCK_METHOD(bool, ShouldSuppressSuggestions, (const GURL& url), (override));
  base::WeakPtr<MultistepFilterUiDelegate> GetWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockUiDelegate> weak_ptr_factory_{this};
};

class FilterNavigationObserverTest : public content::RenderViewHostTestHarness {
 public:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    mock_service_ = std::make_unique<MockMultistepFilterService>(
        std::make_unique<MockAnnotationIndexClient>(),
        std::make_unique<FilterStore>());
    auto delegate = std::make_unique<testing::NiceMock<MockUiDelegate>>();
    delegate_ = delegate.get();

    filter_navigation_observer_ = std::make_unique<FilterNavigationObserver>(
        web_contents(), mock_service_.get(), std::move(delegate));
  }

  void TearDown() override {
    delegate_ = nullptr;
    filter_navigation_observer_.reset();
    mock_service_.reset();
    content::RenderViewHostTestHarness::TearDown();
  }

  MockMultistepFilterService& mock_service() { return *mock_service_; }
  MockUiDelegate& delegate() { return *delegate_; }
  FilterNavigationObserver* observer() {
    return filter_navigation_observer_.get();
  }

  void RecreateObserverWithNullService() {
    auto delegate = std::make_unique<testing::NiceMock<MockUiDelegate>>();
    delegate_ = delegate.get();
    filter_navigation_observer_ = std::make_unique<FilterNavigationObserver>(
        web_contents(), nullptr, std::move(delegate));
  }

 private:
  std::unique_ptr<MockMultistepFilterService> mock_service_;
  raw_ptr<MockUiDelegate> delegate_;
  std::unique_ptr<FilterNavigationObserver> filter_navigation_observer_;
};

TEST_F(FilterNavigationObserverTest, HttpsNavigation) {
  const GURL url("https://www.example.com");
  EXPECT_CALL(delegate(), ClearSuggestion());
  EXPECT_CALL(mock_service(), ExtractAnnotation(url));
  EXPECT_CALL(mock_service(), GenerateFilterSuggestions(url, _));
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url);
}

TEST_F(FilterNavigationObserverTest, HttpNavigation) {
  const GURL url("http://www.example.com");
  EXPECT_CALL(delegate(), ClearSuggestion());
  EXPECT_CALL(mock_service(), ExtractAnnotation(url));
  EXPECT_CALL(mock_service(), GenerateFilterSuggestions(url, _));
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url);
}

TEST_F(FilterNavigationObserverTest, NonHttpNavigation) {
  const GURL url("ftp://www.example.com");
  EXPECT_CALL(delegate(), ClearSuggestion());
  EXPECT_CALL(mock_service(), ExtractAnnotation).Times(0);
  EXPECT_CALL(mock_service(), GenerateFilterSuggestions).Times(0);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url);
}

TEST_F(FilterNavigationObserverTest, SameDocumentNavigation) {
  const GURL url("https://www.example.com");
  EXPECT_CALL(delegate(), ClearSuggestion());
  EXPECT_CALL(mock_service(), ExtractAnnotation(url));
  EXPECT_CALL(mock_service(), GenerateFilterSuggestions(url, _));
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url);
  // Reset expectations to test the next navigation.
  testing::Mock::VerifyAndClearExpectations(&delegate());
  testing::Mock::VerifyAndClearExpectations(&mock_service());

  // Same-document navigations (like fragment changes) should NOT clear
  // suggestions, but SHOULD still allow extraction if they are
  // renderer-initiated with a user gesture.
  const GURL same_doc_url("https://www.example.com/#test");
  EXPECT_CALL(delegate(), ClearSuggestion()).Times(0);
  EXPECT_CALL(mock_service(), ExtractAnnotation(same_doc_url));
  EXPECT_CALL(mock_service(), GenerateFilterSuggestions).Times(0);
  auto navigation = content::NavigationSimulator::CreateRendererInitiated(
      same_doc_url, main_rfh());
  navigation->CommitSameDocument();
}

TEST_F(FilterNavigationObserverTest, SameUrlReCommitNavigation) {
  const GURL url("https://www.example.com");
  EXPECT_CALL(delegate(), ClearSuggestion());
  EXPECT_CALL(mock_service(), ExtractAnnotation(url));
  EXPECT_CALL(mock_service(), GenerateFilterSuggestions(url, _));
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url);

  // Reset expectations to test the next navigation.
  testing::Mock::VerifyAndClearExpectations(&delegate());
  testing::Mock::VerifyAndClearExpectations(&mock_service());

  // Multiple navigations to the same URL should NOT clear suggestions.
  EXPECT_CALL(delegate(), ClearSuggestion()).Times(0);
  EXPECT_CALL(mock_service(), ExtractAnnotation).Times(0);
  EXPECT_CALL(mock_service(), GenerateFilterSuggestions).Times(0);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url);
}

TEST_F(FilterNavigationObserverTest, AbortedNavigation) {
  const GURL url("https://www.example.com");
  EXPECT_CALL(delegate(), ClearSuggestion()).Times(0);
  EXPECT_CALL(mock_service(), ExtractAnnotation).Times(0);
  EXPECT_CALL(mock_service(), GenerateFilterSuggestions).Times(0);
  auto navigation =
      content::NavigationSimulator::CreateBrowserInitiated(url, web_contents());
  navigation->Start();
  navigation->AbortCommit();
}

TEST_F(FilterNavigationObserverTest, SubframeNavigation) {
  const GURL url("https://www.example.com");
  EXPECT_CALL(delegate(), ClearSuggestion());
  EXPECT_CALL(mock_service(), ExtractAnnotation(url));
  EXPECT_CALL(mock_service(), GenerateFilterSuggestions(url, _));
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url);

  const GURL subframe_url("https://www.example.com/subframe");
  EXPECT_CALL(delegate(), ClearSuggestion()).Times(0);
  EXPECT_CALL(mock_service(), ExtractAnnotation).Times(0);
  EXPECT_CALL(mock_service(), GenerateFilterSuggestions).Times(0);
  content::RenderFrameHost* subframe =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("subframe");
  content::NavigationSimulator::NavigateAndCommitFromDocument(subframe_url,
                                                              subframe);
}

TEST_F(FilterNavigationObserverTest, ErrorPageNavigation) {
  const GURL url("https://www.example.com");
  EXPECT_CALL(delegate(), ClearSuggestion());
  EXPECT_CALL(mock_service(), ExtractAnnotation).Times(0);
  EXPECT_CALL(mock_service(), GenerateFilterSuggestions).Times(0);
  content::NavigationSimulator::NavigateAndFailFromBrowser(web_contents(), url,
                                                           net::ERR_TIMED_OUT);
}

TEST_F(FilterNavigationObserverTest, ReloadNavigation) {
  const GURL url("https://www.example.com");
  EXPECT_CALL(delegate(), ClearSuggestion());
  EXPECT_CALL(mock_service(), ExtractAnnotation(url));
  EXPECT_CALL(mock_service(), GenerateFilterSuggestions(url, _));
  content::NavigationSimulator::NavigateAndCommitFromDocument(url, main_rfh());

  testing::Mock::VerifyAndClearExpectations(&delegate());
  testing::Mock::VerifyAndClearExpectations(&mock_service());

  // A reload should be ignored and should NOT clear suggestions.
  content::MockNavigationHandle handle;
  handle.set_url(url);
  handle.set_previous_primary_main_frame_url(url);
  handle.set_has_committed(true);
  handle.set_is_in_primary_main_frame(true);
  handle.set_reload_type(content::ReloadType::NORMAL);
  EXPECT_CALL(handle, HasUserGesture()).WillRepeatedly(testing::Return(false));

  EXPECT_CALL(delegate(), ClearSuggestion()).Times(0);
  EXPECT_CALL(mock_service(), ExtractAnnotation).Times(0);
  EXPECT_CALL(mock_service(), GenerateFilterSuggestions).Times(0);
  observer()->DidFinishNavigation(&handle);
}

TEST_F(FilterNavigationObserverTest, NullService) {
  RecreateObserverWithNullService();

  const GURL url("https://www.example.com");

  EXPECT_CALL(delegate(), ClearSuggestion()).Times(0);
  EXPECT_CALL(mock_service(), ExtractAnnotation).Times(0);
  EXPECT_CALL(mock_service(), GenerateFilterSuggestions).Times(0);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url);
}

TEST_F(FilterNavigationObserverTest, AboutBlankNavigation) {
  const GURL url("about:blank");
  EXPECT_CALL(delegate(), ClearSuggestion());
  EXPECT_CALL(mock_service(), ExtractAnnotation).Times(0);
  EXPECT_CALL(mock_service(), GenerateFilterSuggestions).Times(0);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url);
}

TEST_F(FilterNavigationObserverTest, RendererInitiatedNavigation) {
  const GURL url("https://www.example.com");
  EXPECT_CALL(delegate(), ClearSuggestion());
  EXPECT_CALL(mock_service(), ExtractAnnotation(url));
  EXPECT_CALL(mock_service(), GenerateFilterSuggestions(url, _));
  content::NavigationSimulator::NavigateAndCommitFromDocument(url, main_rfh());
}

TEST_F(FilterNavigationObserverTest,
       RendererInitiatedNavigationWithoutUserGesture) {
  const GURL url("https://www.example.com");
  EXPECT_CALL(delegate(), ClearSuggestion());
  // Renderer-initiated navigation WITHOUT user gesture should NOT trigger
  // extraction.
  EXPECT_CALL(mock_service(), ExtractAnnotation).Times(0);
  EXPECT_CALL(mock_service(), GenerateFilterSuggestions(url, _));

  auto navigation =
      content::NavigationSimulator::CreateRendererInitiated(url, main_rfh());
  navigation->SetHasUserGesture(false);
  navigation->Commit();
}

TEST_F(FilterNavigationObserverTest,
       BrowserInitiatedNavigationWithoutUserGesture) {
  const GURL url("https://www.example.com");
  EXPECT_CALL(delegate(), ClearSuggestion());
  // Browser-initiated navigation WITHOUT user gesture should NOT trigger
  // extraction.
  EXPECT_CALL(mock_service(), ExtractAnnotation).Times(0);
  EXPECT_CALL(mock_service(), GenerateFilterSuggestions(url, _));

  auto navigation =
      content::NavigationSimulator::CreateBrowserInitiated(url, web_contents());
  navigation->SetHasUserGesture(false);
  navigation->Commit();
}

TEST_F(FilterNavigationObserverTest, ReferenceFragmentNavigation) {
  // Navigation to a URL with a reference fragment (cross-document).
  const GURL url("https://www.example.com/#test");
  EXPECT_CALL(delegate(), ClearSuggestion());
  EXPECT_CALL(mock_service(), ExtractAnnotation(url));
  EXPECT_CALL(mock_service(), GenerateFilterSuggestions(url, _));
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url);
}

TEST_F(FilterNavigationObserverTest, PageActivationNavigation) {
  content::MockNavigationHandle handle;
  handle.set_has_committed(true);
  handle.set_is_in_primary_main_frame(true);
  handle.set_is_same_document(false);
  handle.set_is_served_from_bfcache(true);
  handle.set_reload_type(content::ReloadType::NONE);
  handle.set_is_error_page(false);
  handle.set_is_renderer_initiated(false);
  EXPECT_CALL(handle, HasUserGesture()).WillRepeatedly(testing::Return(true));
  const GURL url("https://example.com");
  handle.set_url(url);
  handle.set_previous_primary_main_frame_url(
      GURL("https://anotherexample.com"));

  EXPECT_CALL(delegate(), ClearSuggestion()).Times(1);
  EXPECT_CALL(mock_service(), ExtractAnnotation(url));
  EXPECT_CALL(mock_service(), GenerateFilterSuggestions(url, _));

  observer()->DidFinishNavigation(&handle);
}

TEST_F(FilterNavigationObserverTest, PrimaryMainFrameRenderProcessGone) {
  EXPECT_CALL(delegate(), ClearSuggestion());
  observer()->PrimaryMainFrameRenderProcessGone(
      base::TERMINATION_STATUS_PROCESS_CRASHED);
}

TEST_F(FilterNavigationObserverTest, SubdomainNavigation) {
  const GURL url1("https://sub1.example.com");
  const GURL url2("https://sub2.example.com");

  // First navigation.
  EXPECT_CALL(delegate(), ClearSuggestion());
  EXPECT_CALL(mock_service(), ExtractAnnotation(url1));
  EXPECT_CALL(mock_service(), GenerateFilterSuggestions(url1, _));
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url1);
  testing::Mock::VerifyAndClearExpectations(&delegate());
  testing::Mock::VerifyAndClearExpectations(&mock_service());

  // Second navigation (browser-initiated) to a different subdomain of the same
  // domain.
  EXPECT_CALL(delegate(), ClearSuggestion());
  EXPECT_CALL(mock_service(), ExtractAnnotation(url2));
  EXPECT_CALL(mock_service(), GenerateFilterSuggestions).Times(0);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url2);
}

TEST_F(FilterNavigationObserverTest, LocalhostNavigation) {
  const GURL url1("http://localhost:8080/page1");
  const GURL url2("http://localhost:8080/page2");

  // First navigation.
  EXPECT_CALL(delegate(), ClearSuggestion());
  EXPECT_CALL(mock_service(), ExtractAnnotation(url1));
  EXPECT_CALL(mock_service(), GenerateFilterSuggestions(url1, _));
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url1);
  testing::Mock::VerifyAndClearExpectations(&delegate());
  testing::Mock::VerifyAndClearExpectations(&mock_service());

  // Second navigation on same localhost.
  EXPECT_CALL(delegate(), ClearSuggestion());
  EXPECT_CALL(mock_service(), ExtractAnnotation(url2));
  EXPECT_CALL(mock_service(), GenerateFilterSuggestions).Times(0);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url2);
}

TEST_F(FilterNavigationObserverTest, CrossDomainNavigation) {
  const GURL url1("https://www.example.com");
  const GURL url2("https://www.anotherexample.com");

  // Initial navigation.
  EXPECT_CALL(delegate(), ClearSuggestion());
  EXPECT_CALL(mock_service(), ExtractAnnotation(url1));
  EXPECT_CALL(mock_service(), GenerateFilterSuggestions(url1, _));
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url1);
  testing::Mock::VerifyAndClearExpectations(&delegate());
  testing::Mock::VerifyAndClearExpectations(&mock_service());

  EXPECT_CALL(delegate(), ClearSuggestion());
  EXPECT_CALL(mock_service(), ExtractAnnotation(url2));
  EXPECT_CALL(mock_service(), GenerateFilterSuggestions(url2, _));
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url2);
}

TEST_F(FilterNavigationObserverTest,
       DoesNotRequestSuggestionForFilterInitiatedNavigation) {
  const GURL url("https://www.example.com");
  EXPECT_CALL(delegate(), ClearSuggestion());
  EXPECT_CALL(mock_service(), ExtractAnnotation(url));
  EXPECT_CALL(mock_service(), GenerateFilterSuggestions).Times(0);

  auto navigation =
      content::NavigationSimulator::CreateBrowserInitiated(url, web_contents());
  navigation->Start();
  FilterInitiatedNavigationMarker::CreateForNavigationHandle(
      *navigation->GetNavigationHandle());
  navigation->Commit();
}

TEST_F(FilterNavigationObserverTest, SuppressSuggestions) {
  const GURL url("https://www.example.com");
  EXPECT_CALL(delegate(), ClearSuggestion());
  EXPECT_CALL(mock_service(), ExtractAnnotation(url));

  EXPECT_CALL(delegate(), ShouldSuppressSuggestions(url))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(delegate(), OnSuggestionGenerated(testing::Eq(std::nullopt)));
  EXPECT_CALL(mock_service(), GenerateFilterSuggestions).Times(0);

  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url);
}

}  // namespace

}  // namespace multistep_filter
