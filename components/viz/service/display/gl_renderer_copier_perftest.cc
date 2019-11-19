// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/gl_renderer_copier.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/timer/lap_timer.h"
#include "cc/test/pixel_test_utils.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/frame_sinks/copy_output_util.h"
#include "components/viz/service/display/gl_renderer.h"
#include "components/viz/test/paths.h"
#include "components/viz/test/test_in_process_context_provider.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace viz {

namespace {

// The size of the source texture or framebuffer.
constexpr gfx::Size kSourceSize = gfx::Size(240, 120);

// In order to test coordinate calculations and Y-flipping, the tests will issue
// copy requests for a small region just to the right and below the center of
// the entire source texture/framebuffer.
constexpr gfx::Rect kRequestArea = gfx::Rect(kSourceSize.width() / 2,
                                             kSourceSize.height() / 2,
                                             kSourceSize.width() / 4,
                                             kSourceSize.height() / 4);

constexpr char kMetricPrefixGLRendererCopier[] = "GLRendererCopier.";
constexpr char kMetricReadbackThroughputRunsPerS[] = "readback_throughput";

perf_test::PerfResultReporter SetUpGLRendererCopierReporter(
    const std::string& story) {
  perf_test::PerfResultReporter reporter(kMetricPrefixGLRendererCopier, story);
  reporter.RegisterImportantMetric(kMetricReadbackThroughputRunsPerS, "runs/s");
  return reporter;
}

base::FilePath GetTestFilePath(const base::FilePath::CharType* basename) {
  base::FilePath test_dir;
  base::PathService::Get(Paths::DIR_TEST_DATA, &test_dir);
  return test_dir.Append(base::FilePath(basename));
}

}  // namespace

class GLRendererCopierPerfTest : public testing::Test {
 public:
  GLRendererCopierPerfTest() {
    auto context_provider = base::MakeRefCounted<TestInProcessContextProvider>(
        /*enable_oop_rasterization=*/false, /*support_locking=*/false);
    gpu::ContextResult result = context_provider->BindToCurrentThread();
    DCHECK_EQ(result, gpu::ContextResult::kSuccess);
    gl_ = context_provider->ContextGL();
    texture_deleter_ =
        std::make_unique<TextureDeleter>(base::ThreadTaskRunnerHandle::Get());
    copier_ = std::make_unique<GLRendererCopier>(std::move(context_provider),
                                                 texture_deleter_.get());
  }

  void TearDown() override {
    DeleteSourceFramebuffer();
    DeleteSourceTexture();
    copier_.reset();
    texture_deleter_.reset();
  }

  gfx::Rect DrawToWindowSpace(const gfx::Rect& draw_rect, bool flipped_source) {
    gfx::Rect window_rect = draw_rect;
    if (flipped_source)
      window_rect.set_y(kSourceSize.height() - window_rect.bottom());
    return window_rect;
  }

  // Creates a packed RGBA (bytes_per_pixel=4) bitmap in OpenGL byte/row order
  // from the given SkBitmap.
  std::unique_ptr<uint8_t[]> CreateGLPixelsFromSkBitmap(SkBitmap bitmap,
                                                        bool flipped_source) {
    // |bitmap| could be of any color type (and is usually BGRA). Convert it to
    // a RGBA bitmap in the GL byte order.
    SkBitmap rgba_bitmap;
    rgba_bitmap.allocPixels(SkImageInfo::Make(bitmap.width(), bitmap.height(),
                                              kRGBA_8888_SkColorType,
                                              kPremul_SkAlphaType));
    SkPixmap pixmap;
    const bool success =
        bitmap.peekPixels(&pixmap) && rgba_bitmap.writePixels(pixmap, 0, 0);
    CHECK(success);

    // Copy the RGBA bitmap into a raw byte array, reversing the row order and
    // maybe stripping-out the alpha channel.
    const int bytes_per_pixel = 4;
    std::unique_ptr<uint8_t[]> pixels(
        new uint8_t[rgba_bitmap.width() * rgba_bitmap.height() *
                    bytes_per_pixel]);
    for (int y = 0; y < rgba_bitmap.height(); ++y) {
      const uint8_t* src = static_cast<uint8_t*>(rgba_bitmap.getAddr(0, y));
      const int flipped_y = flipped_source ? rgba_bitmap.height() - y - 1 : y;
      uint8_t* dest =
          pixels.get() + flipped_y * rgba_bitmap.width() * bytes_per_pixel;
      for (int x = 0; x < rgba_bitmap.width(); ++x) {
        *(dest++) = *(src++);
        *(dest++) = *(src++);
        *(dest++) = *(src++);
        if (bytes_per_pixel == 4)
          *(dest++) = *(src++);
        else
          ++src;
      }
    }

    return pixels;
  }

  GLuint CreateSourceTexture(bool flipped_source) {
    CHECK_EQ(0u, source_texture_);
    SkBitmap source_bitmap;
    cc::ReadPNGFile(GetTestFilePath(FILE_PATH_LITERAL("16_color_rects.png")),
                    &source_bitmap);
    source_bitmap.setImmutable();
    gl_->GenTextures(1, &source_texture_);
    gl_->BindTexture(GL_TEXTURE_2D, source_texture_);
    gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl_->TexImage2D(
        GL_TEXTURE_2D, 0, static_cast<GLenum>(GL_RGBA), kSourceSize.width(),
        kSourceSize.height(), 0, static_cast<GLenum>(GL_RGBA), GL_UNSIGNED_BYTE,
        CreateGLPixelsFromSkBitmap(source_bitmap, flipped_source).get());
    gl_->BindTexture(GL_TEXTURE_2D, 0);
    return source_texture_;
  }

  void DeleteSourceTexture() {
    if (source_texture_ != 0) {
      gl_->DeleteTextures(1, &source_texture_);
      source_texture_ = 0;
    }
  }

  void CreateAndBindSourceFramebuffer(GLuint texture) {
    ASSERT_EQ(0u, source_framebuffer_);
    gl_->GenFramebuffers(1, &source_framebuffer_);
    gl_->BindFramebuffer(GL_FRAMEBUFFER, source_framebuffer_);
    gl_->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              GL_TEXTURE_2D, texture, 0);
  }

  void DeleteSourceFramebuffer() {
    if (source_framebuffer_ != 0) {
      gl_->DeleteFramebuffers(1, &source_framebuffer_);
      source_framebuffer_ = 0;
    }
  }

  void CopyFromTextureOrFramebuffer(bool have_source_texture,
                                    CopyOutputResult::Format result_format,
                                    bool scale_by_half,
                                    bool flipped_source,
                                    const std::string& story) {
    std::unique_ptr<CopyOutputResult> result;

    gfx::Rect result_selection(kRequestArea);
    if (scale_by_half)
      result_selection = gfx::ScaleToEnclosingRect(result_selection, 0.5f);

    copy_output::RenderPassGeometry geometry;
    // geometry.result_bounds not used by GLRendererCopier
    geometry.sampling_bounds =
        DrawToWindowSpace(gfx::Rect(kSourceSize), flipped_source);
    geometry.result_selection = result_selection;
    geometry.readback_offset =
        DrawToWindowSpace(geometry.result_selection, flipped_source)
            .OffsetFromOrigin();

    timer_.Reset();
    do {
      base::RunLoop loop;
      auto request = std::make_unique<CopyOutputRequest>(
          result_format,
          base::BindOnce(
              [](std::unique_ptr<CopyOutputResult>* result,
                 base::OnceClosure quit_closure,
                 std::unique_ptr<CopyOutputResult> result_from_copier) {
                *result = std::move(result_from_copier);
                std::move(quit_closure).Run();
              },
              &result, loop.QuitClosure()));
      if (scale_by_half)
        request->SetUniformScaleRatio(2, 1);
      const GLuint source_texture = CreateSourceTexture(flipped_source);
      CreateAndBindSourceFramebuffer(source_texture);

      copier_->CopyFromTextureOrFramebuffer(
          std::move(request), geometry, static_cast<GLenum>(GL_RGBA),
          have_source_texture ? source_texture : 0, kSourceSize, flipped_source,
          gfx::ColorSpace::CreateSRGB());
      loop.Run();

      // Check that a result was produced and is of the expected rect/size.
      ASSERT_TRUE(result);
      ASSERT_FALSE(result->IsEmpty());
      if (scale_by_half)
        ASSERT_EQ(gfx::ScaleToEnclosingRect(kRequestArea, 0.5f),
                  result->rect());
      else
        ASSERT_EQ(kRequestArea, result->rect());

      if (result_format == CopyOutputResult::Format::RGBA_BITMAP) {
        const SkBitmap& result_bitmap = result->AsSkBitmap();
        ASSERT_TRUE(result_bitmap.readyToDraw());
      } else if (result_format == CopyOutputResult::Format::I420_PLANES) {
        const int result_width = result->rect().width();
        const int result_height = result->rect().height();
        const int y_width = result_width;
        const int y_stride = y_width;
        std::unique_ptr<uint8_t[]> y_data(
            new uint8_t[y_stride * result_height]);
        const int chroma_width = (result_width + 1) / 2;
        const int u_stride = chroma_width;
        const int v_stride = chroma_width;
        const int chroma_height = (result_height + 1) / 2;
        std::unique_ptr<uint8_t[]> u_data(
            new uint8_t[u_stride * chroma_height]);
        std::unique_ptr<uint8_t[]> v_data(
            new uint8_t[v_stride * chroma_height]);

        const bool success =
            result->ReadI420Planes(y_data.get(), y_stride, u_data.get(),
                                   u_stride, v_data.get(), v_stride);
        ASSERT_TRUE(success);
      }

      DeleteSourceFramebuffer();
      DeleteSourceTexture();
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    auto reporter = SetUpGLRendererCopierReporter(story);
    reporter.AddResult(kMetricReadbackThroughputRunsPerS,
                       timer_.LapsPerSecond());
  }

 private:
  gpu::gles2::GLES2Interface* gl_ = nullptr;
  std::unique_ptr<TextureDeleter> texture_deleter_;
  std::unique_ptr<GLRendererCopier> copier_;
  GLuint source_texture_ = 0;
  GLuint source_framebuffer_ = 0;
  base::LapTimer timer_;

  DISALLOW_COPY_AND_ASSIGN(GLRendererCopierPerfTest);
};

// Fast-Path: If no transformation is necessary and no new textures need to be
// generated, read-back directly from the currently-bound framebuffer.
TEST_F(GLRendererCopierPerfTest, NoTransformNoNewTextures) {
  CopyFromTextureOrFramebuffer(
      /*have_source_texture=*/false, CopyOutputResult::Format::RGBA_BITMAP,
      /*scale_by_half=*/false, /*flipped_source=*/false,
      "no_transformation_and_no_new_textures");
}

// Source texture is the one attached to the framebuffer, better performance
// than having to make a copy of the framebuffer.
TEST_F(GLRendererCopierPerfTest, HaveTextureResultRGBABitmap) {
  CopyFromTextureOrFramebuffer(
      /*have_source_texture=*/true, CopyOutputResult::Format::RGBA_BITMAP,
      /*scale_by_half=*/true, /*flipped_source=*/false,
      "framebuffer_has_texture_and_result_is_RGBA_BITMAP");
}
TEST_F(GLRendererCopierPerfTest, HaveTextureResultRGBATexture) {
  CopyFromTextureOrFramebuffer(
      /*have_source_texture=*/true, CopyOutputResult::Format::RGBA_TEXTURE,
      /*scale_by_half=*/true, /*flipped_source=*/false,
      "framebuffer_has_texture_and_result_is_RGBA_TEXTURE");
}
TEST_F(GLRendererCopierPerfTest, HaveTextureResultI420Planes) {
  CopyFromTextureOrFramebuffer(
      /*have_source_texture=*/true, CopyOutputResult::Format::I420_PLANES,
      /*scale_by_half=*/true, /*flipped_source=*/false,
      "framebuffer_has_texture_and_result_is_I420_PLANES");
}

// Have to make a copy of the framebuffer for the source texture.
TEST_F(GLRendererCopierPerfTest, NoTextureResultI420Planes) {
  CopyFromTextureOrFramebuffer(
      /*have_source_texture=*/false, CopyOutputResult::Format::I420_PLANES,
      /*scale_by_half=*/true, /*flipped_source=*/false,
      "framebuffer_doesn't_have_texture_and_result_is_I420_PLANES");
}

// Source content is vertically flipped.
TEST_F(GLRendererCopierPerfTest, SourceContentVerticallyFlipped) {
  CopyFromTextureOrFramebuffer(/*have_source_texture=*/true,
                               CopyOutputResult::Format::I420_PLANES,
                               /*scale_by_half=*/true, /*flipped_source=*/true,
                               "source_content_is_vertically_flipped");
}

// Result is not scaled by half.
TEST_F(GLRendererCopierPerfTest, ResultNotScaled) {
  CopyFromTextureOrFramebuffer(/*have_source_texture=*/true,
                               CopyOutputResult::Format::I420_PLANES,
                               /*scale_by_half=*/false, /*flipped_source=*/true,
                               "result_is_not_scaled_by_half");
}

}  // namespace viz
