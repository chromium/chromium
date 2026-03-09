// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/content/filter_navigation_observer.h"

#include "base/functional/callback_helpers.h"
#include "components/multistep_filter/core/multistep_filter_service.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace multistep_filter {

namespace {

class MockMultistepFilterService : public MultistepFilterService {
 public:
  MockMultistepFilterService() : MultistepFilterService(nullptr, nullptr) {}
  ~MockMultistepFilterService() override = default;

  void GenerateFilterSuggestions(
      const GURL& url,
      base::OnceCallback<void(std::optional<UrlFilterSuggestion>)> callback)
      override {
    GenerateFilterSuggestions(url);
  }

  MOCK_METHOD(void, GenerateFilterSuggestions, (const GURL& url));
};

class MockUiDelegate : public FilterNavigationObserver::UiDelegate {
 public:
  MOCK_METHOD(void, ClearSuggestion, (), (override));
  MOCK_METHOD(base::OnceCallback<void(std::optional<UrlFilterSuggestion>)>,
              GetSuggestionCallback,
              (),
              (override));
};

class FilterNavigationObserverTest : public content::RenderViewHostTestHarness {
 public:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    mock_service_ = std::make_unique<MockMultistepFilterService>();
    auto delegate = std::make_unique<MockUiDelegate>();
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
    auto delegate = std::make_unique<MockUiDelegate>();
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
  EXPECT_CALL(delegate(), GetSuggestionCallback())
      .WillOnce(testing::Return(base::DoNothing()));
  EXPECT_CALL(mock_service(), GenerateFilterSuggestions(url));
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url);
}

TEST_F(FilterNavigationObserverTest, HttpNavigation) {
  const GURL url("http://www.example.com");
  EXPECT_CALL(delegate(), ClearSuggestion());
  EXPECT_CALL(delegate(), GetSuggestionCallback())
      .WillOnce(testing::Return(base::DoNothing()));
  EXPECT_CALL(mock_service(), GenerateFilterSuggestions(url));
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url);
}

TEST_F(FilterNavigationObserverTest, NonHttpNavigation) {
  const GURL url("ftp://www.example.com");
  EXPECT_CALL(delegate(), ClearSuggestion());
  EXPECT_CALL(mock_service(), GenerateFilterSuggestions(testing::_)).Times(0);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url);
}

TEST_F(FilterNavigationObserverTest, SameDocumentNavigation) {
  const GURL url("https://www.example.com");
  EXPECT_CALL(delegate(), ClearSuggestion());
  EXPECT_CALL(delegate(), GetSuggestionCallback())
      .WillOnce(testing::Return(base::DoNothing()));
  EXPECT_CALL(mock_service(), GenerateFilterSuggestions(url));
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url);

  const GURL same_doc_url("https://www.example.com/#test");
  EXPECT_CALL(delegate(), ClearSuggestion()).Times(0);
  EXPECT_CALL(mock_service(), GenerateFilterSuggestions(testing::_)).Times(0);
  auto navigation = content::NavigationSimulator::CreateRendererInitiated(
      same_doc_url, main_rfh());
  navigation->CommitSameDocument();
}

TEST_F(FilterNavigationObserverTest, AbortedNavigation) {
  const GURL url("https://www.example.com");
  EXPECT_CALL(delegate(), ClearSuggestion()).Times(0);
  EXPECT_CALL(mock_service(), GenerateFilterSuggestions(testing::_)).Times(0);
  auto navigation =
      content::NavigationSimulator::CreateBrowserInitiated(url, web_contents());
  navigation->Start();
  navigation->AbortCommit();
}

TEST_F(FilterNavigationObserverTest, SubframeNavigation) {
  const GURL url("https://www.example.com");
  EXPECT_CALL(delegate(), ClearSuggestion());
  EXPECT_CALL(delegate(), GetSuggestionCallback())
      .WillOnce(testing::Return(base::DoNothing()));
  EXPECT_CALL(mock_service(), GenerateFilterSuggestions(url));
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url);

  const GURL subframe_url("https://www.example.com/subframe");
  EXPECT_CALL(delegate(), ClearSuggestion()).Times(0);
  EXPECT_CALL(mock_service(), GenerateFilterSuggestions(testing::_)).Times(0);
  content::RenderFrameHost* subframe =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("subframe");
  content::NavigationSimulator::NavigateAndCommitFromDocument(subframe_url,
                                                              subframe);
}

TEST_F(FilterNavigationObserverTest, ErrorPageNavigation) {
  const GURL url("https://www.example.com");
  EXPECT_CALL(delegate(), ClearSuggestion());
  EXPECT_CALL(mock_service(), GenerateFilterSuggestions(testing::_)).Times(0);
  content::NavigationSimulator::NavigateAndFailFromBrowser(web_contents(), url,
                                                           net::ERR_TIMED_OUT);
}

TEST_F(FilterNavigationObserverTest, ReloadNavigation) {
  const GURL url("https://www.example.com");
  EXPECT_CALL(delegate(), ClearSuggestion());
  EXPECT_CALL(delegate(), GetSuggestionCallback())
      .WillOnce(testing::Return(base::DoNothing()));
  EXPECT_CALL(mock_service(), GenerateFilterSuggestions(url));
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url);

  testing::Mock::VerifyAndClearExpectations(&delegate());
  testing::Mock::VerifyAndClearExpectations(&mock_service());

  EXPECT_CALL(delegate(), ClearSuggestion());
  EXPECT_CALL(mock_service(), GenerateFilterSuggestions(testing::_)).Times(0);
  content::NavigationSimulator::Reload(web_contents());
}

TEST_F(FilterNavigationObserverTest, NullWebContentsNavigation) {
  content::MockNavigationHandle handle;
  handle.set_has_committed(true);
  handle.set_is_in_primary_main_frame(true);
  handle.set_is_same_document(false);
  handle.set_reload_type(content::ReloadType::NONE);
  handle.set_is_error_page(false);
  handle.set_url(GURL("https://example.com"));

  EXPECT_CALL(delegate(), ClearSuggestion());
  EXPECT_CALL(mock_service(), GenerateFilterSuggestions(testing::_)).Times(0);

  observer()->DidFinishNavigation(&handle);
}

TEST_F(FilterNavigationObserverTest, NullService) {
  RecreateObserverWithNullService();

  const GURL url("https://www.example.com");

  EXPECT_CALL(delegate(), ClearSuggestion());
  EXPECT_CALL(mock_service(), GenerateFilterSuggestions(testing::_)).Times(0);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url);
}

TEST_F(FilterNavigationObserverTest, AboutBlankNavigation) {
  const GURL url("about:blank");
  EXPECT_CALL(delegate(), ClearSuggestion());
  EXPECT_CALL(mock_service(), GenerateFilterSuggestions(testing::_)).Times(0);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url);
}

TEST_F(FilterNavigationObserverTest, RendererInitiatedNavigation) {
  const GURL url("https://www.example.com");
  EXPECT_CALL(delegate(), ClearSuggestion());
  EXPECT_CALL(delegate(), GetSuggestionCallback())
      .WillOnce(testing::Return(base::DoNothing()));
  EXPECT_CALL(mock_service(), GenerateFilterSuggestions(url));
  content::NavigationSimulator::NavigateAndCommitFromDocument(url, main_rfh());
}

TEST_F(FilterNavigationObserverTest, ReferenceFragmentNavigation) {
  // Navigation to a URL with a reference fragment (cross-document).
  const GURL url("https://www.example.com/#test");
  EXPECT_CALL(delegate(), ClearSuggestion());
  EXPECT_CALL(delegate(), GetSuggestionCallback())
      .WillOnce(testing::Return(base::DoNothing()));
  EXPECT_CALL(mock_service(), GenerateFilterSuggestions(url));
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url);
}

}  // namespace

}  // namespace multistep_filter
