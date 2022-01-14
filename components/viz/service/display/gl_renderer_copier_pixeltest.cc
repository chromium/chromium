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
#include "base/logging.h"
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
#include "components/viz/test/gl_scaler_test_util.h"
#include "components/viz/test/paths.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libyuv/include/libyuv/convert_argb.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/color_transform.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif

namespace viz {

namespace {

base::FilePath GetTestFilePath(const base::FilePath::CharType* basename) {
  base::FilePath test_dir;
  base::PathService::Get(Paths::DIR_TEST_DATA, &test_dir);
  return test_dir.Append(base::FilePath(basename));
}

// Creates a packed RGBA (bytes_per_pixel=4) or RGB (bytes_per_pixel=3) bitmap
// in OpenGL byte/row order from the given SkBitmap.
std::unique_ptr<uint8_t[]> CreateGLPixelsFromSkBitmap(SkBitmap bitmap,
                                                      GLuint source_format,
                                                      bool flip_source) {
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
  const int bytes_per_pixel = source_format == GL_RGBA ? 4 : 3;
  std::unique_ptr<uint8_t[]> pixels(
      new uint8_t[rgba_bitmap.width() * rgba_bitmap.height() *
                  bytes_per_pixel]);
  for (int y = 0; y < rgba_bitmap.height(); ++y) {
    const uint8_t* src = static_cast<uint8_t*>(rgba_bitmap.getAddr(0, y));
    const int flipped_y = flip_source ? rgba_bitmap.height() - y - 1 : y;
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

// Reads back the texture in the given |mailbox| to a SkBitmap in Skia-native
// format.
SkBitmap ReadbackToSkBitmap(gpu::gles2::GLES2Interface* gl,
                            const gpu::Mailbox& mailbox,
                            const gpu::SyncToken& sync_token,
                            const gfx::Size& texture_size) {
  // Bind the texture to a framebuffer from which to read the pixels.
  if (sync_token.HasData())
    gl->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
  GLuint texture = gl->CreateAndConsumeTextureCHROMIUM(mailbox.name);
  GLuint framebuffer = 0;
  gl->GenFramebuffers(1, &framebuffer);
  gl->BindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  gl->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           texture, 0);

  // Read the pixels and convert to SkBitmap form for test comparisons.
  std::unique_ptr<uint8_t[]> pixels(new uint8_t[texture_size.GetArea() * 4]);
  gl->ReadPixels(0, 0, texture_size.width(), texture_size.height(), GL_RGBA,
                 GL_UNSIGNED_BYTE, pixels.get());
  gl->DeleteFramebuffers(1, &framebuffer);
  gl->DeleteTextures(1, &texture);
  return CreateSkBitmapFromGLPixels(pixels.get(), texture_size);
}

// Validates whether all rows are identical (i.e. for each row r != 0, compares
// it with row 0.
void ValidateRows(uint8_t* pixel_data, size_t row_stride, size_t height) {
  for (size_t row = 1; row < height; ++row) {
    for (size_t col = 0; col < row_stride; ++col) {
      EXPECT_NEAR(pixel_data[col], pixel_data[col + row * row_stride], 1)
          << " mismatch in row " << row << ", column " << col;
    }
  }
}

// Returns maximum allowed difference between the expected and actual pixel
// values.
int GetTolerance() {
  return 1;
}

}  // namespace

//
// All tests in this class follow roughly the same pattern::
// - Construct a CopyOutputRequest, with arguments depending on the test
//   parameters and the specific format that is being tested.
// - Upload source texture to GL.
// - Invoke GLRendererCopier::CopyFromTextureOrFramebuffer(), with arguments
//   depending on the test parameters, passing the created CopyOutputRequest.
// - Load the result into memory and compare with baseline.
//
// Parameters:
// 0. GL format to use when uploading source texture.
// 1. True if the copier will also receive the texture in a call to
//    `CopyFromTextureOrFramebuffer()`, false otherwise.
// 2. Destiation for the CopyOutputRequest (native textures or system memory).
// 3. True if the result should be scaled by half in each dimension, false
//    otherwise.
// 4. True if the source texture will be flipped (bottom-up), false otherwise.
class GLRendererCopierPixelTest
    : public cc::PixelTest,
      public testing::WithParamInterface<
          std::tuple<GLenum, bool, CopyOutputResult::Destination, bool, bool>> {
 public:
  // In order to test coordinate calculations and Y-flipping, the tests will
  // issue copy requests for a small region just to the right and below the
  // center of the entire source texture/framebuffer.
  gfx::Rect GetRequestArea() const {
    DCHECK(!source_size_.IsZero());

    gfx::Rect result(source_size_.width() / 2, source_size_.height() / 2,
                     source_size_.width() / 4, source_size_.height() / 4);

    if (scale_by_half_) {
      return gfx::ScaleToEnclosingRect(result, 0.5f);
    }

    return result;
  }

  void SetUp() override {
    SetUpGLWithoutRenderer(gfx::SurfaceOrigin::kBottomLeft);

    texture_deleter_ =
        std::make_unique<TextureDeleter>(base::ThreadTaskRunnerHandle::Get());

    source_gl_format_ = std::get<0>(GetParam());
    have_source_texture_ = std::get<1>(GetParam());
    result_destination_ = std::get<2>(GetParam());
    scale_by_half_ = std::get<3>(GetParam());
    flipped_source_ = std::get<4>(GetParam());

    gl_ = context_provider()->ContextGL();
    copier_ = std::make_unique<GLRendererCopier>(context_provider(),
                                                 texture_deleter_.get());

    ASSERT_TRUE(cc::ReadPNGFile(
        GetTestFilePath(FILE_PATH_LITERAL("16_color_rects.png")),
        &source_bitmap_));
    source_bitmap_.setImmutable();

    source_size_ = gfx::Size(source_bitmap_.width(), source_bitmap_.height());

    source_bitmap_rgba_ =
        GLScalerTestUtil::CopyAndConvertToRGBA(source_bitmap_);
    source_bitmap_rgba_.setImmutable();

    source_bitmap_yuv_ = source_bitmap_rgba_;
    GLScalerTestUtil::ConvertRGBABitmapToYUV(&source_bitmap_yuv_);
    source_bitmap_yuv_.setImmutable();
  }

  void TearDown() override {
    DeleteSourceFramebuffer();
    DeleteSourceTexture();
    copier_.reset();
    texture_deleter_.reset();
  }

  gpu::gles2::GLES2Interface* gl() { return gl_; }

  GLRendererCopier* copier() { return copier_.get(); }

  gfx::Rect DrawToWindowSpace(const gfx::Size& source_size,
                              const gfx::Rect& draw_rect) {
    gfx::Rect window_rect = draw_rect;
    if (flipped_source_)
      window_rect.set_y(source_size.height() - window_rect.bottom());
    return window_rect;
  }

  GLuint CreateSourceTexture(SkBitmap source_bitmap) {
    CHECK_EQ(0u, source_texture_);
    gl_->GenTextures(1, &source_texture_);
    gl_->BindTexture(GL_TEXTURE_2D, source_texture_);
    gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl_->TexImage2D(GL_TEXTURE_2D, 0, source_gl_format_, source_bitmap.width(),
                    source_bitmap.height(), 0, source_gl_format_,
                    GL_UNSIGNED_BYTE,
                    CreateGLPixelsFromSkBitmap(source_bitmap, source_gl_format_,
                                               flipped_source_)
                        .get());
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

 protected:
  // The size of the source texture or framebuffer.
  gfx::Size source_size_;

  GLenum source_gl_format_;
  bool have_source_texture_;
  CopyOutputResult::Destination result_destination_;
  bool scale_by_half_;
  bool flipped_source_;
  SkBitmap source_bitmap_;
  SkBitmap source_bitmap_rgba_;
  SkBitmap source_bitmap_yuv_;

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
#if BUILDFLAG(IS_ANDROID)
#define PIXEL_COMPARATOR() cc::FuzzyPixelOffByOneComparator(false)
#else
#define PIXEL_COMPARATOR() cc::ExactPixelComparator(false)
#endif

TEST_P(GLRendererCopierPixelTest, ExecutesCopyRequestRGBA) {
  // Create and execute a CopyOutputRequest via the GLRendererCopier.
  std::unique_ptr<CopyOutputResult> result;
  {
    base::RunLoop loop;
    auto request = std::make_unique<CopyOutputRequest>(
        CopyOutputRequest::ResultFormat::RGBA, result_destination_,
        base::BindOnce(
            [](std::unique_ptr<CopyOutputResult>* result,
               base::OnceClosure quit_closure,
               std::unique_ptr<CopyOutputResult> result_from_copier) {
              *result = std::move(result_from_copier);
              std::move(quit_closure).Run();
            },
            &result, loop.QuitClosure()));
    if (scale_by_half_) {
      request->SetUniformScaleRatio(2, 1);
    }

    request->set_result_selection(GetRequestArea());

    const GLuint source_texture = CreateSourceTexture(source_bitmap_);
    CreateAndBindSourceFramebuffer(source_texture);

    copy_output::RenderPassGeometry geometry;
    // geometry.result_bounds not used by GLRendererCopier
    geometry.sampling_bounds =
        DrawToWindowSpace(source_size_, gfx::Rect(source_size_));
    geometry.result_selection = request->result_selection();
    geometry.readback_offset =
        DrawToWindowSpace(source_size_, geometry.result_selection)
            .OffsetFromOrigin();

    copier()->CopyFromTextureOrFramebuffer(
        std::move(request), geometry, source_gl_format_,
        have_source_texture_ ? source_texture : 0, source_size_,
        flipped_source_, gfx::ColorSpace::CreateSRGB());
    loop.Run();
  }

  // Check that a result was produced and is of the expected rect/size.
  ASSERT_TRUE(result);
  ASSERT_FALSE(result->IsEmpty());
  ASSERT_EQ(GetRequestArea(), result->rect());

  // Examine the image in the |result|, and compare it to the baseline PNG file.
  absl::optional<CopyOutputResult::ScopedSkBitmap> scoped_bitmap;
  SkBitmap actual;
  if (result_destination_ == CopyOutputResult::Destination::kSystemMemory) {
    scoped_bitmap = result->ScopedAccessSkBitmap();
    actual = scoped_bitmap->bitmap();
  } else {
    actual = ReadbackToSkBitmap(
        gl(), result->GetTextureResult()->planes[0].mailbox,
        result->GetTextureResult()->planes[0].sync_token, result->size());
  }
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

TEST_P(GLRendererCopierPixelTest, ExecutesCopyRequestNV12) {
  if (result_destination_ ==
      CopyOutputRequest::ResultDestination::kNativeTextures) {
    // TODO(https://crbug.com/1216287): Enable once textures are supported.
    GTEST_SKIP()
        << "Enable once the GLRenderer supports producing producing results to "
           "a texture for NV12 format.";
  }

  const gfx::Rect request_area = GetRequestArea();

  // Check if request's width and height are even (required for NV12 format).
  // The test case expects the result size to match the request size exactly,
  // which is not possible with NV12 when the request size dimensions aren't
  // even.
  ASSERT_TRUE(request_area.width() % 2 == 0 && request_area.height() % 2 == 0)
      << " request size is not even, request_area.size()="
      << request_area.size().ToString();

  // Additionally, the test uses helpers that assume pixel data can be packed (4
  // 8-bit values in 1 32-bit pixel).
  ASSERT_TRUE(request_area.width() % 4 == 0)
      << " request width is not divisible by 4, request_area.width()="
      << request_area.width();

  // Create and execute a CopyOutputRequest via the GLRendererCopier.
  std::unique_ptr<CopyOutputResult> result;
  {
    base::RunLoop loop;
    auto request = std::make_unique<CopyOutputRequest>(
        CopyOutputRequest::ResultFormat::NV12_PLANES, result_destination_,
        base::BindOnce(
            [](std::unique_ptr<CopyOutputResult>* result,
               base::OnceClosure quit_closure,
               std::unique_ptr<CopyOutputResult> result_from_copier) {
              *result = std::move(result_from_copier);
              std::move(quit_closure).Run();
            },
            &result, loop.QuitClosure()));
    if (scale_by_half_) {
      request->SetUniformScaleRatio(2, 1);
    }

    request->set_result_selection(request_area);

    // Upload source texture to GL - the texture will be converted to RGBA if
    // necessary.
    const GLuint source_texture = CreateSourceTexture(source_bitmap_);
    CreateAndBindSourceFramebuffer(source_texture);

    copy_output::RenderPassGeometry geometry;
    // geometry.result_bounds not used by GLRendererCopier
    geometry.sampling_bounds =
        DrawToWindowSpace(source_size_, gfx::Rect(source_size_));
    geometry.result_selection = request->result_selection();
    geometry.readback_offset =
        DrawToWindowSpace(source_size_, geometry.result_selection)
            .OffsetFromOrigin();

    copier()->CopyFromTextureOrFramebuffer(
        std::move(request), geometry, source_gl_format_,
        have_source_texture_ ? source_texture : 0, source_size_,
        flipped_source_, gfx::ColorSpace::CreateSRGB());
    loop.Run();
  }

  // Check that a result was produced and is of the expected rect/size.
  ASSERT_TRUE(result);
  ASSERT_FALSE(result->IsEmpty());
  ASSERT_EQ(request_area, result->rect());

  // Examine the image in the |result|, and compare it to the baseline PNG file.
  // Approach is the same as the one in GLNV12ConverterPixelTest.

  // Allocate new bitmap, it will then be populated with Y & UV data.
  SkBitmap actual = GLScalerTestUtil::AllocateRGBABitmap(result->size());
  actual.eraseColor(SkColorSetARGB(0xff, 0x00, 0x00, 0x00));

  SkBitmap luma_plane;
  SkBitmap chroma_planes;

  if (result_destination_ == CopyOutputResult::Destination::kSystemMemory) {
    // Create a bitmap with packed Y values:
    luma_plane = GLScalerTestUtil::AllocateRGBABitmap(
        gfx::Size(result->size().width() / 4, result->size().height()));

    chroma_planes = GLScalerTestUtil::AllocateRGBABitmap(
        gfx::Size(luma_plane.width(), luma_plane.height() / 2));

    result->ReadNV12Planes(
        reinterpret_cast<uint8_t*>(luma_plane.getAddr(0, 0)),
        result->size().width(),
        reinterpret_cast<uint8_t*>(chroma_planes.getAddr(0, 0)),
        result->size().width());
  } else {
    LOG(ERROR) << "Texture results for NV12 are not supported yet!";
    ADD_FAILURE();
  }

  GLScalerTestUtil::UnpackPlanarBitmap(luma_plane, 0, &actual);
  GLScalerTestUtil::UnpackUVBitmap(chroma_planes, &actual);

  const auto png_file_path = GetTestFilePath(
      scale_by_half_ ? FILE_PATH_LITERAL("half_of_one_of_16_color_rects.png")
                     : FILE_PATH_LITERAL("one_of_16_color_rects.png"));

  SkBitmap expected;
  if (!cc::ReadPNGFile(png_file_path, &expected)) {
    LOG(ERROR) << "Cannot read reference image: " << png_file_path.value();
    ADD_FAILURE();
    return;
  }

  expected = GLScalerTestUtil::CopyAndConvertToRGBA(expected);
  GLScalerTestUtil::ConvertRGBABitmapToYUV(&expected);

  constexpr float kAvgAbsoluteErrorLimit = 16.f;
  constexpr int kMaxAbsoluteErrorLimit = 0x80;
  if (!cc::MatchesBitmap(
          actual, expected,
          cc::FuzzyPixelComparator(false, 100.f, 0.f, kAvgAbsoluteErrorLimit,
                                   kMaxAbsoluteErrorLimit, 0))) {
    ADD_FAILURE();
    return;
  }
}

#undef PIXEL_COMPARATOR

// These tests work similarly to `GLRendererCopierPixelTest`, but test various
// request dimensions in more depth.
//
// Parameters:
// 0. Destiation for the CopyOutputRequest (native textures or system memory).
// 1. True if the result should be scaled by half in each dimension, false
//    otherwise.
// 2. True if the request should specify odd coordinates for the result
//    selection rectangle, false otherwise.
// 3. True if the request should specify odd dimensions for the result selection
//    rectangle, false otherwise.
class GLRendererCopierDimensionsPixelTest
    : public cc::PixelTest,
      public testing::WithParamInterface<
          std::tuple<CopyOutputResult::Destination, bool, bool>> {
 public:
  void SetUp() override {
    SetUpGLWithoutRenderer(gfx::SurfaceOrigin::kBottomLeft);

    texture_deleter_ =
        std::make_unique<TextureDeleter>(base::ThreadTaskRunnerHandle::Get());

    result_destination_ = std::get<0>(GetParam());
    scale_by_half_ = std::get<1>(GetParam());
    use_odd_offset_ = std::get<2>(GetParam());

    gl_ = context_provider()->ContextGL();
    copier_ = std::make_unique<GLRendererCopier>(context_provider(),
                                                 texture_deleter_.get());

    // For this test, use a generated bitmap, with 4 groups of 4 pixels each.
    const std::vector<SkColor> kCycle = {
        // Red:
        SkColorSetARGB(0xff, 0xff, 0x00, 0x00),
        SkColorSetARGB(0xff, 0xff, 0x00, 0x00),
        SkColorSetARGB(0xff, 0xff, 0x00, 0x00),
        SkColorSetARGB(0xff, 0xff, 0x00, 0x00),
        // Green:
        SkColorSetARGB(0xff, 0x00, 0xff, 0x00),
        SkColorSetARGB(0xff, 0x00, 0xff, 0x00),
        SkColorSetARGB(0xff, 0x00, 0xff, 0x00),
        SkColorSetARGB(0xff, 0x00, 0xff, 0x00),
        // Blue:
        SkColorSetARGB(0xff, 0x00, 0x00, 0xff),
        SkColorSetARGB(0xff, 0x00, 0x00, 0xff),
        SkColorSetARGB(0xff, 0x00, 0x00, 0xff),
        SkColorSetARGB(0xff, 0x00, 0x00, 0xff),
        // White:
        SkColorSetARGB(0xff, 0xff, 0xff, 0xff),
        SkColorSetARGB(0xff, 0xff, 0xff, 0xff),
        SkColorSetARGB(0xff, 0xff, 0xff, 0xff),
        SkColorSetARGB(0xff, 0xff, 0xff, 0xff),
    };
    source_bitmap_ = GLScalerTestUtil::CreateCyclicalTestImage(
        gfx::Size(800, 600), GLScalerTestUtil::VERTICAL_STRIPES, kCycle, 0);
    // source_bitmap_.setImmutable();
    source_bitmap_size_ =
        gfx::Size(source_bitmap_.width(), source_bitmap_.height());
  }

  void TearDown() override {
    DeleteSourceFramebuffer();
    DeleteSourceTexture();
    copier_.reset();
    texture_deleter_.reset();
  }

  gpu::gles2::GLES2Interface* gl() { return gl_; }

  GLRendererCopier* copier() { return copier_.get(); }

  gfx::Rect DrawToWindowSpace(const gfx::Size& source_size,
                              const gfx::Rect& draw_rect) {
    gfx::Rect window_rect = draw_rect;
    if (flipped_source_)
      window_rect.set_y(source_size.height() - window_rect.bottom());
    return window_rect;
  }

  GLuint CreateSourceTexture(SkBitmap source_bitmap) {
    CHECK_EQ(0u, source_texture_);
    gl_->GenTextures(1, &source_texture_);
    gl_->BindTexture(GL_TEXTURE_2D, source_texture_);
    gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl_->TexImage2D(GL_TEXTURE_2D, 0, source_gl_format_, source_bitmap.width(),
                    source_bitmap.height(), 0, source_gl_format_,
                    GL_UNSIGNED_BYTE,
                    CreateGLPixelsFromSkBitmap(source_bitmap, source_gl_format_,
                                               flipped_source_)
                        .get());
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

 protected:
  GLenum source_gl_format_ = GL_RGBA;
  bool have_source_texture_ = false;
  CopyOutputResult::Destination result_destination_;
  bool scale_by_half_;
  bool flipped_source_ = false;
  bool use_odd_offset_;
  SkBitmap source_bitmap_;
  gfx::Size source_bitmap_size_;

 private:
  gpu::gles2::GLES2Interface* gl_ = nullptr;
  std::unique_ptr<TextureDeleter> texture_deleter_;
  std::unique_ptr<GLRendererCopier> copier_;
  GLuint source_texture_ = 0;
  GLuint source_framebuffer_ = 0;
};

TEST_P(GLRendererCopierDimensionsPixelTest, ExecutesCopyRequestNV12) {
  if (result_destination_ ==
      CopyOutputRequest::ResultDestination::kNativeTextures) {
    // TODO(https://crbug.com/1216287): Enable once textures are supported.
    GTEST_SKIP() << "Enable once the NV12 format supports producing results to "
                    "a texture.";
  }

  // Result should contain 1px green strip at the beginning if the offset is
  // supposed to be odd.
  const gfx::Rect request_area = [this]() {
    // Capture 2x2 or 4x4 blue strip fragment, depending on scaling.
    gfx::Rect result =
        scale_by_half_ ? gfx::Rect(4, 0, 2, 2) : gfx::Rect(8, 0, 4, 4);

    // If we are supposed to ask for a rect with odd offset,
    // make sure that we capture 1 green pixel.
    if (use_odd_offset_) {
      result.set_x(result.x() - 1);
    }

    return result;
  }();

  // Create and execute a CopyOutputRequest via the GLRendererCopier.
  std::unique_ptr<CopyOutputResult> result;
  {
    base::RunLoop loop;
    auto request = std::make_unique<CopyOutputRequest>(
        CopyOutputRequest::ResultFormat::NV12_PLANES, result_destination_,
        base::BindOnce(
            [](std::unique_ptr<CopyOutputResult>* result,
               base::OnceClosure quit_closure,
               std::unique_ptr<CopyOutputResult> result_from_copier) {
              *result = std::move(result_from_copier);
              std::move(quit_closure).Run();
            },
            &result, loop.QuitClosure()));

    if (scale_by_half_) {
      request->SetUniformScaleRatio(2, 1);
    }

    request->set_result_selection(gfx::Rect(request_area));

    // Upload source texture to GL:
    const GLuint source_texture = CreateSourceTexture(source_bitmap_);
    CreateAndBindSourceFramebuffer(source_texture);

    copy_output::RenderPassGeometry geometry;
    // geometry.result_bounds not used by GLRendererCopier
    geometry.sampling_bounds =
        DrawToWindowSpace(source_bitmap_size_, gfx::Rect(source_bitmap_size_));
    geometry.result_selection = request->result_selection();
    geometry.readback_offset =
        DrawToWindowSpace(source_bitmap_size_, geometry.result_selection)
            .OffsetFromOrigin();

    copier()->CopyFromTextureOrFramebuffer(
        std::move(request), geometry, source_gl_format_,
        have_source_texture_ ? source_texture : 0, source_bitmap_size_,
        flipped_source_, gfx::ColorSpace::CreateSRGB());
    loop.Run();
  }

  // Check that a result was produced and is of the expected rect/size.
  ASSERT_TRUE(result);
  ASSERT_FALSE(result->IsEmpty());
  ASSERT_EQ(request_area, result->rect());

  const auto luma_nbytes = copy_output::GetLumaPlaneSize(*result);
  const auto luma_stride = copy_output::GetLumaPlaneStride(*result);

  const auto chroma_nbytes = copy_output::GetChromaPlaneSize(*result);
  const auto chroma_stride = copy_output::GetChromaPlaneStride(*result);

  std::unique_ptr<uint8_t[]> luma_plane =
      std::make_unique<uint8_t[]>(luma_nbytes);
  std::unique_ptr<uint8_t[]> chroma_planes =
      std::make_unique<uint8_t[]>(chroma_nbytes);

  // Examine the image in the |result|, and compare it to the baseline.
  if (result_destination_ == CopyOutputResult::Destination::kSystemMemory) {
    result->ReadNV12Planes(luma_plane.get(), luma_stride, chroma_planes.get(),
                           chroma_stride);
  } else {
    LOG(ERROR) << "Texture results for NV12 are not supported yet!";
    ADD_FAILURE();
  }

  SkBitmap source_bitmap_yuv = source_bitmap_;
  GLScalerTestUtil::ConvertRGBABitmapToYUV(&source_bitmap_yuv);

  // We've asked for a region that starts with one green pixel that is followed
  // by all blue pixels, grab the source colors after they have been converted
  // to YUV to have something to validate against:
  SkColor green_yuv = source_bitmap_yuv.getColor(4, 0);
  SkColor blue_yuv = source_bitmap_yuv.getColor(8, 0);

  // Validate first row of luma (first color channel):
  for (int col = 0; col < luma_stride; ++col) {
    if (col == 0 && use_odd_offset_) {
      EXPECT_NEAR(luma_plane[col], SkColorGetR(green_yuv), GetTolerance());
      continue;
    }

    EXPECT_NEAR(luma_plane[col], SkColorGetR(blue_yuv), GetTolerance());
  }

  // All other luma rows must match the first row:
  ValidateRows(luma_plane.get(), luma_stride, result->rect().height());

  // Validate first row of chroma (second and third color channel):
  for (int col = 0; col < chroma_stride; col += 2) {
    if (col == 0 && use_odd_offset_) {
      EXPECT_NEAR(chroma_planes[col], SkColorGetG(green_yuv), GetTolerance());
      EXPECT_NEAR(chroma_planes[col + 1], SkColorGetB(green_yuv),
                  GetTolerance());
      continue;
    }

    EXPECT_NEAR(chroma_planes[col], SkColorGetG(blue_yuv), GetTolerance());
    EXPECT_NEAR(chroma_planes[col + 1], SkColorGetB(blue_yuv), GetTolerance());
  }

  // All other chroma rows must match the first row:
  ValidateRows(chroma_planes.get(), chroma_stride,
               (result->rect().height() + 1) / 2);
}

TEST_P(GLRendererCopierDimensionsPixelTest, ExecutesCopyRequestI420) {
  if (result_destination_ ==
      CopyOutputRequest::ResultDestination::kNativeTextures) {
    // TODO(https://crbug.com/1216287): Enable once textures are supported.
    GTEST_SKIP() << "Enable once the I420 format supports producing results to "
                    "a texture.";
  }

  // Result should contain 1px green strip at the beginning if the offset is
  // supposed to be odd.
  const gfx::Rect request_area = [this]() {
    // Capture 2x2 or 4x4 blue strip fragment, depending on scaling.
    gfx::Rect result =
        scale_by_half_ ? gfx::Rect(4, 0, 2, 2) : gfx::Rect(8, 0, 4, 4);

    // If we are supposed to ask for a rect with odd offset,
    // make sure that we capture 1 green pixel.
    if (use_odd_offset_) {
      result.set_x(result.x() - 1);
    }

    return result;
  }();

  // Create and execute a CopyOutputRequest via the GLRendererCopier.
  std::unique_ptr<CopyOutputResult> result;
  {
    base::RunLoop loop;
    auto request = std::make_unique<CopyOutputRequest>(
        CopyOutputRequest::ResultFormat::I420_PLANES, result_destination_,
        base::BindOnce(
            [](std::unique_ptr<CopyOutputResult>* result,
               base::OnceClosure quit_closure,
               std::unique_ptr<CopyOutputResult> result_from_copier) {
              *result = std::move(result_from_copier);
              std::move(quit_closure).Run();
            },
            &result, loop.QuitClosure()));

    if (scale_by_half_) {
      request->SetUniformScaleRatio(2, 1);
    }

    request->set_result_selection(gfx::Rect(request_area));

    // Upload source texture to GL:
    const GLuint source_texture = CreateSourceTexture(source_bitmap_);
    CreateAndBindSourceFramebuffer(source_texture);

    copy_output::RenderPassGeometry geometry;
    // geometry.result_bounds not used by GLRendererCopier
    geometry.sampling_bounds =
        DrawToWindowSpace(source_bitmap_size_, gfx::Rect(source_bitmap_size_));
    geometry.result_selection = request->result_selection();
    geometry.readback_offset =
        DrawToWindowSpace(source_bitmap_size_, geometry.result_selection)
            .OffsetFromOrigin();

    copier()->CopyFromTextureOrFramebuffer(
        std::move(request), geometry, source_gl_format_,
        have_source_texture_ ? source_texture : 0, source_bitmap_size_,
        flipped_source_, gfx::ColorSpace::CreateSRGB());
    loop.Run();
  }

  // Check that a result was produced and is of the expected rect/size.
  ASSERT_TRUE(result);
  ASSERT_FALSE(result->IsEmpty());
  ASSERT_EQ(request_area, result->rect());

  // Examine the image in the |result|, and compare it to the baseline PNG file.

  const auto luma_nbytes = copy_output::GetLumaPlaneSize(*result);
  const auto luma_stride = copy_output::GetLumaPlaneStride(*result);

  const auto chroma_nbytes = copy_output::GetChromaPlaneSize(*result);
  const auto chroma_stride = copy_output::GetChromaPlaneStride(*result);

  std::unique_ptr<uint8_t[]> luma_plane =
      std::make_unique<uint8_t[]>(luma_nbytes);
  std::unique_ptr<uint8_t[]> chroma_plane_1 =
      std::make_unique<uint8_t[]>(chroma_nbytes);
  std::unique_ptr<uint8_t[]> chroma_plane_2 =
      std::make_unique<uint8_t[]>(chroma_nbytes);

  if (result_destination_ == CopyOutputResult::Destination::kSystemMemory) {
    result->ReadI420Planes(luma_plane.get(), luma_stride, chroma_plane_1.get(),
                           chroma_stride, chroma_plane_2.get(), chroma_stride);
  } else {
    LOG(ERROR) << "Texture results for I420 are not supported yet!";
    ADD_FAILURE();
  }

  SkBitmap source_bitmap_yuv = source_bitmap_;
  GLScalerTestUtil::ConvertRGBABitmapToYUV(&source_bitmap_yuv);

  // We've asked for a region that starts with one green pixel that is followed
  // by all blue pixels, validate:
  SkColor green_yuv = source_bitmap_yuv.getColor(7, 0);
  SkColor blue_yuv = source_bitmap_yuv.getColor(8, 0);

  // Validate first row of luma (first channel):
  for (int col = 0; col < luma_stride; ++col) {
    if (col == 0 && use_odd_offset_) {
      EXPECT_NEAR(luma_plane[col], SkColorGetR(green_yuv), GetTolerance());
      continue;
    }

    EXPECT_NEAR(luma_plane[col], SkColorGetR(blue_yuv), GetTolerance());
  }

  // All other luma rows must match the first row:
  ValidateRows(luma_plane.get(), luma_stride, result->rect().height());

  // Validate first row of chroma_1 (second channel):
  for (int col = 0; col < chroma_stride; ++col) {
    if (col == 0 && use_odd_offset_) {
      EXPECT_NEAR(chroma_plane_1[col], SkColorGetG(green_yuv), GetTolerance());
      continue;
    }

    EXPECT_NEAR(chroma_plane_1[col], SkColorGetG(blue_yuv), GetTolerance());
  }

  // All other chroma_1 rows must match the first row:
  ValidateRows(chroma_plane_1.get(), chroma_stride,
               (result->rect().height() + 1) / 2);

  // Validate first row of chroma_2 (third channel):
  for (int col = 0; col < chroma_stride; ++col) {
    if (col == 0 && use_odd_offset_) {
      EXPECT_NEAR(chroma_plane_2[col], SkColorGetB(green_yuv), GetTolerance());
      continue;
    }

    EXPECT_NEAR(chroma_plane_2[col], SkColorGetB(blue_yuv), GetTolerance());
  }

  // All other chroma_2 rows must match the first row:
  ValidateRows(chroma_plane_2.get(), chroma_stride,
               (result->rect().height() + 1) / 2);
}

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
        // Result destination.
        testing::Values(CopyOutputResult::Destination::kSystemMemory,
                        CopyOutputResult::Destination::kNativeTextures),
        // Result scaling: Scale by half?
        testing::Values(false, true),
        // Source content is vertically flipped?
        testing::Values(false, true)));

// Instantiate parameter sets for all possible combinations of scenarios
// GLRendererCopier will encounter, which will cause it to follow different
// workflows.
INSTANTIATE_TEST_SUITE_P(
    All,
    GLRendererCopierDimensionsPixelTest,
    testing::Combine(
        // Result destination.
        testing::Values(CopyOutputResult::Destination::kSystemMemory,
                        CopyOutputResult::Destination::kNativeTextures),
        // Result scaling: Scale by half?
        testing::Values(false, true),
        // Use odd offset?
        testing::Values(false, true)));

}  // namespace viz
