// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_FRAME_SINKS_COPY_OUTPUT_RESULT_H_
#define COMPONENTS_VIZ_COMMON_FRAME_SINKS_COPY_OUTPUT_RESULT_H_

#include <array>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/threading/thread_checker.h"
#include "components/viz/common/resources/release_callback.h"
#include "components/viz/common/viz_common_export.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect.h"

class SkBitmap;

namespace viz {

// Base class for providing the result of a CopyOutputRequest. Implementations
// that execute CopyOutputRequests will use a subclass implementation to define
// data storage, access and ownership semantics relative to the lifetime of the
// CopyOutputResult instance.
class VIZ_COMMON_EXPORT CopyOutputResult {
 public:
  enum class Format : uint8_t {
    // A normal bitmap. When the results are returned in system memory, the
    // AsSkBitmap() will return a bitmap in "N32Premul" form. When the results
    // are returned in a texture, it will be a SharedImageFormat::kRGBA_8888
    // texture referred to by a gpu::Mailbox. Client code can optionally take
    // ownership of the texture (via a call to `TakeSharedImageOwnership()`) if
    // it is needed beyond the lifetime of the CopyOutputResult.
    RGBA,
    // I420 format planes. This is intended to be used internally within the VIZ
    // component to support video capture. When requesting this format, results
    // can only be delivered on the same task runner sequence that runs the
    // DirectRenderer implementation. For now, I420 format can be requested only
    // for system memory.
    I420_PLANES,
    // An NV12 image. This is intended to be used internally within the VIZ
    // component to support video capture. When requesting this format, results
    // can only be delivered on the same task runner sequence that runs the
    // DirectRenderer implementation.
    NV12,
    // A RGBAF16 shared texture. Results should be returned in a texture, will
    // be a SharedImageFormat::kRGBA_F16.
    RGBAF16,
  };

  // Specifies how the results are delivered to the issuer of the request.
  // This should usually (but not always!) correspond to the value found in
  // `CopyOutputRequest::result_destination()` of the request that caused this
  // result to be produced. For details, see the comment on
  // `CopyOutputRequest::ResultDestination`.
  enum class Destination : uint8_t {
    // Place the results in system memory.
    kSystemMemory,
    // Place the results in a shared image. The caller can use
    // `GetSharedImageResult()` and `TakeSharedImageOwnership()` to access the
    // results.
    kSharedImage,
  };

  // Maximum number of planes allowed when returning software NV12 results.
  static constexpr size_t kNV12MaxPlanes = 2;

  // Defines the default usage for shared images which are the destination of a
  // `CopyOutputRequest`. Since these shared images will eventually make it back
  // to the client that issued that request, the usage here needs to capture the
  // variety of clients' eventual allowed usages. Note that CopyOutputRequests
  // are not writable via raster (by contract).
  static constexpr gpu::SharedImageUsageSet kDefaultSharedImageUsage =
      gpu::SHARED_IMAGE_USAGE_RASTER_READ |
      gpu::SHARED_IMAGE_USAGE_DISPLAY_READ |
      gpu::SHARED_IMAGE_USAGE_DISPLAY_WRITE;

  CopyOutputResult(Format format,
                   Destination destination,
                   const gfx::Rect& rect,
                   bool needs_lock_for_bitmap);

  CopyOutputResult(const CopyOutputResult&) = delete;
  CopyOutputResult& operator=(const CopyOutputResult&) = delete;

  virtual ~CopyOutputResult();

  // Returns false if the request succeeded and the data accessors will return
  // valid references.
  bool IsEmpty() const;

  // Returns the format of this result.
  Format format() const { return format_; }
  // Returns the destination of this result.
  Destination destination() const { return destination_; }

  // Returns the result Rect, which is the position and size of the image data
  // within the surface/layer (see CopyOutputRequest::set_area()). If a scale
  // ratio was set in the request, this will be in the scaled, NOT the original,
  // coordinate space.
  const gfx::Rect& rect() const { return rect_; }
  const gfx::Size& size() const { return rect_.size(); }

  class ScopedSkBitmap;
  // Return a ScopedSkBitmap object. The scoped_sk_bitmap.bitmap() can be used
  // to access the SkBitmap. The API user should not keep a copy of
  // scoped_sk_bitmap.bitmap(), since the content SkBitmap could become invalid
  // after ScopedSkBitmap is released.
  ScopedSkBitmap ScopedAccessSkBitmap() const;

  // Get the shared image referencing a texture-backed result, or null if this
  // is not a texture-backed result.
  // Clients can either:
  //   1. Let CopyOutputResult retain ownership and the texture will only be
  //      valid for use during the CopyOutputResult's lifetime.
  //   2. Take over ownership by calling `TakeSharedImageOwnership()`, and the
  //      client must guarantee that all release callbacks will be run.
  virtual scoped_refptr<gpu::ClientSharedImage> GetSharedImage();

  // Returns a release callback for the contained shared image. The callback
  // will be empty iff the CopyOutputResult `IsEmpty()` is true.
  virtual ReleaseCallback TakeSharedImageOwnership();

  //
  // Subsampled YUV format result description
  // (valid for I420 and for NV12 formats)
  //
  // Since I420 and NV12 pixel formats subsample chroma planes and the results
  // are returned in a planar manner, care must be taken when interpreting the
  // results and when calculating sizes of memory regions for `ReadI420Planes()`
  // and `ReadNV12()` methods.
  //
  // Callers should follow the memory region size requirements specified in
  // `ReadI420Planes()` and `ReadNV12()` methods, and are advised to call
  // `set_result_selection()` with an even-sized, even-offset gfx::Rect
  // if they intend to "stitch" the results into a subregion of an existing
  // buffer by copying them. Memory region size calculation helpers are
  // available in copy_output_util.h.
  //

  // Copies the image planes of an I420_PLANES result to the caller-provided
  // memory. Returns true if successful, or false if: 1) this result is empty,
  // or 2) the result format is not I420_PLANES and does not provide a
  // conversion implementation.
  //
  // |y_out|, |u_out| and |v_out| point to the start of the memory regions to
  // receive each plane. These memory regions must have the following sizes:
  //
  //   Y plane: y_out_stride * size().height() bytes, with
  //            y_out_stride >= size().width()
  //   U plane: u_out_stride * CEIL(size().height() / 2) bytes, with
  //            u_out_stride >= CEIL(size().width() / 2)
  //   V plane: v_out_stride * CEIL(size().height() / 2) bytes, with
  //            v_out_stride >= CEIL(size().width() / 2)
  //
  // The color space is always Rec.709 (see gfx::ColorSpace::CreateREC709()).
  virtual bool ReadI420Planes(base::span<uint8_t> y_out,
                              int y_out_stride,
                              base::span<uint8_t> u_out,
                              int u_out_stride,
                              base::span<uint8_t> v_out,
                              int v_out_stride) const;
  // Copies the image planes of an NV12 result to the caller-provided memory.
  // Returns true if successful, or false if: 1) this result is empty, or 2) the
  // result format is not NV12 and does not provide a conversion implementation.
  //
  // |y_out| and |uv_out| point to the start of the memory regions to
  // receive each plane. These memory regions must have the following sizes:
  //
  //   Y plane:   y_out_stride * size().height() bytes, with
  //              y_out_stride >= size().width()
  //   UV plane:  uv_out_stride * CEIL(size().height() / 2) bytes, with
  //              uv_out_stride >= 2 * CEIL(size().width() / 2)
  //
  // The color space is always Rec.709 (see gfx::ColorSpace::CreateREC709()).
  virtual bool ReadNV12Planes(base::span<uint8_t> y_out,
                              int y_out_stride,
                              base::span<uint8_t> uv_out,
                              int uv_out_stride) const;

  // Copies the result into |dest|. The result is in N32Premul form. Returns
  // true if successful, or false if: 1) the result is empty, or 2) the result
  // format is not RGBA and conversion is not implemented.
  bool ReadRGBAPlane(base::span<uint8_t> dest, int stride) const;

  // Returns the color space of the image data returned by ReadRGBAPlane().
  virtual gfx::ColorSpace GetRGBAColorSpace() const;

 protected:
  // Lock the content of SkBitmap returned from AsSkBitmap() call.
  // Return true, if lock operation is successful, implementations should
  // keep SkBitmap content validate until UnlockSkBitmap() is called.
  virtual bool LockSkBitmap() const;

  // Unlock the content of SkBitmap returned from AsSkBitmap() call.
  virtual void UnlockSkBitmap() const;

  // Convenience to provide this result in SkBitmap form. Returns a
  // !readyToDraw() bitmap if this result is empty or if a conversion is not
  // possible in the current implementation. The returned SkBitmap also carries
  // its color space information.
  virtual const SkBitmap& AsSkBitmap() const;

  // Accessor for subclasses to initialize the cached SkBitmap.
  SkBitmap* cached_bitmap() const { return &cached_bitmap_; }

 private:
  const Format format_;
  const Destination destination_;
  const gfx::Rect rect_;
  const bool needs_lock_for_bitmap_;

  // Cached bitmap returned by the default implementation of AsSkBitmap().
  mutable SkBitmap cached_bitmap_;
};

// Subclass of CopyOutputResult that provides a RGBA result from an
// SkBitmap (or an I420_PLANES result based on a SkBitmap). Implies that the
// destination is kSystemMemory.
class VIZ_COMMON_EXPORT CopyOutputSkBitmapResult : public CopyOutputResult {
 public:
  CopyOutputSkBitmapResult(Format format,
                           const gfx::Rect& rect,
                           SkBitmap bitmap);
  CopyOutputSkBitmapResult(const gfx::Rect& rect, SkBitmap bitmap);

  CopyOutputSkBitmapResult(const CopyOutputSkBitmapResult&) = delete;
  CopyOutputSkBitmapResult& operator=(const CopyOutputSkBitmapResult&) = delete;

  ~CopyOutputSkBitmapResult() override;

  const SkBitmap& AsSkBitmap() const override;
};

// Subclass of `CopyOutputResult` that holds `ClientSharedImage`. The owner of
// the result must take ownership of the shared image if it wants to use them
// by calling `TakeSharedImageOwnership()`, and then call the `ReleaseCallback`
// when the shared image will no longer be used to release ownership and allow
// the shared image to be reused or destroyed. If ownership is not claimed, it
// will be released when this class is destroyed.
class VIZ_COMMON_EXPORT CopyOutputSharedImageResult : public CopyOutputResult {
 public:
  // Construct a non-empty shared-image result;
  // will create unowned `ClientSharedImage` with the provided metadata.
  CopyOutputSharedImageResult(Format format,
                              const gfx::Rect& rect,
                              const gpu::Mailbox& mailbox,
                              const gfx::ColorSpace& color_space,
                              std::string_view debug_label,
                              ReleaseCallback release_callback);

  // Construct a non-empty shared-image result; `shared_image` must be non-null.
  CopyOutputSharedImageResult(
      Format format,
      const gfx::Rect& rect,
      scoped_refptr<gpu::ClientSharedImage> shared_image,
      ReleaseCallback release_callback);

  CopyOutputSharedImageResult(const CopyOutputSharedImageResult&) = delete;
  CopyOutputSharedImageResult& operator=(const CopyOutputSharedImageResult&) =
      delete;

  ~CopyOutputSharedImageResult() override;

  scoped_refptr<gpu::ClientSharedImage> GetSharedImage() override;

  ReleaseCallback TakeSharedImageOwnership() override;

 private:
  scoped_refptr<gpu::ClientSharedImage> shared_image_;
  ReleaseCallback release_callback_;
};

// Output bitmap and metadata.
struct VIZ_COMMON_EXPORT CopyOutputBitmapWithMetadata {
  SkBitmap bitmap;
};

// Scoped class for accessing SkBitmap in CopyOutputRequest.
// It cannot be used across threads.
class VIZ_COMMON_EXPORT CopyOutputResult::ScopedSkBitmap {
 public:
  ScopedSkBitmap();
  ScopedSkBitmap(ScopedSkBitmap&& other);
  ~ScopedSkBitmap();
  ScopedSkBitmap& operator=(ScopedSkBitmap&& other);

  void reset();

  // Accesses SkBitmap which can only be used in the scope of the
  // ScopedSkBitmap.
  const SkBitmap bitmap() const {
    return result_ ? result_->AsSkBitmap() : SkBitmap();
  }

  // Returns a SkBitmap which can be used out the scope of the ScopedSkBitmap.
  // It makes a copy of the content in CopyOutputResult if it is needed.
  SkBitmap GetOutScopedBitmap() const;

  // Returns a SkBitmap along with other metadata. Makes a copy of the content
  // in CopyOutputResult if needed.
  CopyOutputBitmapWithMetadata GetOutScopedBitmapAndMetadata() const;

 private:
  friend class CopyOutputResult;
  explicit ScopedSkBitmap(const CopyOutputResult* result);

  raw_ptr<const CopyOutputResult> result_ = nullptr;

  THREAD_CHECKER(thread_checker_);
};

// Translate `CopyOutputResult::Format` to `SharedImageFormat`
VIZ_COMMON_EXPORT SharedImageFormat
GetSharedImageFormatFor(CopyOutputResult::Format format);

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_FRAME_SINKS_COPY_OUTPUT_RESULT_H_
