// Copyright 2019 The Chromium Authors. All rights reserved.
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
#include "content/public/renderer/render_view.h"
#include "content/public/test/render_view_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "ui/native_theme/native_theme_features.h"

namespace paint_preview {

namespace {

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

}  // namespace

class PaintPreviewRecorderRenderViewTest : public content::RenderViewTest {
 public:
  PaintPreviewRecorderRenderViewTest() {}
  ~PaintPreviewRecorderRenderViewTest() override {}

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    // TODO(crbug/1022398): This is required to bypass a seemingly unrelated
    // DCHECK for |use_overlay_scrollbars_| in NativeThemeAura on ChromeOS when
    // painting scrollbars when first calling LoadHTML().
    feature_list_.InitAndDisableFeature(features::kOverlayScrollbar);

    RenderViewTest::SetUp();
  }

  content::RenderFrame* GetFrame() { return view_->GetMainRenderFrame(); }

  base::FilePath MakeTestFilePath(const std::string& filename) {
    return temp_dir_.GetPath().AppendASCII(filename);
  }

  base::FilePath RunCapture(content::RenderFrame* frame,
                            mojom::PaintPreviewCaptureResponsePtr* out_response,
                            bool is_main_frame = true) {
    base::FilePath skp_path = MakeTestFilePath("test.skp");

    mojom::PaintPreviewCaptureParamsPtr params =
        mojom::PaintPreviewCaptureParams::New();
    auto token = base::UnguessableToken::Create();
    params->guid = token;
    params->clip_rect = gfx::Rect();
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

TEST_F(PaintPreviewRecorderRenderViewTest, TestCaptureMainFrameAndClipping) {
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
  content::RenderFrame* frame = GetFrame();
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
  SkCanvas canvas(bitmap);
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

TEST_F(PaintPreviewRecorderRenderViewTest, TestCaptureMainFrameWithScroll) {
  LoadHTML(
      "<!doctype html>"
      "<body>"
      "  <div style='width: 600px; height: 80vh; "
      "              background-color: #ff0000'>&nbsp;</div>"
      "  <div style='width: 600px; height: 1200px; "
      "              background-color: #00ff00'>&nbsp;</div>"
      "</body>");

  // Scroll to bottom of page to ensure scroll position has no effect on
  // capture.
  ExecuteJavaScriptForTests("window.scrollTo(0,document.body.scrollHeight);");

  auto out_response = mojom::PaintPreviewCaptureResponse::New();
  content::RenderFrame* frame = GetFrame();
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
  SkCanvas canvas(bitmap);
  canvas.drawPicture(pic);
  // This should be inside the top right corner of the top div. Success means
  // there was no horizontal or vertical clipping as this region is red,
  // matching the div.
  EXPECT_EQ(bitmap.getColor(600, 50), 0xFFFF0000U);
  // This should be inside the bottom of the bottom div. Success means there was
  // no vertical clipping as this region is blue matching the div.
  EXPECT_EQ(bitmap.getColor(50, pic->cullRect().height() - 100), 0xFF00FF00U);
}

TEST_F(PaintPreviewRecorderRenderViewTest, TestCaptureFragment) {
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
  content::RenderFrame* frame = GetFrame();

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

TEST_F(PaintPreviewRecorderRenderViewTest, TestCaptureInvalidFile) {
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

  content::RenderFrame* frame = GetFrame();
  PaintPreviewRecorderImpl paint_preview_recorder(frame);
  paint_preview_recorder.CapturePaintPreview(
      std::move(params),
      base::BindOnce(&OnCaptureFinished,
                     mojom::PaintPreviewStatus::kCaptureFailed, nullptr));
  content::RunAllTasksUntilIdle();
}

TEST_F(PaintPreviewRecorderRenderViewTest, TestCaptureMainFrameAndLocalFrame) {
  LoadHTML(
      "<!doctype html>"
      "<body style='min-height:1000px;'>"
      "  <iframe style='width: 500px, height: 500px'"
      "          srcdoc=\"<div style='width: 100px; height: 100px;"
      "          background-color: #000000'>&nbsp;</div>\"></iframe>"
      "</body>");
  auto out_response = mojom::PaintPreviewCaptureResponse::New();
  content::RenderFrame* frame = GetFrame();

  RunCapture(frame, &out_response);

  EXPECT_TRUE(out_response->embedding_token.has_value());
  EXPECT_EQ(frame->GetWebFrame()->GetEmbeddingToken(),
            out_response->embedding_token.value());
  EXPECT_EQ(out_response->content_id_to_embedding_token.size(), 0U);
}

TEST_F(PaintPreviewRecorderRenderViewTest, TestCaptureLocalFrame) {
  LoadHTML(
      "<!doctype html>"
      "<body style='min-height:1000px;'>"
      "  <iframe style='width: 500px, height: 500px'"
      "          srcdoc=\"<div style='width: 100px; height: 100px;"
      "          background-color: #000000'>&nbsp;</div>\"></iframe>"
      "</body>");
  auto out_response = mojom::PaintPreviewCaptureResponse::New();
  auto* child_frame = content::RenderFrame::FromWebFrame(
      GetFrame()->GetWebFrame()->FirstChild()->ToWebLocalFrame());
  ASSERT_TRUE(child_frame);

  RunCapture(child_frame, &out_response, false);

  EXPECT_TRUE(out_response->embedding_token.has_value());
  EXPECT_EQ(out_response->content_id_to_embedding_token.size(), 0U);
}

TEST_F(PaintPreviewRecorderRenderViewTest, CaptureWithTranslate) {
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
  content::RenderFrame* frame = GetFrame();

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

TEST_F(PaintPreviewRecorderRenderViewTest, CaptureWithTranslateThenRotate) {
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
  content::RenderFrame* frame = GetFrame();

  RunCapture(frame, &out_response);

  EXPECT_TRUE(out_response->embedding_token.has_value());
  EXPECT_EQ(frame->GetWebFrame()->GetEmbeddingToken(),
            out_response->embedding_token.value());
  EXPECT_EQ(out_response->content_id_to_embedding_token.size(), 0U);

  ASSERT_EQ(out_response->links.size(), 1U);
  EXPECT_EQ(out_response->links[0]->url, GURL("http://www.example.com"));
  EXPECT_NEAR(out_response->links[0]->rect.x(), 141, 5);
  EXPECT_NEAR(out_response->links[0]->rect.y(), 18, 5);
#if !defined(OS_ANDROID)
  EXPECT_NEAR(out_response->links[0]->rect.width(), 58, 10);
  EXPECT_NEAR(out_response->links[0]->rect.height(), 58, 10);
#endif
}

TEST_F(PaintPreviewRecorderRenderViewTest, CaptureWithRotateThenTranslate) {
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
  content::RenderFrame* frame = GetFrame();

  RunCapture(frame, &out_response);

  EXPECT_TRUE(out_response->embedding_token.has_value());
  EXPECT_EQ(frame->GetWebFrame()->GetEmbeddingToken(),
            out_response->embedding_token.value());
  EXPECT_EQ(out_response->content_id_to_embedding_token.size(), 0U);

  ASSERT_EQ(out_response->links.size(), 1U);
  EXPECT_EQ(out_response->links[0]->url, GURL("http://www.example.com"));
  EXPECT_NEAR(out_response->links[0]->rect.x(), 111, 5);
  EXPECT_NEAR(out_response->links[0]->rect.y(), 88, 5);
#if !defined(OS_ANDROID)
  EXPECT_NEAR(out_response->links[0]->rect.width(), 58, 10);
  EXPECT_NEAR(out_response->links[0]->rect.height(), 58, 10);
#endif
}

TEST_F(PaintPreviewRecorderRenderViewTest, CaptureWithScale) {
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
  content::RenderFrame* frame = GetFrame();

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

TEST_F(PaintPreviewRecorderRenderViewTest, CaptureSaveRestore) {
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
  content::RenderFrame* frame = GetFrame();

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

}  // namespace paint_preview
