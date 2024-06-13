// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_FRAME_SINKS_VIDEO_CAPTURE_FRAME_SINK_VIDEO_CAPTURER_IMPL_H_
#define COMPONENTS_VIZ_SERVICE_FRAME_SINKS_VIDEO_CAPTURE_FRAME_SINK_VIDEO_CAPTURER_IMPL_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/unguessable_token.h"
#include "components/viz/common/quads/compositor_frame_metadata.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/video_capture_target.h"
#include "components/viz/service/frame_sinks/video_capture/capturable_frame_sink.h"
#include "components/viz/service/frame_sinks/video_capture/in_flight_frame_delivery.h"
#include "components/viz/service/frame_sinks/video_capture/video_capture_overlay.h"
#include "components/viz/service/frame_sinks/video_capture/video_frame_pool.h"
#include "components/viz/service/viz_service_export.h"
#include "media/base/video_frame.h"
#include "media/capture/content/video_capture_oracle.h"
#include "media/video/renderable_gpu_memory_buffer_video_frame_pool.h"
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
class GmbVideoFramePoolContextProvider;

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
// of interest regarding the frame flow, display timing, and changes to the
// frame sink's surface. For some subset of frames, decided by
// media::VideoCaptureOracle, this capturer will make a CopyOutputRequest on the
// surface. Successful CopyOutputResults are then copied into pooled shared
// memory for efficient transport to the consumer.
class VIZ_SERVICE_EXPORT FrameSinkVideoCapturerImpl final
    : public CapturableFrameSink::Client,
      public VideoCaptureOverlay::FrameSource,
      public mojom::FrameSinkVideoCapturer {
 public:
  using GpuMemoryBufferVideoFramePoolContext =
      media::RenderableGpuMemoryBufferVideoFramePool::Context;
  // `frame_sink_manager` must outlive this instance. Binds this instance to the
  // Mojo message pipe endpoint in `receiver`, but `receiver` may be empty for
  // unit testing.
  FrameSinkVideoCapturerImpl(
      FrameSinkVideoCapturerManager& frame_sink_manager,
      GmbVideoFramePoolContextProvider* gmb_video_frame_pool_context_provider,
      mojo::PendingReceiver<mojom::FrameSinkVideoCapturer> receiver,
      std::unique_ptr<media::VideoCaptureOracle> oracle,
      bool log_to_webrtc);

  FrameSinkVideoCapturerImpl(const FrameSinkVideoCapturerImpl&) = delete;
  FrameSinkVideoCapturerImpl& operator=(const FrameSinkVideoCapturerImpl&) =
      delete;

  ~FrameSinkVideoCapturerImpl() final;

  // The currently-requested frame sink for capture. The frame sink manager
  // calls this when it learns of a new CapturableFrameSink to see if the target
  // can be resolved.
  const std::optional<VideoCaptureTarget>& target() const { return target_; }

  // In some cases, the target to resolve is already known and can be passed
  // directly instead of attempting to resolve it in `ResolveTarget`. This is
  // called by the frame sink manager.
  void SetResolvedTarget(CapturableFrameSink* target);

  // Notifies this capturer that the current target will be destroyed, and the
  // FrameSinkId associated with it has become invalid. This is called by the
  // frame sink manager.
  void OnTargetWillGoAway();

  // mojom::FrameSinkVideoCapturer implementation:
  void SetFormat(media::VideoPixelFormat format) final;
  void SetMinCapturePeriod(base::TimeDelta min_capture_period) final;
  void SetMinSizeChangePeriod(base::TimeDelta min_period) final;
  void SetResolutionConstraints(const gfx::Size& min_size,
                                const gfx::Size& max_size,
                                bool use_fixed_aspect_ratio) final;
  void SetAutoThrottlingEnabled(bool enabled) final;
  void ChangeTarget(const std::optional<VideoCaptureTarget>& target,
                    uint32_t sub_capture_target_version) final;
  void Start(mojo::PendingRemote<mojom::FrameSinkVideoConsumer> consumer,
             mojom::BufferFormatPreference buffer_format_preference) final;
  void Stop() final;
  // If currently stopped, starts the refresh frame timer to guarantee a frame
  // representing the most up-to-date content will be sent to the consumer in
  // the near future. This refresh operation will be canceled if a compositing
  // event triggers a frame capture in the meantime, and will result in a frame
  // sent to the consumer with a delay of up to one second.
  void RequestRefreshFrame() final;
  void CreateOverlay(int32_t stacking_index,
                     mojo::PendingReceiver<mojom::FrameSinkVideoCaptureOverlay>
                         receiver) final;

  // VideoCaptureOverlay::FrameSource implementation:
  gfx::Size GetSourceSize() final;
  void InvalidateRect(const gfx::Rect& rect) final;
  void OnOverlayConnectionLost(VideoCaptureOverlay* overlay) final;
  // Executes a refresh capture, if conditions permit. Otherwise, schedules a
  // later retry. Note that the retry "polling" should be a short-term state,
  // since it only occurs until the oracle allows the next frame capture to take
  // place. If a refresh was already pending, it is canceled in favor of this
  // new refresh.
  void RefreshNow() final;

  // Default configuration.
  static constexpr media::VideoPixelFormat kDefaultPixelFormat =
      media::PIXEL_FORMAT_I420;
  static constexpr gfx::ColorSpace kDefaultColorSpace =
      gfx::ColorSpace::CreateREC709();

  // The maximum number of frames in-flight in the capture pipeline, reflecting
  // the storage capacity dedicated for this purpose. Since the size of the pool
  // does not align with the number of frames that can be in-flight (the
  // capturer can artificially keep a marked frame alive, even if it's not
  // currently in-flight), we will crate a pool with capacity equal to
  // `kDesignLimitMaxFrames + 1`.
  //
  // Example numbers, for a frame pool that is fully-allocated with 11 frames of
  // size 1920x1080, using the I420 pixel format (12 bits per pixel). Then:
  //
  //   storage_bytes_for_all_frames = 11 * (1920 * 1080 * 12/8)
  //     --> ~32.63 MB
  //
  // In practice, only 0-3 frames will be in-flight at a time, depending on the
  // content change rate and system performance.
  static constexpr int kDesignLimitMaxFrames = 10;
  static constexpr int kFramePoolCapacity = kDesignLimitMaxFrames + 1;

  // A safe, sustainable maximum number of frames in-flight. In other words,
  // exceeding 60% of the design limit is considered "red line" operation.
  static constexpr float kTargetPipelineUtilization = 0.6f;

  // The maximum refresh delay. Provides an upper limit for how long the
  // capture source will remain idle. Callers of the API that request a refresh
  // frame may end up waiting up to this long.
  static constexpr base::TimeDelta kMaxRefreshDelay = base::Seconds(1);

  // Calculate content rectangle
  // Given a `visible_rect` representing visible rectangle of some video frame,
  // calculates a centered rectangle that fits entirely within `visible_rect`
  // and has the same aspect ratio as `source_size`, taking into account
  // `pixel_format`.
  static gfx::Rect GetContentRectangle(const gfx::Rect& visible_rect,
                                       const gfx::Size& source_size,
                                       media::VideoPixelFormat pixel_format);

 private:
  friend class FrameSinkVideoCapturerTest;

  using OracleFrameNumber =
      decltype(std::declval<media::VideoCaptureOracle>().next_frame_number());

  // If the refresh timer is not currently running, this schedules a call
  // to RefreshInternal with kRefreshRequest as the event.
  void MaybeScheduleRefreshFrame();

  // Sets the `dirty_rect_` to maximum size and updates the content version.
  void InvalidateEntireSource();

  // Returns the delay that should be used when setting the refresh timer. This
  // is based on the current oracle prediction for frame duration and is
  // bounded by `kMaxRefreshDelay`.
  base::TimeDelta GetDelayBeforeNextRefreshAttempt() const;

  // Called whenever a major damage event, such as a capture parameter change, a
  // resolved target change, etc., occurs. This marks the entire source as dirty
  // and ensures the consumer will receive a refresh frame with up-to-date
  // content.
  void RefreshEntireSourceNow();

  // CapturableFrameSink::Client implementation:
  void OnFrameDamaged(const gfx::Size& root_render_pass_size,
                      const gfx::Rect& damage_rect,
                      base::TimeTicks target_display_time,
                      const CompositorFrameMetadata& frame_metadata) final;
  bool IsVideoCaptureStarted() final;

  // Resolves the capturable frame sink target using the current value of
  // `target_`, detaching this capturer from the previous target (if any), and
  // attaching to the new found target. This is called by the frame sink
  // manager. If `target_` is null, the capturer goes idle and expects this
  // method to be called again in the near future, once the target becomes known
  // to the frame sink manager.
  void ResolveTarget();

  // If the target is resolved, returns true.
  // Otherwise, makes one attempt to resolve the target, and returns
  // true iff the attempt was successful.
  bool TryResolveTarget();

  // Helper method that actually implements the refresh logic. `event` is used
  // to determine if the refresh is urgent for scheduling purposes.
  void RefreshInternal(media::VideoCaptureOracle::Event event);

  // Returns a list of the overlays in rendering order.
  std::vector<VideoCaptureOverlay*> GetOverlaysInOrder() const;

  // Consults the VideoCaptureOracle to decide whether to capture a frame,
  // then ensures prerequisites are met before initiating the capture: that
  // there is a consumer present and that the pipeline is not already full.
  void MaybeCaptureFrame(media::VideoCaptureOracle::Event event,
                         const gfx::Rect& damage_rect,
                         base::TimeTicks event_time,
                         const CompositorFrameMetadata& frame_metadata);

  // Used by FrameCapture to track the possible outcomes of a capture.
  enum class CaptureResult : uint8_t {
    kPending = 0,  // Outcome not yet known.
    kSuccess,
    // Unable to read data out of the CopyOutputResult.
    kI420ReadbackFailed,
    kARGBReadbackFailed,
    kGpuMemoryBufferReadbackFailed,
    kNV12ReadbackFailed,
    // Subcapture target changed during the capture process.
    kSubCaptureTargetChanged,
    // Oracle rejected the capture.  See VideoCaptureOracle::CompleteCapture.
    kOracleRejectedFrame
  };

  // One instance is created for each capture. It wraps the `media::VideoFrame`
  // that will be eventually delivered to the client along with bookkeeping
  // information for the capture process, and also tracks whether the capture
  // was successful or not.
  class FrameCapture {
   public:
    FrameCapture(int64_t capture_frame_number,
                 OracleFrameNumber oracle_frame_number,
                 int64_t content_version,
                 gfx::Rect content_rect,
                 CapturableFrameSink::RegionProperties region_properties,
                 scoped_refptr<media::VideoFrame> frame,
                 base::TimeTicks request_time);
    // Default ctor and copy-ability is required; this is stored in a
    // std::priority_queue.
    FrameCapture();
    FrameCapture(const FrameCapture&);
    FrameCapture(FrameCapture&&);
    FrameCapture& operator=(const FrameCapture&);
    FrameCapture& operator=(FrameCapture&&);
    ~FrameCapture();
    bool operator<(const FrameCapture& other) const;

    // Returns true if the capture has reached a terminal state.
    bool finished() const { return result_ != CaptureResult::kPending; }

    // Returns true if the capture is successful so far.
    bool success() const { return result_ == CaptureResult::kSuccess; }

    CaptureResult result() const { return result_; }

    // Marks the capture as successful; however this may be overridden by
    // CaptureFailed() at a later stage in the process.
    void CaptureSucceeded();

    // Marks the capture as failed.  `frame` will be set to null, but a copy of
    // its metadata will be retained.
    void CaptureFailed(CaptureResult result);

    // Returns the video frame metadata for this capture.
    const media::VideoFrameMetadata& frame_metadata() const;

    // The current capture frame number, starting from zero and incremented
    // by one for every CopyOutputRequest.
    int64_t capture_frame_number;

    // The oracle's frame number for this frame. Unlike `capture_frame_number`,
    // this gets incremented with each call to MaybeCaptureFrame, whether or not
    // we decide to actually capture a frame. If the pipeline is too full, the
    // `oracle_frame_number` will increment while the `capture_frame_number`
    // will not.
    OracleFrameNumber oracle_frame_number;

    // The current content version of the capturer at time of request. The
    // content version is incremented whenever the source or a subsection
    // of the source gets invalidated, and is used to determine which frames
    // become marked and can be resurrected.
    int64_t content_version;

    // The subsection of the output frame that the content gets scaled to
    // and outputted on, in post-scaled physical pixels.
    gfx::Rect content_rect;

    // Properties of the region being captured.
    CapturableFrameSink::RegionProperties region_properties;

    // The actual frame.
    scoped_refptr<media::VideoFrame> frame;

    // When the request for capture was made.
    base::TimeTicks request_time;

   private:
    // A copy of the frame's metadata retained even if the original frame is
    // discarded.
    std::optional<media::VideoFrameMetadata> frame_metadata_;

    // The result of the capture.
    CaptureResult result_ = CaptureResult::kPending;
  };

  // Extracts the image data from the copy output result, populating the
  // content region of a [possibly letterboxed] video frame.
  void DidCopyFrame(FrameCapture capture,
                    std::unique_ptr<CopyOutputResult> result);

  // Places the capture-in-progress `frame_capture` into `delivery_queue_` and
  // calls MaybeDeliverFrame(), one frame at a time, in-order.
  void OnFrameReadyForDelivery(const FrameCapture& frame_capture);

  // Delivers a frame to the consumer, if the capture was successful and
  // VideoCaptureOracle allows it.  If no frame is able to be delivered, some
  // state will be updated, but nothing will be sent to the consumer.
  void MaybeDeliverFrame(FrameCapture frame_capture);

  // For ARGB format, ensures that every dimension of `size` is positive. For
  // I420 format, ensures that every dimension is even and at least 2.
  gfx::Size AdjustSizeForPixelFormat(const gfx::Size& size) const;

  // Expands `rect` such that its x, y, right, and bottom values are even
  // numbers.
  static gfx::Rect ExpandRectToI420SubsampleBoundaries(const gfx::Rect& rect);

  void OnLog(const std::string& message);

  bool ShouldMark(const media::VideoFrame& frame,
                  int64_t content_version) const;

  // Marks `frame` for resurrection, using `content_version` as its content
  // version. If `frame` is null, the frame marking will be cleared, in which
  // case the `content_version` parameter is ignored.
  void MarkFrame(scoped_refptr<media::VideoFrame> frame,
                 int64_t content_version = -1);

  // Returns true if currently marked frame can be resurrected. `size` specifies
  // the desired size of the frame
  bool CanResurrectFrame(const gfx::Size& size) const;

  scoped_refptr<media::VideoFrame> ResurrectFrame() const {
    return marked_frame_;
  }

  // Should be called when the frame that has been delivered to the consumer has
  // been released by it.
  void NotifyFrameReleased(scoped_refptr<media::VideoFrame> frame);

  // Returns pipeline utilization as a fraction of kTargetPipelineUtilization.
  // May be more than 1.0 if the current pool utilization is greater than
  // kTargetPipelineUtilization.
  //
  // Pipeline utilization is different from pool utilization, since a marked
  // frame may be returned multiple times w/o increasing pool utilization, but
  // it would increase pipeline utilization.
  float GetPipelineUtilization() const;

  // Informs the consumer that the frame was dropped due to being cropped
  // to zero pixels. Only informs the consumer if this is the first such
  // frame since the last actually-delivered frame, so as to avoid being
  // overly chatty and waste CPU.
  void MaybeInformConsumerOfEmptyRegion();

  // Owner/Manager of this instance.
  const raw_ref<FrameSinkVideoCapturerManager> frame_sink_manager_;

  // Mojo receiver for this instance.
  mojo::Receiver<mojom::FrameSinkVideoCapturer> receiver_{this};

  // Represents this instance as an issuer of CopyOutputRequests. The Surface
  // uses this to auto-cancel stale requests (i.e., prior requests that did not
  // execute). Also, the implementations that execute CopyOutputRequests use
  // this as a hint to cache objects for a huge performance improvement.
  const base::UnguessableToken copy_request_source_;

  // Use the default base::TimeTicks clock; but allow unit tests to provide a
  // replacement.
  raw_ptr<const base::TickClock> clock_;

  // Current image format.
  media::VideoPixelFormat pixel_format_ = kDefaultPixelFormat;

  // Models current content change/draw behavior and proposes when to capture
  // frames, and at what size and frame rate.
  const std::unique_ptr<media::VideoCaptureOracle> oracle_;

  // The target requested by the client, as provided in the last call to
  // ChangeTarget(). May be nullopt if no target is currently set.
  std::optional<VideoCaptureTarget> target_;

  // The resolved target of video capture, or null if the requested target does
  // not yet exist (or no longer exists).
  raw_ptr<CapturableFrameSink> resolved_target_ = nullptr;

  // The current video frame consumer. This is set when Start() is called and
  // cleared when Stop() is called.
  mojo::Remote<mojom::FrameSinkVideoConsumer> consumer_;

  // The portion of the source content that has changed, but has not yet been
  // captured.
  gfx::Rect dirty_rect_;

  // Allows determining whether or not the frame size has changed since the last
  // captured frame.
  gfx::Rect last_frame_visible_rect_;

  // True after Start() and false after Stop().
  bool video_capture_started_ = false;
  // Our consumer-preferred VideoBufferHandle type. Valid only when
  // `video_capture_started_` is true.
  mojom::BufferFormatPreference buffer_format_preference_ =
      mojom::BufferFormatPreference::kDefault;

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
  std::optional<base::OneShotTimer> refresh_frame_retry_timer_;

  raw_ptr<GmbVideoFramePoolContextProvider>
      gmb_video_frame_pool_context_provider_;

  // Provides a pool of VideoFrames that can be efficiently delivered across
  // processes. The size of this pool is used to limit the maximum number of
  // frames in-flight at any one time.
  std::unique_ptr<VideoFramePool> frame_pool_;

  // Increased every time the source content changes or a forced refresh is
  // requested.
  int64_t content_version_ = 0;

  int64_t content_version_in_marked_frame_ = -1;

  // The frame that is currently marked. Marked frame is the frame that has the
  // most recent content version (with video overlays already applied), and thus
  // can be directly returned to the consumer if the current content version
  // matches the content version in the marked frame.
  scoped_refptr<media::VideoFrame> marked_frame_ = nullptr;

  // Number of frames that are currently in flight (i.e. in use by consumer).
  int num_frames_in_flight_ = 0;

  // A queue of captured frames pending delivery. This queue is used to re-order
  // frames, if they should happen to be captured out-of-order.
  std::priority_queue<FrameCapture> delivery_queue_;

  // The Oracle-provided media timestamp of the first frame. This is used to
  // compute the relative media stream timestamps for each successive frame.
  std::optional<base::TimeTicks> first_frame_media_ticks_;

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

  // Avoids being overly chatty and wasting CPU when informing the consumer
  // of frames dropped due to being cropped to zero pixels.
  bool consumer_informed_of_empty_region_ = false;

  // The sub-capture-target-version allows Viz to communicate back to Blink the
  // information of which crop-target each frame is associated with. Better, if
  // cropTo() is called multiple times, oscillating between two targets, Blink
  // can even tell whether the frame is cropped to an earlier or later
  // invocation of cropTo() for a given target, because the
  // sub-capture-target-version keeps increasing.
  uint32_t sub_capture_target_version_ = 0;

  // A weak pointer factory used for cancelling the results from any in-flight
  // copy output requests.
  base::WeakPtrFactory<FrameSinkVideoCapturerImpl> capture_weak_factory_{this};
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_FRAME_SINKS_VIDEO_CAPTURE_FRAME_SINK_VIDEO_CAPTURER_IMPL_H_
