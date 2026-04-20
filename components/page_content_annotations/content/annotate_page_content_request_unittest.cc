// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/content/annotate_page_content_request.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/page_content_annotations/content/page_content_extraction_service.h"
#include "components/page_content_annotations/core/page_content_annotations_features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"
#include "url/gurl.h"

namespace page_content_annotations {

class TestPageContentExtractionService : public PageContentExtractionService {
 public:
  explicit TestPageContentExtractionService(
      os_crypt_async::OSCryptAsync* os_crypt_async,
      const base::FilePath& profile_path)
      : PageContentExtractionService(os_crypt_async,
                                     profile_path,
                                     &mock_tracker_) {}
  ~TestPageContentExtractionService() override = default;

  void OnPageContentExtracted(
      content::Page& page,
      scoped_refptr<const RefCountedAnnotatedPageContent>
          annotated_page_content,
      const std::vector<uint8_t>& screenshot_data,
      std::optional<int> tab_id) override {
    last_extracted_content_ = ExtractedPageContentResult(
        std::move(annotated_page_content), base::Time::Now(), false, screenshot_data);
    extraction_count_++;
    if (quit_closure_) {
      std::move(quit_closure_).Run();
    }
  }

  int extraction_count() const { return extraction_count_; }
  const std::optional<ExtractedPageContentResult>& last_extracted_content()
      const {
    return last_extracted_content_;
  }

  void SetQuitClosure(base::OnceClosure quit_closure) {
    quit_closure_ = std::move(quit_closure);
  }

 private:
  int extraction_count_ = 0;
  std::optional<ExtractedPageContentResult> last_extracted_content_;
  feature_engagement::test::MockTracker mock_tracker_;
  base::OnceClosure quit_closure_;
};

class AnnotatePageContentRequestTest
    : public content::RenderViewHostTestHarness {
 public:
  AnnotatePageContentRequestTest()
      : content::RenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    os_crypt_async_ = os_crypt_async::GetTestOSCryptAsyncForTesting();

    extraction_service_.emplace(os_crypt_async_.get(),
                                browser_context()->GetPath());

    RecreateRequest();
  }

  void TearDown() override {
    request_ = nullptr;
    extraction_service_.reset();
    os_crypt_async_.reset();
    content::RenderViewHostTestHarness::TearDown();
  }

  void SetTriggeringMode(const std::string& mode,
                         const std::string& capture_delay = "0s") {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kAnnotatedPageContentExtraction,
        {{"triggering_mode", mode}, {"capture_delay", capture_delay}});
  }

  void RecreateRequest() {
    request_ = nullptr;
    web_contents()->RemoveUserData(AnnotatedPageContentRequest::UserDataKey());
    AnnotatedPageContentRequest::CreateForWebContents(
        web_contents(), extraction_service_.value(),
        base::BindRepeating(
            [](AnnotatePageContentRequestTest* test, content::WebContents&,
               const FetchPageContextOptions& options,
               std::unique_ptr<FetchPageProgressListener>,
               FetchPageContextResultCallback callback) {
              test->previous_options_ =
                  options.annotated_page_content_options->Clone();
              auto page_content =
                  std::make_unique<optimization_guide::AIPageContentResult>();
              auto result = std::make_unique<FetchPageContextResult>();
              result->annotated_page_content_result =
                  PageContentResultWithEndTime(std::move(*page_content));
              std::move(callback).Run(std::move(result));
            },
            base::Unretained(this)),
        base::BindRepeating([](content::WebContents* web_contents) {
          return std::make_optional(reinterpret_cast<int64_t>(web_contents));
        }));
    request_ = AnnotatedPageContentRequest::FromWebContents(web_contents());
  }

  std::unique_ptr<content::MockNavigationHandle> CreateHandle(
      bool committed,
      bool is_same_document) {
    std::unique_ptr<content::MockNavigationHandle> handle =
        std::make_unique<content::MockNavigationHandle>(GURL(), main_rfh());
    handle->set_has_committed(committed);
    handle->set_is_same_document(is_same_document);
    return handle;
  }

  void SimulatePageLoad() {
    auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
        GURL("https://example.com/"), web_contents());
    navigation->Start();
    request_->PrimaryPageChanged(web_contents()->GetPrimaryPage());
    navigation->Commit();
    auto handle = CreateHandle(true, false);
    request_->DidFinishNavigation(handle.get());
    request_->DidStopLoading();
    request_->OnFirstContentfulPaintInPrimaryMainFrame();
  }

  void WaitForExtraction() {
    base::RunLoop run_loop;
    extraction_service_->SetQuitClosure(run_loop.QuitClosure());
    run_loop.Run();
  }

  TestPageContentExtractionService& extraction_service() {
    return extraction_service_.value();
  }

  const blink::mojom::AIPageContentOptionsPtr& last_options() const {
    return previous_options_;
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_async_;
  std::optional<TestPageContentExtractionService> extraction_service_;
  raw_ptr<AnnotatedPageContentRequest> request_ = nullptr;
  blink::mojom::AIPageContentOptionsPtr previous_options_;
};

TEST_F(AnnotatePageContentRequestTest, OnLoadTrigger) {
  SetTriggeringMode("on_load");

  SimulatePageLoad();
  WaitForExtraction();

  EXPECT_EQ(extraction_service().extraction_count(), 1);
  EXPECT_TRUE(extraction_service().last_extracted_content().has_value());

  // Hiding should not trigger another extraction.
  web_contents()->WasHidden();
  request_->OnVisibilityChanged(content::Visibility::HIDDEN);
  EXPECT_EQ(extraction_service().extraction_count(), 1);
}

TEST_F(AnnotatePageContentRequestTest, OnHiddenTrigger) {
  SetTriggeringMode("on_hidden");

  SimulatePageLoad();

  // Should not extract on load.
  EXPECT_EQ(extraction_service().extraction_count(), 0);

  // Hiding should trigger extraction.
  web_contents()->WasHidden();
  request_->OnVisibilityChanged(content::Visibility::HIDDEN);
  WaitForExtraction();
  EXPECT_EQ(extraction_service().extraction_count(), 1);
  EXPECT_TRUE(extraction_service().last_extracted_content().has_value());

  // Showing and hiding again should trigger another extraction.
  web_contents()->WasShown();
  request_->OnVisibilityChanged(content::Visibility::VISIBLE);
  // No extraction expected.
  EXPECT_EQ(extraction_service().extraction_count(), 1);

  web_contents()->WasHidden();
  request_->OnVisibilityChanged(content::Visibility::HIDDEN);
  WaitForExtraction();
  EXPECT_EQ(extraction_service().extraction_count(), 2);
}

TEST_F(AnnotatePageContentRequestTest,
       OnHiddenTrigger_NoDuplicateScheduledExtractions) {
  SetTriggeringMode("on_hidden", /*capture_delay=*/"5s");

  SimulatePageLoad();

  // Hide the tab. Extraction should be scheduled (but delayed 5s).
  web_contents()->WasHidden();
  request_->OnVisibilityChanged(content::Visibility::HIDDEN);

  // At this point, no extraction has happened because of the 5s delay.
  EXPECT_EQ(extraction_service().extraction_count(), 0);

  // Hide the tab again before the 5s delay finishes.
  // This should be a no-op because it's already kScheduled.
  web_contents()->WasHidden();
  request_->OnVisibilityChanged(content::Visibility::HIDDEN);

  // Now speed up time to trigger the extraction scheduled tasks.
  task_environment()->FastForwardBy(base::Seconds(5));

  // Should only have extracted once despite multiple hide events.
  EXPECT_EQ(extraction_service().extraction_count(), 1);
}

TEST_F(AnnotatePageContentRequestTest, OnLoadAndHiddenTrigger) {
  SetTriggeringMode("on_load_and_hidden");

  SimulatePageLoad();
  WaitForExtraction();

  // Should extract on load.
  EXPECT_EQ(extraction_service().extraction_count(), 1);
  EXPECT_TRUE(extraction_service().last_extracted_content().has_value());

  // Hiding should trigger another extraction.
  web_contents()->WasHidden();
  request_->OnVisibilityChanged(content::Visibility::HIDDEN);
  WaitForExtraction();
  EXPECT_EQ(extraction_service().extraction_count(), 2);

  // Showing and hiding again should trigger another extraction.
  web_contents()->WasShown();
  request_->OnVisibilityChanged(content::Visibility::VISIBLE);
  // No extraction expected.
  EXPECT_EQ(extraction_service().extraction_count(), 2);

  web_contents()->WasHidden();
  request_->OnVisibilityChanged(content::Visibility::HIDDEN);
  WaitForExtraction();
  EXPECT_EQ(extraction_service().extraction_count(), 3);
}

TEST_F(AnnotatePageContentRequestTest, OnLoadAndHiddenTrigger_LoadWhileHidden) {
  SetTriggeringMode("on_load_and_hidden");

  // Start with the tab hidden.
  web_contents()->WasHidden();
  request_->OnVisibilityChanged(content::Visibility::HIDDEN);

  SimulatePageLoad();
  WaitForExtraction();

  // Should extract on load, even if hidden.
  EXPECT_EQ(extraction_service().extraction_count(), 1);
  EXPECT_TRUE(extraction_service().last_extracted_content().has_value());

  // Showing should not trigger extraction.
  web_contents()->WasShown();
  request_->OnVisibilityChanged(content::Visibility::VISIBLE);
  EXPECT_EQ(extraction_service().extraction_count(), 1);

  // Hiding again should trigger another extraction.
  web_contents()->WasHidden();
  request_->OnVisibilityChanged(content::Visibility::HIDDEN);
  WaitForExtraction();
  EXPECT_EQ(extraction_service().extraction_count(), 2);
}

TEST_F(AnnotatePageContentRequestTest, ResetOnNewNavigation) {
  SetTriggeringMode("on_load");

  SimulatePageLoad();
  WaitForExtraction();

  EXPECT_EQ(extraction_service().extraction_count(), 1);

  // New navigation.
  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://example.com/2"), web_contents());
  navigation->Start();
  request_->PrimaryPageChanged(web_contents()->GetPrimaryPage());
  navigation->Commit();
  auto handle = CreateHandle(true, false);
  request_->DidFinishNavigation(handle.get());
  request_->DidStopLoading();
  request_->OnFirstContentfulPaintInPrimaryMainFrame();
  WaitForExtraction();

  EXPECT_EQ(extraction_service().extraction_count(), 2);
}

TEST_F(AnnotatePageContentRequestTest, ExcludeAdRelatedFlag_FeatureDisabled) {
  SetTriggeringMode("on_load");

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAnnotatedPageContentNonSalientFiltering);
  RecreateRequest();

  SimulatePageLoad();
  WaitForExtraction();

  ASSERT_TRUE(last_options());
  EXPECT_FALSE(last_options()->non_salient_content_config);
}

TEST_F(AnnotatePageContentRequestTest,
       ExcludeAdRelatedFlag_FeatureParamDisabled) {
  SetTriggeringMode("on_load");

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kAnnotatedPageContentNonSalientFiltering,
      {{"exclude_ad_related", "false"}});
  RecreateRequest();

  SimulatePageLoad();
  WaitForExtraction();

  ASSERT_TRUE(last_options());
  EXPECT_FALSE(last_options()->non_salient_content_config);
}

TEST_F(AnnotatePageContentRequestTest, ExcludeAdRelatedFlag_Enabled) {
  SetTriggeringMode("on_load");

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kAnnotatedPageContentNonSalientFiltering,
      {{"exclude_ad_related", "true"}});
  RecreateRequest();

  SimulatePageLoad();
  WaitForExtraction();

  ASSERT_TRUE(last_options());
  ASSERT_TRUE(last_options()->non_salient_content_config);
  EXPECT_TRUE(last_options()->non_salient_content_config->exclude_ad_related);
}

TEST_F(AnnotatePageContentRequestTest, OnLoadTrigger_ExtractsEvenWhileHidden) {
  SetTriggeringMode("on_load");

  // Tab starts completely hidden.
  web_contents()->WasHidden();
  request_->OnVisibilityChanged(content::Visibility::HIDDEN);

  SimulatePageLoad();
  WaitForExtraction();

  // Extraction should succeed immediately on load despite being in the
  // background.
  EXPECT_EQ(extraction_service().extraction_count(), 1);
}

TEST_F(AnnotatePageContentRequestTest, OnHiddenTrigger_NoExtractionOnLoad) {
  SetTriggeringMode("on_hidden");

  // Load the page while entirely visible.
  SimulatePageLoad();

  // Ensure no extraction fired upon load completion, because we aren't using an
  // OnLoad mode.
  EXPECT_EQ(extraction_service().extraction_count(), 0);
}

TEST_F(AnnotatePageContentRequestTest,
       ShouldScheduleExtraction_WaitingForLifecycle) {
  SetTriggeringMode("on_hidden");

  // Not finished loading, but triggered a hide event!
  web_contents()->WasHidden();
  request_->OnVisibilityChanged(content::Visibility::HIDDEN);

  // Should definitively not have scheduled or fired an extraction.
  // Wait using FastForward in case there's an erroneous 0s delay task.
  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_EQ(extraction_service().extraction_count(), 0);

  // After this, trigger load and then see that it is triggered.
  SimulatePageLoad();
  WaitForExtraction();
  EXPECT_EQ(extraction_service().extraction_count(), 1);
}

TEST_F(AnnotatePageContentRequestTest,
       OnHiddenTrigger_AsyncVisibilityRaceCondition) {
  SetTriggeringMode("on_hidden");

  // We need to simulate an asynchronous callback that we control manually.
  auto async_callback = base::MakeRefCounted<
      base::RefCountedData<FetchPageContextResultCallback>>();

  auto manual_callback = base::BindRepeating(
      [](scoped_refptr<base::RefCountedData<FetchPageContextResultCallback>>
             saved_callback,
         content::WebContents&, const FetchPageContextOptions&,
         std::unique_ptr<FetchPageProgressListener>,
         FetchPageContextResultCallback callback) {
        saved_callback->data = std::move(callback);
      },
      async_callback);

  // Overwrite the synchronous request with our manual mock.
  request_ = nullptr;
  web_contents()->RemoveUserData(AnnotatedPageContentRequest::UserDataKey());
  AnnotatedPageContentRequest::CreateForWebContents(
      web_contents(), extraction_service(), manual_callback,
      base::BindRepeating([](content::WebContents* web_contents) {
        return std::make_optional(reinterpret_cast<int64_t>(web_contents));
      }));
  request_ = AnnotatedPageContentRequest::FromWebContents(web_contents());

  SimulatePageLoad();

  // 1. Hide the tab -> Extraction is scheduled and begins.
  web_contents()->WasHidden();
  request_->OnVisibilityChanged(content::Visibility::HIDDEN);

  // Fast forward past any immediate capture delays to post the request
  // callback.
  task_environment()->FastForwardBy(base::Seconds(1));

  // The callback should now be captured (async call is "in flight").
  ASSERT_TRUE(async_callback->data);
  EXPECT_EQ(extraction_service().extraction_count(), 0);

  // 2. Tab becomes visible before the async call returns.
  // Under the old bug, this repeatedly toggled the state.
  web_contents()->WasShown();
  request_->OnVisibilityChanged(content::Visibility::VISIBLE);

  // 3. The async call finally returns.
  auto page_content =
      std::make_unique<optimization_guide::AIPageContentResult>();
  auto result = std::make_unique<FetchPageContextResult>();
  result->annotated_page_content_result =
      PageContentResultWithEndTime(std::move(*page_content));
  std::move(async_callback->data).Run(std::move(result));

  // Explicitly call FastForward to ensure any post-completion tasks run.
  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_EQ(extraction_service().extraction_count(), 1);

  // clear the callback pointer.
  async_callback->data = FetchPageContextResultCallback();

  // 4. Hide the tab again.
  // With the bug fixed, it should correctly identify it as a new hide
  // transition and extract again.
  web_contents()->WasHidden();
  request_->OnVisibilityChanged(content::Visibility::HIDDEN);

  task_environment()->FastForwardBy(base::Seconds(1));

  ASSERT_TRUE(async_callback->data);

  auto page_content2 =
      std::make_unique<optimization_guide::AIPageContentResult>();
  auto result2 = std::make_unique<FetchPageContextResult>();
  result2->annotated_page_content_result =
      PageContentResultWithEndTime(std::move(*page_content2));
  std::move(async_callback->data).Run(std::move(result2));

  task_environment()->FastForwardBy(base::Seconds(1));

  EXPECT_EQ(extraction_service().extraction_count(), 2);
}

TEST_F(AnnotatePageContentRequestTest, RefreshAPC) {
  SetTriggeringMode("on_load");

  SimulatePageLoad();
  WaitForExtraction();

  EXPECT_EQ(extraction_service().extraction_count(), 1);

  base::test::TestFuture<std::optional<ExtractedPageContentResult>>
      refresh_future;
  request_->RefreshExtractedPageContentAndEligibilityForPage(
      refresh_future.GetCallback());

  std::optional<ExtractedPageContentResult> result = refresh_future.Get();
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(extraction_service().extraction_count(), 2);
}

TEST_F(AnnotatePageContentRequestTest, RefreshAPC_Batching) {
  SetTriggeringMode("on_load");

  SimulatePageLoad();
  WaitForExtraction();

  EXPECT_EQ(extraction_service().extraction_count(), 1);

  // Create another request with an async fetcher to test batching.
  FetchPageContextResultCallback saved_callback;
  auto async_request = AnnotatedPageContentRequest::CreateForTesting(
      web_contents(), extraction_service(),
      base::BindRepeating(
          [](FetchPageContextResultCallback* saved, content::WebContents&,
             const FetchPageContextOptions&,
             std::unique_ptr<FetchPageProgressListener>,
             FetchPageContextResultCallback callback) {
            *saved = std::move(callback);
          },
          &saved_callback),
      base::BindRepeating([](content::WebContents* web_contents) {
        return std::make_optional(reinterpret_cast<int64_t>(web_contents));
      }));

  base::test::TestFuture<std::optional<ExtractedPageContentResult>> future1;
  base::test::TestFuture<std::optional<ExtractedPageContentResult>> future2;

  async_request->RefreshExtractedPageContentAndEligibilityForPage(
      future1.GetCallback());
  async_request->RefreshExtractedPageContentAndEligibilityForPage(
      future2.GetCallback());

  // The request is now running, but has not completed.
  EXPECT_TRUE(saved_callback);
  EXPECT_EQ(extraction_service().extraction_count(), 1);

  // Complete the async call.
  auto page_content =
      std::make_unique<optimization_guide::AIPageContentResult>();
  auto result = std::make_unique<FetchPageContextResult>();
  result->annotated_page_content_result =
      PageContentResultWithEndTime(std::move(*page_content));
  std::move(saved_callback).Run(std::move(result));

  EXPECT_TRUE(future1.Get().has_value());
  EXPECT_TRUE(future2.Get().has_value());
  EXPECT_EQ(extraction_service().extraction_count(), 2);
}

TEST_F(AnnotatePageContentRequestTest, RefreshAPC_PdfShortCircuit) {
  SetTriggeringMode("on_load");

  // Simulate a PDF navigation.
  std::unique_ptr<content::NavigationSimulator> simulator =
      content::NavigationSimulator::CreateBrowserInitiated(
          GURL("https://example.com/file.pdf"), web_contents());
  simulator->SetContentsMimeType("application/pdf");
  simulator->Commit();

  base::test::TestFuture<std::optional<ExtractedPageContentResult>>
      refresh_future;
  request_->RefreshExtractedPageContentAndEligibilityForPage(
      refresh_future.GetCallback());

  // PDF extraction is not supported.
  EXPECT_FALSE(refresh_future.Get().has_value());
}

TEST_F(AnnotatePageContentRequestTest,
       RefreshAPC_WhileInitialExtractionPending) {
  SetTriggeringMode("on_load");

  SimulatePageLoad();

  base::test::TestFuture<std::optional<ExtractedPageContentResult>>
      refresh_future;
  request_->RefreshExtractedPageContentAndEligibilityForPage(
      refresh_future.GetCallback());

  EXPECT_EQ(extraction_service().extraction_count(), 0);

  WaitForExtraction();

  EXPECT_TRUE(refresh_future.Get().has_value());
  EXPECT_EQ(extraction_service().extraction_count(), 1);
}

TEST_F(AnnotatePageContentRequestTest, RefreshAPC_ExtractionFailure) {
  // Ensures extraction is enabled for this class.
  SetTriggeringMode("on_load");
  // Create a request with a failing fetcher.
  auto failing_request = AnnotatedPageContentRequest::CreateForTesting(
      web_contents(), extraction_service(),
      base::BindRepeating([](content::WebContents&,
                             const FetchPageContextOptions&,
                             std::unique_ptr<FetchPageProgressListener>,
                             FetchPageContextResultCallback callback) {
        FetchPageContextErrorDetails error;
        error.error_code = FetchPageContextError::kUnknown;
        std::move(callback).Run(base::unexpected(error));
      }),
      base::BindRepeating([](content::WebContents* web_contents) {
        return std::make_optional(reinterpret_cast<int64_t>(web_contents));
      }));

  base::test::TestFuture<std::optional<ExtractedPageContentResult>>
      refresh_future;
  failing_request->RefreshExtractedPageContentAndEligibilityForPage(
      refresh_future.GetCallback());

  EXPECT_FALSE(refresh_future.Get().has_value());
}

TEST_F(AnnotatePageContentRequestTest, RefreshAPC_NavigationWhileRunning) {
  // Ensures extraction is enabled for this class.
  SetTriggeringMode("on_load");
  // Create a request with an async fetcher (saves the callback).
  FetchPageContextResultCallback saved_callback;
  auto async_request = AnnotatedPageContentRequest::CreateForTesting(
      web_contents(), extraction_service(),
      base::BindRepeating(
          [](FetchPageContextResultCallback* saved, content::WebContents&,
             const FetchPageContextOptions&,
             std::unique_ptr<FetchPageProgressListener>,
             FetchPageContextResultCallback callback) {
            *saved = std::move(callback);
          },
          &saved_callback),
      base::BindRepeating([](content::WebContents* web_contents) {
        return std::make_optional(reinterpret_cast<int64_t>(web_contents));
      }));

  base::test::TestFuture<std::optional<ExtractedPageContentResult>>
      refresh_future;
  async_request->RefreshExtractedPageContentAndEligibilityForPage(
      refresh_future.GetCallback());

  // Extraction should be scheduled now.
  EXPECT_TRUE(saved_callback);
  EXPECT_EQ(extraction_service().extraction_count(), 0);

  // Simulate another navigation.
  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://example_new.com/"), web_contents());
  navigation->Start();
  async_request->PrimaryPageChanged(web_contents()->GetPrimaryPage());
  navigation->Commit();

  // The callback should error out due to the navigation.
  EXPECT_FALSE(refresh_future.Get().has_value());
}

TEST_F(AnnotatePageContentRequestTest, GetAsync_AlreadyExtracted) {
  SetTriggeringMode("on_load");

  SimulatePageLoad();
  WaitForExtraction();

  EXPECT_EQ(extraction_service().extraction_count(), 1);

  base::test::TestFuture<std::optional<ExtractedPageContentResult>>
      content_future;
  request_->GetCachedContentAndEligibilityAsync(content_future.GetCallback());

  base::test::TestFuture<std::optional<bool>> eligibility_future;
  request_->GetServerUploadEligibilityAsync(eligibility_future.GetCallback());

  EXPECT_TRUE(content_future.Get().has_value());
  EXPECT_TRUE(eligibility_future.Get().has_value());

  EXPECT_EQ(extraction_service().extraction_count(), 1);
}

TEST_F(AnnotatePageContentRequestTest, GetAsync_BeforeExtraction) {
  SetTriggeringMode("on_load");

  SimulatePageLoad();

  base::test::TestFuture<std::optional<ExtractedPageContentResult>>
      content_future;
  request_->GetCachedContentAndEligibilityAsync(content_future.GetCallback());

  base::test::TestFuture<std::optional<bool>> eligibility_future;
  request_->GetServerUploadEligibilityAsync(eligibility_future.GetCallback());

  EXPECT_FALSE(content_future.IsReady());
  EXPECT_FALSE(eligibility_future.IsReady());

  // Extraction count should be 0 before extraction.
  EXPECT_EQ(extraction_service().extraction_count(), 0);

  WaitForExtraction();

  EXPECT_TRUE(content_future.Get().has_value());
  EXPECT_TRUE(eligibility_future.Get().has_value());
  EXPECT_EQ(extraction_service().extraction_count(), 1);
}

TEST_F(AnnotatePageContentRequestTest, GetAsync_InvalidateOnNavigation) {
  SetTriggeringMode("on_load");

  SimulatePageLoad();

  base::test::TestFuture<std::optional<ExtractedPageContentResult>>
      content_future;
  request_->GetCachedContentAndEligibilityAsync(content_future.GetCallback());

  base::test::TestFuture<std::optional<bool>> eligibility_future;
  request_->GetServerUploadEligibilityAsync(eligibility_future.GetCallback());

  EXPECT_FALSE(content_future.IsReady());
  EXPECT_FALSE(eligibility_future.IsReady());

  // Navigate to a new URL before extraction finishes.
  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://example.com/2"), web_contents());
  navigation->Start();
  request_->PrimaryPageChanged(web_contents()->GetPrimaryPage());
  navigation->Commit();

  ASSERT_TRUE(content_future.Wait());
  EXPECT_FALSE(content_future.Get().has_value());

  ASSERT_TRUE(eligibility_future.Wait());
  EXPECT_FALSE(eligibility_future.Get().has_value());
}

TEST_F(AnnotatePageContentRequestTest,
       GetAsync_ReturnNulloptWhenVisibleInOnHiddenMode) {
  SetTriggeringMode("on_hidden");

  SimulatePageLoad();

  base::test::TestFuture<std::optional<ExtractedPageContentResult>>
      content_future;
  request_->GetCachedContentAndEligibilityAsync(content_future.GetCallback());

  base::test::TestFuture<std::optional<bool>> eligibility_future;
  request_->GetServerUploadEligibilityAsync(eligibility_future.GetCallback());

  // Return nullopt as no extraction is scheduled (so the wait is indefinite)
  EXPECT_TRUE(content_future.IsReady());
  EXPECT_FALSE(content_future.Get().has_value());

  EXPECT_TRUE(eligibility_future.IsReady());
  EXPECT_FALSE(eligibility_future.Get().has_value());
}

TEST_F(AnnotatePageContentRequestTest, GetAsync_OnPdfPages) {
  SetTriggeringMode("on_load");

  // Simulate a PDF navigation.
  std::unique_ptr<content::NavigationSimulator> simulator =
      content::NavigationSimulator::CreateBrowserInitiated(
          GURL("https://example.com/file.pdf"), web_contents());
  simulator->SetContentsMimeType("application/pdf");
  simulator->Commit();

  base::test::TestFuture<std::optional<ExtractedPageContentResult>>
      content_future;
  request_->GetCachedContentAndEligibilityAsync(content_future.GetCallback());

  base::test::TestFuture<std::optional<bool>> eligibility_future;
  request_->GetServerUploadEligibilityAsync(eligibility_future.GetCallback());

  EXPECT_TRUE(content_future.IsReady());
  EXPECT_FALSE(content_future.Get().has_value());

  EXPECT_TRUE(eligibility_future.IsReady());
  EXPECT_FALSE(eligibility_future.Get().has_value());
}

}  // namespace page_content_annotations
