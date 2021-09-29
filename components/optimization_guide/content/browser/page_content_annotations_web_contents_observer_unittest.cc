// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_content_annotations_web_contents_observer.h"

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/google/core/common/google_switches.h"
#include "components/history/core/browser/history_service.h"
#include "components/optimization_guide/content/browser/page_content_annotations_service.h"
#include "components/optimization_guide/content/browser/page_text_dump_result.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

class TestPageTextObserver : public PageTextObserver {
 public:
  explicit TestPageTextObserver(content::WebContents* web_contents)
      : PageTextObserver(web_contents) {}

  void AddConsumer(PageTextObserver::Consumer* consumer) override {
    add_consumer_called_ = true;
  }
  bool add_consumer_called() const { return add_consumer_called_; }

  // We don't test remove consumer since there is no guaranteed ordering when
  // WebContentsObservers are destroyed, so we may hit a segfault.

 private:
  bool add_consumer_called_ = false;
};

class FakePageContentAnnotationsService : public PageContentAnnotationsService {
 public:
  explicit FakePageContentAnnotationsService(
      OptimizationGuideModelProvider* optimization_guide_model_provider,
      history::HistoryService* history_service)
      : PageContentAnnotationsService(optimization_guide_model_provider,
                                      history_service) {}
  ~FakePageContentAnnotationsService() override = default;

  void Annotate(const HistoryVisit& visit, const std::string& text) override {
    last_annotation_request_.emplace(std::make_pair(visit, text));
  }

  void ExtractRelatedSearches(const HistoryVisit& visit,
                              content::WebContents* web_contents) override {
    last_related_searches_extraction_request_.emplace(
        std::make_pair(visit, web_contents));
  }

  absl::optional<std::pair<HistoryVisit, std::string>> last_annotation_request()
      const {
    return last_annotation_request_;
  }

  absl::optional<std::pair<HistoryVisit, content::WebContents*>>
  last_related_searches_extraction_request() const {
    return last_related_searches_extraction_request_;
  }

 private:
  absl::optional<std::pair<HistoryVisit, std::string>> last_annotation_request_;
  absl::optional<std::pair<HistoryVisit, content::WebContents*>>
      last_related_searches_extraction_request_;
};

class PageContentAnnotationsWebContentsObserverTest
    : public content::RenderViewHostTestHarness {
 public:
  PageContentAnnotationsWebContentsObserverTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kPageContentAnnotations,
        {{"extract_related_searches", "false"}});
  }

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    optimization_guide_model_provider_ =
        std::make_unique<TestOptimizationGuideModelProvider>();
    history_service_ = std::make_unique<history::HistoryService>();
    page_content_annotations_service_ =
        std::make_unique<FakePageContentAnnotationsService>(
            optimization_guide_model_provider_.get(), history_service_.get());

    page_text_observer_ = new TestPageTextObserver(web_contents());
    web_contents()->SetUserData(TestPageTextObserver::UserDataKey(),
                                base::WrapUnique(page_text_observer_));

    PageContentAnnotationsWebContentsObserver::CreateForWebContents(
        web_contents(), page_content_annotations_service_.get());
  }

  void TearDown() override {
    page_text_observer_ = nullptr;
    page_content_annotations_service_.reset();
    optimization_guide_model_provider_.reset();

    content::RenderViewHostTestHarness::TearDown();
  }

  FakePageContentAnnotationsService* service() {
    return page_content_annotations_service_.get();
  }

  PageContentAnnotationsWebContentsObserver* helper() {
    return PageContentAnnotationsWebContentsObserver::FromWebContents(
        web_contents());
  }

  TestPageTextObserver* page_text_observer() { return page_text_observer_; }

  std::unique_ptr<PageTextObserver::ConsumerTextDumpRequest>
  RequestTextDumpForUrl(const GURL& url) {
    content::MockNavigationHandle navigation_handle(url, main_rfh());
    navigation_handle.set_url(url);
    // PageTextObserver is guaranteed to call MaybeRequestFrameTextDump after
    // the navigation has been committed.
    navigation_handle.set_has_committed(true);
    return helper()->MaybeRequestFrameTextDump(&navigation_handle);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestOptimizationGuideModelProvider>
      optimization_guide_model_provider_;
  std::unique_ptr<history::HistoryService> history_service_;
  std::unique_ptr<FakePageContentAnnotationsService>
      page_content_annotations_service_;
  TestPageTextObserver* page_text_observer_;
};

TEST_F(PageContentAnnotationsWebContentsObserverTest,
       HooksIntoPageTextObserver) {
  EXPECT_TRUE(page_text_observer()->add_consumer_called());
}

TEST_F(PageContentAnnotationsWebContentsObserverTest,
       DoesNotRequestForNonHttpHttps) {
  EXPECT_EQ(RequestTextDumpForUrl(GURL("chrome://new-tab")), nullptr);
}

TEST_F(PageContentAnnotationsWebContentsObserverTest,
       RequestsForMainFrameHttpUrlCallbackDispatchesToService) {
  // Navigate and commit so there is an entry. In actual situations, we are
  // guaranteed that MaybeRequestFrameTextDump will only be called for
  // committed frames.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://test.com"));

  std::unique_ptr<PageTextObserver::ConsumerTextDumpRequest> request =
      RequestTextDumpForUrl(GURL("http://test.com"));
  ASSERT_TRUE(request);
  ASSERT_TRUE(request->callback);
  EXPECT_EQ(features::MaxSizeForPageContentTextDump(), request->max_size);
  EXPECT_TRUE(request->dump_amp_subframes);
  EXPECT_EQ(std::set<mojom::TextDumpEvent>{mojom::TextDumpEvent::kFirstLayout},
            request->events);

  // Invoke OnTextDumpReceived.
  FrameTextDumpResult frame_result =
      FrameTextDumpResult::Initialize(mojom::TextDumpEvent::kFirstLayout,
                                      content::GlobalRenderFrameHostId(),
                                      /*amp_frame=*/false,
                                      /*unique_navigation_id=*/1)
          .CompleteWithContents(u"some text");
  PageTextDumpResult result;
  result.AddFrameTextDumpResult(frame_result);
  std::move(request->callback).Run(std::move(result));

  absl::optional<std::pair<HistoryVisit, std::string>> last_annotation_request =
      service()->last_annotation_request();
  EXPECT_TRUE(last_annotation_request.has_value());
  EXPECT_EQ(last_annotation_request->first.url, GURL("http://test.com"));
  EXPECT_EQ(last_annotation_request->second, "some text");
}

TEST_F(PageContentAnnotationsWebContentsObserverTest,
       RequestsRelatedSearchesForMainFrameSRPUrl) {
  // Navigate to non-Google SRP and commit.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://www.foo.com/search?q=a"));

  absl::optional<std::pair<HistoryVisit, content::WebContents*>> last_request =
      service()->last_related_searches_extraction_request();
  EXPECT_FALSE(last_request.has_value());

  // Overwrite Google base URL.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kGoogleBaseURL, "http://www.foo.com/");

  // Navigate to Google SRP and commit.
  // No request should be sent since extracting related searches is disabled.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://www.foo.com/search?q=a"));
  last_request = service()->last_related_searches_extraction_request();
  EXPECT_FALSE(last_request.has_value());
}

class PageContentAnnotationsWebContentsObserverRelatedSearchesTest
    : public PageContentAnnotationsWebContentsObserverTest {
 public:
  PageContentAnnotationsWebContentsObserverRelatedSearchesTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kPageContentAnnotations,
        {{"extract_related_searches", "true"}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(PageContentAnnotationsWebContentsObserverRelatedSearchesTest,
       RequestsRelatedSearchesForMainFrameSRPUrl) {
  // Navigate to non-Google SRP and commit.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://www.foo.com/search?q=a"));

  absl::optional<std::pair<HistoryVisit, content::WebContents*>> last_request =
      service()->last_related_searches_extraction_request();
  EXPECT_FALSE(last_request.has_value());

  // Overwrite Google base URL.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kGoogleBaseURL, "http://www.foo.com/");

  // Navigate to Google SRP and commit.
  // Expect a request to be sent since extracting related searches is enabled.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://www.foo.com/search?q=a"));
  last_request = service()->last_related_searches_extraction_request();
  EXPECT_TRUE(last_request.has_value());
  EXPECT_EQ(last_request->first.url, GURL("http://www.foo.com/search?q=a"));
  EXPECT_EQ(last_request->second, web_contents());
}

}  // namespace optimization_guide
