// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_HOST_CLIENT_FRAME_SINK_VIDEO_CAPTURER_H_
#define COMPONENTS_VIZ_HOST_CLIENT_FRAME_SINK_VIDEO_CAPTURER_H_

#include <vector>

#include "base/callback.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/host/viz_host_export.h"
#include "media/base/video_types.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/viz/privileged/mojom/compositing/frame_sink_video_capture.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"

namespace viz {

// Client library for using FrameSinkVideoCapturer. Clients should use this
// instead of talking directly to FrameSinkVideoCapturer in order to survive Viz
// crashes.
//
// An instance of ClientFrameSinkVideoCapturer must only be used in the same
// sequence (e.g., single-threaded).
//
// TODO(samans): Move this class and all its dependencies to the client
// directory.
class VIZ_HOST_EXPORT ClientFrameSinkVideoCapturer
    : private mojom::FrameSinkVideoConsumer {
 public:
  // A re-connectable FrameSinkVideoCaptureOverlay. See CreateOverlay().
  class VIZ_HOST_EXPORT Overlay : public mojom::FrameSinkVideoCaptureOverlay {
   public:
    Overlay(base::WeakPtr<ClientFrameSinkVideoCapturer> client_capturer,
            int32_t stacking_index);
    ~Overlay() final;

    int32_t stacking_index() const { return stacking_index_; }

    // mojom::FrameSinkVideoCaptureOverlay implementation.
    void SetImageAndBounds(const SkBitmap& image,
                           const gfx::RectF& bounds) final;
    void SetBounds(const gfx::RectF& bounds) final;

   private:
    friend class ClientFrameSinkVideoCapturer;
    void DisconnectPermanently();
    void EstablishConnection(mojom::FrameSinkVideoCapturer* capturer);

    SEQUENCE_CHECKER(sequence_checker_);

    base::WeakPtr<ClientFrameSinkVideoCapturer> client_capturer_;
    const int32_t stacking_index_;
    mojo::Remote<mojom::FrameSinkVideoCaptureOverlay> overlay_;

    SkBitmap image_;
    gfx::RectF bounds_;

    DISALLOW_COPY_AND_ASSIGN(Overlay);
  };

  using EstablishConnectionCallback = base::RepeatingCallback<void(
      mojo::PendingReceiver<mojom::FrameSinkVideoCapturer>)>;

  explicit ClientFrameSinkVideoCapturer(EstablishConnectionCallback callback);
  ~ClientFrameSinkVideoCapturer() override;

  // See FrameSinkVideoCapturer for documentation.
  void SetFormat(media::VideoPixelFormat format, gfx::ColorSpace color_space);
  void SetMinCapturePeriod(base::TimeDelta min_capture_period);
  void SetMinSizeChangePeriod(base::TimeDelta min_period);
  void SetResolutionConstraints(const gfx::Size& min_size,
                                const gfx::Size& max_size,
                                bool use_fixed_aspect_ratio);
  void SetAutoThrottlingEnabled(bool enabled);
  void ChangeTarget(const base::Optional<FrameSinkId>& frame_sink_id);
  void Stop();
  void RequestRefreshFrame();

  // Similar to FrameSinkVideoCapturer::Start, but takes in a pointer directly
  // to the FrameSinkVideoConsumer implemenation class (as opposed to a
  // mojo::InterfacePtr or a proxy object).
  void Start(mojom::FrameSinkVideoConsumer* consumer);

  // Similar to Stop() but also resets the consumer immediately so no further
  // messages (even OnStopped()) will be delivered to the consumer.
  void StopAndResetConsumer();

  // Similar to FrameSinkVideoCapturer::CreateOverlay, except that it returns an
  // owned pointer to an Overlay.
  std::unique_ptr<Overlay> CreateOverlay(int32_t stacking_index);

 private:
  struct Format {
    Format(media::VideoPixelFormat pixel_format, gfx::ColorSpace color_space);

    media::VideoPixelFormat pixel_format;
    gfx::ColorSpace color_space;
  };

  struct ResolutionConstraints {
    ResolutionConstraints(const gfx::Size& min_size,
                          const gfx::Size& max_size,
                          bool use_fixed_aspect_ratio);

    gfx::Size min_size;
    gfx::Size max_size;
    bool use_fixed_aspect_ratio;
  };

  // mojom::FrameSinkVideoConsumer implementation.
  void OnFrameCaptured(
      base::ReadOnlySharedMemoryRegion data,
      media::mojom::VideoFrameInfoPtr info,
      const gfx::Rect& content_rect,
      mojo::PendingRemote<mojom::FrameSinkVideoConsumerFrameCallbacks>
          callbacks) final;
  void OnStopped() final;

  // Establishes connection to FrameSinkVideoCapturer and sends the existing
  // configuration.
  void EstablishConnection();

  // Called when the message pipe is gone. Will call EstablishConnection after
  // some delay.
  void OnConnectionError();

  void StartInternal();

  void OnOverlayDestroyed(Overlay* overlay);

  SEQUENCE_CHECKER(sequence_checker_);

  // The following variables keep the latest arguments provided to their
  // corresponding method in mojom::FrameSinkVideoCapturer. The arguments are
  // saved so we can resend them if viz crashes and a new FrameSinkVideoCapturer
  // has to be created.
  base::Optional<Format> format_;
  base::Optional<base::TimeDelta> min_capture_period_;
  base::Optional<base::TimeDelta> min_size_change_period_;
  base::Optional<ResolutionConstraints> resolution_constraints_;
  base::Optional<bool> auto_throttling_enabled_;
  base::Optional<FrameSinkId> target_;
  // Overlays are owned by the callers of CreateOverlay().
  std::vector<Overlay*> overlays_;
  bool is_started_ = false;

  mojom::FrameSinkVideoConsumer* consumer_ = nullptr;
  EstablishConnectionCallback establish_connection_callback_;
  mojo::Remote<mojom::FrameSinkVideoCapturer> capturer_remote_;
  mojo::Receiver<mojom::FrameSinkVideoConsumer> consumer_receiver_{this};

  base::WeakPtrFactory<ClientFrameSinkVideoCapturer> weak_factory_{this};
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_HOST_CLIENT_FRAME_SINK_VIDEO_CAPTURER_H_
