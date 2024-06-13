// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_FRAME_SINKS_VIDEO_CAPTURE_VIDEO_CAPTURE_OVERLAY_H_
#define COMPONENTS_VIZ_SERVICE_FRAME_SINKS_VIDEO_CAPTURE_VIDEO_CAPTURE_OVERLAY_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "components/viz/service/frame_sinks/video_capture/capturable_frame_sink.h"
#include "components/viz/service/viz_service_export.h"
#include "media/base/video_types.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/viz/privileged/mojom/compositing/frame_sink_video_capture.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/color_transform.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"

namespace media {
class VideoFrame;
}

namespace viz {

// An overlay image to be blitted onto video frames. A mojo client sets the
// image, position, and size of the overlay in the video frame; and then this
// VideoCaptureOverlay scales the image and maps its color space to match that
// of the video frame before the blitting.
//
// As an optimization, the client's bitmap image is transformed (scaled, color
// space converted, and pre-multiplied by alpha), and then this cached Sprite is
// re-used for blitting to all successive video frames until some change
// requires a different transformation. MakeRenderer() produces a Renderer
// callback that holds a reference to an existing Sprite, or will create a new
// one if necessary. The Renderer callback can then be run at any point in the
// future, unaffected by later image, size, or color space settings changes.
//
// The blit algorithm uses naive linear blending. Thus, the use of non-linear
// color spaces will cause losses in color accuracy.
class VIZ_SERVICE_EXPORT VideoCaptureOverlay
    : public mojom::FrameSinkVideoCaptureOverlay {
 public:
  // Interface for notifying the frame source when changes to the overlay's
  // state occur.
  class VIZ_SERVICE_EXPORT FrameSource {
   public:
    // Returns the current size of the source, or empty if unknown.
    virtual gfx::Size GetSourceSize() = 0;

    // Notifies the FrameSource that the given source |rect| needs to be
    // re-captured soon. One or more calls to this method will be followed-up
    // with a call to RequestRefreshFrame().
    virtual void InvalidateRect(const gfx::Rect& rect) = 0;

    // Notifies the FrameSource that another frame should be captured and have
    // its VideoCaptureOverlay re-rendered to reflect an updated overlay
    // image and/or position. The overlay image and position may change often,
    // so this method may be called frequently.
    virtual void RefreshNow() = 0;

    // Notifies the FrameSource that the VideoCaptureOverlay has lost its mojo
    // binding.
    virtual void OnOverlayConnectionLost(VideoCaptureOverlay* overlay) = 0;

   protected:
    virtual ~FrameSource();
  };

  // A OnceCallback that, when run, renders the overlay on a VideoFrame.
  using OnceRenderer = base::OnceCallback<void(media::VideoFrame*)>;

  // |frame_source| must outlive this instance.
  VideoCaptureOverlay(
      FrameSource& frame_source,
      mojo::PendingReceiver<mojom::FrameSinkVideoCaptureOverlay> receiver);

  VideoCaptureOverlay(const VideoCaptureOverlay&) = delete;
  VideoCaptureOverlay& operator=(const VideoCaptureOverlay&) = delete;

  ~VideoCaptureOverlay() override;

  // mojom::FrameSinkVideoCaptureOverlay implementation:
  void SetImageAndBounds(const SkBitmap& image, const gfx::RectF& bounds) final;
  void SetBounds(const gfx::RectF& bounds) final;
  void OnCapturedMouseEvent(const gfx::Point& coordinates) final {}

  const SkBitmap& bitmap() const { return image_; }

  struct VIZ_SERVICE_EXPORT CapturedFrameProperties {
    // Properties of the region associated with the video capture sub target
    // that has been selected for capture.
    CapturableFrameSink::RegionProperties region_properties;

    // The subsection of the video frame, in output pixels, that the overlay
    // gets outputted onto.
    gfx::Rect content_rect;

    // The frame's pixel format.
    media::VideoPixelFormat format;

    std::string ToString() const;
  };

  // Returns a OnceCallback that, when run, renders this VideoCaptureOverlay on
  // a VideoFrame. The overlay's position and size are computed based on the
  // given content |region_in_frame|. Returns a null OnceCallback if there is
  // nothing to render at this time.
  virtual OnceRenderer MakeRenderer(const CapturedFrameProperties& properties);

  struct VIZ_SERVICE_EXPORT BlendInformation {
    // Source region that we will blend from, expressed in the coordinate system
    // of the overlay's |image_|.
    gfx::Rect source_region;

    // Source region that we will blend from, expressed in the coordinate system
    // of the *scaled* overlay's |image_|. Scaled overlay's image is computed
    // by |sprite_|. This should have the same scale as the content (aka
    // VideoFrame).
    gfx::Rect source_region_scaled;

    // Destination region that we will blend into, expressed in the coordinate
    // system of the content (aka VideoFrame). This will be
    // 4:2:0-format-friendly (i.e. all dimensions and coordinates will be even).
    // This should be compatible with `CopyOutputResult::rect()`, which is
    // in turn influenced by `CopyOutputRequest::area()` and
    // `CopyOutputRequest::result_selection()`.
    gfx::Rect destination_region_content;

    std::string ToString() const;
  };

  // Computes information related to blending current overlay over the captured
  // frame described by |properties|. Returns nullopt if the blend needs to be
  // skipped (e.g. because it would be a no-op).
  std::optional<BlendInformation> CalculateBlendInformation(
      const CapturedFrameProperties& properties) const;

  // Returns a OnceCallback that renders all of the given |overlays| in
  // order. The remaining arguments are the same as in MakeRenderer(). This is a
  // convenience that produces a single callback, so that client code need not
  // deal with collections of callbacks. Returns a null OnceCallback if there is
  // nothing to render at this time.
  static OnceRenderer MakeCombinedRenderer(
      const std::vector<VideoCaptureOverlay*>& overlays,
      const CapturedFrameProperties& properties);

 private:
  // Transforms the overlay SkBitmap image by scaling and converting its color
  // space, and then blitting it onto a VideoFrame. The transformation is lazy:
  // Meaning, the transformation is executed upon the first call to Blit(), and
  // the result is cached for re-use for later Blit() calls. The transformation
  // is re-executed if the color space of the VideoFrame changes (rarely).
  class Sprite : public base::RefCounted<Sprite> {
   public:
    Sprite(const SkBitmap& image,
           const gfx::Size& size,
           const media::VideoPixelFormat format);

    Sprite(const Sprite&) = delete;
    Sprite& operator=(const Sprite&) = delete;

    const gfx::Size& size() const { return size_; }
    media::VideoPixelFormat format() const { return format_; }

    // Blends the transformed |image_| over the |frame|. |src_rect| describes
    // the region from |transformed_image_| (which is computed on-demand by
    // scaling |image_| to the desired |size_|) that will be blended over the
    // destination. |blit_rect| describes the region of |frame| that will be
    // blended over.
    void Blend(const gfx::Rect& src_rect,
               const gfx::Rect& blit_rect,
               media::VideoFrame* frame);

   private:
    friend class base::RefCounted<Sprite>;
    ~Sprite();

    void TransformImage();

    // As Sprites can be long-lived and hidden from external code within
    // callbacks, ensure that all Blit() calls are in-sequence.
    SEQUENCE_CHECKER(sequence_checker_);

    // Starts-out as the original, unscaled overlay image. The first call to
    // TransformImage() replaces it with a scaled one.
    SkBitmap image_;

    // The size, format, and color space of the cached transformed image.
    const gfx::Size size_;
    const media::VideoPixelFormat format_;
    gfx::ColorSpace color_space_;

    // The transformed source image data. For blitting to ARGB format video
    // frames, the source image data will consist of 4 elements per pixel pixel
    // (A, R, G, B). For blitting to the I420 format, the source image data is
    // not interlaved: Instead, there are 5 planes of data (one minus alpha, Y,
    // subsampled one minus alpha, U, V). For both formats, the color components
    // are premultiplied for more-efficient Blit()'s.
    std::unique_ptr<float[]> transformed_image_;
  };

  // Computes the region of the source that, if changed, would require
  // re-rendering the overlay.
  gfx::Rect ComputeSourceMutationRect() const;

  const raw_ref<FrameSource> frame_source_;

  mojo::Receiver<mojom::FrameSinkVideoCaptureOverlay> receiver_;

  // The currently-set overlay image.
  SkBitmap image_;

  // If empty, the overlay is currently hidden. Otherwise, this consists of
  // coordinates where the range [0.0,1.0) indicates the relative position+size
  // within the bounds of the video frame's content region (e.g., 0.0 refers to
  // the top or left edge; 1.0 to just after the bottom or right edge).
  gfx::RectF bounds_;

  // The current Sprite. This is set to null whenever a settings change requires
  // a new Sprite to be generated from the |image_|.
  scoped_refptr<Sprite> sprite_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_FRAME_SINKS_VIDEO_CAPTURE_VIDEO_CAPTURE_OVERLAY_H_
