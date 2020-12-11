// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/gl_renderer_copier.h"

#include <stdint.h>

#include <cstring>
#include <memory>
#include <tuple>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/test_switches.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "cc/test/pixel_test.h"
#include "cc/test/pixel_test_utils.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/frame_sinks/copy_output_util.h"
#include "components/viz/service/display/gl_renderer.h"
#include "components/viz/test/paths.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkPixmap.h"
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

base::FilePath GetTestFilePath(const base::FilePath::CharType* basename) {
  base::FilePath test_dir;
  base::PathService::Get(Paths::DIR_TEST_DATA, &test_dir);
  return test_dir.Append(base::FilePath(basename));
}

}  // namespace

class GLRendererCopierPixelTest
    : public cc::PixelTest,
      public testing::WithParamInterface<
          std::tuple<GLenum, bool, CopyOutputResult::Format, bool, bool>> {
 public:
  void SetUp() override {
    SetUpGLWithoutRenderer(gfx::SurfaceOrigin::kBottomLeft);

    texture_deleter_ =
        std::make_unique<TextureDeleter>(base::ThreadTaskRunnerHandle::Get());

    source_gl_format_ = std::get<0>(GetParam());
    have_source_texture_ = std::get<1>(GetParam());
    result_format_ = std::get<2>(GetParam());
    scale_by_half_ = std::get<3>(GetParam());
    flipped_source_ = std::get<4>(GetParam());

    gl_ = context_provider()->ContextGL();
    copier_ = std::make_unique<GLRendererCopier>(context_provider(),
                                                 texture_deleter_.get());

    ASSERT_TRUE(cc::ReadPNGFile(
        GetTestFilePath(FILE_PATH_LITERAL("16_color_rects.png")),
        &source_bitmap_));
    source_bitmap_.setImmutable();
  }

  void TearDown() override {
    DeleteSourceFramebuffer();
    DeleteSourceTexture();
    copier_.reset();
    texture_deleter_.reset();
  }

  GLRendererCopier* copier() { return copier_.get(); }

  gfx::Rect DrawToWindowSpace(const gfx::Rect& draw_rect) {
    gfx::Rect window_rect = draw_rect;
    if (flipped_source_)
      window_rect.set_y(kSourceSize.height() - window_rect.bottom());
    return window_rect;
  }

  // Creates a packed RGBA (bytes_per_pixel=4) or RGB (bytes_per_pixel=3) bitmap
  // in OpenGL byte/row order from the given SkBitmap.
  std::unique_ptr<uint8_t[]> CreateGLPixelsFromSkBitmap(SkBitmap bitmap) {
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
    const int bytes_per_pixel = source_gl_format_ == GL_RGBA ? 4 : 3;
    std::unique_ptr<uint8_t[]> pixels(
        new uint8_t[rgba_bitmap.width() * rgba_bitmap.height() *
                    bytes_per_pixel]);
    for (int y = 0; y < rgba_bitmap.height(); ++y) {
      const uint8_t* src = static_cast<uint8_t*>(rgba_bitmap.getAddr(0, y));
      const int flipped_y = flipped_source_ ? rgba_bitmap.height() - y - 1 : y;
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

  // Returns a SkBitmap, given a packed RGBA bitmap in OpenGL byte/row order.
  SkBitmap CreateSkBitmapFromGLPixels(const uint8_t* pixels,
                                      const gfx::Size& size) {
    SkBitmap bitmap;
    bitmap.allocPixels(
        SkImageInfo::Make(size.width(), size.height(), kRGBA_8888_SkColorType,
                          kPremul_SkAlphaType),
        size.width() * 4);
    for (int y = 0; y < size.height(); ++y) {
      const int flipped_y = size.height() - y - 1;
      const uint8_t* const src_row = pixels + flipped_y * size.width() * 4;
      void* const dest_row = bitmap.getAddr(0, y);
      std::memcpy(dest_row, src_row, size.width() * 4);
    }
    return bitmap;
  }

  GLuint CreateSourceTexture() {
    CHECK_EQ(0u, source_texture_);
    gl_->GenTextures(1, &source_texture_);
    gl_->BindTexture(GL_TEXTURE_2D, source_texture_);
    gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl_->TexImage2D(GL_TEXTURE_2D, 0, source_gl_format_, kSourceSize.width(),
                    kSourceSize.height(), 0, source_gl_format_,
                    GL_UNSIGNED_BYTE,
                    CreateGLPixelsFromSkBitmap(source_bitmap_).get());
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

  // Reads back the texture in the given |mailbox| to a SkBitmap in Skia-native
  // format.
  SkBitmap ReadbackToSkBitmap(const gpu::Mailbox& mailbox,
                              const gpu::SyncToken& sync_token,
                              const gfx::Size& texture_size) {
    // Bind the texture to a framebuffer from which to read the pixels.
    if (sync_token.HasData())
      gl_->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
    GLuint texture = gl_->CreateAndConsumeTextureCHROMIUM(mailbox.name);
    GLuint framebuffer = 0;
    gl_->GenFramebuffers(1, &framebuffer);
    gl_->BindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    gl_->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              GL_TEXTURE_2D, texture, 0);

    // Read the pixels and convert to SkBitmap form for test comparisons.
    std::unique_ptr<uint8_t[]> pixels(new uint8_t[texture_size.GetArea() * 4]);
    gl_->ReadPixels(0, 0, texture_size.width(), texture_size.height(), GL_RGBA,
                    GL_UNSIGNED_BYTE, pixels.get());
    gl_->DeleteFramebuffers(1, &framebuffer);
    gl_->DeleteTextures(1, &texture);
    return CreateSkBitmapFromGLPixels(pixels.get(), texture_size);
  }

 protected:
  GLenum source_gl_format_;
  bool have_source_texture_;
  CopyOutputResult::Format result_format_;
  bool scale_by_half_;
  bool flipped_source_;
  SkBitmap source_bitmap_;

 private:
  gpu::gles2::GLES2Interface* gl_ = nullptr;
  std::unique_ptr<TextureDeleter> texture_deleter_;
  std::unique_ptr<GLRendererCopier> copier_;
  GLuint source_texture_ = 0;
  GLuint source_framebuffer_ = 0;
};

// On Android KitKat bots (but not newer ones), the left column of pixels in the
// result is off-by-one in the red channel. Use the off-by-one camparator as a
// workaround.
#if defined(OS_ANDROID)
#define PIXEL_COMPARATOR() cc::FuzzyPixelOffByOneComparator(false)
#else
#define PIXEL_COMPARATOR() cc::ExactPixelComparator(false)
#endif

TEST_P(GLRendererCopierPixelTest, ExecutesCopyRequest) {
  // Create and execute a CopyOutputRequest via the GLRendererCopier.
  std::unique_ptr<CopyOutputResult> result;
  {
    base::RunLoop loop;
    auto request = std::make_unique<CopyOutputRequest>(
        result_format_,
        base::BindOnce(
            [](std::unique_ptr<CopyOutputResult>* result,
               base::OnceClosure quit_closure,
               std::unique_ptr<CopyOutputResult> result_from_copier) {
              *result = std::move(result_from_copier);
              std::move(quit_closure).Run();
            },
            &result, loop.QuitClosure()));
    if (scale_by_half_) {
      request->set_result_selection(
          gfx::ScaleToEnclosingRect(gfx::Rect(kRequestArea), 0.5f));
      request->SetUniformScaleRatio(2, 1);
    } else {
      request->set_result_selection(gfx::Rect(kRequestArea));
    }
    const GLuint source_texture = CreateSourceTexture();
    CreateAndBindSourceFramebuffer(source_texture);

    copy_output::RenderPassGeometry geometry;
    // geometry.result_bounds not used by GLRendererCopier
    geometry.sampling_bounds = DrawToWindowSpace(gfx::Rect(kSourceSize));
    geometry.result_selection = request->result_selection();
    geometry.readback_offset =
        DrawToWindowSpace(geometry.result_selection).OffsetFromOrigin();

    copier()->CopyFromTextureOrFramebuffer(
        std::move(request), geometry, source_gl_format_,
        have_source_texture_ ? source_texture : 0, kSourceSize, flipped_source_,
        gfx::ColorSpace::CreateSRGB());
    loop.Run();
  }

  // Check that a result was produced and is of the expected rect/size.
  ASSERT_TRUE(result);
  ASSERT_FALSE(result->IsEmpty());
  if (scale_by_half_)
    ASSERT_EQ(gfx::ScaleToEnclosingRect(kRequestArea, 0.5f), result->rect());
  else
    ASSERT_EQ(kRequestArea, result->rect());

  // Examine the image in the |result|, and compare it to the baseline PNG file.
  const SkBitmap actual =
      (result_format_ == CopyOutputResult::Format::RGBA_BITMAP)
          ? result->AsSkBitmap()
          : ReadbackToSkBitmap(result->GetTextureResult()->mailbox,
                               result->GetTextureResult()->sync_token,
                               result->size());
  const auto png_file_path = GetTestFilePath(
      scale_by_half_ ? FILE_PATH_LITERAL("half_of_one_of_16_color_rects.png")
                     : FILE_PATH_LITERAL("one_of_16_color_rects.png"));
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kRebaselinePixelTests))
    EXPECT_TRUE(cc::WritePNGFile(actual, png_file_path, false));
  if (!cc::MatchesPNGFile(actual, png_file_path, PIXEL_COMPARATOR())) {
    LOG(ERROR) << "Entire source: " << cc::GetPNGDataUrl(source_bitmap_);
    ADD_FAILURE();
  }
}

#undef PIXEL_COMPARATOR

// Instantiate parameter sets for all possible combinations of scenarios
// GLRendererCopier will encounter, which will cause it to follow different
// workflows.
INSTANTIATE_TEST_SUITE_P(
    All,
    GLRendererCopierPixelTest,
    testing::Combine(
        // Source framebuffer GL format.
        testing::Values(static_cast<GLenum>(GL_RGBA),
                        static_cast<GLenum>(GL_RGB)),
        // Source: Have texture too?
        testing::Values(false, true),
        // Result format.
        testing::Values(CopyOutputResult::Format::RGBA_BITMAP,
                        CopyOutputResult::Format::RGBA_TEXTURE),
        // Result scaling: Scale by half?
        testing::Values(false, true),
        // Source content is vertically flipped?
        testing::Values(false, true)));

}  // namespace viz
