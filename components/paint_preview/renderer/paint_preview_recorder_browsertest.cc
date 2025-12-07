// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "components/paint_preview/common/file_stream.h"
#include "components/paint_preview/common/mojom/paint_preview_recorder.mojom-data-view.h"
#include "components/paint_preview/common/mojom/paint_preview_recorder.mojom-forward.h"
#include "components/paint_preview/common/mojom/paint_preview_recorder.mojom.h"
#include "components/paint_preview/common/mojom/paint_preview_types.mojom.h"
#include "components/paint_preview/renderer/paint_preview_recorder_impl.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/test/render_view_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/public/web/web_testing_support.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "ui/native_theme/features/native_theme_features.h"

namespace paint_preview {

namespace {

using testing::AllOf;
using testing::Gt;
using testing::Lt;

constexpr char kCompositeAfterPaint[] = "CompositeAfterPaint";

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

  std::tuple<base::FilePath, mojom::PaintPreviewCaptureResponsePtr> RunCapture(
      content::RenderFrame* frame,
      bool is_main_frame = true,
      gfx::Rect clip_rect = gfx::Rect(),
      mojom::ClipCoordOverride clip_x_coord_override =
          mojom::ClipCoordOverride::kNone,
      mojom::ClipCoordOverride clip_y_coord_override =
          mojom::ClipCoordOverride::kNone) {
    base::FilePath skp_path = MakeTestFilePath("test.skp");

    mojom::PaintPreviewCaptureParamsPtr params =
        mojom::PaintPreviewCaptureParams::New();
    auto token = base::UnguessableToken::Create();
    params->guid = token;
    params->geometry_metadata_params = mojom::GeometryMetadataParams::New();
    params->geometry_metadata_params->clip_rect = clip_rect;
    params->geometry_metadata_params->clip_x_coord_override =
        clip_x_coord_override;
    params->geometry_metadata_params->clip_y_coord_override =
        clip_y_coord_override;
    params->geometry_metadata_params->clip_rect_is_hint = false;
    params->is_main_frame = is_main_frame;
    params->capture_links = true;
    base::File skp_file(
        skp_path, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
    params->file = std::move(skp_file);

    PaintPreviewRecorderImpl paint_preview_recorder(frame);
    base::test::TestFuture<base::expected<mojom::PaintPreviewCaptureResponsePtr,
                                          mojom::PaintPreviewStatus>>
        future;
    paint_preview_recorder.CapturePaintPreview(std::move(params),
                                               future.GetCallback());

    EXPECT_THAT(future.Get(), base::test::HasValue());
    return {skp_path, future.Take().value()};
  }

  mojom::GeometryMetadataResponsePtr GetGeometryMetadata(
      content::RenderFrame* frame,
      gfx::Rect clip_rect = gfx::Rect(),
      mojom::ClipCoordOverride clip_x_coord_override =
          mojom::ClipCoordOverride::kNone,
      mojom::ClipCoordOverride clip_y_coord_override =
          mojom::ClipCoordOverride::kNone) {
    auto params = mojom::GeometryMetadataParams::New();
    params->clip_rect = clip_rect;
    params->clip_x_coord_override = clip_x_coord_override;
    params->clip_y_coord_override = clip_y_coord_override;
    params->clip_rect_is_hint = false;

    base::test::TestFuture<mojom::GeometryMetadataResponsePtr> future;
    PaintPreviewRecorderImpl paint_preview_recorder(frame);
    paint_preview_recorder.GetGeometryMetadata(std::move(params),
                                               future.GetCallback());
    return future.Take();
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

  content::RenderFrame* frame = GetMainRenderFrame();
  auto [skp_path, out_response] = RunCapture(frame);

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
  EXPECT_EQ(bitmap.getColor(600, 50), SK_ColorRED);
  // This should be inside the bottom of the second top level div. Success means
  // there was no vertical clipping as this region is black matching the div. If
  // the yellow div within the orange div overflowed then this would be yellow
  // and fail.
  EXPECT_EQ(bitmap.getColor(50, pic->cullRect().height() - 150), SK_ColorBLACK);
  // This should be for the white background in the bottom right. This checks
  // that the background is not clipped.
  EXPECT_EQ(bitmap.getColor(pic->cullRect().width() - 50,
                            pic->cullRect().height() - 50),
            SK_ColorWHITE);
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

  content::RenderFrame* frame = GetMainRenderFrame();
  auto [skp_path, out_response] = RunCapture(frame);

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
  EXPECT_EQ(bitmap.getColor(600, 50), SK_ColorRED);
  // This should be inside the bottom of the bottom div. Success means there was
  // no vertical clipping as this region is blue matching the div.
  EXPECT_EQ(bitmap.getColor(50, pic->cullRect().height() - 100), SK_ColorGREEN);
}

TEST_P(PaintPreviewRecorderRenderViewTest,
       TestCaptureMainFrameAboutScrollPosition) {
  LoadHTML(
      "<!doctype html>"
      "<body>"
      "  <div style='width: 600px; height: 200px; "
      "              background-color: #ff0000'>&nbsp;</div>"
      "  <div style='width: 5000px; height: 5000px; "
      "              background-color: #00ff00'>&nbsp;</div>"
      "</body>");

  // Scroll to bottom right of page to ensure scroll position has no effect on
  // capture.
  ExecuteJavaScriptForTests(
      "window.scrollTo(document.body.scrollWidth,document.body.scrollHeight);");
  content::RunAllTasksUntilIdle();

  content::RenderFrame* frame = GetMainRenderFrame();
  auto [skp_path, out_response] =
      RunCapture(frame, true, gfx::Rect(0, 0, 500, 500),
                 mojom::ClipCoordOverride::kCenterOnScrollOffset,
                 mojom::ClipCoordOverride::kCenterOnScrollOffset);

  EXPECT_TRUE(out_response->embedding_token.has_value());
  EXPECT_EQ(frame->GetWebFrame()->GetEmbeddingToken(),
            out_response->embedding_token.value());
  EXPECT_EQ(out_response->content_id_to_embedding_token.size(), 0U);

  // Scroll offset should be within the [0, 500] bounds.
  EXPECT_THAT(out_response->geometry_metadata->scroll_offsets.x(),
              AllOf(Gt(0), Lt(500)));
  EXPECT_THAT(out_response->geometry_metadata->scroll_offsets.y(),
              AllOf(Gt(0), Lt(500)));

  // Both frame offsets should be > 0 in this case.
  EXPECT_GT(out_response->geometry_metadata->frame_offsets.x(), 0);
  EXPECT_GT(out_response->geometry_metadata->frame_offsets.y(), 0);

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
  EXPECT_EQ(bitmap.getColor(50, 10), SK_ColorGREEN);
  EXPECT_EQ(bitmap.getColor(50, pic->cullRect().height() - 10), SK_ColorGREEN);
}

TEST_P(PaintPreviewRecorderRenderViewTest,
       TestCaptureMainFrameAboutScrollPositionYAxis) {
  LoadHTML(
      "<!doctype html>"
      "<body>"
      "  <div style='width: 600px; height: 200px; "
      "              background-color: #ff0000'>&nbsp;</div>"
      "  <div style='width: 5000px; height: 5000px; "
      "              background-color: #00ff00'>&nbsp;</div>"
      "</body>");

  // Scroll to bottom right of page to ensure scroll position has no effect on
  // capture.
  ExecuteJavaScriptForTests(
      "window.scrollTo(document.body.scrollWidth,document.body.scrollHeight);");
  content::RunAllTasksUntilIdle();

  content::RenderFrame* frame = GetMainRenderFrame();
  auto [skp_path, out_response] =
      RunCapture(frame, true, gfx::Rect(0, 0, 500, 500),
                 /*clip_x_coord_override=*/
                 mojom::ClipCoordOverride::kNone, /*clip_y_coord_override=*/
                 mojom::ClipCoordOverride::kCenterOnScrollOffset);

  EXPECT_TRUE(out_response->embedding_token.has_value());
  EXPECT_EQ(frame->GetWebFrame()->GetEmbeddingToken(),
            out_response->embedding_token.value());
  EXPECT_EQ(out_response->content_id_to_embedding_token.size(), 0U);

  // Scroll offset should be within the [0, 500] bounds.
  EXPECT_THAT(out_response->geometry_metadata->scroll_offsets.x(),
              AllOf(Gt(0), Lt(500)));
  EXPECT_THAT(out_response->geometry_metadata->scroll_offsets.y(),
              AllOf(Gt(0), Lt(500)));

  // Only Y frame offset should be > 0 in this case.
  EXPECT_EQ(out_response->geometry_metadata->frame_offsets.x(), 0);
  EXPECT_GT(out_response->geometry_metadata->frame_offsets.y(), 0);

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
  EXPECT_EQ(bitmap.getColor(50, 10), SK_ColorGREEN);
  EXPECT_EQ(bitmap.getColor(50, pic->cullRect().height() - 10), SK_ColorGREEN);
}

TEST_P(PaintPreviewRecorderRenderViewTest,
       TestCaptureMainFrameAboutScrollPositionXAxis) {
  LoadHTML(
      "<!doctype html>"
      "<body>"
      "  <div style='width: 600px; height: 200px; "
      "              background-color: #ff0000'>&nbsp;</div>"
      "  <div style='width: 5000px; height: 5000px; "
      "              background-color: #00ff00'>&nbsp;</div>"
      "</body>");

  // Scroll to bottom right of page to ensure scroll position has no effect on
  // capture.
  ExecuteJavaScriptForTests(
      "window.scrollTo(document.body.scrollWidth,document.body.scrollHeight);");
  content::RunAllTasksUntilIdle();

  content::RenderFrame* frame = GetMainRenderFrame();
  auto [skp_path, out_response] =
      RunCapture(frame, true, gfx::Rect(0, 0, 500, 500),
                 /*clip_x_coord_override=*/
                 mojom::ClipCoordOverride::
                     kCenterOnScrollOffset, /*clip_y_coord_override=*/
                 mojom::ClipCoordOverride::kNone);

  EXPECT_TRUE(out_response->embedding_token.has_value());
  EXPECT_EQ(frame->GetWebFrame()->GetEmbeddingToken(),
            out_response->embedding_token.value());
  EXPECT_EQ(out_response->content_id_to_embedding_token.size(), 0U);

  // Scroll offset should be within the [0, 500] bounds.
  EXPECT_THAT(out_response->geometry_metadata->scroll_offsets.x(),
              AllOf(Gt(0), Lt(500)));
  EXPECT_THAT(out_response->geometry_metadata->scroll_offsets.y(),
              AllOf(Gt(0), Lt(500)));

  // Only X frame offset should be > 0 in this case.
  EXPECT_GT(out_response->geometry_metadata->frame_offsets.x(), 0);
  EXPECT_EQ(out_response->geometry_metadata->frame_offsets.y(), 0);

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
  EXPECT_EQ(bitmap.getColor(50, 10), SK_ColorWHITE);
  EXPECT_EQ(bitmap.getColor(50, pic->cullRect().height() - 10), SK_ColorGREEN);
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

  // Scroll to bottom right of page to ensure scroll position has no effect on
  // capture.
  ExecuteJavaScriptForTests(
      "window.scrollTo(document.body.scrollWidth,document.body.scrollHeight);");
  content::RunAllTasksUntilIdle();

  content::RenderFrame* frame = GetMainRenderFrame();
  auto [skp_path, out_response] =
      RunCapture(frame, true, gfx::Rect(0, 0, 500, 2000),
                 mojom::ClipCoordOverride::kCenterOnScrollOffset,
                 mojom::ClipCoordOverride::kCenterOnScrollOffset);

  EXPECT_TRUE(out_response->embedding_token.has_value());
  EXPECT_EQ(frame->GetWebFrame()->GetEmbeddingToken(),
            out_response->embedding_token.value());
  EXPECT_EQ(out_response->content_id_to_embedding_token.size(), 0U);

  // Scroll offset should be within the [0, 2000] bounds and closer to the
  // bottom as it was clamped.
  EXPECT_THAT(out_response->geometry_metadata->scroll_offsets.y(),
              AllOf(Gt(1100), Lt(2000)));

  // Frame offset should be > 0 in this case.
  EXPECT_GT(out_response->geometry_metadata->frame_offsets.y(), 0);

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
  EXPECT_EQ(bitmap.getColor(50, 10), SK_ColorGREEN);
  EXPECT_EQ(bitmap.getColor(50, pic->cullRect().height() - 10), SK_ColorGREEN);
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

  // Scroll to bottom right of page to ensure scroll position has no effect on
  // capture.
  ExecuteJavaScriptForTests(
      "window.scrollTo(document.body.scrollWidth,document.body.scrollHeight);");
  content::RunAllTasksUntilIdle();

  content::RenderFrame* frame = GetMainRenderFrame();
  auto [skp_path, out_response] = RunCapture(
      frame, true, gfx::Rect(), mojom::ClipCoordOverride::kCenterOnScrollOffset,
      mojom::ClipCoordOverride::kCenterOnScrollOffset);

  EXPECT_TRUE(out_response->embedding_token.has_value());
  EXPECT_EQ(frame->GetWebFrame()->GetEmbeddingToken(),
            out_response->embedding_token.value());
  EXPECT_EQ(out_response->content_id_to_embedding_token.size(), 0U);

  EXPECT_GT(out_response->geometry_metadata->scroll_offsets.y(), 0);

  // Frame offset should be 0 in this case.
  EXPECT_EQ(out_response->geometry_metadata->frame_offsets.x(), 0);
  EXPECT_EQ(out_response->geometry_metadata->frame_offsets.y(), 0);

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
  EXPECT_EQ(bitmap.getColor(600, 50), SK_ColorRED);
  EXPECT_EQ(bitmap.getColor(50, pic->cullRect().height() - 100), SK_ColorGREEN);
}

TEST_P(PaintPreviewRecorderRenderViewTest,
       TestCaptureMainFrameAtScrollPosition_HalfwayScrolled) {
  LoadHTML(
      "<!doctype html>"
      "<body>"
      "  <div style='width: 600px; height: 200px; "
      "              background-color: #ff0000'>&nbsp;</div>"
      "  <div style='width: 5000px; height: 5000px; "
      "              background-color: #00ff00'>&nbsp;</div>"
      "</body>");

  ExecuteJavaScriptForTests(
      "window.scrollTo(document.body.scrollWidth / 2, "
      "document.body.scrollHeight / 2);");
  content::RunAllTasksUntilIdle();

  content::RenderFrame* frame = GetMainRenderFrame();
  auto [skp_path, out_response] =
      RunCapture(frame, true, gfx::Rect(0, 0, 500, 500),
                 mojom::ClipCoordOverride::kScrollOffset,
                 mojom::ClipCoordOverride::kScrollOffset);

  EXPECT_TRUE(out_response->embedding_token.has_value());
  EXPECT_EQ(frame->GetWebFrame()->GetEmbeddingToken(),
            out_response->embedding_token.value());
  EXPECT_EQ(out_response->content_id_to_embedding_token.size(), 0U);

  // The capture origin is positioned *at* the scroll offsets.
  EXPECT_EQ(out_response->geometry_metadata->scroll_offsets.x(), 0);
  EXPECT_EQ(out_response->geometry_metadata->scroll_offsets.y(), 0);

  // The capture origin is exactly halfway down each dimension (since we
  // scrolled halfway in each direction).
  EXPECT_EQ(out_response->geometry_metadata->frame_offsets.x(), 5000 / 2);
  EXPECT_EQ(out_response->geometry_metadata->frame_offsets.y(),
            (200 + 5000) / 2);

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
  EXPECT_EQ(bitmap.getColor(50, 10), SK_ColorGREEN);
  EXPECT_EQ(bitmap.getColor(50, pic->cullRect().height() - 10), SK_ColorGREEN);
}

TEST_P(PaintPreviewRecorderRenderViewTest,
       TestCaptureMainFrameAtScrollPosition_ScrollToElement) {
  LoadHTML(
      "<!doctype html>"
      "<body>"
      "  <div style='width: 600px; height: 200px; "
      "              background-color: #ff0000'>&nbsp;</div>"
      "  <div style='width: 5000px; height: 5000px; "
      "              background-color: #00ff00'>&nbsp;</div>"
      "</body>");

  ExecuteJavaScriptForTests("window.scrollTo(0, 200);");
  content::RunAllTasksUntilIdle();

  content::RenderFrame* frame = GetMainRenderFrame();
  auto [skp_path, out_response] =
      RunCapture(frame, true, gfx::Rect(0, 0, 500, 500),
                 mojom::ClipCoordOverride::kScrollOffset,
                 mojom::ClipCoordOverride::kScrollOffset);

  EXPECT_TRUE(out_response->embedding_token.has_value());
  EXPECT_EQ(frame->GetWebFrame()->GetEmbeddingToken(),
            out_response->embedding_token.value());
  EXPECT_EQ(out_response->content_id_to_embedding_token.size(), 0U);

  // The capture origin is positioned *at* the scroll offsets.
  EXPECT_EQ(out_response->geometry_metadata->scroll_offsets.x(), 0);
  EXPECT_EQ(out_response->geometry_metadata->scroll_offsets.y(), 0);

  // The frame offsets are the same as the window's scroll offsets.
  EXPECT_EQ(out_response->geometry_metadata->frame_offsets.x(), 0);
  EXPECT_EQ(out_response->geometry_metadata->frame_offsets.y(), 200);

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
  // Top left of the capture should be green, since we scrolled to the start of
  // the green div.
  EXPECT_EQ(bitmap.getColor(10, 10), SK_ColorGREEN);
}

TEST_P(PaintPreviewRecorderRenderViewTest,
       TestCaptureMainFrameAtScrollPosition_Clamped) {
  LoadHTML(
      "<!doctype html>"
      "<body>"
      "  <div style='width: 600px; height: 200px; "
      "              background-color: #ff0000'>&nbsp;</div>"
      "  <div style='width: 5000px; height: 5000px; "
      "              background-color: #00ff00'>&nbsp;</div>"
      "</body>");

  ExecuteJavaScriptForTests(
      "window.scrollTo(document.body.scrollWidth, "
      "document.body.scrollHeight);");
  content::RunAllTasksUntilIdle();

  content::RenderFrame* frame = GetMainRenderFrame();
  auto [skp_path, out_response] =
      RunCapture(frame, true, gfx::Rect(0, 0, 500, 500),
                 mojom::ClipCoordOverride::kScrollOffset,
                 mojom::ClipCoordOverride::kScrollOffset);

  EXPECT_TRUE(out_response->embedding_token.has_value());
  EXPECT_EQ(frame->GetWebFrame()->GetEmbeddingToken(),
            out_response->embedding_token.value());
  EXPECT_EQ(out_response->content_id_to_embedding_token.size(), 0U);

  // The capture origin is positioned *at* the scroll offsets.
  EXPECT_EQ(out_response->geometry_metadata->scroll_offsets.x(), 0);
  EXPECT_EQ(out_response->geometry_metadata->scroll_offsets.y(), 0);

  // The capture origin is more than halfway scrolled in each dimension. The
  // exact placement depends on the size of the viewport.
  EXPECT_THAT(out_response->geometry_metadata->frame_offsets.x(),
              AllOf(Gt(5000 / 2), Lt(5000)));
  EXPECT_THAT(out_response->geometry_metadata->frame_offsets.y(),
              AllOf(Gt((200 + 5000) / 2), Lt(200 + 5000)));

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
  EXPECT_EQ(bitmap.getColor(50, 10), SK_ColorGREEN);
  EXPECT_EQ(bitmap.getColor(50, pic->cullRect().height() - 10), SK_ColorGREEN);
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
  content::RenderFrame* frame = GetMainRenderFrame();

  auto [skp_path, out_response] = RunCapture(frame);

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
  params->geometry_metadata_params = mojom::GeometryMetadataParams::New();
  params->geometry_metadata_params->clip_rect = gfx::Rect();
  params->is_main_frame = true;
  params->capture_links = true;
  params->max_capture_size = 0;
  base::File skp_file;  // Invalid file.
  params->file = std::move(skp_file);

  content::RenderFrame* frame = GetMainRenderFrame();
  base::test::TestFuture<base::expected<mojom::PaintPreviewCaptureResponsePtr,
                                        mojom::PaintPreviewStatus>>
      future;
  PaintPreviewRecorderImpl paint_preview_recorder(frame);
  paint_preview_recorder.CapturePaintPreview(std::move(params),
                                             future.GetCallback());
  EXPECT_THAT(future.Take(),
              base::test::ErrorIs(mojom::PaintPreviewStatus::kCaptureFailed));
}

TEST_P(PaintPreviewRecorderRenderViewTest, TestCaptureInvalidXYClip) {
  LoadHTML("<body></body>");

  mojom::PaintPreviewCaptureParamsPtr params =
      mojom::PaintPreviewCaptureParams::New();
  auto token = base::UnguessableToken::Create();
  params->guid = token;
  params->geometry_metadata_params = mojom::GeometryMetadataParams::New();
  params->geometry_metadata_params->clip_rect =
      gfx::Rect(1000000, 1000000, 10, 10);
  params->is_main_frame = true;
  params->capture_links = true;
  params->max_capture_size = 0;
  base::FilePath skp_path = MakeTestFilePath("test.skp");
  base::File skp_file(skp_path,
                      base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  params->file = std::move(skp_file);

  content::RenderFrame* frame = GetMainRenderFrame();
  base::test::TestFuture<base::expected<mojom::PaintPreviewCaptureResponsePtr,
                                        mojom::PaintPreviewStatus>>
      future;
  PaintPreviewRecorderImpl paint_preview_recorder(frame);
  paint_preview_recorder.CapturePaintPreview(std::move(params),
                                             future.GetCallback());
  EXPECT_THAT(future.Take(),
              base::test::ErrorIs(mojom::PaintPreviewStatus::kCaptureFailed));
}

TEST_P(PaintPreviewRecorderRenderViewTest, TestCaptureMainFrameAndLocalFrame) {
  LoadHTML(
      "<!doctype html>"
      "<body style='min-height:1000px;'>"
      "  <iframe style='width: 500px, height: 500px'"
      "          srcdoc=\"<div style='width: 100px; height: 100px;"
      "          background-color: #000000'>&nbsp;</div>\"></iframe>"
      "</body>");
  content::RenderFrame* frame = GetMainRenderFrame();

  auto [skp_path, out_response] = RunCapture(frame);

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
  auto* child_frame = content::RenderFrame::FromWebFrame(
      GetMainRenderFrame()->GetWebFrame()->FirstChild()->ToWebLocalFrame());
  ASSERT_TRUE(child_frame);

  auto [skp_path, out_response] = RunCapture(child_frame, false);

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
  auto* child_web_frame =
      GetMainRenderFrame()->GetWebFrame()->FirstChild()->ToWebLocalFrame();
  auto* child_frame = content::RenderFrame::FromWebFrame(child_web_frame);
  ASSERT_TRUE(child_frame);

  child_web_frame->SetScrollOffset(gfx::PointF(0, 400));

  auto [skp_path, out_response] = RunCapture(child_frame, false);

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
  EXPECT_EQ(bitmap.getColor(50, 50), SK_ColorGREEN);
  EXPECT_EQ(bitmap.getColor(50, 800), SK_ColorRED);
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

  content::RenderFrame* frame = GetMainRenderFrame();
  gfx::Rect clip_rect = gfx::Rect(150, 150, 300, 300);
  auto [skp_path, out_response] = RunCapture(frame, true, clip_rect);

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
  EXPECT_EQ(bitmap.getColor(100, 100), SK_ColorYELLOW);

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

  content::RenderFrame* frame = GetMainRenderFrame();
  const size_t kLarge = 1000000;
  gfx::Rect clip_rect = gfx::Rect(0, 0, kLarge, kLarge);
  auto [skp_path, out_response] = RunCapture(frame, true, clip_rect);

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

  content::RenderFrame* frame = GetMainRenderFrame();
  gfx::Rect clip_rect = gfx::Rect(1, 1, 0, 0);
  auto [skp_path, out_response] = RunCapture(frame, true, clip_rect);

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
  content::RenderFrame* frame = GetMainRenderFrame();

  auto [skp_path, out_response] = RunCapture(frame);

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
  content::RenderFrame* frame = GetMainRenderFrame();

  auto [skp_path, out_response] = RunCapture(frame);

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
  content::RenderFrame* frame = GetMainRenderFrame();

  auto [skp_path, out_response] = RunCapture(frame);

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
  content::RenderFrame* frame = GetMainRenderFrame();

  auto [skp_path, out_response] = RunCapture(frame);

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
  content::RenderFrame* frame = GetMainRenderFrame();

  auto [skp_path, out_response] = RunCapture(frame);

  EXPECT_TRUE(out_response->embedding_token.has_value());
  EXPECT_EQ(frame->GetWebFrame()->GetEmbeddingToken(),
            out_response->embedding_token.value());
  EXPECT_EQ(out_response->content_id_to_embedding_token.size(), 0U);

  ASSERT_EQ(out_response->links.size(), 2U);
  EXPECT_EQ(out_response->links[0]->url, GURL("http://www.chromium.org"));
  EXPECT_NEAR(out_response->links[0]->rect.x(), 40, 3);
  EXPECT_NEAR(out_response->links[0]->rect.y(), 122, 3);
  EXPECT_NEAR(out_response->links[0]->rect.width(), 84, 5);
  EXPECT_NEAR(out_response->links[0]->rect.height(), 20, 3);

  EXPECT_EQ(out_response->links[1]->url, GURL("http://www.example.com"));
  EXPECT_NEAR(out_response->links[1]->rect.x(), 60, 3);
  EXPECT_NEAR(out_response->links[1]->rect.y(), 40, 3);
  EXPECT_NEAR(out_response->links[1]->rect.width(), 70, 3);
  EXPECT_NEAR(out_response->links[1]->rect.height(), 20, 3);
}

TEST_P(PaintPreviewRecorderRenderViewTest, TestGetGeometryMetadata) {
  LoadHTML(
      "<!doctype html>"
      "<body>"
      "  <div style='width: 600px; height: 200px; "
      "              background-color: #ff0000'>&nbsp;</div>"
      "  <div style='width: 5000px; height: 5000px; "
      "              background-color: #00ff00'>&nbsp;</div>"
      "</body>");

  // Scroll to bottom right of page.
  ExecuteJavaScriptForTests(
      "window.scrollTo(document.body.scrollWidth,document.body.scrollHeight);");
  content::RunAllTasksUntilIdle();

  mojom::GeometryMetadataResponsePtr metadata =
      GetGeometryMetadata(GetMainRenderFrame(), gfx::Rect(0, 0, 500, 500),
                          mojom::ClipCoordOverride::kCenterOnScrollOffset,
                          mojom::ClipCoordOverride::kCenterOnScrollOffset);

  ASSERT_TRUE(metadata);

  // Scroll offset should be within the [0, 500] bounds.
  EXPECT_THAT(metadata->scroll_offsets.x(), AllOf(Gt(0), Lt(500)));
  EXPECT_THAT(metadata->scroll_offsets.y(), AllOf(Gt(0), Lt(500)));

  // Both frame offsets should be > 0 in this case.
  EXPECT_GT(metadata->frame_offsets.x(), 0);
  EXPECT_GT(metadata->frame_offsets.y(), 0);
}

INSTANTIATE_TEST_SUITE_P(All,
                         PaintPreviewRecorderRenderViewTest,
                         testing::Values(true, false),
                         CompositeAfterPaintToString);

}  // namespace paint_preview
