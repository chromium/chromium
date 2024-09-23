// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/renderer/paint_preview_recorder_impl.h"

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "components/paint_preview/common/file_stream.h"
#include "components/paint_preview/common/mojom/paint_preview_recorder.mojom.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/test/render_view_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/public/web/web_testing_support.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "ui/native_theme/native_theme_features.h"

namespace paint_preview {

namespace {

constexpr char kCompositeAfterPaint[] = "CompositeAfterPaint";

// Checks that |status| == |expected_status| and loads |response| into
// |out_response| if |expected_status| == kOk. If |expected_status| != kOk
// |out_response| can safely be nullptr.
void OnCaptureFinished(mojom::PaintPreviewStatus expected_status,
                       mojom::PaintPreviewCaptureResponsePtr* out_response,
                       mojom::PaintPreviewStatus status,
                       mojom::PaintPreviewCaptureResponsePtr response) {
  EXPECT_EQ(status, expected_status);
  if (expected_status == mojom::PaintPreviewStatus::kOk)
    *out_response = std::move(response);
}

std::string CompositeAfterPaintToString(
    const ::testing::TestParamInfo<bool>& cap_enabled) {
  if (cap_enabled.param) {
    return "WithCompositeAfterPaint";
  }
  return "NoCompositeAfterPaint";
}

}  // namespace

class PaintPreviewRecorderRenderViewTest
    : public content::RenderViewTest,
      public ::testing::WithParamInterface<bool> {
 public:
  PaintPreviewRecorderRenderViewTest() {
    std::vector<base::test::FeatureRef> enabled;
    // TODO(crbug.com/40106592): This is required to bypass a seemingly
    // unrelated DCHECK for |use_overlay_scrollbars_| in NativeThemeAura on
    // ChromeOS when painting scrollbars when first calling LoadHTML().
    feature_list_.InitAndDisableFeature(features::kOverlayScrollbar);
    blink::WebTestingSupport::SaveRuntimeFeatures();
    blink::WebRuntimeFeatures::EnableFeatureFromString(kCompositeAfterPaint,
                                                       GetParam());
  }

  ~PaintPreviewRecorderRenderViewTest() override {
    // Restore blink runtime features to their original values.
    blink::WebTestingSupport::ResetRuntimeFeatures();
  }

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    RenderViewTest::SetUp();
    // Hide scrollbars so that they don't affect the bitmap color results.
    web_view_->GetSettings()->SetHideScrollbars(true);
  }

  base::FilePath MakeTestFilePath(const std::string& filename) {
    return temp_dir_.GetPath().AppendASCII(filename);
  }

  base::FilePath RunCapture(content::RenderFrame* frame,
                            mojom::PaintPreviewCaptureResponsePtr* out_response,
                            bool is_main_frame = true,
                            gfx::Rect clip_rect = gfx::Rect()) {
    base::FilePath skp_path = MakeTestFilePath("test.skp");

    mojom::PaintPreviewCaptureParamsPtr params =
        mojom::PaintPreviewCaptureParams::New();
    auto token = base::UnguessableToken::Create();
    params->guid = token;
    params->clip_rect = clip_rect;
    params->clip_rect_is_hint = false;
    params->is_main_frame = is_main_frame;
    params->capture_links = true;
    base::File skp_file(
        skp_path, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
    params->file = std::move(skp_file);

    PaintPreviewRecorderImpl paint_preview_recorder(frame);
    paint_preview_recorder.CapturePaintPreview(
        std::move(params),
        base::BindOnce(&OnCaptureFinished, mojom::PaintPreviewStatus::kOk,
                       out_response));
    content::RunAllTasksUntilIdle();
    return skp_path;
  }

 private:
  base::ScopedTempDir temp_dir_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(PaintPreviewRecorderRenderViewTest, TestCaptureMainFrameAndClipping) {
  LoadHTML(
      "<!doctype html>"
      "<body>"
      "  <div style='width: 600px; height: 80vh; "
      "              background-color: #ff0000'>&nbsp;</div>"
      "  <a style='display:inline-block' href='http://www.google.com'>Foo</a>"
      "  <div style='width: 100px; height: 600px; "
      "              background-color: #000000'>&nbsp;</div>"
      "  <div style='overflow: hidden; width: 100px; height: 100px;"
      "              background: orange;'>"
      "    <div style='width: 500px; height: 500px;"
      "                background: yellow;'></div>"
      "  </div>"
      "</body>");

  auto out_response = mojom::PaintPreviewCaptureResponse::New();
  content::RenderFrame* frame = GetMainRenderFrame();
  base::FilePath skp_path = RunCapture(frame, &out_response);

  EXPECT_TRUE(out_response->embedding_token.has_value());
  EXPECT_EQ(frame->GetWebFrame()->GetEmbeddingToken(),
            out_response->embedding_token.value());
  EXPECT_EQ(out_response->content_id_to_embedding_token.size(), 0U);

  EXPECT_EQ(out_response->links.size(), 1U);
  EXPECT_EQ(out_response->links[0]->url, GURL("http://www.google.com/"));
  // Relaxed checks on dimensions and no checks on positions. This is not
  // intended to test the rendering behavior of the page only that a link
  // was captured and has a bounding box.
  EXPECT_GT(out_response->links[0]->rect.width(), 0);
  EXPECT_GT(out_response->links[0]->rect.height(), 0);

  sk_sp<SkPicture> pic;
  {
    base::ScopedAllowBlockingForTesting scope;
    FileRStream rstream(base::File(
        skp_path, base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_READ));
    pic = SkPicture::MakeFromStream(&rstream, nullptr);
  }
  // The min page height is the sum of the three top level divs of 800. The min
  // width is that of the widest div at 600.
  EXPECT_GE(pic->cullRect().height(), 800);
  EXPECT_GE(pic->cullRect().width(), 600);
  SkBitmap bitmap;
  ASSERT_TRUE(bitmap.tryAllocN32Pixels(pic->cullRect().width(),
                                       pic->cullRect().height()));
  SkCanvas canvas(bitmap, SkSurfaceProps{});
  canvas.drawPicture(pic);
  // This should be inside the top right corner of the first top level div.
  // Success means there was no horizontal clipping as this region is red,
  // matching the div.
  EXPECT_EQ(bitmap.getColor(600, 50), 0xFFFF0000U);
  // This should be inside the bottom of the second top level div. Success means
  // there was no vertical clipping as this region is black matching the div. If
  // the yellow div within the orange div overflowed then this would be yellow
  // and fail.
  EXPECT_EQ(bitmap.getColor(50, pic->cullRect().height() - 150), 0xFF000000U);
  // This should be for the white background in the bottom right. This checks
  // that the background is not clipped.
  EXPECT_EQ(bitmap.getColor(pic->cullRect().width() - 50,
                            pic->cullRect().height() - 50),
            0xFFFFFFFFU);
}

TEST_P(PaintPreviewRecorderRenderViewTest, TestCaptureMainFrameWithScroll) {
  LoadHTML(
      "<!doctype html>"
      "<body>"
      "  <div style='width: 600px; height: 200px; "
      "              background-color: #ff0000'>&nbsp;</div>"
      "  <div style='width: 600px; height: 5000px; "
      "              background-color: #00ff00'>&nbsp;</div>"
      "</body>");

  // Scroll to bottom of page to ensure scroll position has no effect on
  // capture.
  ExecuteJavaScriptForTests("window.scrollTo(0,document.body.scrollHeight);");
  content::RunAllTasksUntilIdle();

  auto out_response = mojom::PaintPreviewCaptureResponse::New();
  content::RenderFrame* frame = GetMainRenderFrame();
  base::FilePath skp_path = RunCapture(frame, &out_response);

  EXPECT_TRUE(out_response->embedding_token.has_value());
  EXPECT_EQ(frame->GetWebFrame()->GetEmbeddingToken(),
            out_response->embedding_token.value());
  EXPECT_EQ(out_response->content_id_to_embedding_token.size(), 0U);

  // Relaxed checks on dimensions and no checks on positions. This is not
  // intended to intensively test the rendering behavior of the page.
  sk_sp<SkPicture> pic;
  {
    base::ScopedAllowBlockingForTesting scope;
    FileRStream rstream(base::File(
        skp_path, base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_READ));
    pic = SkPicture::MakeFromStream(&rstream, nullptr);
  }
  SkBitmap bitmap;
  ASSERT_TRUE(bitmap.tryAllocN32Pixels(pic->cullRect().width(),
                                       pic->cullRect().height()));
  SkCanvas canvas(bitmap, SkSurfaceProps{});
  canvas.drawPicture(pic);
  // This should be inside the top right corner of the top div. Success means
  // there was no horizontal or vertical clipping as this region is red,
  // matching the div.
  EXPECT_EQ(bitmap.getColor(600, 50), 0xFFFF0000U);
  // This should be inside the bottom of the bottom div. Success means there was
  // no vertical clipping as this region is blue matching the div.
  EXPECT_EQ(bitmap.getColor(50, pic->cullRect().height() - 100), 0xFF00FF00U);
}

TEST_P(PaintPreviewRecorderRenderViewTest,
       TestCaptureMainFrameAboutScrollPosition) {
  LoadHTML(
      "<!doctype html>"
      "<body>"
      "  <div style='width: 600px; height: 200px; "
      "              background-color: #ff0000'>&nbsp;</div>"
      "  <div style='width: 600px; height: 5000px; "
      "              background-color: #00ff00'>&nbsp;</div>"
      "</body>");

  // Scroll to bottom of page to ensure scroll position has no effect on
  // capture.
  ExecuteJavaScriptForTests("window.scrollTo(0,document.body.scrollHeight);");
  content::RunAllTasksUntilIdle();

  auto out_response = mojom::PaintPreviewCaptureResponse::New();
  content::RenderFrame* frame = GetMainRenderFrame();
  base::FilePath skp_path =
      RunCapture(frame, &out_response, true, gfx::Rect(-1, -1, 500, 500));

  EXPECT_TRUE(out_response->embedding_token.has_value());
  EXPECT_EQ(frame->GetWebFrame()->GetEmbeddingToken(),
            out_response->embedding_token.value());
  EXPECT_EQ(out_response->content_id_to_embedding_token.size(), 0U);

  // Scroll offset should be within the [0, 500] bounds.
  EXPECT_GT(out_response->scroll_offsets.y(), 0);
  EXPECT_LT(out_response->scroll_offsets.y(), 500);

  // Frame offset should be > 0 in this case.
  EXPECT_GT(out_response->frame_offsets.y(), 0);

  // Relaxed checks on dimensions and no checks on positions. This is not
  // intended to intensively test the rendering behavior of the page.
  sk_sp<SkPicture> pic;
  {
    base::ScopedAllowBlockingForTesting scope;
    FileRStream rstream(base::File(
        skp_path, base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_READ));
    pic = SkPicture::MakeFromStream(&rstream, nullptr);
  }
  SkBitmap bitmap;
  ASSERT_TRUE(bitmap.tryAllocN32Pixels(pic->cullRect().width(),
                                       pic->cullRect().height()));
  SkCanvas canvas(bitmap, SkSurfaceProps{});
  canvas.drawPicture(pic);
  EXPECT_EQ(bitmap.getColor(50, 10), 0xFF00FF00U);
  EXPECT_EQ(bitmap.getColor(50, pic->cullRect().height() - 10), 0xFF00FF00U);
}

TEST_P(PaintPreviewRecorderRenderViewTest,
       TestCaptureMainFrameAboutScrollPositionClampedToEdge) {
  LoadHTML(
      "<!doctype html>"
      "<body>"
      "  <div style='width: 600px; height: 200px; "
      "              background-color: #ff0000'>&nbsp;</div>"
      "  <div style='width: 600px; height: 5000px; "
      "              background-color: #00ff00'>&nbsp;</div>"
      "</body>");

  // Scroll to bottom of page to ensure scroll position has no effect on
  // capture.
  ExecuteJavaScriptForTests("window.scrollTo(0,document.body.scrollHeight);");
  content::RunAllTasksUntilIdle();

  auto out_response = mojom::PaintPreviewCaptureResponse::New();
  content::RenderFrame* frame = GetMainRenderFrame();
  base::FilePath skp_path =
      RunCapture(frame, &out_response, true, gfx::Rect(-1, -1, 500, 2000));

  EXPECT_TRUE(out_response->embedding_token.has_value());
  EXPECT_EQ(frame->GetWebFrame()->GetEmbeddingToken(),
            out_response->embedding_token.value());
  EXPECT_EQ(out_response->content_id_to_embedding_token.size(), 0U);

  // Scroll offset should be within the [0, 2000] bounds and closer to the
  // bottom as it was clamped.
  EXPECT_GT(out_response->scroll_offsets.y(), 1100);
  EXPECT_LT(out_response->scroll_offsets.y(), 2000);

  // Frame offset should be > 0 in this case.
  EXPECT_GT(out_response->frame_offsets.y(), 0);

  // Relaxed checks on dimensions and no checks on positions. This is not
  // intended to intensively test the rendering behavior of the page.
  sk_sp<SkPicture> pic;
  {
    base::ScopedAllowBlockingForTesting scope;
    FileRStream rstream(base::File(
        skp_path, base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_READ));
    pic = SkPicture::MakeFromStream(&rstream, nullptr);
  }
  SkBitmap bitmap;
  EXPECT_EQ(pic->cullRect().width(), 500);
  EXPECT_EQ(pic->cullRect().height(), 2000);
  ASSERT_TRUE(bitmap.tryAllocN32Pixels(pic->cullRect().width(),
                                       pic->cullRect().height()));
  SkCanvas canvas(bitmap, SkSurfaceProps{});
  canvas.drawPicture(pic);
  EXPECT_EQ(bitmap.getColor(50, 10), 0xFF00FF00U);
  EXPECT_EQ(bitmap.getColor(50, pic->cullRect().height() - 10), 0xFF00FF00U);
}

TEST_P(PaintPreviewRecorderRenderViewTest,
       TestCaptureMainFrameIgnoreScrollPosition) {
  LoadHTML(
      "<!doctype html>"
      "<body>"
      "  <div style='width: 600px; height: 200px; "
      "              background-color: #ff0000'>&nbsp;</div>"
      "  <div style='width: 600px; height: 5000px; "
      "              background-color: #00ff00'>&nbsp;</div>"
      "</body>");

  // Scroll to bottom of page to ensure scroll position has no effect on
  // capture.
  ExecuteJavaScriptForTests("window.scrollTo(0,document.body.scrollHeight);");
  content::RunAllTasksUntilIdle();

  auto out_response = mojom::PaintPreviewCaptureResponse::New();
  content::RenderFrame* frame = GetMainRenderFrame();
  base::FilePath skp_path =
      RunCapture(frame, &out_response, true, gfx::Rect(-1, -1, 0, 0));

  EXPECT_TRUE(out_response->embedding_token.has_value());
  EXPECT_EQ(frame->GetWebFrame()->GetEmbeddingToken(),
            out_response->embedding_token.value());
  EXPECT_EQ(out_response->content_id_to_embedding_token.size(), 0U);

  EXPECT_GT(out_response->scroll_offsets.y(), 0);

  // Frame offset should be 0 in this case.
  EXPECT_EQ(out_response->frame_offsets.x(), 0);
  EXPECT_EQ(out_response->frame_offsets.y(), 0);

  // Relaxed checks on dimensions and no checks on positions. This is not
  // intended to intensively test the rendering behavior of the page.
  sk_sp<SkPicture> pic;
  {
    base::ScopedAllowBlockingForTesting scope;
    FileRStream rstream(base::File(
        skp_path, base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_READ));
    pic = SkPicture::MakeFromStream(&rstream, nullptr);
  }
  SkBitmap bitmap;
  ASSERT_TRUE(bitmap.tryAllocN32Pixels(pic->cullRect().width(),
                                       pic->cullRect().height()));
  SkCanvas canvas(bitmap, SkSurfaceProps{});
  canvas.drawPicture(pic);
  EXPECT_EQ(bitmap.getColor(600, 50), 0xFFFF0000U);
  EXPECT_EQ(bitmap.getColor(50, pic->cullRect().height() - 100), 0xFF00FF00U);
}

TEST_P(PaintPreviewRecorderRenderViewTest, TestCaptureFragment) {
  // Use position absolute position to check that the captured link dimensions
  // match what is specified.
  LoadHTML(
      "<!doctype html>"
      "<body style='min-height:1000px;'>"
      "  <a style='position: absolute; left: -15px; top: 0px; width: 40px; "
      "   height: 30px;' href='#fragment'>Foo</a>"
      "  <h1 id='fragment'>I'm a fragment</h1>"
      "</body>");
  auto out_response = mojom::PaintPreviewCaptureResponse::New();
  content::RenderFrame* frame = GetMainRenderFrame();

  RunCapture(frame, &out_response);

  EXPECT_TRUE(out_response->embedding_token.has_value());
  EXPECT_EQ(frame->GetWebFrame()->GetEmbeddingToken(),
            out_response->embedding_token.value());
  EXPECT_EQ(out_response->content_id_to_embedding_token.size(), 0U);

  EXPECT_EQ(out_response->links.size(), 1U);
  EXPECT_EQ(out_response->links[0]->url, GURL("fragment"));
  EXPECT_EQ(out_response->links[0]->rect.x(), -15);
  EXPECT_EQ(out_response->links[0]->rect.y(), 0);
  EXPECT_EQ(out_response->links[0]->rect.width(), 40);
  EXPECT_EQ(out_response->links[0]->rect.height(), 30);
}

TEST_P(PaintPreviewRecorderRenderViewTest, TestCaptureInvalidFile) {
  LoadHTML("<body></body>");

  mojom::PaintPreviewCaptureParamsPtr params =
      mojom::PaintPreviewCaptureParams::New();
  auto token = base::UnguessableToken::Create();
  params->guid = token;
  params->clip_rect = gfx::Rect();
  params->is_main_frame = true;
  params->capture_links = true;
  params->max_capture_size = 0;
  base::File skp_file;  // Invalid file.
  params->file = std::move(skp_file);

  content::RenderFrame* frame = GetMainRenderFrame();
  PaintPreviewRecorderImpl paint_preview_recorder(frame);
  paint_preview_recorder.CapturePaintPreview(
      std::move(params),
      base::BindOnce(&OnCaptureFinished,
                     mojom::PaintPreviewStatus::kCaptureFailed, nullptr));
  content::RunAllTasksUntilIdle();
}

TEST_P(PaintPreviewRecorderRenderViewTest, TestCaptureInvalidXYClip) {
  LoadHTML("<body></body>");

  mojom::PaintPreviewCaptureParamsPtr params =
      mojom::PaintPreviewCaptureParams::New();
  auto token = base::UnguessableToken::Create();
  params->guid = token;
  params->clip_rect = gfx::Rect(1000000, 1000000, 10, 10);
  params->is_main_frame = true;
  params->capture_links = true;
  params->max_capture_size = 0;
  base::FilePath skp_path = MakeTestFilePath("test.skp");
  base::File skp_file(skp_path,
                      base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  params->file = std::move(skp_file);

  content::RenderFrame* frame = GetMainRenderFrame();
  PaintPreviewRecorderImpl paint_preview_recorder(frame);
  paint_preview_recorder.CapturePaintPreview(
      std::move(params),
      base::BindOnce(&OnCaptureFinished,
                     mojom::PaintPreviewStatus::kCaptureFailed, nullptr));
  content::RunAllTasksUntilIdle();
}

TEST_P(PaintPreviewRecorderRenderViewTest, TestCaptureMainFrameAndLocalFrame) {
  LoadHTML(
      "<!doctype html>"
      "<body style='min-height:1000px;'>"
      "  <iframe style='width: 500px, height: 500px'"
      "          srcdoc=\"<div style='width: 100px; height: 100px;"
      "          background-color: #000000'>&nbsp;</div>\"></iframe>"
      "</body>");
  auto out_response = mojom::PaintPreviewCaptureResponse::New();
  content::RenderFrame* frame = GetMainRenderFrame();

  RunCapture(frame, &out_response);

  EXPECT_TRUE(out_response->embedding_token.has_value());
  EXPECT_EQ(frame->GetWebFrame()->GetEmbeddingToken(),
            out_response->embedding_token.value());
  EXPECT_EQ(out_response->content_id_to_embedding_token.size(), 0U);
}

TEST_P(PaintPreviewRecorderRenderViewTest, TestCaptureLocalFrame) {
  LoadHTML(
      "<!doctype html>"
      "<body style='min-height:1000px;'>"
      "  <iframe style='width: 500px; height: 500px'"
      "          srcdoc=\"<div style='width: 100px; height: 100px;"
      "          background-color: #000000'>&nbsp;</div>\"></iframe>"
      "</body>");
  auto out_response = mojom::PaintPreviewCaptureResponse::New();
  auto* child_frame = content::RenderFrame::FromWebFrame(
      GetMainRenderFrame()->GetWebFrame()->FirstChild()->ToWebLocalFrame());
  ASSERT_TRUE(child_frame);

  RunCapture(child_frame, &out_response, false);

  EXPECT_TRUE(out_response->embedding_token.has_value());
  EXPECT_EQ(out_response->content_id_to_embedding_token.size(), 0U);
}

TEST_P(PaintPreviewRecorderRenderViewTest, TestCaptureUnclippedLocalFrame) {
  LoadHTML(
      "<!doctype html>"
      "<body style='min-height:1000px;'>"
      "  <iframe style='width: 500px; height: 500px'"
      "          srcdoc=\"<div style='width: 500px; height: 100px;"
      "          background-color: #00FF00'>&nbsp;</div>"
      "          <div style='width: 500px; height: 900px;"
      "          background-color: #FF0000'>&nbsp;</div>\"></iframe>"
      "</body>");
  auto out_response = mojom::PaintPreviewCaptureResponse::New();
  auto* child_web_frame =
      GetMainRenderFrame()->GetWebFrame()->FirstChild()->ToWebLocalFrame();
  auto* child_frame = content::RenderFrame::FromWebFrame(child_web_frame);
  ASSERT_TRUE(child_frame);

  child_web_frame->SetScrollOffset(gfx::PointF(0, 400));

  base::FilePath skp_path = RunCapture(child_frame, &out_response, false);

  EXPECT_TRUE(out_response->embedding_token.has_value());
  EXPECT_EQ(out_response->content_id_to_embedding_token.size(), 0U);

  sk_sp<SkPicture> pic;
  {
    base::ScopedAllowBlockingForTesting scope;
    FileRStream rstream(base::File(
        skp_path, base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_READ));
    pic = SkPicture::MakeFromStream(&rstream, nullptr);
  }
  EXPECT_EQ(pic->cullRect().width(), child_web_frame->DocumentSize().width());
  EXPECT_EQ(pic->cullRect().height(), child_web_frame->DocumentSize().height());

  SkBitmap bitmap;
  ASSERT_TRUE(bitmap.tryAllocN32Pixels(pic->cullRect().width(),
                                       pic->cullRect().height()));
  SkCanvas canvas(bitmap);
  canvas.drawPicture(pic);
  EXPECT_EQ(bitmap.getColor(50, 50), 0xFF00FF00U);
  EXPECT_EQ(bitmap.getColor(50, 800), 0xFFFF0000U);
}

TEST_P(PaintPreviewRecorderRenderViewTest, TestCaptureCustomClipRect) {
  LoadHTML(
      "<!doctype html>"
      "<body>"
      "  <div style='width: 600px; height: 600px; background-color: #0000ff;'>"
      "     <div style='width: 300px; height: 300px; background-color: "
      "          #ffff00; position: relative; left: 150px; top: 150px'></div>"
      "  </div>"
      "  <a style='position: absolute; left: 160px; top: 170px; width: 40px; "
      "   height: 30px;' href='http://www.example.com'>Foo</a>"
      "</body>");

  auto out_response = mojom::PaintPreviewCaptureResponse::New();
  content::RenderFrame* frame = GetMainRenderFrame();
  gfx::Rect clip_rect = gfx::Rect(150, 150, 300, 300);
  base::FilePath skp_path = RunCapture(frame, &out_response, true, clip_rect);

  EXPECT_TRUE(out_response->embedding_token.has_value());
  EXPECT_EQ(frame->GetWebFrame()->GetEmbeddingToken(),
            out_response->embedding_token.value());
  EXPECT_EQ(out_response->content_id_to_embedding_token.size(), 0U);

  sk_sp<SkPicture> pic;
  {
    base::ScopedAllowBlockingForTesting scope;
    FileRStream rstream(base::File(
        skp_path, base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_READ));
    pic = SkPicture::MakeFromStream(&rstream, nullptr);
  }
  EXPECT_EQ(pic->cullRect().height(), 300);
  EXPECT_EQ(pic->cullRect().width(), 300);
  SkBitmap bitmap;
  ASSERT_TRUE(bitmap.tryAllocN32Pixels(pic->cullRect().width(),
                                       pic->cullRect().height()));
  SkCanvas canvas(bitmap);
  canvas.drawPicture(pic);
  EXPECT_EQ(bitmap.getColor(100, 100), 0xFFFFFF00U);

  ASSERT_EQ(out_response->links.size(), 1U);
  EXPECT_EQ(out_response->links[0]->url, GURL("http://www.example.com"));
  EXPECT_EQ(out_response->links[0]->rect.x(), 10);
  EXPECT_EQ(out_response->links[0]->rect.y(), 20);
  EXPECT_EQ(out_response->links[0]->rect.width(), 40);
  EXPECT_EQ(out_response->links[0]->rect.height(), 30);
}

TEST_P(PaintPreviewRecorderRenderViewTest, TestCaptureWithClamp) {
  LoadHTML(
      "<!doctype html>"
      "<body>"
      "  <div style='width: 600px; height: 600px; background-color: #0000ff;'>"
      "     <div style='width: 300px; height: 300px; background-color: "
      "          #ffff00; position: relative; left: 150px; top: 150px'></div>"
      "  </div>"
      "  <a style='position: absolute; left: 160px; top: 170px; width: 40px; "
      "   height: 30px;' href='http://www.example.com'>Foo</a>"
      "</body>");

  auto out_response = mojom::PaintPreviewCaptureResponse::New();
  content::RenderFrame* frame = GetMainRenderFrame();
  const size_t kLarge = 1000000;
  gfx::Rect clip_rect = gfx::Rect(0, 0, kLarge, kLarge);
  base::FilePath skp_path = RunCapture(frame, &out_response, true, clip_rect);

  EXPECT_TRUE(out_response->embedding_token.has_value());
  EXPECT_EQ(frame->GetWebFrame()->GetEmbeddingToken(),
            out_response->embedding_token.value());
  EXPECT_EQ(out_response->content_id_to_embedding_token.size(), 0U);

  sk_sp<SkPicture> pic;
  {
    base::ScopedAllowBlockingForTesting scope;
    FileRStream rstream(base::File(
        skp_path, base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_READ));
    pic = SkPicture::MakeFromStream(&rstream, nullptr);
  }
  EXPECT_LT(pic->cullRect().height(), kLarge);
  EXPECT_LT(pic->cullRect().width(), kLarge);
}

TEST_P(PaintPreviewRecorderRenderViewTest, TestCaptureFullIfWidthHeightAre0) {
  LoadHTML(
      "<!doctype html>"
      "<body>"
      "  <div style='width: 600px; height: 600px; background-color: #0000ff;'>"
      "     <div style='width: 300px; height: 300px; background-color: "
      "          #ffff00; position: relative; left: 150px; top: 150px'></div>"
      "  </div>"
      "  <a style='position: absolute; left: 160px; top: 170px; width: 40px; "
      "   height: 30px;' href='http://www.example.com'>Foo</a>"
      "</body>");

  auto out_response = mojom::PaintPreviewCaptureResponse::New();
  content::RenderFrame* frame = GetMainRenderFrame();
  gfx::Rect clip_rect = gfx::Rect(1, 1, 0, 0);
  base::FilePath skp_path = RunCapture(frame, &out_response, true, clip_rect);

  EXPECT_TRUE(out_response->embedding_token.has_value());
  EXPECT_EQ(frame->GetWebFrame()->GetEmbeddingToken(),
            out_response->embedding_token.value());
  EXPECT_EQ(out_response->content_id_to_embedding_token.size(), 0U);

  sk_sp<SkPicture> pic;
  {
    base::ScopedAllowBlockingForTesting scope;
    FileRStream rstream(base::File(
        skp_path, base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_READ));
    pic = SkPicture::MakeFromStream(&rstream, nullptr);
  }
  EXPECT_GT(pic->cullRect().height(), 0U);
  EXPECT_GT(pic->cullRect().width(), 0U);
}

TEST_P(PaintPreviewRecorderRenderViewTest, CaptureWithTranslate) {
  // URLs should be annotated correctly when a CSS transform is applied.
  LoadHTML(
      R"(
      <!doctype html>
      <body>
      <div style="display: inline-block;
                  padding: 16px;
                  font-size: 16px;">
        <div style="padding: 16px;
                    transform: translate(10px, 20px);
                    margin-bottom: 30px;">
          <div>
            <a href="http://www.example.com" style="display: block;
                                                    width: 70px;
                                                    height: 20px;">
              <div>Example</div>
            </a>
          </div>
        </div>
      </div>
    </body>)");
  auto out_response = mojom::PaintPreviewCaptureResponse::New();
  content::RenderFrame* frame = GetMainRenderFrame();

  RunCapture(frame, &out_response);

  EXPECT_TRUE(out_response->embedding_token.has_value());
  EXPECT_EQ(frame->GetWebFrame()->GetEmbeddingToken(),
            out_response->embedding_token.value());
  EXPECT_EQ(out_response->content_id_to_embedding_token.size(), 0U);

  ASSERT_EQ(out_response->links.size(), 1U);
  EXPECT_EQ(out_response->links[0]->url, GURL("http://www.example.com"));
  EXPECT_NEAR(out_response->links[0]->rect.x(), 50, 3);
  EXPECT_NEAR(out_response->links[0]->rect.y(), 60, 3);
  EXPECT_NEAR(out_response->links[0]->rect.width(), 70, 3);
  EXPECT_NEAR(out_response->links[0]->rect.height(), 20, 3);
}

TEST_P(PaintPreviewRecorderRenderViewTest, CaptureWithTranslateThenRotate) {
  // URLs should be annotated correctly when a CSS transform is applied.
  LoadHTML(
      R"(
      <!doctype html>
      <body>
      <div style="display: inline-block;
                  padding: 16px;
                  font-size: 16px;">
        <div style="padding: 16px;
                    transform: translate(100px, 0) rotate(45deg);
                    margin-bottom: 30px;">
          <div>
            <a href="http://www.example.com" style="display: block;
                                                    width: 70px;
                                                    height: 20px;">
              <div>Example</div>
            </a>
          </div>
        </div>
      </div>
    </body>)");
  auto out_response = mojom::PaintPreviewCaptureResponse::New();
  content::RenderFrame* frame = GetMainRenderFrame();

  RunCapture(frame, &out_response);

  EXPECT_TRUE(out_response->embedding_token.has_value());
  EXPECT_EQ(frame->GetWebFrame()->GetEmbeddingToken(),
            out_response->embedding_token.value());
  EXPECT_EQ(out_response->content_id_to_embedding_token.size(), 0U);

  ASSERT_EQ(out_response->links.size(), 1U);
  EXPECT_EQ(out_response->links[0]->url, GURL("http://www.example.com"));
  EXPECT_NEAR(out_response->links[0]->rect.x(), 141, 5);
  EXPECT_NEAR(out_response->links[0]->rect.y(), 18, 5);
#if !BUILDFLAG(IS_ANDROID)
  EXPECT_NEAR(out_response->links[0]->rect.width(), 58, 10);
  EXPECT_NEAR(out_response->links[0]->rect.height(), 58, 10);
#endif
}

TEST_P(PaintPreviewRecorderRenderViewTest, CaptureWithRotateThenTranslate) {
  // URLs should be annotated correctly when a CSS transform is applied.
  LoadHTML(
      R"(
      <!doctype html>
      <body>
      <div style="display: inline-block;
                  padding: 16px;
                  font-size: 16px;">
        <div style="padding: 16px;
                    transform: rotate(45deg) translate(100px, 0);
                    margin-bottom: 30px;">
          <div>
            <a href="http://www.example.com" style="display: block;
                                                    width: 70px;
                                                    height: 20px;">
              <div>Example</div>
            </a>
          </div>
        </div>
      </div>
    </body>)");
  auto out_response = mojom::PaintPreviewCaptureResponse::New();
  content::RenderFrame* frame = GetMainRenderFrame();

  RunCapture(frame, &out_response);

  EXPECT_TRUE(out_response->embedding_token.has_value());
  EXPECT_EQ(frame->GetWebFrame()->GetEmbeddingToken(),
            out_response->embedding_token.value());
  EXPECT_EQ(out_response->content_id_to_embedding_token.size(), 0U);

  ASSERT_EQ(out_response->links.size(), 1U);
  EXPECT_EQ(out_response->links[0]->url, GURL("http://www.example.com"));
  EXPECT_NEAR(out_response->links[0]->rect.x(), 111, 5);
  EXPECT_NEAR(out_response->links[0]->rect.y(), 88, 5);
#if !BUILDFLAG(IS_ANDROID)
  EXPECT_NEAR(out_response->links[0]->rect.width(), 58, 10);
  EXPECT_NEAR(out_response->links[0]->rect.height(), 58, 10);
#endif
}

TEST_P(PaintPreviewRecorderRenderViewTest, CaptureWithScale) {
  // URLs should be annotated correctly when a CSS transform is applied.
  LoadHTML(
      R"(
      <!doctype html>
      <body>
      <div style="display: inline-block;
                  padding: 16px;
                  font-size: 16px;">
        <div style="padding: 16px;
                    transform: scale(2, 1);
                    margin-bottom: 30px;">
          <div>
            <a href="http://www.example.com" style="display: block;
                                                    width: 70px;
                                                    height: 20px;">
              <div>Example</div>
            </a>
          </div>
        </div>
      </div>
    </body>)");
  auto out_response = mojom::PaintPreviewCaptureResponse::New();
  content::RenderFrame* frame = GetMainRenderFrame();

  RunCapture(frame, &out_response);

  EXPECT_TRUE(out_response->embedding_token.has_value());
  EXPECT_EQ(frame->GetWebFrame()->GetEmbeddingToken(),
            out_response->embedding_token.value());
  EXPECT_EQ(out_response->content_id_to_embedding_token.size(), 0U);

  ASSERT_EQ(out_response->links.size(), 1U);
  EXPECT_EQ(out_response->links[0]->url, GURL("http://www.example.com"));
  EXPECT_NEAR(out_response->links[0]->rect.x(), 5, 3);
  EXPECT_NEAR(out_response->links[0]->rect.y(), 40, 3);
  EXPECT_NEAR(out_response->links[0]->rect.width(), 140, 3);
  EXPECT_NEAR(out_response->links[0]->rect.height(), 20, 3);
}

TEST_P(PaintPreviewRecorderRenderViewTest, CaptureSaveRestore) {
  // URLs should be annotated correctly when a CSS transform is applied.
  LoadHTML(
      R"(
      <!doctype html>
      <body>
      <div style="display: inline-block;
                  padding: 16px;
                  font-size: 16px;">
        <div style="padding: 16px;
                    transform: translate(20px, 0);
                    margin-bottom: 30px;">
          <div>
            <a href="http://www.example.com" style="display: block;
                                                    width: 70px;
                                                    height: 20px;">
              <div>Example</div>
            </a>
          </div>
        </div>
        <div style="padding: 16px;
                    transform: none;
                    margin-bottom: 30px;">
          <div>
            <a href="http://www.chromium.org" style="display: block;
                                                     width: 80px;
                                                     height: 20px;">
              <div>Chromium</div>
            </a>
          </div>
        </div>
      </div>
    </body>)");
  auto out_response = mojom::PaintPreviewCaptureResponse::New();
  content::RenderFrame* frame = GetMainRenderFrame();

  RunCapture(frame, &out_response);

  EXPECT_TRUE(out_response->embedding_token.has_value());
  EXPECT_EQ(frame->GetWebFrame()->GetEmbeddingToken(),
            out_response->embedding_token.value());
  EXPECT_EQ(out_response->content_id_to_embedding_token.size(), 0U);

  ASSERT_EQ(out_response->links.size(), 2U);
  EXPECT_EQ(out_response->links[0]->url, GURL("http://www.chromium.org"));
  EXPECT_NEAR(out_response->links[0]->rect.x(), 40, 3);
  EXPECT_NEAR(out_response->links[0]->rect.y(), 122, 3);
  EXPECT_NEAR(out_response->links[0]->rect.width(), 80, 3);
  EXPECT_NEAR(out_response->links[0]->rect.height(), 20, 3);

  EXPECT_EQ(out_response->links[1]->url, GURL("http://www.example.com"));
  EXPECT_NEAR(out_response->links[1]->rect.x(), 60, 3);
  EXPECT_NEAR(out_response->links[1]->rect.y(), 40, 3);
  EXPECT_NEAR(out_response->links[1]->rect.width(), 70, 3);
  EXPECT_NEAR(out_response->links[1]->rect.height(), 20, 3);
}

INSTANTIATE_TEST_SUITE_P(All,
                         PaintPreviewRecorderRenderViewTest,
                         testing::Values(true, false),
                         CompositeAfterPaintToString);

}  // namespace paint_preview
