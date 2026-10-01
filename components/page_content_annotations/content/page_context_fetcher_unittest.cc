// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/content/page_context_fetcher.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/token.h"
#include "base/types/expected.h"
#include "build/buildflag.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/page_content_annotations/content/page_context_fetcher_metrics.h"
#include "components/viz/common/surfaces/tracked_element_rects.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "pdf/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_PDF)
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "components/page_content_annotations/content/pdf_content_fetcher.h"
#include "components/pdf/browser/pdf_document_helper.h"
#include "components/pdf/browser/pdf_document_helper_client.h"
#include "components/pdf/common/constants.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "pdf/mojom/pdf.mojom.h"
#endif  // BUILDFLAG(ENABLE_PDF)

namespace page_content_annotations {

namespace {

#if BUILDFLAG(ENABLE_PDF)
class TestPDFDocumentHelperClient : public pdf::PDFDocumentHelperClient {
 public:
  TestPDFDocumentHelperClient() = default;
  ~TestPDFDocumentHelperClient() override = default;
};

class FakePdfListener : public pdf::mojom::PdfListener {
 public:
  FakePdfListener() = default;
  ~FakePdfListener() override { Disconnect(); }

  mojo::PendingRemote<pdf::mojom::PdfListener> BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void Disconnect() {
    receiver_.reset();
    pending_bytes_callback_.Reset();
    pending_text_callback_.Reset();
  }

  bool HasPendingBytesCallback() const {
    return !pending_bytes_callback_.is_null();
  }

  bool HasPendingTextCallback() const {
    return !pending_text_callback_.is_null();
  }

  void WaitForBytesRequest() {
    if (HasPendingBytesCallback()) {
      return;
    }
    base::RunLoop run_loop;
    quit_on_bytes_request_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void WaitForTextRequest() {
    if (HasPendingTextCallback()) {
      return;
    }
    base::RunLoop run_loop;
    quit_on_text_request_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void RunPendingBytesCallback(
      pdf::mojom::PdfListener::GetPdfBytesStatus status,
      const std::vector<uint8_t>& bytes,
      uint32_t page_count) {
    if (pending_bytes_callback_) {
      std::move(pending_bytes_callback_).Run(status, bytes, page_count);
      receiver_.FlushForTesting();
    }
  }

  void RunPendingTextCallback(const std::u16string& text) {
    if (pending_text_callback_) {
      std::move(pending_text_callback_).Run(text);
      receiver_.FlushForTesting();
    }
  }

  // pdf::mojom::PdfListener:
  void SetCaretPosition(const gfx::PointF& position) override {}
  void MoveRangeSelectionExtent(const gfx::PointF& extent) override {}
  void SetSelectionBase(const gfx::PointF& base) override {}
  void GetPdfBytes(uint32_t size_limit, GetPdfBytesCallback callback) override {
    pending_bytes_callback_ = std::move(callback);
    if (quit_on_bytes_request_) {
      std::move(quit_on_bytes_request_).Run();
    }
  }
  void GetPageText(int32_t page_index, GetPageTextCallback callback) override {
    pending_text_callback_ = std::move(callback);
    if (quit_on_text_request_) {
      std::move(quit_on_text_request_).Run();
    }
  }
  void GetMostVisiblePageIndex(
      GetMostVisiblePageIndexCallback callback) override {}
  void HasMeaningfulText(HasMeaningfulTextCallback callback) override {}
  void HasJavaScript(HasJavaScriptCallback callback) override {}
  void IsPasswordProtected(IsPasswordProtectedCallback callback) override {}
#if BUILDFLAG(ENABLE_PDF_SAVE_TO_DRIVE)
  void GetSaveDataBufferHandlerForDrive(
      pdf::mojom::SaveRequestType request_type,
      GetSaveDataBufferHandlerForDriveCallback callback) override {}
#endif  // BUILDFLAG(ENABLE_PDF_SAVE_TO_DRIVE)

 private:
  GetPdfBytesCallback pending_bytes_callback_;
  GetPageTextCallback pending_text_callback_;
  base::OnceClosure quit_on_bytes_request_;
  base::OnceClosure quit_on_text_request_;
  mojo::Receiver<pdf::mojom::PdfListener> receiver_{this};
};

#endif  // BUILDFLAG(ENABLE_PDF)

}  // namespace

TEST(PageContextFetcherTest, RedactScreenshotOnWorkerThread) {
  base::HistogramTester histograms;
  SkBitmap bitmap;
  bitmap.allocN32Pixels(100, 100);
  bitmap.eraseColor(SK_ColorBLUE);

  std::vector<gfx::Rect> redaction_rects;
  redaction_rects.emplace_back(10, 10, 20, 20);

  base::expected<SkBitmap, std::string> redacted =
      PageContextFetcher::RedactScreenshotOnWorkerThread(
          bitmap, redaction_rects, SkColors::kRed);

  ASSERT_TRUE(redacted.has_value());
  ASSERT_EQ(redacted->width(), 100);
  ASSERT_EQ(redacted->height(), 100);

  // Check a pixel that should NOT be redacted (remains blue).
  EXPECT_EQ(redacted->getColor(5, 5), SK_ColorBLUE);
  EXPECT_EQ(redacted->getColor(50, 50), SK_ColorBLUE);

  // Check a pixel that SHOULD be redacted (becomes red).
  EXPECT_EQ(redacted->getColor(15, 15), SK_ColorRED);
  EXPECT_EQ(redacted->getColor(25, 25), SK_ColorRED);

  histograms.ExpectUniqueSample("Glic.PageContextFetcher.ScreenshotRedacted",
                                true, 1);
}

TEST(PageContextFetcherTest, RedactScreenshotOnWorkerThreadNoRedaction) {
  base::HistogramTester histograms;
  SkBitmap bitmap;
  bitmap.allocN32Pixels(100, 100);
  bitmap.eraseColor(SK_ColorBLUE);

  std::vector<gfx::Rect> redaction_rects;

  base::expected<SkBitmap, std::string> redacted =
      PageContextFetcher::RedactScreenshotOnWorkerThread(
          bitmap, redaction_rects, SkColors::kRed);

  ASSERT_TRUE(redacted.has_value());
  // Verify the result is correct.
  EXPECT_EQ(redacted->getColor(50, 50), SK_ColorBLUE);
  // Verify the optimization: the original bitmap should be returned without
  // unnecessary copies when no redaction is performed.
  EXPECT_EQ(bitmap.getPixels(), redacted->getPixels());

  histograms.ExpectUniqueSample("Glic.PageContextFetcher.ScreenshotRedacted",
                                false, 1);
}

class PageContextFetcherIframeInfoTest
    : public content::RenderViewHostTestHarness {};

TEST_F(PageContextFetcherIframeInfoTest, AddIframeInfoSuccess) {
  base::HistogramTester histograms;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      blink::features::kAIPageContentTrackedElementsIframe);

  // Setup frames.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://main-frame.com"));
  content::RenderFrameHost* subframe =
      content::NavigationSimulator::NavigateAndCommitFromDocument(
          GURL("https://main-frame.com/subframe"),
          content::RenderFrameHostTester::For(main_rfh())
              ->AppendChild("subframe"));

  // Create a tracked element rect for the iframe.
  viz::TrackedElementRect iframe_rect(
      base::Token(1, 2), gfx::Rect(10, 20, 150, 250),
      /*should_add_to_compositor_frame_metadata=*/true,
      /*should_exclude_fixed_and_sticky_occlusions=*/false,
      subframe->GetFrameToken(), main_rfh()->GetFrameToken());
  viz::TrackedElementRects tracked_element_rects = {
      {viz::TrackedElementFeature::kIframeTracking, {iframe_rect}}};

  // Collect the tracked iframe element.
  PageContextFetcher fetcher(
      base::NullCallback(), /*progress_listener=*/nullptr,
      /*fetch_pdf_content_callback=*/base::NullCallback());
  fetcher.Observe(web_contents());
  fetcher.CollectTrackedElementRectsForIframes(tracked_element_rects);

  // Check that the intermediate iframe_info_ is populated.
  ASSERT_EQ(fetcher.iframe_info_.size(), 1u);
  EXPECT_EQ(fetcher.iframe_info_[0].url(), "https://main-frame.com/subframe");
  EXPECT_EQ(fetcher.iframe_info_[0].security_origin().value(),
            "https://main-frame.com");
  EXPECT_EQ(fetcher.iframe_info_[0].bounding_box().x(), 10);
  EXPECT_EQ(fetcher.iframe_info_[0].bounding_box().y(), 20);
  EXPECT_EQ(fetcher.iframe_info_[0].bounding_box().width(), 150);
  EXPECT_EQ(fetcher.iframe_info_[0].bounding_box().height(), 250);

  // Setup the remaining state and call MaybeAddIframeInfo.
  fetcher.pending_result_ = std::make_unique<FetchPageContextResult>();
  optimization_guide::AIPageContentResult apc_result;
  fetcher.pending_result_->annotated_page_content_result =
      base::ok(PageContentResultWithEndTime(std::move(apc_result)));
  fetcher.screenshot_capture_done_ = true;
  fetcher.annotated_page_content_done_ = true;
  fetcher.MaybeAddIframeInfo();

  // Verify metrics and result.
  histograms.ExpectUniqueSample("Glic.PageContextFetcher.IframeInfoAddedToAPC",
                                true, 1);
  histograms.ExpectUniqueSample(
      "Glic.PageContextFetcher.IframeInfoHasUrlOrigin", true, 1);
  histograms.ExpectTotalCount(
      "Glic.PageContextFetcher.ScreenshotInfo.IframeInfo.ProtoSize", 1);

  const auto& screenshot_info =
      fetcher.pending_result_->annotated_page_content_result->proto
          .gemini_in_chrome_page_metadata()
          .screenshot_info();
  ASSERT_EQ(screenshot_info.iframe_info_size(), 1);
  EXPECT_EQ(screenshot_info.iframe_info(0).url(),
            "https://main-frame.com/subframe");
  EXPECT_EQ(screenshot_info.iframe_info(0).security_origin().value(),
            "https://main-frame.com");
  EXPECT_EQ(screenshot_info.iframe_info(0).bounding_box().x(), 10);
  EXPECT_EQ(screenshot_info.iframe_info(0).bounding_box().y(), 20);
  EXPECT_EQ(screenshot_info.iframe_info(0).bounding_box().width(), 150);
  EXPECT_EQ(screenshot_info.iframe_info(0).bounding_box().height(), 250);

  ASSERT_TRUE(fetcher.pending_result_->screenshot_info.has_value());
  EXPECT_THAT(fetcher.pending_result_->screenshot_info.value(),
              base::test::EqualsProto(
                  fetcher.pending_result_->annotated_page_content_result->proto
                      .gemini_in_chrome_page_metadata()
                      .screenshot_info()));
}

TEST_F(PageContextFetcherIframeInfoTest, AddIframeInfoNoUrlOrigin) {
  base::HistogramTester histograms;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      blink::features::kAIPageContentTrackedElementsIframe);

  // Create a tracked element rect for a stale iframe (not in the current tree).
  blink::LocalFrameToken fake_iframe_token;
  blink::LocalFrameToken fake_parent_token;
  viz::TrackedElementRect iframe_rect(
      base::Token(3, 4), gfx::Rect(30, 40, 350, 450),
      /*should_add_to_compositor_frame_metadata=*/true,
      /*should_exclude_fixed_and_sticky_occlusions=*/false, fake_iframe_token,
      fake_parent_token);
  viz::TrackedElementRects tracked_element_rects = {
      {viz::TrackedElementFeature::kIframeTracking, {iframe_rect}}};

  // Collect the tracked iframe element.
  PageContextFetcher fetcher(
      base::NullCallback(), /*progress_listener=*/nullptr,
      /*fetch_pdf_content_callback=*/base::NullCallback());
  fetcher.Observe(web_contents());
  fetcher.CollectTrackedElementRectsForIframes(tracked_element_rects);

  // Check that the intermediate iframe_info_ is populated.
  ASSERT_EQ(fetcher.iframe_info_.size(), 1u);
  EXPECT_FALSE(fetcher.iframe_info_[0].has_url());
  EXPECT_FALSE(fetcher.iframe_info_[0].has_security_origin());
  EXPECT_EQ(fetcher.iframe_info_[0].bounding_box().x(), 30);
  EXPECT_EQ(fetcher.iframe_info_[0].bounding_box().y(), 40);
  EXPECT_EQ(fetcher.iframe_info_[0].bounding_box().width(), 350);
  EXPECT_EQ(fetcher.iframe_info_[0].bounding_box().height(), 450);

  // Setup the remaining state and call MaybeAddIframeInfo.
  fetcher.pending_result_ = std::make_unique<FetchPageContextResult>();
  optimization_guide::AIPageContentResult apc_result;
  fetcher.pending_result_->annotated_page_content_result =
      base::ok(PageContentResultWithEndTime(std::move(apc_result)));
  fetcher.screenshot_capture_done_ = true;
  fetcher.annotated_page_content_done_ = true;
  fetcher.MaybeAddIframeInfo();

  // Verify metrics and result.
  histograms.ExpectUniqueSample("Glic.PageContextFetcher.IframeInfoAddedToAPC",
                                true, 1);
  histograms.ExpectUniqueSample(
      "Glic.PageContextFetcher.IframeInfoHasUrlOrigin", false, 1);
  histograms.ExpectTotalCount(
      "Glic.PageContextFetcher.ScreenshotInfo.IframeInfo.ProtoSize", 1);

  const auto& screenshot_info =
      fetcher.pending_result_->annotated_page_content_result->proto
          .gemini_in_chrome_page_metadata()
          .screenshot_info();
  ASSERT_EQ(screenshot_info.iframe_info_size(), 1);
  EXPECT_FALSE(screenshot_info.iframe_info(0).has_url());
  EXPECT_FALSE(screenshot_info.iframe_info(0).has_security_origin());
  EXPECT_EQ(screenshot_info.iframe_info(0).bounding_box().x(), 30);
  EXPECT_EQ(screenshot_info.iframe_info(0).bounding_box().y(), 40);
  EXPECT_EQ(screenshot_info.iframe_info(0).bounding_box().width(), 350);
  EXPECT_EQ(screenshot_info.iframe_info(0).bounding_box().height(), 450);

  ASSERT_TRUE(fetcher.pending_result_->screenshot_info.has_value());
  EXPECT_THAT(fetcher.pending_result_->screenshot_info.value(),
              base::test::EqualsProto(
                  fetcher.pending_result_->annotated_page_content_result->proto
                      .gemini_in_chrome_page_metadata()
                      .screenshot_info()));
}

TEST_F(PageContextFetcherIframeInfoTest, NoIframeInfoWhenFeatureDisabled) {
  base::HistogramTester histograms;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      blink::features::kAIPageContentTrackedElementsIframe);

  // Setup frames.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://main-frame.com"));
  content::RenderFrameHost* subframe =
      content::NavigationSimulator::NavigateAndCommitFromDocument(
          GURL("https://main-frame.com/subframe"),
          content::RenderFrameHostTester::For(main_rfh())
              ->AppendChild("subframe"));

  // Create a tracked element rect for the iframe.
  viz::TrackedElementRect iframe_rect(
      base::Token(1, 2), gfx::Rect(10, 20, 150, 250),
      /*should_add_to_compositor_frame_metadata=*/true,
      /*should_exclude_fixed_and_sticky_occlusions=*/false,
      subframe->GetFrameToken(), main_rfh()->GetFrameToken());
  viz::TrackedElementRects tracked_element_rects = {
      {viz::TrackedElementFeature::kIframeTracking, {iframe_rect}}};

  // When the feature is disabled, CollectTrackedElementRectsForIframes should
  // do nothing.
  PageContextFetcher fetcher(
      base::NullCallback(), /*progress_listener=*/nullptr,
      /*fetch_pdf_content_callback=*/base::NullCallback());
  fetcher.Observe(web_contents());
  fetcher.CollectTrackedElementRectsForIframes(tracked_element_rects);

  EXPECT_TRUE(fetcher.iframe_info_.empty());

  // Setup the remaining state and call MaybeAddIframeInfo.
  fetcher.pending_result_ = std::make_unique<FetchPageContextResult>();
  optimization_guide::AIPageContentResult apc_result;
  fetcher.pending_result_->annotated_page_content_result =
      base::ok(PageContentResultWithEndTime(std::move(apc_result)));
  fetcher.screenshot_capture_done_ = true;
  fetcher.annotated_page_content_done_ = true;
  fetcher.MaybeAddIframeInfo();

  // Verify metrics and that nothing was added to the result.
  histograms.ExpectTotalCount("Glic.PageContextFetcher.IframeInfoHasUrlOrigin",
                              0);
  histograms.ExpectTotalCount("Glic.PageContextFetcher.IframeInfoAddedToAPC",
                              0);

  EXPECT_FALSE(fetcher.pending_result_->screenshot_info.has_value());

  const auto& screenshot_info =
      fetcher.pending_result_->annotated_page_content_result->proto
          .gemini_in_chrome_page_metadata()
          .screenshot_info();
  EXPECT_EQ(screenshot_info.iframe_info_size(), 0);
}

#if BUILDFLAG(ENABLE_PDF)
class PageContextFetcherPdfTest : public content::RenderViewHostTestHarness {
 public:
  PageContextFetcherPdfTest() = default;
  ~PageContextFetcherPdfTest() override = default;

  base::test::TestFuture<FetchPageContextResultCallbackArg>& GetFuture() {
    return future_;
  }

  PageContextFetcher& GetFetcher() { return *fetcher_; }

 protected:
  void TearDown() override {
    fetcher_.reset();
    content::RenderViewHostTestHarness::TearDown();
  }

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    content::NavigationSimulator::NavigateAndCommitFromBrowser(
        web_contents(), GURL("https://example.com"));
    fetcher_ = std::make_unique<PageContextFetcher>(
        base::NullCallback(), /*progress_listener=*/nullptr,
        /*fetch_pdf_content_callback=*/base::NullCallback());
    fetcher_->Observe(web_contents());
    fetcher_->pending_result_ = std::make_unique<FetchPageContextResult>();
    fetcher_->callback_ = future_.GetCallback();
    fetcher_->initialization_done_ = true;
    fetcher_->screenshot_done_ = true;
    fetcher_->inner_text_done_ = true;
    fetcher_->annotated_page_content_done_ = true;
  }

 private:
  std::unique_ptr<PageContextFetcher> fetcher_;
  base::test::TestFuture<FetchPageContextResultCallbackArg> future_;
};

// PDF bytes extraction supports top-level PDF and embedded PDF.
class PageContextFetcherPdfBytesExtractionTest
    : public PageContextFetcherPdfTest,
      public testing::WithParamInterface<bool> {
 public:
  PageContextFetcherPdfBytesExtractionTest() = default;
  ~PageContextFetcherPdfBytesExtractionTest() override = default;

  bool IsTopLevelPDF() const { return GetParam(); }

  void ReceivedPdfBytes(uint32_t pdf_size_limit,
                        pdf::mojom::PdfListener::GetPdfBytesStatus status,
                        const std::vector<uint8_t>& pdf_bytes) {
    ConvertPdfBytesToResultForTesting(
        url::Origin::Create(GURL("https://example.com")), IsTopLevelPDF(),
        pdf_size_limit, status, pdf_bytes, /*page_count=*/1,
        base::BindOnce(&PageContextFetcher::ReceivedPdfResult,
                       base::Unretained(&GetFetcher())));
  }
};

TEST_P(PageContextFetcherPdfBytesExtractionTest, ReceivedPdfBytes) {
  base::HistogramTester histograms;
  std::vector<uint8_t> pdf_bytes(1024, 1);
  ReceivedPdfBytes(/*pdf_size_limit=*/1024,
                   pdf::mojom::PdfListener::GetPdfBytesStatus::kSuccess,
                   pdf_bytes);

  histograms.ExpectTotalCount(IsTopLevelPDF()
                                  ? kPdfBytesTopLevelLatencyHistogram
                                  : kPdfBytesEmbeddedLatencyHistogram,
                              1);
  // The bytes size is recorded as 1KB in the UMA.
  histograms.ExpectUniqueSample(IsTopLevelPDF()
                                    ? kPdfBytesTopLevelSizeHistogram
                                    : kPdfBytesEmbeddedSizeHistogram,
                                1, 1);
  histograms.ExpectUniqueSample(
      IsTopLevelPDF() ? kPdfBytesTopLevelSizeLimitExceededHistogram
                      : kPdfBytesEmbeddedSizeLimitExceededHistogram,
      false, 1);

  auto result = GetFuture().Take();
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE((*result)->pdf_result.has_value());
  EXPECT_FALSE((*result)->pdf_result->size_exceeded);
  const auto* bytes =
      std::get_if<std::vector<uint8_t>>(&(*result)->pdf_result->data);
  ASSERT_TRUE(bytes);
  EXPECT_EQ(*bytes, pdf_bytes);
}

// When the original PDF bytes exceed the limit, `ReceivedPdfBytes` will receive
// an empty vector as the extraction result.
TEST_P(PageContextFetcherPdfBytesExtractionTest,
       OriginalPDFBytesSizeLimitExceeded) {
  base::HistogramTester histograms;
  std::vector<uint8_t> empty_bytes;
  ReceivedPdfBytes(
      /*pdf_size_limit=*/1024,
      pdf::mojom::PdfListener::GetPdfBytesStatus::kSizeLimitExceeded,
      empty_bytes);

  histograms.ExpectTotalCount(IsTopLevelPDF()
                                  ? kPdfBytesTopLevelLatencyHistogram
                                  : kPdfBytesEmbeddedLatencyHistogram,
                              1);
  // When the extraction status is not successful, no size sample is recorded.
  histograms.ExpectTotalCount(IsTopLevelPDF() ? kPdfBytesTopLevelSizeHistogram
                                              : kPdfBytesEmbeddedSizeHistogram,
                              0);
  histograms.ExpectUniqueSample(
      IsTopLevelPDF() ? kPdfBytesTopLevelSizeLimitExceededHistogram
                      : kPdfBytesEmbeddedSizeLimitExceededHistogram,
      true, 1);

  auto result = GetFuture().Take();
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE((*result)->pdf_result.has_value());
  EXPECT_TRUE((*result)->pdf_result->size_exceeded);
  const auto* bytes =
      std::get_if<std::vector<uint8_t>>(&(*result)->pdf_result->data);
  ASSERT_TRUE(bytes);
  EXPECT_TRUE(bytes->empty());
}

// Even though the original PDF bytes do not exceed the limit, it is possible
// that `ReceivedPdfBytes` receives bytes that exceed the limit. See comments in
// `PageContextFetcher::ReceivedPdfBytes`.
TEST_P(PageContextFetcherPdfBytesExtractionTest,
       ReceivedPdfBytesSizeLimitExceeded) {
  base::HistogramTester histograms;
  std::vector<uint8_t> pdf_bytes(4096, 1);
  ReceivedPdfBytes(/*pdf_size_limit=*/1024,
                   pdf::mojom::PdfListener::GetPdfBytesStatus::kSuccess,
                   pdf_bytes);

  histograms.ExpectTotalCount(IsTopLevelPDF()
                                  ? kPdfBytesTopLevelLatencyHistogram
                                  : kPdfBytesEmbeddedLatencyHistogram,
                              1);
  histograms.ExpectUniqueSample(IsTopLevelPDF()
                                    ? kPdfBytesTopLevelSizeHistogram
                                    : kPdfBytesEmbeddedSizeHistogram,
                                4, 1);
  histograms.ExpectUniqueSample(
      IsTopLevelPDF() ? kPdfBytesTopLevelSizeLimitExceededHistogram
                      : kPdfBytesEmbeddedSizeLimitExceededHistogram,
      true, 1);

  auto result = GetFuture().Take();
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE((*result)->pdf_result.has_value());
  EXPECT_TRUE((*result)->pdf_result->size_exceeded);
  const auto* bytes =
      std::get_if<std::vector<uint8_t>>(&(*result)->pdf_result->data);
  ASSERT_TRUE(bytes);
  EXPECT_TRUE(bytes->empty());
}

INSTANTIATE_TEST_SUITE_P(All,
                         PageContextFetcherPdfBytesExtractionTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "TopLevel" : "Embedded";
                         });

// PDF text extraction supports only top-level PDF.
class PageContextFetcherPdfTextExtractionTest
    : public PageContextFetcherPdfTest {
 public:
  PageContextFetcherPdfTextExtractionTest() = default;
  ~PageContextFetcherPdfTextExtractionTest() override = default;

  void ReceivedPdfText(url::Origin pdf_origin,
                       uint32_t text_byte_limit,
                       const std::u16string& text) {
    ConvertPdfTextToResultForTesting(
        std::move(pdf_origin), text_byte_limit, text,
        base::BindOnce(&PageContextFetcher::ReceivedPdfResult,
                       base::Unretained(&GetFetcher())));
  }
};

TEST_F(PageContextFetcherPdfTextExtractionTest, ReceivedPdfText) {
  base::HistogramTester histograms;
  const std::u16string text(2048, 'a');
  ReceivedPdfText(url::Origin::Create(GURL("https://example.com")),
                  /*text_byte_limit=*/1024 * 1024, text);

  histograms.ExpectUniqueSample(kPdfTextExtractionStatusHistogram,
                                PdfTextExtractionStatus::kSuccess, 1);
  histograms.ExpectTotalCount(kPdfTextTopLevelLatencyHistogram, 1);
  histograms.ExpectUniqueSample(kPdfTextTopLevelSizeHistogram,
                                base::span(text).size_bytes() / 1024, 1);
  histograms.ExpectUniqueSample(kPdfTextTopLevelSizeLimitExceededHistogram,
                                false, 1);

  auto result = GetFuture().Take();
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE((*result)->pdf_result.has_value());
  EXPECT_FALSE((*result)->pdf_result->size_exceeded);
  const auto* str = std::get_if<std::string>(&(*result)->pdf_result->data);
  ASSERT_TRUE(str);
  EXPECT_EQ(*str, base::UTF16ToUTF8(text));
}

TEST_F(PageContextFetcherPdfTextExtractionTest,
       ReceivedPdfTextSizeLimitExceeded) {
  base::HistogramTester histograms;
  const std::u16string text(2048, 'a');
  ReceivedPdfText(url::Origin::Create(GURL("https://example.com")),
                  /*text_byte_limit=*/5, text);

  histograms.ExpectUniqueSample(kPdfTextExtractionStatusHistogram,
                                PdfTextExtractionStatus::kSuccess, 1);
  histograms.ExpectTotalCount(kPdfTextTopLevelLatencyHistogram, 1);
  // The recorded size should be the size of the text returned from PDFium,
  // which is the `text` passed to `ReceivedPdfText` without any processing.
  // So even if the `text` size exceeds the `text_byte_limit`, the recorded size
  // is the size before any truncation by `ReceivedPdfText`.
  histograms.ExpectUniqueSample(kPdfTextTopLevelSizeHistogram,
                                base::span(text).size_bytes() / 1024, 1);
  histograms.ExpectUniqueSample(kPdfTextTopLevelSizeLimitExceededHistogram,
                                true, 1);

  auto result = GetFuture().Take();
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE((*result)->pdf_result.has_value());
  EXPECT_TRUE((*result)->pdf_result->size_exceeded);
  const auto* str = std::get_if<std::string>(&(*result)->pdf_result->data);
  ASSERT_TRUE(str);
  // The final result is truncated according to `text_byte_limit`.
  EXPECT_EQ(*str, "aaaaa");
}

TEST_F(PageContextFetcherPdfTextExtractionTest, ReceivedPdfTextEmpty) {
  base::HistogramTester histograms;
  ReceivedPdfText(url::Origin::Create(GURL("https://example.com")),
                  /*text_byte_limit=*/1024, u"");

  histograms.ExpectUniqueSample(kPdfTextExtractionStatusHistogram,
                                PdfTextExtractionStatus::kEmptyText, 1);
  histograms.ExpectTotalCount(kPdfTextTopLevelLatencyHistogram, 1);
  histograms.ExpectUniqueSample(kPdfTextTopLevelSizeHistogram, 0, 1);
  histograms.ExpectUniqueSample(kPdfTextTopLevelSizeLimitExceededHistogram,
                                false, 1);

  auto result = GetFuture().Take();
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE((*result)->pdf_result.has_value());
  EXPECT_FALSE((*result)->pdf_result->size_exceeded);
  const auto* str = std::get_if<std::string>(&(*result)->pdf_result->data);
  ASSERT_TRUE(str);
  EXPECT_TRUE(str->empty());
}

class PageContextFetcherHangingPdfTest
    : public content::RenderViewHostTestHarness {
 public:
  PageContextFetcherHangingPdfTest()
      : content::RenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~PageContextFetcherHangingPdfTest() override = default;

  void AttachPdfDocumentHelper(content::RenderFrameHost* rfh,
                               FakePdfListener& listener) {
    pdf::PDFDocumentHelper::CreateForCurrentDocument(
        rfh, std::make_unique<TestPDFDocumentHelperClient>());
    auto* helper = pdf::PDFDocumentHelper::GetForCurrentDocument(rfh);
    helper->SetListener(listener.BindNewPipeAndPassRemote());
    helper->OnDocumentLoadComplete();
  }

  void NavigateTopLevelPdf(content::WebContents* contents, const GURL& url) {
    auto simulator =
        content::NavigationSimulator::CreateBrowserInitiated(url, contents);
    simulator->SetContentsMimeType(pdf::kPDFMimeType);
    simulator->Commit();
  }

  bool IsPdfDone(const PageContextFetcher& fetcher) const {
    return fetcher.pdf_done_;
  }
};

TEST_F(PageContextFetcherHangingPdfTest, TopLevelPdfBytesExtractionTimeout) {
  base::HistogramTester histograms;
  NavigateTopLevelPdf(web_contents(), GURL("https://example.com/test.pdf"));
  FakePdfListener fake_listener;
  AttachPdfDocumentHelper(main_rfh(), fake_listener);

  PageContextFetcher fetcher(
      base::NullCallback(), /*progress_listener=*/nullptr,
      base::BindRepeating(&FetchPdfContentForWebContents));
  base::test::TestFuture<FetchPageContextResultCallbackArg> future;
  FetchPageContextOptions options;
  options.pdf_options.emplace(PdfOptions::Format::kBytes, /*size_limit=*/1024);

  fetcher.FetchStart(*web_contents(), options, future.GetCallback());
  fake_listener.WaitForBytesRequest();

  EXPECT_TRUE(fake_listener.HasPendingBytesCallback());
  EXPECT_FALSE(IsPdfDone(fetcher));
  EXPECT_FALSE(future.IsReady());

  // Advance clock to reach timeout.
  task_environment()->FastForwardBy(kPdfExtractionTimeout.Get());

  ASSERT_TRUE(future.Wait());
  EXPECT_TRUE(IsPdfDone(fetcher));

  // Time out, PDF result should be null.
  auto result = future.Take();
  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE((*result)->pdf_result.has_value());

  // Late extraction result arriving after timeout should be ignored.
  fake_listener.RunPendingBytesCallback(
      pdf::mojom::PdfListener::GetPdfBytesStatus::kSuccess, {1, 2, 3}, 1);

  histograms.ExpectTotalCount(kPdfBytesTopLevelLatencyHistogram, 0);
  histograms.ExpectTotalCount(kPdfBytesTopLevelSizeHistogram, 0);
  histograms.ExpectTotalCount(kPdfBytesTopLevelSizeLimitExceededHistogram, 0);
  histograms.ExpectTotalCount("Glic.PageContextFetcher.Total", 1);
}

TEST_F(PageContextFetcherHangingPdfTest, TopLevelPdfTextExtractionTimeout) {
  base::HistogramTester histograms;
  NavigateTopLevelPdf(web_contents(), GURL("https://example.com/test.pdf"));
  FakePdfListener fake_listener;
  AttachPdfDocumentHelper(main_rfh(), fake_listener);

  PageContextFetcher fetcher(
      base::NullCallback(), /*progress_listener=*/nullptr,
      base::BindRepeating(&FetchPdfContentForWebContents));
  base::test::TestFuture<FetchPageContextResultCallbackArg> future;
  FetchPageContextOptions options;
  options.pdf_options.emplace(PdfOptions::Format::kText, /*size_limit=*/1024);

  fetcher.FetchStart(*web_contents(), options, future.GetCallback());
  fake_listener.WaitForTextRequest();

  EXPECT_TRUE(fake_listener.HasPendingTextCallback());
  EXPECT_FALSE(IsPdfDone(fetcher));
  EXPECT_FALSE(future.IsReady());

  // Advance clock to reach timeout.
  task_environment()->FastForwardBy(kPdfExtractionTimeout.Get());

  ASSERT_TRUE(future.Wait());
  EXPECT_TRUE(IsPdfDone(fetcher));

  // Time out, PDF result should be null.
  auto result = future.Take();
  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE((*result)->pdf_result.has_value());

  // Late extraction result arriving after timeout should be ignored.
  fake_listener.RunPendingTextCallback(u"late page text");

  histograms.ExpectTotalCount(kPdfTextTopLevelLatencyHistogram, 0);
  histograms.ExpectTotalCount(kPdfTextExtractionStatusHistogram, 0);
  histograms.ExpectTotalCount(kPdfTextTopLevelSizeHistogram, 0);
  histograms.ExpectTotalCount(kPdfTextTopLevelSizeLimitExceededHistogram, 0);
  histograms.ExpectTotalCount("Glic.PageContextFetcher.Total", 1);
}

TEST_F(PageContextFetcherHangingPdfTest, EmbeddedPdfBytesExtractionTimeout) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kGlicEmbeddedPdfBytesExtraction);

  base::HistogramTester histograms;
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://example.com/main.html"));
  content::RenderFrameHost* pdf_frame =
      content::NavigationSimulator::NavigateAndCommitFromDocument(
          GURL("https://example.com/embedded.pdf"),
          content::RenderFrameHostTester::For(main_rfh())
              ->AppendChild("pdf_iframe"));

  FakePdfListener fake_listener;
  AttachPdfDocumentHelper(pdf_frame, fake_listener);

  PageContextFetcher fetcher(
      base::NullCallback(), /*progress_listener=*/nullptr,
      base::BindRepeating(&FetchPdfContentForWebContents));
  base::test::TestFuture<FetchPageContextResultCallbackArg> future;
  FetchPageContextOptions options;
  options.pdf_options.emplace(PdfOptions::Format::kBytes, /*size_limit=*/1024);

  fetcher.FetchStart(*web_contents(), options, future.GetCallback());
  fake_listener.WaitForBytesRequest();

  EXPECT_TRUE(fake_listener.HasPendingBytesCallback());
  EXPECT_FALSE(IsPdfDone(fetcher));
  EXPECT_FALSE(future.IsReady());

  // Advance clock to reach timeout.
  task_environment()->FastForwardBy(kPdfExtractionTimeout.Get());

  ASSERT_TRUE(future.Wait());
  EXPECT_TRUE(IsPdfDone(fetcher));

  // Time out, PDF result should be null.
  auto result = future.Take();
  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE((*result)->pdf_result.has_value());

  // Late extraction result arriving after timeout should be ignored.
  fake_listener.RunPendingBytesCallback(
      pdf::mojom::PdfListener::GetPdfBytesStatus::kSuccess, {1, 2, 3}, 1);

  histograms.ExpectTotalCount(kPdfBytesEmbeddedLatencyHistogram, 0);
  histograms.ExpectTotalCount(kPdfBytesEmbeddedSizeHistogram, 0);
  histograms.ExpectTotalCount(kPdfBytesEmbeddedSizeLimitExceededHistogram, 0);
  histograms.ExpectTotalCount("Glic.PageContextFetcher.Total", 1);
}

TEST_F(PageContextFetcherHangingPdfTest, TopLevelPdfBytesExtractionDisconnect) {
  NavigateTopLevelPdf(web_contents(), GURL("https://example.com/test.pdf"));
  FakePdfListener fake_listener;
  AttachPdfDocumentHelper(main_rfh(), fake_listener);

  PageContextFetcher fetcher(
      base::NullCallback(), /*progress_listener=*/nullptr,
      base::BindRepeating(&FetchPdfContentForWebContents));
  base::test::TestFuture<FetchPageContextResultCallbackArg> future;
  FetchPageContextOptions options;
  options.pdf_options.emplace(PdfOptions::Format::kBytes, /*size_limit=*/1024);

  fetcher.FetchStart(*web_contents(), options, future.GetCallback());
  fake_listener.WaitForBytesRequest();

  EXPECT_TRUE(fake_listener.HasPendingBytesCallback());
  EXPECT_FALSE(IsPdfDone(fetcher));
  EXPECT_FALSE(future.IsReady());

  // Simulate PDFium IPC pipe disconnection (e.g. PDFium process crash).
  fake_listener.Disconnect();

  // The page context extraction is done. The PDF result should be null.
  ASSERT_TRUE(future.Wait());
  EXPECT_TRUE(IsPdfDone(fetcher));
  auto result = future.Take();
  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE((*result)->pdf_result.has_value());
}

TEST_F(PageContextFetcherHangingPdfTest, TopLevelPdfTextExtractionDisconnect) {
  NavigateTopLevelPdf(web_contents(), GURL("https://example.com/test.pdf"));
  FakePdfListener fake_listener;
  AttachPdfDocumentHelper(main_rfh(), fake_listener);

  PageContextFetcher fetcher(
      base::NullCallback(), /*progress_listener=*/nullptr,
      base::BindRepeating(&FetchPdfContentForWebContents));
  base::test::TestFuture<FetchPageContextResultCallbackArg> future;
  FetchPageContextOptions options;
  options.pdf_options.emplace(PdfOptions::Format::kText, /*size_limit=*/1024);

  fetcher.FetchStart(*web_contents(), options, future.GetCallback());
  fake_listener.WaitForTextRequest();

  EXPECT_TRUE(fake_listener.HasPendingTextCallback());
  EXPECT_FALSE(IsPdfDone(fetcher));
  EXPECT_FALSE(future.IsReady());

  // Simulate PDFium IPC pipe disconnection (e.g. PDFium process crash).
  fake_listener.Disconnect();

  // The page context extraction is done. The PDF result should be null.
  ASSERT_TRUE(future.Wait());
  EXPECT_TRUE(IsPdfDone(fetcher));
  auto result = future.Take();
  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE((*result)->pdf_result.has_value());
}

TEST_F(PageContextFetcherHangingPdfTest,
       EmbeddedPdfBytesExtractionIframeNavigates) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kGlicEmbeddedPdfBytesExtraction);

  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://example.com/main.html"));
  content::RenderFrameHost* pdf_frame =
      content::NavigationSimulator::NavigateAndCommitFromDocument(
          GURL("https://example.com/embedded.pdf"),
          content::RenderFrameHostTester::For(main_rfh())
              ->AppendChild("pdf_iframe"));

  FakePdfListener fake_listener;
  AttachPdfDocumentHelper(pdf_frame, fake_listener);

  PageContextFetcher fetcher(
      base::NullCallback(), /*progress_listener=*/nullptr,
      base::BindRepeating(&FetchPdfContentForWebContents));
  base::test::TestFuture<FetchPageContextResultCallbackArg> future;
  FetchPageContextOptions options;
  options.pdf_options.emplace(PdfOptions::Format::kBytes, /*size_limit=*/1024);

  fetcher.FetchStart(*web_contents(), options, future.GetCallback());
  fake_listener.WaitForBytesRequest();

  EXPECT_TRUE(fake_listener.HasPendingBytesCallback());
  EXPECT_FALSE(IsPdfDone(fetcher));
  EXPECT_FALSE(future.IsReady());

  // Navigate the iframe away while the extraction is still in progress.
  // This destroys the old document's PDFDocumentHelper and disconnects the IPC
  // pipe.
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://example.com/other.html"), pdf_frame);

  // The page context extraction is done. The PDF result should be null.
  ASSERT_TRUE(future.Wait());
  EXPECT_TRUE(IsPdfDone(fetcher));
  auto result = future.Take();
  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE((*result)->pdf_result.has_value());
}

TEST_F(PageContextFetcherHangingPdfTest,
       WebContentsDestroyedDuringHangingPdfExtraction) {
  std::unique_ptr<content::WebContents> test_contents = CreateTestWebContents();
  NavigateTopLevelPdf(test_contents.get(),
                      GURL("https://example.com/test.pdf"));
  FakePdfListener fake_listener;
  AttachPdfDocumentHelper(test_contents->GetPrimaryMainFrame(), fake_listener);

  PageContextFetcher fetcher(
      base::NullCallback(), /*progress_listener=*/nullptr,
      base::BindRepeating(&FetchPdfContentForWebContents));
  base::test::TestFuture<FetchPageContextResultCallbackArg> future;
  FetchPageContextOptions options;
  options.pdf_options.emplace(PdfOptions::Format::kBytes, /*size_limit=*/1024);

  fetcher.FetchStart(*test_contents, options, future.GetCallback());
  fake_listener.WaitForBytesRequest();

  EXPECT_TRUE(fake_listener.HasPendingBytesCallback());
  EXPECT_FALSE(IsPdfDone(fetcher));
  EXPECT_FALSE(future.IsReady());

  // Destroying `test_contents` tears down the FrameTree and PDFDocumentHelper
  // inside `~WebContentsImpl()`, which drops the PDF extraction Mojo callback.
  // The drop handler posts a task to abort the PDF extraction. By the time the
  // posted task runs, the WebContents has been destroyed, and
  // `PageContextFetcher` must return `kWebContentsWentAway` instead of a valid
  // `FetchPageContextResult`.
  test_contents.reset();

  ASSERT_TRUE(future.Wait());
  auto result = future.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().error_code,
            FetchPageContextError::kWebContentsWentAway);
}

#endif  // BUILDFLAG(ENABLE_PDF)

// Tests `PageContextFetcher` with a fake `FetchPdfContentCallback`. Not guarded
// by `ENABLE_PDF`, since the callback can be implemented without the PDF
// viewer.
class PageContextFetcherPdfCallbackTest
    : public content::RenderViewHostTestHarness {
 public:
  PageContextFetcherPdfCallbackTest()
      : content::RenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~PageContextFetcherPdfCallbackTest() override = default;

 protected:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    content::NavigationSimulator::NavigateAndCommitFromBrowser(
        web_contents(), GURL("https://example.com/doc.pdf"));
    options_.pdf_options.emplace(PdfOptions::Format::kBytes,
                                 /*size_limit=*/4096u);
  }

  FetchPageContextOptions options_;
};

TEST_F(PageContextFetcherPdfCallbackTest, FetchPdfContentCallbackSuccess) {
  std::vector<uint8_t> expected_bytes = {'%', 'P', 'D', 'F',
                                         '-', '1', '.', '7'};
  url::Origin expected_origin =
      url::Origin::Create(GURL("https://example.com"));

  FetchPdfContentCallback pdf_callback = base::BindRepeating(
      [](url::Origin origin, std::vector<uint8_t> bytes,
         content::WebContents& web_contents, const PdfOptions& options,
         FetchPdfContentResultCallback callback) {
        EXPECT_EQ(options.format(), PdfOptions::Format::kBytes);
        EXPECT_EQ(options.size_limit(), 4096u);
        std::move(callback).Run(PdfResult(std::move(origin), std::move(bytes)));
      },
      expected_origin, expected_bytes);

  PageContextFetcher fetcher(
      /*get_screenshot_service_callback=*/base::NullCallback(),
      /*progress_listener=*/nullptr, std::move(pdf_callback));

  base::test::TestFuture<FetchPageContextResultCallbackArg> future;
  fetcher.FetchStart(*web_contents(), options_, future.GetCallback());

  FetchPageContextResultCallbackArg result_arg = future.Take();
  ASSERT_TRUE(result_arg.has_value());
  ASSERT_TRUE(result_arg.value()->pdf_result.has_value());
  EXPECT_EQ(result_arg.value()->pdf_result->origin, expected_origin);
  EXPECT_FALSE(result_arg.value()->pdf_result->size_exceeded);
  const auto* bytes =
      std::get_if<std::vector<uint8_t>>(&result_arg.value()->pdf_result->data);
  ASSERT_TRUE(bytes);
  EXPECT_EQ(*bytes, expected_bytes);
}

TEST_F(PageContextFetcherPdfCallbackTest, FetchPdfContentNullCallback) {
  PageContextFetcher fetcher(
      /*get_screenshot_service_callback=*/base::NullCallback(),
      /*progress_listener=*/nullptr,
      /*fetch_pdf_content_callback=*/base::NullCallback());

  base::test::TestFuture<FetchPageContextResultCallbackArg> future;
  fetcher.FetchStart(*web_contents(), options_, future.GetCallback());

  FetchPageContextResultCallbackArg result_arg = future.Take();
  ASSERT_TRUE(result_arg.has_value());
  EXPECT_FALSE(result_arg.value()->pdf_result.has_value());
}

// A synchronous null result completes the fetch without waiting for the PDF
// extraction timeout.
TEST_F(PageContextFetcherPdfCallbackTest, FetchPdfContentCallbackSyncNull) {
  PageContextFetcher fetcher(
      /*get_screenshot_service_callback=*/base::NullCallback(),
      /*progress_listener=*/nullptr,
      base::BindRepeating([](content::WebContents& web_contents,
                             const PdfOptions& options,
                             FetchPdfContentResultCallback callback) {
        std::move(callback).Run(std::nullopt);
      }));

  base::test::TestFuture<FetchPageContextResultCallbackArg> future;
  fetcher.FetchStart(*web_contents(), options_, future.GetCallback());

  ASSERT_TRUE(future.IsReady());
  FetchPageContextResultCallbackArg result_arg = future.Take();
  ASSERT_TRUE(result_arg.has_value());
  EXPECT_FALSE(result_arg.value()->pdf_result.has_value());
}

// If the callback never replies, the fetch completes without a PDF result after
// the timeout, and the result callback is cancelled so a late result is
// ignored.
TEST_F(PageContextFetcherPdfCallbackTest, FetchPdfContentCallbackTimeout) {
  FetchPdfContentResultCallback pending_callback;
  PageContextFetcher fetcher(
      /*get_screenshot_service_callback=*/base::NullCallback(),
      /*progress_listener=*/nullptr,
      base::BindLambdaForTesting([&](content::WebContents& web_contents,
                                     const PdfOptions& options,
                                     FetchPdfContentResultCallback callback) {
        pending_callback = std::move(callback);
      }));

  base::test::TestFuture<FetchPageContextResultCallbackArg> future;
  fetcher.FetchStart(*web_contents(), options_, future.GetCallback());
  ASSERT_TRUE(pending_callback);
  EXPECT_FALSE(pending_callback.IsCancelled());
  EXPECT_FALSE(future.IsReady());

  // Advance clock to reach timeout.
  task_environment()->FastForwardBy(kPdfExtractionTimeout.Get());

  ASSERT_TRUE(future.IsReady());
  FetchPageContextResultCallbackArg result_arg = future.Take();
  ASSERT_TRUE(result_arg.has_value());
  EXPECT_FALSE(result_arg.value()->pdf_result.has_value());

  // Late result arriving after timeout should be ignored.
  EXPECT_TRUE(pending_callback.IsCancelled());
  std::move(pending_callback)
      .Run(PdfResult(url::Origin::Create(GURL("https://example.com"))));
}

}  // namespace page_content_annotations
