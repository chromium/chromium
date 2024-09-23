// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_FRAME_SINKS_BLIT_REQUEST_H_
#define COMPONENTS_VIZ_COMMON_FRAME_SINKS_BLIT_REQUEST_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/viz_common_export.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "third_party/skia/include/core/SkImage.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

namespace viz {

// `BlendBitmap` can be added to `BlitRequest`, and signifies that the caller
// would like to blend (SrcOver) the specified bitmap onto the results of the
// `CopyOutputRequest` that (transitively, via `BlitRequest`) contains this
// blend bitmap.
class VIZ_COMMON_EXPORT BlendBitmap {
 public:
  // |source_region| is expressed in |bitmap|'s coordinate system
  // (i.e. 0,0 width x height rectangle). |destination_region| is expressed
  // in the captured content's coordinate system. |bitmap| is the bitmap
  // that will be blended over the CopyOutputRequest's output.
  explicit BlendBitmap(const gfx::Rect& source_region,
                       const gfx::Rect& destination_region,
                       sk_sp<SkImage> image);

  BlendBitmap(BlendBitmap&& other);
  BlendBitmap& operator=(BlendBitmap&& other);

  ~BlendBitmap();

  SkImage* image() const { return image_.get(); }
  const gfx::Rect& source_region() const { return source_region_; }
  const gfx::Rect& destination_region() const { return destination_region_; }

  std::string ToString() const;

 private:
  // Region in the |image_| that will be blended over.
  gfx::Rect source_region_;
  // Region in the destination that will be blended onto.
  gfx::Rect destination_region_;
  // The image that will be blended.
  sk_sp<SkImage> image_;
};

// Enum used to specify letteboxing behavior for a BlitRequest.
enum class LetterboxingBehavior {
  // No letterboxing is needed - only the destination region will be written
  // into by the handler of CopyOutputRequest.
  kDoNotLetterbox,
  // Letterboxing is needed - everything outside of the destination region
  // will be filled with black by the handler of CopyOutputRequest.
  kLetterbox
};

// Structure describing a blit operation that can be appended to
// `CopyOutputRequest` if the callers want to place the results of the operation
// in textures that they own.
class VIZ_COMMON_EXPORT BlitRequest {
 public:
  explicit BlitRequest(const gfx::Point& destination_region_offset,
                       LetterboxingBehavior letterboxing_behavior,
                       const gpu::Mailbox& mailbox,
                       const gpu::SyncToken& sync_token,
                       bool populates_gpu_memory_buffer);

  BlitRequest(BlitRequest&& other);
  BlitRequest& operator=(BlitRequest&& other);

  ~BlitRequest();

  std::string ToString() const;

  const gfx::Point& destination_region_offset() const {
    return destination_region_offset_;
  }

  LetterboxingBehavior letterboxing_behavior() const {
    return letterboxing_behavior_;
  }

  const gpu::Mailbox& mailbox() const { return mailbox_; }

  const gpu::SyncToken& sync_token() const { return sync_token_; }

  bool populates_gpu_memory_buffer() const {
    return populates_gpu_memory_buffer_;
  }

  // Appends a new `BlendBitmap` request to this blit request.
  // |source_region| is expressed in |image|'s coordinate system
  // (i.e. 0,0 width x height rectangle). |destination_region| is expressed
  // in the captured content's coordinate system. |image| is the image
  // that will be blended over the CopyOutputRequest's output.
  void AppendBlendBitmap(const gfx::Rect& source_region,
                         const gfx::Rect& destination_region,
                         sk_sp<SkImage> image) {
    blend_bitmaps_.emplace_back(source_region, destination_region,
                                std::move(image));
  }

  const std::vector<BlendBitmap>& blend_bitmaps() const {
    return blend_bitmaps_;
  }

 private:
  // Offset from the origin of the image represented by the `mailbox_`.
  // The results of the blit request will be placed at that offset in those
  // images.
  gfx::Point destination_region_offset_;

  // Specifies the letterboxing behavior of this request.
  LetterboxingBehavior letterboxing_behavior_;

  // The image that will be populated. The texture can (but doesn't have to) be
  // backed by a GpuMemoryBuffer.
  gpu::Mailbox mailbox_;

  // SyncToken to wait on before accessing `mailbox_`.
  gpu::SyncToken sync_token_;

  // True if `mailbox_` describes a shared image that has been created from a
  // GpuMemoryBuffer. In this case, the CopyOutputResult needs to be sent out
  // only after it's safe to map the GpuMemoryBuffer to system memory.
  bool populates_gpu_memory_buffer_;

  // Collection of bitmaps that will be blended onto the texture.
  // They will be blended in order (so if i < j, bitmap at offset i will
  // be blended before bitmap at offset j), using SrcOver blend mode.
  std::vector<BlendBitmap> blend_bitmaps_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_FRAME_SINKS_BLIT_REQUEST_H_
