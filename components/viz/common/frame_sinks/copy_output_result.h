// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_FRAME_SINKS_COPY_OUTPUT_RESULT_H_
#define COMPONENTS_VIZ_COMMON_FRAME_SINKS_COPY_OUTPUT_RESULT_H_

#include <array>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/threading/thread_checker.h"
#include "components/viz/common/resources/release_callback.h"
#include "components/viz/common/viz_common_export.h"
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
    // ownership of the texture (via a call to `TakeTextureOwnership()`) if it
    // is needed beyond the lifetime of the CopyOutputResult.
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
  };

  // Specifies how the results are delivered to the issuer of the request.
  // This should usually (but not always!) correspond to the value found in
  // CopyOutputRequest::result_destination() of the request that caused this
  // result to be produced. For details, see the comment on
  // CopyOutputRequest::ResultDestination.
  enum class Destination : uint8_t {
    // Place the results in system memory.
    kSystemMemory,
    // Place the results in native textures. The GPU textures are returned via a
    // mailbox. The caller can use |GetTextureResult()| and
    // |TakeTextureOwnership()| to access the results.
    kNativeTextures,
  };

  // Maximum number of planes allowed when returning software NV12 results.
  static constexpr size_t kNV12MaxPlanes = 2;

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

  // Returns a pointer with a mailbox referencing a texture-backed result, or
  // null if this is not a texture-backed result.
  // Clients can either:
  //   1. Let CopyOutputResult retain ownership and the texture will only be
  //      valid for use during CopyOutputResult's lifetime.
  //   2. Take over ownership of the texture by calling TakeTextureOwnership(),
  //      and the client must guarantee all the release callbacks will be run at
  //      some point.
  // Even when the returned pointer is non-null, the object that it points to
  // can be default-constructed (the resulting mailboxes can be empty) in the
  // case of a failed reply, in which case IsEmpty() would report true.
  // NOTE: The shared image referenced by the mailbox are read-only and only
  // accessible by raster interface (from the client's POV).
  struct VIZ_COMMON_EXPORT TextureResult {
    gpu::Mailbox mailbox;
    gfx::ColorSpace color_space;

    TextureResult(const gpu::Mailbox& mailbox,
                  const gfx::ColorSpace& color_space);

    TextureResult(const TextureResult& other);
    TextureResult& operator=(const TextureResult& other);
  };
  virtual const TextureResult* GetTextureResult() const;

  using ReleaseCallbacks = std::vector<ReleaseCallback>;
  // Returns a vector of release callbacks for the textures in |mailbox_holders|
  // array of TextureResult. `i`th element in this collection is a release
  // callback for the `i`th element in |mailbox_holders| array. The size of the
  // collection must match the number of valid entries in |mailbox_holders|
  // array. The vector will be empty iff the CopyOutputResult |IsEmpty()| is
  // true.
  virtual ReleaseCallbacks TakeTextureOwnership();

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
  virtual bool ReadI420Planes(uint8_t* y_out,
                              int y_out_stride,
                              uint8_t* u_out,
                              int u_out_stride,
                              uint8_t* v_out,
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
  virtual bool ReadNV12Planes(uint8_t* y_out,
                              int y_out_stride,
                              uint8_t* uv_out,
                              int uv_out_stride) const;

  // Copies the result into |dest|. The result is in N32Premul form. Returns
  // true if successful, or false if: 1) the result is empty, or 2) the result
  // format is not RGBA and conversion is not implemented.
  virtual bool ReadRGBAPlane(uint8_t* dest, int stride) const;

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

// Subclass of CopyOutputResult that holds references to textures (via
// mailboxes). The owner of the result must take ownership of the textures if it
// wants to use them by calling |TakeTextureOwnership()|, and then call the
// ReleaseCallbacks when the textures will no longer be used to release
// ownership and allow the textures to be reused or destroyed. If ownership is
// not claimed, it will be released when this class is destroyed.
class VIZ_COMMON_EXPORT CopyOutputTextureResult : public CopyOutputResult {
 public:
  // Construct a non-empty texture result:
  CopyOutputTextureResult(Format format,
                          const gfx::Rect& rect,
                          TextureResult texture_result,
                          ReleaseCallbacks release_callbacks);

  CopyOutputTextureResult(const CopyOutputTextureResult&) = delete;
  CopyOutputTextureResult& operator=(const CopyOutputTextureResult&) = delete;

  ~CopyOutputTextureResult() override;

  const TextureResult* GetTextureResult() const override;
  ReleaseCallbacks TakeTextureOwnership() override;

 private:
  TextureResult texture_result_;
  ReleaseCallbacks release_callbacks_;
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

 private:
  friend class CopyOutputResult;
  explicit ScopedSkBitmap(const CopyOutputResult* result);

  raw_ptr<const CopyOutputResult> result_ = nullptr;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_FRAME_SINKS_COPY_OUTPUT_RESULT_H_
