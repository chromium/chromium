// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_FRAME_SINKS_VIDEO_CAPTURE_FRAME_SINK_VIDEO_CAPTURER_IMPL_H_
#define COMPONENTS_VIZ_SERVICE_FRAME_SINKS_VIDEO_CAPTURE_FRAME_SINK_VIDEO_CAPTURER_IMPL_H_

#include <stdint.h>

#include <memory>
#include <queue>
#include <vector>
#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/unguessable_token.h"
#include "components/viz/common/quads/compositor_frame_metadata.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/service/frame_sinks/video_capture/capturable_frame_sink.h"
#include "components/viz/service/frame_sinks/video_capture/in_flight_frame_delivery.h"
#include "components/viz/service/frame_sinks/video_capture/interprocess_frame_pool.h"
#include "components/viz/service/frame_sinks/video_capture/video_capture_overlay.h"
#include "components/viz/service/viz_service_export.h"
#include "media/base/video_frame.h"
#include "media/capture/content/video_capture_oracle.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/viz/privileged/mojom/compositing/frame_sink_video_capture.mojom.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace gfx {
class Size;
}  // namespace gfx

namespace viz {

class CopyOutputResult;
class FrameSinkVideoCapturerManager;

// Captures the frames of a CompositorFrameSink's surface as a video stream. See
// mojom for usage details.
//
// FrameSinkVideoCapturerImpl is owned by FrameSinkManagerImpl. An instance is
// destroyed either: 1) just after the Mojo binding has closed; or 2) when the
// FrameSinkManagerImpl is shutting down.
//
// The capturer also works with FrameSinkManagerImpl to resolve the capture
// target, a CapturableFrameSink, from a given FrameSinkId. Since there is a
// possible race between when the capturer learns of a new FrameSinkId and when
// FrameSinkManagerImpl learns of it (e.g., because Mojo method invocations can
// be out-of-order), the capturer allows for the possibility that a requested
// target will resolve at a later point. In this case, it is the responsibility
// of FrameSinkManagerImpl to call SetResolvedTarget() once the target becomes
// known to it.
//
// Once the target is resolved, this capturer attaches to it to receive events
// of interest regarding the frame flow, display timiming, and changes to the
// frame sink's surface. For some subset of frames, decided by
// media::VideoCaptureOracle, this capturer will make a CopyOutputRequest on the
// surface. Successful CopyOutputResults are then copied into pooled shared
// memory for efficient transport to the consumer.
class VIZ_SERVICE_EXPORT FrameSinkVideoCapturerImpl final
    : public CapturableFrameSink::Client,
      public VideoCaptureOverlay::FrameSource,
      public mojom::FrameSinkVideoCapturer {
 public:
  // |frame_sink_manager| must outlive this instance. Binds this instance to the
  // Mojo message pipe endpoint in |receiver|, but |receiver| may be empty for
  // unit testing.
  FrameSinkVideoCapturerImpl(
      FrameSinkVideoCapturerManager* frame_sink_manager,
      mojo::PendingReceiver<mojom::FrameSinkVideoCapturer> receiver,
      std::unique_ptr<media::VideoCaptureOracle> oracle,
      bool log_to_webrtc);

  ~FrameSinkVideoCapturerImpl() final;

  // The currently-requested frame sink for capture. The frame sink manager
  // calls this when it learns of a new CapturableFrameSink to see if the target
  // can be resolved.
  const FrameSinkId& requested_target() const { return requested_target_; }

  // Sets the resolved target, detaching this capturer from the previous target
  // (if any), and attaching to the new target. This is called by the frame sink
  // manager. If |target| is null, the capturer goes idle and expects this
  // method to be called again in the near future, once the target becomes known
  // to the frame sink manager.
  void SetResolvedTarget(CapturableFrameSink* target);

  // Notifies this capturer that the current target will be destroyed, and the
  // FrameSinkId associated with it has become invalid. This is called by the
  // frame sink manager.
  void OnTargetWillGoAway();

  // mojom::FrameSinkVideoCapturer implementation:
  void SetFormat(media::VideoPixelFormat format,
                 const gfx::ColorSpace& color_space) final;
  void SetMinCapturePeriod(base::TimeDelta min_capture_period) final;
  void SetMinSizeChangePeriod(base::TimeDelta min_period) final;
  void SetResolutionConstraints(const gfx::Size& min_size,
                                const gfx::Size& max_size,
                                bool use_fixed_aspect_ratio) final;
  void SetAutoThrottlingEnabled(bool enabled) final;
  void ChangeTarget(const base::Optional<FrameSinkId>& frame_sink_id) final;
  void Start(mojo::PendingRemote<mojom::FrameSinkVideoConsumer> consumer) final;
  void Stop() final;
  void RequestRefreshFrame() final;
  void CreateOverlay(int32_t stacking_index,
                     mojo::PendingReceiver<mojom::FrameSinkVideoCaptureOverlay>
                         receiver) final;

  // Default configuration.
  static constexpr media::VideoPixelFormat kDefaultPixelFormat =
      media::PIXEL_FORMAT_I420;
  static constexpr gfx::ColorSpace kDefaultColorSpace =
      gfx::ColorSpace::CreateREC709();

  // The maximum number of frames in-flight in the capture pipeline, reflecting
  // the storage capacity dedicated for this purpose. Example numbers, for a
  // frame pool that is fully-allocated with 10 frames of size 1920x1080, using
  // the I420 pixel format (12 bits per pixel). Then:
  //
  //   storage_bytes_for_all_ten_frames = 10 * (1920 * 1080 * 12/8)
  //     --> ~29.7 MB
  //
  // In practice, only 0-3 frames will be in-flight at a time, depending on the
  // content change rate and system performance.
  static constexpr int kDesignLimitMaxFrames = 10;

  // A safe, sustainable maximum number of frames in-flight. In other words,
  // exceeding 60% of the design limit is considered "red line" operation.
  static constexpr float kTargetPipelineUtilization = 0.6f;

 private:
  friend class FrameSinkVideoCapturerTest;

  using OracleFrameNumber =
      decltype(std::declval<media::VideoCaptureOracle>().next_frame_number());

  // Starts the refresh frame timer to guarantee a frame representing the most
  // up-to-date content will be sent to the consumer in the near future. This
  // refresh operation will be canceled if a compositing event triggers a frame
  // capture in the meantime.
  void ScheduleRefreshFrame();

  // Returns the delay that should be used when setting the refresh timer. This
  // is based on the current oracle prediction for frame duration.
  base::TimeDelta GetDelayBeforeNextRefreshAttempt() const;

  // Called whenever a major damage event, such as a capture parameter change, a
  // resolved target change, etc., occurs. This marks the entire source as dirty
  // and ensures the consumer will receive a refresh frame with up-to-date
  // content.
  void RefreshEntireSourceSoon();

  // Executes a refresh capture, if conditions permit. Otherwise, schedules a
  // later retry. Note that the retry "polling" should be a short-term state,
  // since it only occurs until the oracle allows the next frame capture to take
  // place. If a refresh was already pending, it is canceled in favor of this
  // new refresh.
  void RefreshSoon();

  // CapturableFrameSink::Client implementation:
  void OnFrameDamaged(const gfx::Size& frame_size,
                      const gfx::Rect& damage_rect,
                      base::TimeTicks target_display_time,
                      const CompositorFrameMetadata& frame_metadata) final;

  // VideoCaptureOverlay::FrameSource implementation:
  gfx::Size GetSourceSize() final;
  void InvalidateRect(const gfx::Rect& rect) final;
  void OnOverlayConnectionLost(VideoCaptureOverlay* overlay) final;

  void InvalidateEntireSource();

  // Returns a list of the overlays in rendering order.
  std::vector<VideoCaptureOverlay*> GetOverlaysInOrder() const;

  // Consults the VideoCaptureOracle to decide whether to capture a frame,
  // then ensures prerequisites are met before initiating the capture: that
  // there is a consumer present and that the pipeline is not already full.
  void MaybeCaptureFrame(media::VideoCaptureOracle::Event event,
                         const gfx::Rect& damage_rect,
                         base::TimeTicks event_time,
                         const CompositorFrameMetadata& frame_metadata);

  // Extracts the image data from the copy output |result|, populating the
  // |content_rect| region of a [possibly letterboxed] video |frame|.
  void DidCopyFrame(int64_t capture_frame_number,
                    OracleFrameNumber oracle_frame_number,
                    int64_t content_version,
                    const gfx::Rect& content_rect,
                    VideoCaptureOverlay::OnceRenderer overlay_renderer,
                    scoped_refptr<media::VideoFrame> frame,
                    base::TimeTicks request_time,
                    std::unique_ptr<CopyOutputResult> result);

  // Places the frame in the |delivery_queue_| and calls MaybeDeliverFrame(),
  // one frame at a time, in-order. |frame| may be null to indicate a
  // completed, but unsuccessful capture.
  void OnFrameReadyForDelivery(int64_t capture_frame_number,
                               OracleFrameNumber oracle_frame_number,
                               const gfx::Rect& content_rect,
                               scoped_refptr<media::VideoFrame> frame);

  // Delivers a |frame| to the consumer, if the VideoCaptureOracle allows
  // it. |frame| can be null to indicate a completed, but unsuccessful capture.
  // In this case, some state will be updated, but nothing will be sent to the
  // consumer.
  void MaybeDeliverFrame(OracleFrameNumber oracle_frame_number,
                         const gfx::Rect& content_rect,
                         scoped_refptr<media::VideoFrame> frame);

  // For ARGB format, ensures that every dimension of |size| is positive. For
  // I420 format, ensures that every dimension is even and at least 2.
  gfx::Size AdjustSizeForPixelFormat(const gfx::Size& size);

  // Expands |rect| such that its x, y, right, and bottom values are even
  // numbers.
  static gfx::Rect ExpandRectToI420SubsampleBoundaries(const gfx::Rect& rect);

  void OnLog(const std::string& message);

  // Owner/Manager of this instance.
  FrameSinkVideoCapturerManager* const frame_sink_manager_;

  // Mojo receiver for this instance.
  mojo::Receiver<mojom::FrameSinkVideoCapturer> receiver_{this};

  // Represents this instance as an issuer of CopyOutputRequests. The Surface
  // uses this to auto-cancel stale requests (i.e., prior requests that did not
  // execute). Also, the implementations that execute CopyOutputRequests use
  // this as a hint to cache objects for a huge performance improvement.
  const base::UnguessableToken copy_request_source_;

  // Use the default base::TimeTicks clock; but allow unit tests to provide a
  // replacement.
  const base::TickClock* clock_;

  // Current image format.
  media::VideoPixelFormat pixel_format_ = kDefaultPixelFormat;
  gfx::ColorSpace color_space_ = kDefaultColorSpace;

  // Models current content change/draw behavior and proposes when to capture
  // frames, and at what size and frame rate.
  const std::unique_ptr<media::VideoCaptureOracle> oracle_;

  // The target requested by the client, as provided in the last call to
  // ChangeTarget().
  FrameSinkId requested_target_;

  // The resolved target of video capture, or null if the requested target does
  // not yet exist (or no longer exists).
  CapturableFrameSink* resolved_target_ = nullptr;

  // The current video frame consumer. This is set when Start() is called and
  // cleared when Stop() is called.
  mojo::Remote<mojom::FrameSinkVideoConsumer> consumer_;

  // The portion of the source content that has changed, but has not yet been
  // captured.
  gfx::Rect dirty_rect_;

  // Allows determining whether or not the frame size has changed since the last
  // captured frame.
  gfx::Rect last_frame_visible_rect_;

  // These are sequence counters used to ensure that the frames are being
  // delivered in the same order they are captured.
  int64_t next_capture_frame_number_ = 0;
  int64_t next_delivery_frame_number_ = 0;

  // This timer is started whenever the consumer needs another frame delivered.
  // This might be because: 1) the consumer was just started and needs an
  // initial frame; 2) the capture target changed; 3) the oracle rejected
  // an event for timing reasons; 4) to satisfy explicit requests for a refresh
  // frame, when RequestRefreshFrame() has been called.
  //
  // Note: This is always set, but the instance is overridden for unit testing.
  base::Optional<base::OneShotTimer> refresh_frame_retry_timer_;

  // Provides a pool of VideoFrames that can be efficiently delivered across
  // processes. The size of this pool is used to limit the maximum number of
  // frames in-flight at any one time.
  InterprocessFramePool frame_pool_;

  // Increased every time the source content changes or a forced refresh is
  // requested.
  int64_t content_version_ = 0;

  int64_t content_version_in_marked_frame_ = -1;

  gfx::Size marked_frame_size_;

  // A queue of captured frames pending delivery. This queue is used to re-order
  // frames, if they should happen to be captured out-of-order.
  struct CapturedFrame {
    int64_t capture_frame_number;
    OracleFrameNumber oracle_frame_number;
    gfx::Rect content_rect;
    scoped_refptr<media::VideoFrame> frame;
    CapturedFrame(int64_t capture_frame_number,
                  OracleFrameNumber oracle_frame_number,
                  const gfx::Rect& content_rect,
                  scoped_refptr<media::VideoFrame> frame);
    CapturedFrame(const CapturedFrame& other);
    ~CapturedFrame();
    bool operator<(const CapturedFrame& other) const;
  };
  std::priority_queue<CapturedFrame> delivery_queue_;

  // The Oracle-provided media timestamp of the first frame. This is used to
  // compute the relative media stream timestamps for each successive frame.
  base::Optional<base::TimeTicks> first_frame_media_ticks_;

  // Zero or more overlays to be rendered over each captured video frame. The
  // order of the entries in this map determines the order in which each overlay
  // is rendered. This is important because alpha blending between overlays can
  // make a difference in the overall results.
  base::flat_map<int32_t, std::unique_ptr<VideoCaptureOverlay>> overlays_;

  // The visible height of the top-controls in the last CompositorFrameMetadata
  // received.
  double last_top_controls_visible_height_ = 0.f;

  // This class assumes its control operations and async callbacks won't execute
  // simultaneously.
  SEQUENCE_CHECKER(sequence_checker_);

  // A weak pointer factory used for cancelling consumer feedback from any
  // in-flight frame deliveries.
  base::WeakPtrFactory<media::VideoCaptureOracle> feedback_weak_factory_;

  // Enables debug log messages to be sent to webrtc native log.
  const bool log_to_webrtc_;

  // A weak pointer factory used for cancelling the results from any in-flight
  // copy output requests.
  base::WeakPtrFactory<FrameSinkVideoCapturerImpl> capture_weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FrameSinkVideoCapturerImpl);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_FRAME_SINKS_VIDEO_CAPTURE_FRAME_SINK_VIDEO_CAPTURER_IMPL_H_
