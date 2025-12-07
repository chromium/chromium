// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/frame_sinks/copy_output_result.h"

#include <cstddef>
#include <utility>

#include "base/check_op.h"
#include "base/memory/scoped_refptr.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "third_party/libyuv/include/libyuv.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#include "ui/gfx/color_space.h"

namespace viz {

CopyOutputResult::CopyOutputResult(Format format,
                                   Destination destination,
                                   const gfx::Rect& rect,
                                   bool needs_lock_for_bitmap)
    : format_(format),
      destination_(destination),
      rect_(rect),
      needs_lock_for_bitmap_(needs_lock_for_bitmap) {
  DCHECK(format_ == Format::RGBA || format_ == Format::RGBAF16 ||
         format_ == Format::I420_PLANES || format == Format::NV12);
  DCHECK(destination_ == Destination::kSystemMemory ||
         destination_ == Destination::kSharedImage);
}

CopyOutputResult::~CopyOutputResult() = default;

bool CopyOutputResult::IsEmpty() const {
  return rect_.IsEmpty();
}

bool CopyOutputResult::LockSkBitmap() const {
  return true;
}

void CopyOutputResult::UnlockSkBitmap() const {}

const SkBitmap& CopyOutputResult::AsSkBitmap() const {
  DCHECK(!cached_bitmap_.readyToDraw() || cached_bitmap_.colorSpace());
  return cached_bitmap_;
}

CopyOutputResult::ScopedSkBitmap CopyOutputResult::ScopedAccessSkBitmap()
    const {
  return ScopedSkBitmap(this);
}

ReleaseCallback CopyOutputResult::TakeSharedImageOwnership() {
  return {};
}

scoped_refptr<gpu::ClientSharedImage> CopyOutputResult::GetSharedImage() {
  return nullptr;
}

bool CopyOutputResult::ReadI420Planes(base::span<uint8_t> y_out,
                                      int y_out_stride,
                                      base::span<uint8_t> u_out,
                                      int u_out_stride,
                                      base::span<uint8_t> v_out,
                                      int v_out_stride) const {
  auto scoped_sk_bitmap = ScopedAccessSkBitmap();
  const SkBitmap& bitmap = scoped_sk_bitmap.bitmap();
  if (!bitmap.readyToDraw())
    return false;

  const uint8_t* pixels = static_cast<uint8_t*>(bitmap.getPixels());
  // The conversion below ignores color space completely, and it's not even
  // sRGB→Rec.709. Unfortunately, hand-optimized routines are not available, and
  // a perfect conversion using gfx::ColorTransform would execute way too
  // slowly. See SoftwareRenderer for related comments on its lack of color
  // space management (due to performance concerns).
  // TODO(crbug.com/384959115): Verify span size before calling into libyuv.
  if (bitmap.colorType() == kBGRA_8888_SkColorType) {
    return 0 == libyuv::ARGBToI420(pixels, bitmap.rowBytes(), y_out.data(),
                                   y_out_stride, u_out.data(), u_out_stride,
                                   v_out.data(), v_out_stride, bitmap.width(),
                                   bitmap.height());
  } else if (bitmap.colorType() == kRGBA_8888_SkColorType) {
    return 0 == libyuv::ABGRToI420(pixels, bitmap.rowBytes(), y_out.data(),
                                   y_out_stride, u_out.data(), u_out_stride,
                                   v_out.data(), v_out_stride, bitmap.width(),
                                   bitmap.height());
  }

  // Other SkBitmap color types could be supported, but are currently never
  // being used.
  NOTIMPLEMENTED() << "Unsupported format, bitmap.colorType()="
                   << bitmap.colorType();
  return false;
}

bool CopyOutputResult::ReadNV12Planes(base::span<uint8_t> y_out,
                                      int y_out_stride,
                                      base::span<uint8_t> uv_out,
                                      int uv_out_stride) const {
  auto scoped_sk_bitmap = ScopedAccessSkBitmap();
  const SkBitmap& bitmap = scoped_sk_bitmap.bitmap();
  if (!bitmap.readyToDraw())
    return false;

  const uint8_t* pixels = static_cast<uint8_t*>(bitmap.getPixels());
  // The conversion below ignores color space completely, and it's not even
  // sRGB→Rec.709. Unfortunately, hand-optimized routines are not available, and
  // a perfect conversion using gfx::ColorTransform would execute way too
  // slowly. See SoftwareRenderer for related comments on its lack of color
  // space management (due to performance concerns).
  // TODO(crbug.com/384959115): Verify span size before calling into libyuv.
  if (bitmap.colorType() == kBGRA_8888_SkColorType) {
    return 0 == libyuv::ARGBToNV12(pixels, bitmap.rowBytes(), y_out.data(),
                                   y_out_stride, uv_out.data(), uv_out_stride,
                                   bitmap.width(), bitmap.height());
  } else if (bitmap.colorType() == kRGBA_8888_SkColorType) {
    return 0 == libyuv::ABGRToNV12(pixels, bitmap.rowBytes(), y_out.data(),
                                   y_out_stride, uv_out.data(), uv_out_stride,
                                   bitmap.width(), bitmap.height());
  }

  // Other SkBitmap color types could be supported, but are currently never
  // being used.
  NOTIMPLEMENTED() << "Unsupported format, bitmap.colorType()="
                   << bitmap.colorType();
  return false;
}

bool CopyOutputResult::ReadRGBAPlane(base::span<uint8_t> dest,
                                     int stride) const {
  auto scoped_sk_bitmap = ScopedAccessSkBitmap();
  const SkBitmap& bitmap = scoped_sk_bitmap.bitmap();
  if (!bitmap.readyToDraw())
    return false;

  DCHECK(bitmap.colorSpace());
  SkImageInfo image_info =
      SkImageInfo::MakeN32(bitmap.width(), bitmap.height(), kPremul_SkAlphaType,
                           bitmap.refColorSpace());
  CHECK_GE(dest.size(), image_info.computeByteSize(stride));
  bitmap.readPixels(image_info, dest.data(), stride, 0, 0);
  return true;
}

gfx::ColorSpace CopyOutputResult::GetRGBAColorSpace() const {
  auto scoped_sk_bitmap = ScopedAccessSkBitmap();
  const SkBitmap& bitmap = scoped_sk_bitmap.bitmap();
  if (!bitmap.readyToDraw())
    return gfx::ColorSpace();
  DCHECK(bitmap.colorSpace());
  return gfx::ColorSpace(*(bitmap.colorSpace()));
}

CopyOutputSkBitmapResult::CopyOutputSkBitmapResult(const gfx::Rect& rect,
                                                   SkBitmap bitmap)
    : CopyOutputSkBitmapResult(Format::RGBA, rect, std::move(bitmap)) {}

CopyOutputSkBitmapResult::CopyOutputSkBitmapResult(Format format,
                                                   const gfx::Rect& rect,
                                                   SkBitmap bitmap)
    : CopyOutputResult(format, Destination::kSystemMemory, rect, false) {
  if (!rect.IsEmpty()) {
    DCHECK(!bitmap.pixelRef() || bitmap.pixelRef()->unique());
    DCHECK(!bitmap.readyToDraw() || bitmap.colorSpace());
    // Hold a reference to the |bitmap|'s pixels, for AsSkBitmap().
    *(cached_bitmap()) = std::move(bitmap);
  }
}

const SkBitmap& CopyOutputSkBitmapResult::AsSkBitmap() const {
  SkBitmap* const bitmap = cached_bitmap();

  if (rect().IsEmpty())
    return *bitmap;  // Return "null" bitmap for empty result.

  const SkImageInfo image_info = SkImageInfo::MakeN32Premul(
      rect().width(), rect().height(), bitmap->refColorSpace());
  if (bitmap->info() == image_info && bitmap->readyToDraw())
    return *bitmap;  // Return bitmap in expected format.

  // The bitmap is not in the "native optimized" format. Convert it once for
  // this and all future calls of this method.
  SkBitmap replacement;
  replacement.allocPixels(image_info);
  replacement.eraseColor(SK_ColorBLACK);
  SkPixmap src_pixmap;
  if (bitmap->peekPixels(&src_pixmap)) {
    // Note: writePixels() can fail, but then the replacement bitmap will be
    // left with part/all solid black due to the eraseColor() call above.
    replacement.writePixels(src_pixmap);
  }
  *bitmap = replacement;

  return *bitmap;
}

CopyOutputSkBitmapResult::~CopyOutputSkBitmapResult() = default;

CopyOutputSharedImageResult::CopyOutputSharedImageResult(
    Format format,
    const gfx::Rect& rect,
    const gpu::Mailbox& mailbox,
    const gfx::ColorSpace& color_space,
    std::string_view debug_label,
    ReleaseCallback release_callback)
    : CopyOutputSharedImageResult(
          format,
          rect,
          base::WrapRefCounted(new gpu::ClientSharedImage(
              mailbox,
              gpu::SharedImageInfo{GetSharedImageFormatFor(format), rect.size(),
                                   color_space, kDefaultSharedImageUsage,
                                   debug_label})),
          std::move(release_callback)) {}

CopyOutputSharedImageResult::CopyOutputSharedImageResult(
    Format format,
    const gfx::Rect& rect,
    scoped_refptr<gpu::ClientSharedImage> shared_image,
    ReleaseCallback release_callback)
    : CopyOutputResult(format, Destination::kSharedImage, rect, false),
      shared_image_(std::move(shared_image)),
      release_callback_(std::move(release_callback)) {
  // check non-null `shared_image_`
  DCHECK(shared_image_);
  // If we're constructing empty result, all mailbox_holders must be zero.
  // Otherwise, the first mailbox must be non-zero.
  DCHECK_EQ(rect.IsEmpty(), shared_image_->mailbox().IsZero());
  // If we're constructing empty result, the callbacks must be empty.
  // From definition of implication: p => q  <=>  !p || q.
  DCHECK(!rect.IsEmpty() || release_callback_.is_null());
  // Color space must be valid for non-empty results.
  DCHECK(rect.IsEmpty() || shared_image_->color_space().IsValid());
}

CopyOutputSharedImageResult::~CopyOutputSharedImageResult() {
  if (release_callback_) {
    std::move(release_callback_).Run(gpu::SyncToken(), false);
  }
}

scoped_refptr<gpu::ClientSharedImage>
CopyOutputSharedImageResult::GetSharedImage() {
  return shared_image_;
}

ReleaseCallback CopyOutputSharedImageResult::TakeSharedImageOwnership() {
  return std::move(release_callback_);
}

CopyOutputResult::ScopedSkBitmap::ScopedSkBitmap() = default;

CopyOutputResult::ScopedSkBitmap::ScopedSkBitmap(
    const CopyOutputResult* result) {
  DCHECK(result);
  if (!result->needs_lock_for_bitmap_ || result->LockSkBitmap())
    result_ = result;
}

CopyOutputResult::ScopedSkBitmap::ScopedSkBitmap(ScopedSkBitmap&& other) {
  *this = std::move(other);
}

CopyOutputResult::ScopedSkBitmap::~ScopedSkBitmap() {
  reset();
}

CopyOutputResult::ScopedSkBitmap& CopyOutputResult::ScopedSkBitmap::operator=(
    ScopedSkBitmap&& other) {
  DCHECK_CALLED_ON_VALID_THREAD(other.thread_checker_);
  reset();
  std::swap(result_, other.result_);
  return *this;
}

void CopyOutputResult::ScopedSkBitmap::reset() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!result_)
    return;
  if (result_->needs_lock_for_bitmap_) {
#if DCHECK_IS_ON()
    // We are going to unlock the content of the bitmap, so we need to make
    // sure there is no other ref of the bitmap content.
    auto* ref = bitmap().pixelRef();
    DCHECK(!ref || ref->unique());
#endif
    result_->UnlockSkBitmap();
    result_ = nullptr;
  }
}

SkBitmap CopyOutputResult::ScopedSkBitmap::GetOutScopedBitmap() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!result_)
    return SkBitmap();

  const auto& bitmap = result_->AsSkBitmap();

  // If result_->needs_lock_for_bitmap_ is false, then bitmap can be used out of
  // the scope.
  if (!result_->needs_lock_for_bitmap_)
    return bitmap;

  // Make a new SkBitmap and copy content to this new SkBitmap.
  SkBitmap bitmap_copy;
  if (bitmap.readyToDraw()) {
    bitmap_copy.allocPixels(bitmap.info());
    bitmap.readPixels(bitmap_copy.pixmap(), 0, 0);
  }
  return bitmap_copy;
}

CopyOutputBitmapWithMetadata
CopyOutputResult::ScopedSkBitmap::GetOutScopedBitmapAndMetadata() const {
  return CopyOutputBitmapWithMetadata{.bitmap = GetOutScopedBitmap()};
}

VIZ_COMMON_EXPORT SharedImageFormat
GetSharedImageFormatFor(CopyOutputResult::Format format) {
  switch (format) {
    case CopyOutputResult::Format::RGBA:
      return SinglePlaneFormat::kRGBA_8888;
    case CopyOutputResult::Format::RGBAF16:
      return SinglePlaneFormat::kRGBA_F16;
    case CopyOutputResult::Format::I420_PLANES:
      return MultiPlaneFormat::kI420;
    case CopyOutputResult::Format::NV12:
      return MultiPlaneFormat::kNV12;
  }
}

}  // namespace viz
