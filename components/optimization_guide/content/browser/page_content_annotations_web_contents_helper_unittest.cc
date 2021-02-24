// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_content_annotations_web_contents_helper.h"

#include "base/strings/utf_string_conversions.h"
#include "components/optimization_guide/content/browser/test_optimization_guide_decider.h"
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
      OptimizationGuideDecider* optimization_guide_decider)
      : PageContentAnnotationsService(optimization_guide_decider) {}
  ~FakePageContentAnnotationsService() override = default;

  void Annotate(const HistoryVisit& visit,
                const base::string16& text) override {
    last_annotation_request_.emplace(std::make_pair(visit, text));
  }

  base::Optional<std::pair<HistoryVisit, base::string16>>
  last_annotation_request() const {
    return last_annotation_request_;
  }

 private:
  base::Optional<std::pair<HistoryVisit, base::string16>>
      last_annotation_request_;
};

class PageContentAnnotationsWebContentsHelperTest
    : public content::RenderViewHostTestHarness {
 public:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    optimization_guide_decider_ =
        std::make_unique<TestOptimizationGuideDecider>();
    page_content_annotations_service_ =
        std::make_unique<FakePageContentAnnotationsService>(
            optimization_guide_decider_.get());

    page_text_observer_ = new TestPageTextObserver(web_contents());
    web_contents()->SetUserData(TestPageTextObserver::UserDataKey(),
                                base::WrapUnique(page_text_observer_));

    PageContentAnnotationsWebContentsHelper::CreateForWebContents(
        web_contents(), page_content_annotations_service_.get());
  }

  void TearDown() override {
    page_text_observer_ = nullptr;
    page_content_annotations_service_.reset();
    optimization_guide_decider_.reset();

    content::RenderViewHostTestHarness::TearDown();
  }

  FakePageContentAnnotationsService* service() {
    return page_content_annotations_service_.get();
  }

  PageContentAnnotationsWebContentsHelper* helper() {
    return PageContentAnnotationsWebContentsHelper::FromWebContents(
        web_contents());
  }

  TestPageTextObserver* page_text_observer() { return page_text_observer_; }

  std::unique_ptr<PageTextObserver::ConsumerTextDumpRequest>
  RequestTextDumpForUrl(const GURL& url, bool is_main_frame) {
    content::RenderFrameHost* frame = main_rfh();
    if (!is_main_frame) {
      // Commit main frame, so we can attach the subframe.
      content::NavigationSimulator::NavigateAndCommitFromBrowser(
          web_contents(), GURL("http://test.com"));
      frame = content::RenderFrameHostTester::For(main_rfh())
                  ->AppendChild("subframe");
    }
    content::MockNavigationHandle navigation_handle(url, frame);
    navigation_handle.set_url(url);
    // PageTextObserver is guaranteed to call MaybeRequestFrameTextDump after
    // the navigation has been committeed.
    navigation_handle.set_has_committed(true);
    return helper()->MaybeRequestFrameTextDump(&navigation_handle);
  }

 private:
  std::unique_ptr<TestOptimizationGuideDecider> optimization_guide_decider_;
  std::unique_ptr<FakePageContentAnnotationsService>
      page_content_annotations_service_;
  TestPageTextObserver* page_text_observer_;
};

TEST_F(PageContentAnnotationsWebContentsHelperTest, HooksIntoPageTextObserver) {
  EXPECT_TRUE(page_text_observer()->add_consumer_called());
}

TEST_F(PageContentAnnotationsWebContentsHelperTest,
       DoesNotRequestForNonHttpHttps) {
  EXPECT_EQ(
      RequestTextDumpForUrl(GURL("chrome://new-tab"), /*is_main_frame=*/true),
      nullptr);
}

TEST_F(PageContentAnnotationsWebContentsHelperTest, DoesNotRequestForSubframe) {
  EXPECT_EQ(
      RequestTextDumpForUrl(GURL("http://test.com"), /*is_main_frame=*/false),
      nullptr);
}

TEST_F(PageContentAnnotationsWebContentsHelperTest,
       RequestsForMainFrameHttpUrlCallbackDispatchesToService) {
  // Navigate and commit so there is an entry. In actual situations, we are
  // guaranteed that MaybeRequestFrameTextDump will only be called for
  // committed frames.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://test.com"));

  std::unique_ptr<PageTextObserver::ConsumerTextDumpRequest> request =
      RequestTextDumpForUrl(GURL("http://test.com"), /*is_main_frame=*/true);
  ASSERT_TRUE(request);

  // Invoke OnTextDumpReceived.
  std::move(request->callback).Run(base::ASCIIToUTF16("some text"));

  base::Optional<std::pair<HistoryVisit, base::string16>>
      last_annotation_request = service()->last_annotation_request();
  EXPECT_TRUE(last_annotation_request.has_value());
  EXPECT_EQ(last_annotation_request->first.url, GURL("http://test.com"));
  EXPECT_EQ(last_annotation_request->second, base::ASCIIToUTF16("some text"));
}

}  // namespace optimization_guide
