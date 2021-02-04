// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_FRAME_SINKS_COPY_OUTPUT_RESULT_H_
#define COMPONENTS_VIZ_COMMON_FRAME_SINKS_COPY_OUTPUT_RESULT_H_

#include <memory>

#include "components/viz/common/resources/single_release_callback.h"
#include "components/viz/common/viz_common_export.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
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
    // A normal bitmap in system memory. AsSkBitmap() will return a bitmap in
    // "N32Premul" form.
    RGBA_BITMAP,
    // A GL_RGBA texture, referred to by a gpu::Mailbox. Client code can
    // optionally take ownership of the texture (via TakeTextureOwnership()), if
    // it is needed beyond the lifetime of CopyOutputResult.
    RGBA_TEXTURE,
    // I420 format planes in system memory. This is intended to be used
    // internally within the VIZ component to support video capture. When
    // requesting this format, results can only be delivered on the same task
    // runner sequence that runs the DirectRenderer implementation.
    I420_PLANES,
  };

  CopyOutputResult(Format format, const gfx::Rect& rect);

  virtual ~CopyOutputResult();

  // Returns false if the request succeeded and the data accessors will return
  // valid references.
  bool IsEmpty() const;

  // Returns the format of this result.
  Format format() const { return format_; }

  // Returns the result Rect, which is the position and size of the image data
  // within the surface/layer (see CopyOutputRequest::set_area()). If a scale
  // ratio was set in the request, this will be in the scaled, NOT the original,
  // coordinate space.
  const gfx::Rect& rect() const { return rect_; }
  const gfx::Size& size() const { return rect_.size(); }

  // Convenience to provide this result in SkBitmap form. Returns a
  // !readyToDraw() bitmap if this result is empty or if a conversion is not
  // possible in the current implementation. The returned SkBitmap also carries
  // its color space information.
  virtual const SkBitmap& AsSkBitmap() const;

  // Returns a pointer with the gpu::Mailbox referencing a RGBA_TEXTURE result,
  // or null if this is not a RGBA_TEXTURE result. Clients can either:
  //   1. Let CopyOutputResult retain ownership and the texture will only be
  //      valid for use during CopyOutputResult's lifetime.
  //   2. Take over ownership of the texture by calling TakeTextureOwnership(),
  //      and the client must guarantee the release callback will be run at some
  //      point.
  // Even when non-null the resulting mailbox can be empty in the case of a
  // failed reply, in which case IsEmpty() would report true. The texture object
  // associated with the mailbox has a GL_TEXTURE_2D target.
  struct TextureResult {
    gpu::Mailbox mailbox;
    gpu::SyncToken sync_token;
    gfx::ColorSpace color_space;

    TextureResult() = default;

    TextureResult(const gpu::Mailbox& mailbox,
                  const gpu::SyncToken& sync_token,
                  const gfx::ColorSpace& color_space)
        : mailbox(mailbox), sync_token(sync_token), color_space(color_space) {}
  };
  virtual const TextureResult* GetTextureResult() const;
  virtual std::unique_ptr<SingleReleaseCallback> TakeTextureOwnership();

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

  // Copies the result of an RGBA_BITMAP into |dest|. The result is in N32Premul
  // form. Returns true if successful, or false if: 1) the result is empty, or
  // 2) the result format is not RGBA_BITMAP and conversion is not implemented.
  virtual bool ReadRGBAPlane(uint8_t* dest, int stride) const;

  // Returns the color space of the image data returned by ReadRGBAPlane().
  virtual gfx::ColorSpace GetRGBAColorSpace() const;

 protected:
  // Accessor for subclasses to initialize the cached SkBitmap.
  SkBitmap* cached_bitmap() const { return &cached_bitmap_; }

 private:
  const Format format_;
  const gfx::Rect rect_;

  // Cached bitmap returned by the default implementation of AsSkBitmap().
  mutable SkBitmap cached_bitmap_;

  DISALLOW_COPY_AND_ASSIGN(CopyOutputResult);
};

// Subclass of CopyOutputResult that provides a RGBA_BITMAP result from an
// SkBitmap (or an I420_PLANES result based on a SkBitmap).
class VIZ_COMMON_EXPORT CopyOutputSkBitmapResult : public CopyOutputResult {
 public:
  CopyOutputSkBitmapResult(Format format,
                           const gfx::Rect& rect,
                           const SkBitmap& bitmap);
  CopyOutputSkBitmapResult(const gfx::Rect& rect, const SkBitmap& bitmap);
  ~CopyOutputSkBitmapResult() override;

  const SkBitmap& AsSkBitmap() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CopyOutputSkBitmapResult);
};

// Subclass of CopyOutputResult that holds a reference to a texture (via
// a mailbox). The owner of the result must take ownership of the texture
// if it wants to use it by calling TakeTextureOwnership(), and then call the
// SingleReleaseCallback when the texture will no longer be used to release
// ownership and allow the texture to be reused or destroyed. If ownership is
// not claimed, it will be released when this class is destroyed.
class VIZ_COMMON_EXPORT CopyOutputTextureResult : public CopyOutputResult {
 public:
  CopyOutputTextureResult(
      const gfx::Rect& rect,
      const gpu::Mailbox& mailbox,
      const gpu::SyncToken& sync_token,
      const gfx::ColorSpace& color_space,
      std::unique_ptr<SingleReleaseCallback> release_callback);
  ~CopyOutputTextureResult() override;

  const TextureResult* GetTextureResult() const override;
  std::unique_ptr<SingleReleaseCallback> TakeTextureOwnership() override;

 private:
  TextureResult texture_result_;
  std::unique_ptr<SingleReleaseCallback> release_callback_;

  DISALLOW_COPY_AND_ASSIGN(CopyOutputTextureResult);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_FRAME_SINKS_COPY_OUTPUT_RESULT_H_
