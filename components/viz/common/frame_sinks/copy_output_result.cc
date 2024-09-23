// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/frame_sinks/copy_output_result.h"

#include <utility>

#include "base/check_op.h"
#include "base/notreached.h"
#include "third_party/libyuv/include/libyuv.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#include "ui/gfx/color_space.h"

namespace viz {

CopyOutputResult::TextureResult::TextureResult(
    const CopyOutputResult::TextureResult& other) = default;
CopyOutputResult::TextureResult& CopyOutputResult::TextureResult::operator=(
    const CopyOutputResult::TextureResult& other) = default;

CopyOutputResult::TextureResult::TextureResult(
    const gpu::Mailbox& mailbox,
    const gfx::ColorSpace& color_space)
    : mailbox(mailbox), color_space(color_space) {}

CopyOutputResult::CopyOutputResult(Format format,
                                   Destination destination,
                                   const gfx::Rect& rect,
                                   bool needs_lock_for_bitmap)
    : format_(format),
      destination_(destination),
      rect_(rect),
      needs_lock_for_bitmap_(needs_lock_for_bitmap) {
  DCHECK(format_ == Format::RGBA || format_ == Format::I420_PLANES ||
         format == Format::NV12);
  DCHECK(destination_ == Destination::kSystemMemory ||
         destination_ == Destination::kNativeTextures);
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

const CopyOutputResult::TextureResult* CopyOutputResult::GetTextureResult()
    const {
  return nullptr;
}

CopyOutputResult::ReleaseCallbacks CopyOutputResult::TakeTextureOwnership() {
  return {};
}

bool CopyOutputResult::ReadI420Planes(uint8_t* y_out,
                                      int y_out_stride,
                                      uint8_t* u_out,
                                      int u_out_stride,
                                      uint8_t* v_out,
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
  if (bitmap.colorType() == kBGRA_8888_SkColorType) {
    return 0 == libyuv::ARGBToI420(pixels, bitmap.rowBytes(), y_out,
                                   y_out_stride, u_out, u_out_stride, v_out,
                                   v_out_stride, bitmap.width(),
                                   bitmap.height());
  } else if (bitmap.colorType() == kRGBA_8888_SkColorType) {
    return 0 == libyuv::ABGRToI420(pixels, bitmap.rowBytes(), y_out,
                                   y_out_stride, u_out, u_out_stride, v_out,
                                   v_out_stride, bitmap.width(),
                                   bitmap.height());
  }

  // Other SkBitmap color types could be supported, but are currently never
  // being used.
  NOTIMPLEMENTED() << "Unsupported format, bitmap.colorType()="
                   << bitmap.colorType();
  return false;
}

bool CopyOutputResult::ReadNV12Planes(uint8_t* y_out,
                                      int y_out_stride,
                                      uint8_t* uv_out,
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
  if (bitmap.colorType() == kBGRA_8888_SkColorType) {
    return 0 == libyuv::ARGBToNV12(pixels, bitmap.rowBytes(), y_out,
                                   y_out_stride, uv_out, uv_out_stride,
                                   bitmap.width(), bitmap.height());
  } else if (bitmap.colorType() == kRGBA_8888_SkColorType) {
    return 0 == libyuv::ABGRToNV12(pixels, bitmap.rowBytes(), y_out,
                                   y_out_stride, uv_out, uv_out_stride,
                                   bitmap.width(), bitmap.height());
  }

  // Other SkBitmap color types could be supported, but are currently never
  // being used.
  NOTIMPLEMENTED() << "Unsupported format, bitmap.colorType()="
                   << bitmap.colorType();
  return false;
}

bool CopyOutputResult::ReadRGBAPlane(uint8_t* dest, int stride) const {
  auto scoped_sk_bitmap = ScopedAccessSkBitmap();
  const SkBitmap& bitmap = scoped_sk_bitmap.bitmap();
  if (!bitmap.readyToDraw())
    return false;

  DCHECK(bitmap.colorSpace());
  SkImageInfo image_info =
      SkImageInfo::MakeN32(bitmap.width(), bitmap.height(), kPremul_SkAlphaType,
                           bitmap.refColorSpace());
  bitmap.readPixels(image_info, dest, stride, 0, 0);
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

CopyOutputTextureResult::CopyOutputTextureResult(
    Format format,
    const gfx::Rect& rect,
    TextureResult texture_result,
    ReleaseCallbacks release_callbacks)
    : CopyOutputResult(format, Destination::kNativeTextures, rect, false),
      texture_result_(std::move(texture_result)),
      release_callbacks_(std::move(release_callbacks)) {
  // If we're constructing empty result, all mailbox_holders must be zero.
  // Otherwise, the first mailbox must be non-zero.
  DCHECK_EQ(rect.IsEmpty(), texture_result_.mailbox.IsZero());
  // If we're constructing empty result, the callbacks must be empty.
  // From definition of implication: p => q  <=>  !p || q.
  DCHECK(!rect.IsEmpty() || release_callbacks_.empty());
  // Color space must be valid for non-empty results.
  DCHECK(rect.IsEmpty() || texture_result_.color_space.IsValid());
}

CopyOutputTextureResult::~CopyOutputTextureResult() {
  for (auto& release_callback : release_callbacks_) {
    // No need to check if release_callback is valid, when texture ownership
    // is taken away from us, we zero out release_callbacks_ and the loop would
    // not be entered.
    std::move(release_callback).Run(gpu::SyncToken(), false);
  }
}

const CopyOutputResult::TextureResult*
CopyOutputTextureResult::GetTextureResult() const {
  return &texture_result_;
}

CopyOutputResult::ReleaseCallbacks
CopyOutputTextureResult::TakeTextureOwnership() {
  texture_result_.mailbox = {};
  texture_result_.color_space = {};

  CopyOutputResult::ReleaseCallbacks result = std::move(release_callbacks_);
  release_callbacks_.clear();

  return result;
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

}  // namespace viz
