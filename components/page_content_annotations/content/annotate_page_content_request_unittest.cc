// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/content/annotate_page_content_request.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/page_content_annotations/content/page_content_extraction_service.h"
#include "components/page_content_annotations/core/page_content_annotations_features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/page.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "pdf/buildflags.h"
#include "testing/gtest/include/gtest/gtest-param-test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

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

  void OnPageContentExtracted(content::Page& page,
                              PageContent page_content,
                              const std::vector<uint8_t>& screenshot_data,
                              std::optional<int> tab_id) override {
    extraction_count_++;
    if (RefCountedPDFTextPtr pdf_text_ptr =
            GetPDFTextPtrFromPageContent(page_content)) {
      last_extracted_pdf_text_ = pdf_text_ptr->data;
    } else {
      last_extracted_content_ = ExtractedPageContentResult(
          GetAnnotatedPageContentPtrFromPageContent(std::move(page_content)),
          base::Time::Now(), false, screenshot_data);
    }

    if (quit_closure_) {
      std::move(quit_closure_).Run();
    }
  }

  int extraction_count() const { return extraction_count_; }
  const std::optional<ExtractedPageContentResult>& last_extracted_content()
      const {
    return last_extracted_content_;
  }
  const std::optional<std::string>& last_extracted_pdf_text() const {
    return last_extracted_pdf_text_;
  }

  void SetQuitClosure(base::OnceClosure quit_closure) {
    quit_closure_ = std::move(quit_closure);
  }

 private:
  int extraction_count_ = 0;
  std::optional<ExtractedPageContentResult> last_extracted_content_;
  std::optional<std::string> last_extracted_pdf_text_;
  feature_engagement::test::MockTracker mock_tracker_;
  base::OnceClosure quit_closure_;
};

class AnnotatePageContentRequestTest
    : public content::RenderViewHostTestHarness,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  static std::string DescribeParams(
      const testing::TestParamInfo<std::tuple<bool, bool>>& info) {
    auto [is_pdf_text_extraction_enabled, is_page_settled_monitor_enabled] =
        info.param;
    return base::StrCat({
        is_pdf_text_extraction_enabled ? "PDFTextExtractionEnabled"
                                       : "PDFTextExtractionDisabled",
        "_",
        is_page_settled_monitor_enabled ? "PageSettledMonitorEnabled"
                                        : "PageSettledMonitorDisabled",
    });
  }

  AnnotatePageContentRequestTest()
      : content::RenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    std::vector<base::test::FeatureRefAndParams> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    if (IsPDFTextExtractionEnabled()) {
      enabled_features.emplace_back(
          features::kAnnotatedPageContentPDFTextExtraction,
          base::FieldTrialParams{{"max_text_byte_size", "100"}});
    } else {
      disabled_features.emplace_back(
          features::kAnnotatedPageContentPDFTextExtraction);
    }

    if (IsPageSettledMonitorEnabled()) {
      enabled_features.emplace_back(
          features::kPageContentExtractionUsingPageSettledMonitor,
          base::FieldTrialParams{});
    } else {
      disabled_features.emplace_back(
          features::kPageContentExtractionUsingPageSettledMonitor);
    }

    feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                disabled_features);
  }

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

  bool IsPDFTextExtractionEnabled() const { return std::get<0>(GetParam()); }

  // Tests exercise `AnnotatedPageContentRequest::StartExtraction` are only
  // supported on platforms that enable `ENABLE_PDF` build flag.
  bool PlatformSupportsPDF() const {
#if BUILDFLAG(ENABLE_PDF)
    return true;
#else
    return false;
#endif  // BUILDFLAG(ENABLE_PDF)
  }

  bool IsPageSettledMonitorEnabled() const { return std::get<1>(GetParam()); }

  void SetTriggeringMode(const std::string& mode,
                         const std::string& capture_delay = "0s") {
    triggering_mode_feature_list_.InitAndEnableFeatureWithParameters(
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
                  options.annotated_page_content_options
                      ? options.annotated_page_content_options->Clone()
                      : nullptr;

              auto result = std::make_unique<FetchPageContextResult>();
              if (options.annotated_page_content_options) {
                auto page_content =
                    std::make_unique<optimization_guide::AIPageContentResult>();
                result->annotated_page_content_result =
                    PageContentResultWithEndTime(std::move(*page_content));
                std::move(callback).Run(std::move(result));
              } else {
                CHECK(options.pdf_options)
                    << "Either `annotated_page_content_options` or "
                       "`pdf_options` should be non-null.";
                CHECK_GT(options.pdf_options->size_limit(), 0u)
                    << "`pdf_options` does not allow `size_limit` being 0.";
                PdfResult pdf_result{
                    url::Origin::Create(GURL("https://example.com")),
                    "Sample PDF text"};
                result->pdf_result = std::move(pdf_result);
                std::move(callback).Run(std::move(result));
              }
            },
            base::Unretained(this)),
        base::BindRepeating([](content::WebContents* web_contents) {
          return std::make_optional(reinterpret_cast<int64_t>(web_contents));
        }));
    request_ = AnnotatedPageContentRequest::FromWebContents(web_contents());
  }

  std::unique_ptr<content::MockNavigationHandle> CreateHandle(
      bool committed,
      bool is_same_document,
      bool should_update_history = true) {
    std::unique_ptr<content::MockNavigationHandle> handle =
        std::make_unique<content::MockNavigationHandle>(GURL(), main_rfh());
    handle->set_has_committed(committed);
    handle->set_is_same_document(is_same_document);
    ON_CALL(*handle, ShouldUpdateHistory())
        .WillByDefault(testing::Return(should_update_history));
    return handle;
  }

  void SimulateNavigation(const GURL& url) {
    auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
        url, web_contents());
    navigation->Start();
    navigation->Commit();
  }

  void WaitForPageSettled() {
    if (!IsPageSettledMonitorEnabled()) {
      return;
    }

    base::RunLoop run_loop;
    request_->SetPageSettledCallbackForTesting(run_loop.QuitClosure());
    run_loop.Run();
  }

  void SimulatePageStablization() {
    request_->OnFirstContentfulPaintInPrimaryMainFrame();
    WaitForPageSettled();
  }

  void SimulatePageLoad(const GURL& url = GURL("https://example.com/")) {
    SimulateNavigation(url);
    SimulatePageStablization();
  }

  void SimulatePDFLoad(const GURL& url) {
    auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
        url, web_contents());
    navigation->Start();
    navigation->SetContentsMimeType("application/pdf");
    navigation->Commit();
    WaitForPageSettled();
  }

  // Wait until either APC or PDF text is extracted.
  void WaitForExtraction() {
    base::RunLoop run_loop;
    extraction_service_->SetQuitClosure(run_loop.QuitClosure());
    run_loop.Run();
  }

  void TriggerPrimaryPageChanged(content::Page& page) {
    request_->PrimaryPageChanged(page);
  }

  void TriggerDidStopLoading() { request_->DidStopLoading(); }

  TestPageContentExtractionService& extraction_service() {
    return extraction_service_.value();
  }

  const blink::mojom::AIPageContentOptionsPtr& last_options() const {
    return previous_options_;
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  base::test::ScopedFeatureList triggering_mode_feature_list_;
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_async_;
  std::optional<TestPageContentExtractionService> extraction_service_;
  raw_ptr<AnnotatedPageContentRequest> request_ = nullptr;
  blink::mojom::AIPageContentOptionsPtr previous_options_;
};

TEST_P(AnnotatePageContentRequestTest, OnLoadTrigger) {
  SetTriggeringMode("on_load");

  SimulatePageLoad();
  WaitForExtraction();

  EXPECT_EQ(extraction_service().extraction_count(), 1);
  EXPECT_TRUE(extraction_service().last_extracted_content().has_value());

  // Hiding should not trigger another extraction.
  web_contents()->WasHidden();
  EXPECT_EQ(extraction_service().extraction_count(), 1);
}

TEST_P(AnnotatePageContentRequestTest, OnHiddenTrigger) {
  SetTriggeringMode("on_hidden");

  SimulatePageLoad();

  // Should not extract on load.
  EXPECT_EQ(extraction_service().extraction_count(), 0);

  // Hiding should trigger extraction.
  web_contents()->WasHidden();
  WaitForExtraction();
  EXPECT_EQ(extraction_service().extraction_count(), 1);
  EXPECT_TRUE(extraction_service().last_extracted_content().has_value());

  // Showing and hiding again should trigger another extraction.
  web_contents()->WasShown();
  // No extraction expected.
  EXPECT_EQ(extraction_service().extraction_count(), 1);

  web_contents()->WasHidden();
  WaitForExtraction();
  EXPECT_EQ(extraction_service().extraction_count(), 2);
}

TEST_P(AnnotatePageContentRequestTest,
       OnHiddenTrigger_NoDuplicateScheduledExtractions) {
  SetTriggeringMode("on_hidden");

  SimulatePageLoad();

  // Hide the tab. Extraction should be scheduled.
  web_contents()->WasHidden();

  // At this point, no extraction has happened because the posted task is not
  // run yet.
  EXPECT_EQ(extraction_service().extraction_count(), 0);

  web_contents()->WasShown();

  // Hide the tab again. This should be a no-op because it's already kScheduled.
  web_contents()->WasHidden();

  // Now run the posted task to start extraction.
  task_environment()->FastForwardBy(base::TimeDelta());

  // Should only have extracted once despite multiple hide events.
  EXPECT_EQ(extraction_service().extraction_count(), 1);
}

TEST_P(AnnotatePageContentRequestTest, OnLoadAndHiddenTrigger) {
  SetTriggeringMode("on_load_and_hidden");

  SimulatePageLoad();
  WaitForExtraction();

  // Should extract on load.
  EXPECT_EQ(extraction_service().extraction_count(), 1);
  EXPECT_TRUE(extraction_service().last_extracted_content().has_value());

  // Hiding should trigger another extraction.
  web_contents()->WasHidden();
  WaitForExtraction();
  EXPECT_EQ(extraction_service().extraction_count(), 2);

  // Showing and hiding again should trigger another extraction.
  web_contents()->WasShown();
  // No extraction expected.
  EXPECT_EQ(extraction_service().extraction_count(), 2);

  web_contents()->WasHidden();
  WaitForExtraction();
  EXPECT_EQ(extraction_service().extraction_count(), 3);
}

TEST_P(AnnotatePageContentRequestTest, OnLoadAndHiddenTrigger_LoadWhileHidden) {
  SetTriggeringMode("on_load_and_hidden");

  // Start with the tab hidden.
  web_contents()->WasHidden();

  SimulatePageLoad();
  WaitForExtraction();

  // Should extract on load, even if hidden.
  EXPECT_EQ(extraction_service().extraction_count(), 1);
  EXPECT_TRUE(extraction_service().last_extracted_content().has_value());

  // Showing should not trigger extraction.
  web_contents()->WasShown();
  EXPECT_EQ(extraction_service().extraction_count(), 1);

  // Hiding again should trigger another extraction.
  web_contents()->WasHidden();
  WaitForExtraction();
  EXPECT_EQ(extraction_service().extraction_count(), 2);
}

TEST_P(AnnotatePageContentRequestTest, ResetOnNewNavigation) {
  SetTriggeringMode("on_load");

  SimulatePageLoad();
  WaitForExtraction();

  EXPECT_EQ(extraction_service().extraction_count(), 1);

  // New navigation.
  SimulatePageLoad(GURL("https://example.com/2"));
  WaitForExtraction();

  EXPECT_EQ(extraction_service().extraction_count(), 2);
}

TEST_P(AnnotatePageContentRequestTest, SameDocumentNavigation) {
  SetTriggeringMode("on_load");

  SimulatePageLoad();
  WaitForExtraction();

  EXPECT_EQ(extraction_service().extraction_count(), 1);

  // Simulate a same-document navigation by navigating to a fragment.
  SimulateNavigation(GURL("https://example.com/#fragment"));

  // Same-document navigations don't wait for load or FCP.
  WaitForExtraction();

  EXPECT_EQ(extraction_service().extraction_count(), 2);
}

TEST_P(AnnotatePageContentRequestTest, ExcludeAdRelatedFlag_FeatureDisabled) {
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

TEST_P(AnnotatePageContentRequestTest,
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

TEST_P(AnnotatePageContentRequestTest, ExcludeAdRelatedFlag_Enabled) {
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

TEST_P(AnnotatePageContentRequestTest, OnLoadTrigger_ExtractsEvenWhileHidden) {
  SetTriggeringMode("on_load");

  // Tab starts completely hidden.
  web_contents()->WasHidden();

  SimulatePageLoad();
  WaitForExtraction();

  // Extraction should succeed immediately on load despite being in the
  // background.
  EXPECT_EQ(extraction_service().extraction_count(), 1);
}

TEST_P(AnnotatePageContentRequestTest, OnHiddenTrigger_NoExtractionOnLoad) {
  SetTriggeringMode("on_hidden");

  // Load the page while entirely visible.
  SimulatePageLoad();

  // Ensure no extraction fired upon load completion, because we aren't using an
  // OnLoad mode.
  EXPECT_EQ(extraction_service().extraction_count(), 0);
}

TEST_P(AnnotatePageContentRequestTest,
       ShouldScheduleExtraction_WaitingForLifecycle) {
  SetTriggeringMode("on_hidden");

  // Not finished loading, but triggered a hide event!
  web_contents()->WasHidden();

  // Should definitively not have scheduled or fired an extraction.
  // Wait using FastForward in case there's an erroneous 0s delay task.
  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_EQ(extraction_service().extraction_count(), 0);

  // After this, trigger load and then see that it is triggered.
  SimulatePageLoad();
  WaitForExtraction();
  EXPECT_EQ(extraction_service().extraction_count(), 1);
}

TEST_P(AnnotatePageContentRequestTest,
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

  // Fast forward past any immediate capture delays to post the request
  // callback.
  task_environment()->FastForwardBy(base::Seconds(1));

  // The callback should now be captured (async call is "in flight").
  ASSERT_TRUE(async_callback->data);
  EXPECT_EQ(extraction_service().extraction_count(), 0);

  // 2. Tab becomes visible before the async call returns.
  // Under the old bug, this repeatedly toggled the state.
  web_contents()->WasShown();

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

TEST_P(AnnotatePageContentRequestTest, RefreshAPC) {
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

TEST_P(AnnotatePageContentRequestTest, RefreshAPC_Batching) {
  SetTriggeringMode("on_load");

  // Override the request with an async fetcher to test batching.
  FetchPageContextResultCallback saved_callback;
  base::RunLoop run_loop;
  request_ = nullptr;
  web_contents()->RemoveUserData(AnnotatedPageContentRequest::UserDataKey());
  AnnotatedPageContentRequest::CreateForWebContents(
      web_contents(), extraction_service(),
      base::BindRepeating(
          [](FetchPageContextResultCallback* saved,
             base::RepeatingClosure quit_closure, content::WebContents&,
             const FetchPageContextOptions&,
             std::unique_ptr<FetchPageProgressListener>,
             FetchPageContextResultCallback callback) {
            *saved = std::move(callback);
            quit_closure.Run();
          },
          &saved_callback, run_loop.QuitClosure()),
      base::BindRepeating([](content::WebContents* web_contents) {
        return std::make_optional(reinterpret_cast<int64_t>(web_contents));
      }));
  request_ = AnnotatedPageContentRequest::FromWebContents(web_contents());

  SimulatePageLoad();
  run_loop.Run();

  base::test::TestFuture<std::optional<ExtractedPageContentResult>> future1;
  base::test::TestFuture<std::optional<ExtractedPageContentResult>> future2;

  request_->RefreshExtractedPageContentAndEligibilityForPage(
      future1.GetCallback());
  request_->RefreshExtractedPageContentAndEligibilityForPage(
      future2.GetCallback());

  // The request is now running, but has not completed.
  EXPECT_TRUE(saved_callback);
  EXPECT_EQ(extraction_service().extraction_count(), 0);

  // Complete the async call.
  auto page_content =
      std::make_unique<optimization_guide::AIPageContentResult>();
  auto result = std::make_unique<FetchPageContextResult>();
  result->annotated_page_content_result =
      PageContentResultWithEndTime(std::move(*page_content));
  std::move(saved_callback).Run(std::move(result));

  EXPECT_TRUE(future1.Get().has_value());
  EXPECT_TRUE(future2.Get().has_value());
  EXPECT_EQ(extraction_service().extraction_count(), 1);
}

// Page content getter methods do not support PDF pages.
// TODO(b/487632737): Support on-demand PDF text extraction.
TEST_P(AnnotatePageContentRequestTest, RefreshAPCPdfShortCircuit) {
  SetTriggeringMode("on_load");
  SimulatePDFLoad(GURL("https://example.com/file.pdf"));

  base::test::TestFuture<std::optional<ExtractedPageContentResult>>
      refresh_future;
  request_->RefreshExtractedPageContentAndEligibilityForPage(
      refresh_future.GetCallback());

  EXPECT_FALSE(refresh_future.Get().has_value());
}

TEST_P(AnnotatePageContentRequestTest,
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

TEST_P(AnnotatePageContentRequestTest, RefreshAPC_ExtractionFailure) {
  // Ensures extraction is enabled for this class.
  SetTriggeringMode("on_load");

  // Override the request with a failing fetcher.
  request_ = nullptr;
  web_contents()->RemoveUserData(AnnotatedPageContentRequest::UserDataKey());
  AnnotatedPageContentRequest::CreateForWebContents(
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
  request_ = AnnotatedPageContentRequest::FromWebContents(web_contents());

  SimulatePageLoad();

  base::test::TestFuture<std::optional<ExtractedPageContentResult>>
      refresh_future;
  request_->RefreshExtractedPageContentAndEligibilityForPage(
      refresh_future.GetCallback());
  EXPECT_FALSE(refresh_future.Get().has_value());
}

TEST_P(AnnotatePageContentRequestTest, RefreshAPC_NavigationWhileRunning) {
  // Ensures extraction is enabled for this class.
  SetTriggeringMode("on_load");

  // Override the request with an async fetcher.
  FetchPageContextResultCallback saved_callback;
  base::RunLoop run_loop;
  request_ = nullptr;
  web_contents()->RemoveUserData(AnnotatedPageContentRequest::UserDataKey());
  AnnotatedPageContentRequest::CreateForWebContents(
      web_contents(), extraction_service(),
      base::BindRepeating(
          [](FetchPageContextResultCallback* saved,
             base::RepeatingClosure quit_closure, content::WebContents&,
             const FetchPageContextOptions&,
             std::unique_ptr<FetchPageProgressListener>,
             FetchPageContextResultCallback callback) {
            *saved = std::move(callback);
            quit_closure.Run();
          },
          &saved_callback, run_loop.QuitClosure()),
      base::BindRepeating([](content::WebContents* web_contents) {
        return std::make_optional(reinterpret_cast<int64_t>(web_contents));
      }));
  request_ = AnnotatedPageContentRequest::FromWebContents(web_contents());

  SimulatePageLoad();
  run_loop.Run();

  base::test::TestFuture<std::optional<ExtractedPageContentResult>>
      refresh_future;
  request_->RefreshExtractedPageContentAndEligibilityForPage(
      refresh_future.GetCallback());

  // Extraction should be scheduled now.
  EXPECT_TRUE(saved_callback);
  EXPECT_EQ(extraction_service().extraction_count(), 0);

  // Simulate another navigation.
  SimulateNavigation(GURL("https://example_new.com/"));

  // The callback should error out due to the navigation.
  EXPECT_FALSE(refresh_future.Get().has_value());
}

TEST_P(AnnotatePageContentRequestTest, GetAsync_AlreadyExtracted) {
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

TEST_P(AnnotatePageContentRequestTest, GetAsync_BeforeExtraction) {
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

TEST_P(AnnotatePageContentRequestTest, GetAsync_InvalidateOnNavigation) {
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
  SimulateNavigation(GURL("https://example.com/2"));

  ASSERT_TRUE(content_future.Wait());
  EXPECT_FALSE(content_future.Get().has_value());

  ASSERT_TRUE(eligibility_future.Wait());
  EXPECT_FALSE(eligibility_future.Get().has_value());
}

TEST_P(AnnotatePageContentRequestTest,
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

// Async page content getter methods do not support PDF pages.
// TODO(b/487632737): Support on-demand PDF text extraction.
TEST_P(AnnotatePageContentRequestTest, GetAsyncOnPdfPages) {
  SetTriggeringMode("on_load");
  SimulatePDFLoad(GURL("https://example.com/file.pdf"));

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

TEST_P(AnnotatePageContentRequestTest, Metrics_OnLoadTrigger) {
  base::HistogramTester histogram_tester;
  SetTriggeringMode("on_load");

  SimulatePageLoad();
  WaitForExtraction();

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageContentExtraction.AutomaticOnLoad."
      "ExtractionLatency",
      IsPageSettledMonitorEnabled() ? 0 : 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageContentExtraction.AutomaticOnLoad."
      "StabilityLatency",
      IsPageSettledMonitorEnabled() ? 0 : 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageContentExtraction.AutomaticOnLoad.OverallLatency",
      IsPageSettledMonitorEnabled() ? 0 : 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentExtraction.TriggerSource",
      AnnotatedPageContentRequest::TriggerSource::kOnLoad, 1);
}

TEST_P(AnnotatePageContentRequestTest, Metrics_OnHiddenTrigger) {
  base::HistogramTester histogram_tester;
  SetTriggeringMode("on_hidden");

  SimulatePageLoad();

  // Hide the page to trigger extraction.
  web_contents()->WasHidden();
  WaitForExtraction();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentExtraction.TriggerSource",
      AnnotatedPageContentRequest::TriggerSource::kOnHidden, 1);

  // AutomaticOnLoad latency metrics should not be logged for OnHidden.
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageContentExtraction.AutomaticOnLoad."
      "ExtractionLatency",
      0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageContentExtraction.AutomaticOnLoad."
      "StabilityLatency",
      0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageContentExtraction.AutomaticOnLoad.OverallLatency",
      0);
}

TEST_P(AnnotatePageContentRequestTest, Metrics_OnDemandTrigger) {
  SetTriggeringMode("on_load");

  SimulatePageLoad();
  WaitForExtraction();

  base::HistogramTester histogram_tester;

  base::test::TestFuture<std::optional<ExtractedPageContentResult>> future;
  request_->RefreshExtractedPageContentAndEligibilityForPage(
      future.GetCallback());

  EXPECT_TRUE(future.Get().has_value());

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentExtraction.TriggerSource",
      AnnotatedPageContentRequest::TriggerSource::kOnDemand, 1);

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageContentExtraction.OnDemand.Latency", 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentExtraction.OnDemand.Success", true, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentExtraction.OnDemand.IsPDF", false, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentExtraction.OnDemand.StateAtRequest2", 4,
      1);  // 4 corresponds to Lifecycle::kExtracted
}

TEST_P(AnnotatePageContentRequestTest, Metrics_CacheHit) {
  SetTriggeringMode("on_load");

  SimulatePageLoad();

  base::HistogramTester histogram_tester;

  // Miss case: before extraction completes.
  std::optional<ExtractedPageContentResult> result =
      request_->GetCachedContentAndEligibility();
  EXPECT_FALSE(result.has_value());
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentExtraction.IsCacheHit", false, 1);

  WaitForExtraction();

  // Hit case: after extraction.
  result = request_->GetCachedContentAndEligibility();
  EXPECT_TRUE(result.has_value());
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.PageContentExtraction.IsCacheHit", true, 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageContentExtraction.IsCacheHit", 2);
}

TEST_P(AnnotatePageContentRequestTest, Metrics_CacheHit_Async) {
  SetTriggeringMode("on_load");

  SimulatePageLoad();

  base::HistogramTester histogram_tester;

  // Miss case: before extraction completes.
  base::test::TestFuture<std::optional<ExtractedPageContentResult>> future;
  request_->GetCachedContentAndEligibilityAsync(future.GetCallback());

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.PageContentExtraction.IsCacheHit", false, 1);

  WaitForExtraction();

  EXPECT_TRUE(future.Get().has_value());

  // Hit case: after extraction, requesting again should hit immediately.
  base::test::TestFuture<std::optional<ExtractedPageContentResult>> future2;
  request_->GetCachedContentAndEligibilityAsync(future2.GetCallback());

  EXPECT_TRUE(future2.Get().has_value());
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.PageContentExtraction.IsCacheHit", true, 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageContentExtraction.IsCacheHit", 2);
}

TEST_P(AnnotatePageContentRequestTest,
       Metrics_OnLoadAndHiddenTrigger_LoadWhileHidden) {
  base::HistogramTester histogram_tester;
  SetTriggeringMode("on_load_and_hidden");

  // Tab starts completely hidden.
  web_contents()->WasHidden();

  SimulatePageLoad();
  WaitForExtraction();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentExtraction.TriggerSource",
      AnnotatedPageContentRequest::TriggerSource::kOnHidden, 1);
}

TEST_P(AnnotatePageContentRequestTest,
       Metrics_OnLoadAndHiddenTrigger_HideBeforeFCP) {
  base::HistogramTester histogram_tester;
  SetTriggeringMode("on_load_and_hidden");

  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://example.com/"), web_contents());
  navigation->Start();
  navigation->Commit();

  // Hide the tab before page stabilized.
  TriggerDidStopLoading();
  web_contents()->WasHidden();
  SimulatePageStablization();

  WaitForExtraction();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentExtraction.TriggerSource",
      AnnotatedPageContentRequest::TriggerSource::kOnHidden, 1);
}

TEST_P(AnnotatePageContentRequestTest,
       Metrics_OnLoadTrigger_SameDocumentNavigation) {
  base::HistogramTester histogram_tester;
  SetTriggeringMode("on_load");

  SimulatePageLoad();
  WaitForExtraction();

  EXPECT_EQ(extraction_service().extraction_count(), 1);

  // Latency metrics should've been recorded.
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.PageContentExtraction.TriggerSource",
      AnnotatedPageContentRequest::TriggerSource::kOnLoad, 1);

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageContentExtraction.AutomaticOnLoad."
      "StabilityLatency",
      IsPageSettledMonitorEnabled() ? 0 : 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageContentExtraction.AutomaticOnLoad."
      "ExtractionLatency",
      IsPageSettledMonitorEnabled() ? 0 : 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageContentExtraction.AutomaticOnLoad.OverallLatency",
      IsPageSettledMonitorEnabled() ? 0 : 1);

  // Simulate a same-document navigation.
  auto same_doc_nav = content::NavigationSimulator::CreateRendererInitiated(
      GURL("https://example.com/#test"), web_contents()->GetPrimaryMainFrame());

  // Forces ShouldUpdateHistory() to be true.
  same_doc_nav->SetTransition(ui::PAGE_TRANSITION_LINK);
  same_doc_nav->CommitSameDocument();
  WaitForExtraction();

  EXPECT_EQ(extraction_service().extraction_count(), 2);

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.PageContentExtraction.TriggerSource",
      AnnotatedPageContentRequest::TriggerSource::kOnLoad, 2);

  // We do not expect new latency metrics to be recorded as the navigation was
  // same-document.
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageContentExtraction.AutomaticOnLoad."
      "StabilityLatency",
      IsPageSettledMonitorEnabled() ? 0 : 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageContentExtraction.AutomaticOnLoad."
      "ExtractionLatency",
      IsPageSettledMonitorEnabled() ? 0 : 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageContentExtraction.AutomaticOnLoad.OverallLatency",
      IsPageSettledMonitorEnabled() ? 0 : 1);
}

TEST_P(AnnotatePageContentRequestTest, OnLoadTriggerPDFExtraction) {
  SetTriggeringMode("on_load");
  SimulatePDFLoad(GURL("https://example.com/file.pdf"));

  if (IsPDFTextExtractionEnabled() && PlatformSupportsPDF()) {
    WaitForExtraction();
    EXPECT_EQ(extraction_service().extraction_count(), 1);
    EXPECT_EQ(extraction_service().last_extracted_pdf_text(),
              "Sample PDF text");
  } else {
    // When feature is disabled:
    // - If platform enables PDF build flag, the count of PDF page is requested
    // and logged to UKM metrics. No extraction takes place.
    // - If platform disables PDF build flag, no operation is performed for PDF.
    task_environment()->FastForwardBy(base::Seconds(1));
    EXPECT_EQ(extraction_service().extraction_count(), 0);
    EXPECT_FALSE(extraction_service().last_extracted_pdf_text().has_value());
  }
}

// For PDF documents, on-hidden trigger is not allowed. This is because PDF
// content is almost always static. Repeatedly extracting text does not add much
// value.
TEST_P(AnnotatePageContentRequestTest, OnHiddenTriggerNoPDFExtraction) {
  SetTriggeringMode("on_hidden");
  SimulatePDFLoad(GURL("https://example.com/file.pdf"));

  // No extraction triggered upon load completion.
  EXPECT_EQ(extraction_service().extraction_count(), 0);

  // Hide the tab.
  web_contents()->WasHidden();

  // PDF document does not trigger extraction on hidden.
  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_EQ(extraction_service().extraction_count(), 0);
  EXPECT_FALSE(extraction_service().last_extracted_pdf_text().has_value());
}

TEST_P(AnnotatePageContentRequestTest,
       OnLoadAndHiddenTriggerPDFTextExtraction) {
  SetTriggeringMode("on_load_and_hidden");
  SimulatePDFLoad(GURL("https://example.com/file.pdf"));

  if (IsPDFTextExtractionEnabled() && PlatformSupportsPDF()) {
    // Extraction should happen when load completes.
    WaitForExtraction();
    EXPECT_EQ(extraction_service().extraction_count(), 1);
    EXPECT_EQ(extraction_service().last_extracted_pdf_text(),
              "Sample PDF text");

    // Hide the tab.
    web_contents()->WasHidden();

    // PDF document does not trigger extraction on hidden.
    task_environment()->FastForwardBy(base::Seconds(1));
    EXPECT_EQ(extraction_service().extraction_count(), 1);
  } else {
    // When feature is disabled:
    // - If platform enables PDF build flag, the count of PDF page is requested
    // and logged to UKM metrics. No extraction takes place.
    // - If platform disables PDF build flag, no operation is performed for PDF.
    task_environment()->FastForwardBy(base::Seconds(1));
    EXPECT_EQ(extraction_service().extraction_count(), 0);
    EXPECT_FALSE(extraction_service().last_extracted_pdf_text().has_value());

    // Hide the tab.
    web_contents()->WasHidden();

    // PDF document does not trigger extraction on hidden.
    task_environment()->FastForwardBy(base::Seconds(1));
    EXPECT_EQ(extraction_service().extraction_count(), 0);
  }
}

TEST_P(AnnotatePageContentRequestTest, InterleavedNavigations) {
  SetTriggeringMode("on_load");

  auto nav_1 = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://example.com/"), web_contents());
  nav_1->Start();
  nav_1->ReadyToCommit();

  // The second navigation fails before the first one commits.
  auto nav_2 = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://example.com/$"), web_contents());
  nav_2->Start();
  nav_2->Fail(net::ERR_TIMED_OUT);
  nav_2->AbortCommit();

  // The first navigation commits and triggers the extraction.
  nav_1->Commit();
  SimulatePageStablization();

  WaitForExtraction();

  EXPECT_EQ(extraction_service().extraction_count(), 1);
  EXPECT_TRUE(extraction_service().last_extracted_content().has_value());
}

TEST_P(AnnotatePageContentRequestTest, NavigationToReadyLatency) {
  base::HistogramTester histogram_tester;
  SetTriggeringMode("on_load");

  SimulatePageLoad();

  histogram_tester.ExpectTotalCount(
      IsPageSettledMonitorEnabled()
          ? "OptimizationGuide.PageContentExtraction.NavigationToReadyLatency."
            "CrossDocument.PageSettledMonitor"
          : "OptimizationGuide.PageContentExtraction.NavigationToReadyLatency."
            "CrossDocument.Legacy",
      1);
}

TEST_P(AnnotatePageContentRequestTest, NavigationToReadyLatency_OnHidden) {
  base::HistogramTester histogram_tester;
  SetTriggeringMode("on_hidden");

  SimulatePageLoad();

  // Metric should be recorded even if extraction is not yet scheduled.
  histogram_tester.ExpectTotalCount(
      IsPageSettledMonitorEnabled()
          ? "OptimizationGuide.PageContentExtraction.NavigationToReadyLatency."
            "CrossDocument.PageSettledMonitor"
          : "OptimizationGuide.PageContentExtraction.NavigationToReadyLatency."
            "CrossDocument.Legacy",
      1);

  // Hiding should trigger extraction, but not record the metric again.
  web_contents()->WasHidden();

  histogram_tester.ExpectTotalCount(
      IsPageSettledMonitorEnabled()
          ? "OptimizationGuide.PageContentExtraction.NavigationToReadyLatency."
            "CrossDocument.PageSettledMonitor"
          : "OptimizationGuide.PageContentExtraction.NavigationToReadyLatency."
            "CrossDocument.Legacy",
      1);
}

TEST_P(AnnotatePageContentRequestTest, NavigationToReadyLatency_SameDocument) {
  base::HistogramTester histogram_tester;
  SetTriggeringMode("on_load");

  SimulatePageLoad();
  histogram_tester.ExpectTotalCount(
      IsPageSettledMonitorEnabled()
          ? "OptimizationGuide.PageContentExtraction.NavigationToReadyLatency."
            "CrossDocument.PageSettledMonitor"
          : "OptimizationGuide.PageContentExtraction.NavigationToReadyLatency."
            "CrossDocument.Legacy",
      1);

  // Perform same-document navigation.
  auto navigation = content::NavigationSimulator::CreateRendererInitiated(
      GURL("https://example.com/#ref"), main_rfh());
  navigation->CommitSameDocument();
  SimulatePageStablization();

  histogram_tester.ExpectTotalCount(
      IsPageSettledMonitorEnabled()
          ? "OptimizationGuide.PageContentExtraction.NavigationToReadyLatency."
            "SameDocument.PageSettledMonitor"
          : "OptimizationGuide.PageContentExtraction.NavigationToReadyLatency."
            "SameDocument.Legacy",
      1);
}

INSTANTIATE_TEST_SUITE_P(All,
                         AnnotatePageContentRequestTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool()),
                         &AnnotatePageContentRequestTest::DescribeParams);

}  // namespace page_content_annotations
