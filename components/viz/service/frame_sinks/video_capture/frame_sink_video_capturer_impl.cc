// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/viz/service/frame_sinks/video_capture/frame_sink_video_capturer_impl.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/tracing_buildflags.h"
#include "build/build_config.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/frame_sinks/copy_output_util.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "components/viz/service/frame_sinks/video_capture/frame_sink_video_capturer_manager.h"
#include "components/viz/service/frame_sinks/video_capture/gpu_memory_buffer_video_frame_pool.h"
#include "components/viz/service/frame_sinks/video_capture/shared_memory_video_frame_pool.h"
#include "media/base/limits.h"
#include "media/base/media_switches.h"
#include "media/base/video_types.h"
#include "media/base/video_util.h"
#include "media/capture/mojom/video_capture_buffer.mojom.h"
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

using media::VideoCaptureOracle;
using media::VideoFrame;
using media::VideoFrameMetadata;

// Helper macro to log ".CaptureDuration" histograms. `format` needs to be a
// string literal, `sample` is a sample that will be logged.
#define UMA_HISTOGRAM_CAPTURE_DURATION(format, sample)                       \
  do {                                                                       \
    UMA_HISTOGRAM_CUSTOM_TIMES(                                              \
        "Viz.FrameSinkVideoCapturer." format ".CaptureDuration", sample,     \
        base::Milliseconds(1), base::Seconds(1), 50);                        \
    UMA_HISTOGRAM_CUSTOM_TIMES("Viz.FrameSinkVideoCapturer.CaptureDuration", \
                               sample, base::Milliseconds(1),                \
                               base::Seconds(1), 50);                        \
  } while (false)

// Helper macro to log ".CaptureSucceeded" histograms. `format` needs to be a
// string literal, `success` is a boolean that will be logged.
#define UMA_HISTOGRAM_CAPTURE_SUCCEEDED(format, success)                    \
  do {                                                                      \
    UMA_HISTOGRAM_BOOLEAN(                                                  \
        "Viz.FrameSinkVideoCapturer." format ".CaptureSucceeded", success); \
    UMA_HISTOGRAM_BOOLEAN("Viz.FrameSinkVideoCapturer.CaptureSucceeded",    \
                          success);                                         \
  } while (false)

namespace viz {

namespace {

// The largest Rect possible.
constexpr gfx::Rect kMaxRect = gfx::Rect(0,
                                         0,
                                         std::numeric_limits<int>::max(),
                                         std::numeric_limits<int>::max());

// Note about RGBA/BGRA/ARGB pixel format names:
// In FrameSinkVideoCapturer, ARGB is a "format name", the frames it gives
// could be RGBA/BGRA depends on platform and the preference of the buffer
// format. When user wants ARGB result, it requests a CopyOutputRequest with
// ResultFormat::RGBA which gives RGBA/BGRA results depends on platform and
// where the result is stored (buffer format preference).
// Currently, kPreferGpuMemoryBuffer + ARGB will request BGRA as pixel format,
// but kDefault + ARGB will be platform dependent because CopyOutputRequest
// will use kN32_SkColorType (RGBA on Android, BGRA elsewhere) mostly, and use
// kRGBA_8888_SkColorType on iOS.
// This is also documented in the mojom comments (https://crrev.com/c/5418235)
// about SetFormat, indicating the ARGB format may produce RGBA/BGRA frames
// depends on platform.

// Get the frame pool for the specific format. We need context_provider if the
// format is NV12 or ARGB (when buffer_format_preference is kNativeTexture).
// Thus, buffer_format_preference is also needed to tell which mode ARGB use.
std::unique_ptr<VideoFramePool> GetVideoFramePoolForFormat(
    media::VideoPixelFormat format,
    int capacity,
    mojom::BufferFormatPreference buffer_format_preference,
    GmbVideoFramePoolContextProvider* context_provider) {
  CHECK(format == media::PIXEL_FORMAT_I420 ||
        format == media::PIXEL_FORMAT_NV12 ||
        format == media::PIXEL_FORMAT_ARGB);

  switch (format) {
    case media::PIXEL_FORMAT_I420:
      return std::make_unique<SharedMemoryVideoFramePool>(capacity);
    case media::PIXEL_FORMAT_ARGB: {
      switch (buffer_format_preference) {
        case mojom::BufferFormatPreference::kPreferGpuMemoryBuffer:
          return std::make_unique<GpuMemoryBufferVideoFramePool>(
              capacity, format, gfx::ColorSpace::CreateSRGB(),
              context_provider);
        case mojom::BufferFormatPreference::kDefault:
          return std::make_unique<SharedMemoryVideoFramePool>(capacity);
        default:
          NOTREACHED();
      }
    }
    case media::PIXEL_FORMAT_NV12:
      return std::make_unique<GpuMemoryBufferVideoFramePool>(
          capacity, format, gfx::ColorSpace::CreateREC709(), context_provider);
    default:
      NOTREACHED();
  }
}

CopyOutputRequest::ResultFormat VideoPixelFormatToCopyOutputRequestFormat(
    media::VideoPixelFormat format) {
  switch (format) {
    case media::PIXEL_FORMAT_I420:
      return CopyOutputRequest::ResultFormat::I420_PLANES;
    case media::PIXEL_FORMAT_NV12:
      return CopyOutputRequest::ResultFormat::NV12;
    case media::PIXEL_FORMAT_ARGB:
      return CopyOutputRequest::ResultFormat::RGBA;
    default:
      NOTREACHED();
  }
}

bool IsCompatibleWithFormat(const gfx::Rect& rect,
                            media::VideoPixelFormat format) {
  CHECK(format == media::PIXEL_FORMAT_I420 ||
        format == media::PIXEL_FORMAT_NV12 ||
        format == media::PIXEL_FORMAT_ARGB);
  if (format == media::PIXEL_FORMAT_ARGB) {
    // No special requirements:
    return true;
  }

  return rect.origin().x() % 2 == 0 && rect.origin().y() % 2 == 0 &&
         rect.width() % 2 == 0 && rect.height() % 2 == 0;
}

int AsPercent(float value) {
  return base::saturated_cast<int>(std::nearbyint(value * 100.0f));
}

perfetto::Track FrameInUseTrack(const media::VideoFrameMetadata& metadata) {
  return perfetto::Track(static_cast<uint64_t>(
      (metadata.capture_begin_time.value() - base::TimeTicks())
          .InMicroseconds()));
}

perfetto::Track CaptureTrack(const media::VideoFrameMetadata& metadata) {
  return perfetto::Track(static_cast<uint64_t>(
      (metadata.reference_time.value() - base::TimeTicks()).InMicroseconds()));
}

}  // namespace

// static
constexpr media::VideoPixelFormat
    FrameSinkVideoCapturerImpl::kDefaultPixelFormat;
// static
constexpr gfx::ColorSpace FrameSinkVideoCapturerImpl::kDefaultColorSpace;
// static
constexpr int FrameSinkVideoCapturerImpl::kDesignLimitMaxFrames;
// static
constexpr int FrameSinkVideoCapturerImpl::kFramePoolCapacity;
// static
constexpr float FrameSinkVideoCapturerImpl::kTargetPipelineUtilization;
// static
constexpr base::TimeDelta FrameSinkVideoCapturerImpl::kMaxRefreshDelay;

FrameSinkVideoCapturerImpl::FrameSinkVideoCapturerImpl(
    FrameSinkVideoCapturerManager& frame_sink_manager,
    GmbVideoFramePoolContextProvider* gmb_video_frame_pool_context_provider,
    mojo::PendingReceiver<mojom::FrameSinkVideoCapturer> receiver,
    std::unique_ptr<media::VideoCaptureOracle> oracle,
    bool log_to_webrtc)
    : frame_sink_manager_(frame_sink_manager),
      copy_request_source_(base::UnguessableToken::Create()),
      clock_(base::DefaultTickClock::GetInstance()),
      oracle_(std::move(oracle)),
      gmb_video_frame_pool_context_provider_(
          gmb_video_frame_pool_context_provider),
      feedback_weak_factory_(oracle_.get()),
      log_to_webrtc_(log_to_webrtc) {
  CHECK(oracle_);
  if (log_to_webrtc_) {
    oracle_->SetLogCallback(base::BindRepeating(
        &FrameSinkVideoCapturerImpl::OnLog, base::Unretained(this)));
  }
  // Instantiate a default base::OneShotTimer instance.
  refresh_frame_retry_timer_.emplace();

  if (receiver.is_valid()) {
    receiver_.Bind(std::move(receiver));
    receiver_.set_disconnect_handler(
        base::BindOnce(&FrameSinkVideoCapturerManager::OnCapturerConnectionLost,
                       base::Unretained(&*frame_sink_manager_), this));
  }
}

FrameSinkVideoCapturerImpl::~FrameSinkVideoCapturerImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  Stop();
  SetResolvedTarget(nullptr);
}

void FrameSinkVideoCapturerImpl::ResolveTarget() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  SetResolvedTarget(
      target_ ? frame_sink_manager_->FindCapturableFrameSink(target_.value())
              : nullptr);
}

bool FrameSinkVideoCapturerImpl::TryResolveTarget() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!resolved_target_) {
    ResolveTarget();
  }

  return resolved_target_;
}

void FrameSinkVideoCapturerImpl::SetResolvedTarget(
    CapturableFrameSink* target) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (resolved_target_ == target) {
    return;
  }

  TRACE_EVENT_INSTANT(
      "gpu.capture", "SetResolvedTarget", "current",
      resolved_target_ ? resolved_target_->GetFrameSinkId().ToString() : "None",
      "new", target ? target->GetFrameSinkId().ToString() : "None");

  if (resolved_target_) {
    resolved_target_->DetachCaptureClient(this);
  }
  resolved_target_ = target;
  if (resolved_target_) {
    resolved_target_->AttachCaptureClient(this);
    RefreshEntireSourceNow();
  } else {
    MaybeInformConsumerOfEmptyRegion();
    // The capturer will remain idle until either: 1) the requested target is
    // re-resolved by the `frame_sink_manager_`, or 2) a new target is set via a
    // call to ChangeTarget().
  }
}

void FrameSinkVideoCapturerImpl::OnTargetWillGoAway() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  SetResolvedTarget(nullptr);
}

void FrameSinkVideoCapturerImpl::SetFormat(media::VideoPixelFormat format) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool format_changed = false;

  if (format != media::PIXEL_FORMAT_I420 &&
      format != media::PIXEL_FORMAT_ARGB &&
      format != media::PIXEL_FORMAT_NV12) {
    LOG(DFATAL) << "Invalid pixel format: Only I420, ARGB & NV12 formats are "
                   "supported.";
  } else {
    // We only support NV12 if we got a context provider for pool creation:
    CHECK(format != media::PIXEL_FORMAT_NV12 ||
          gmb_video_frame_pool_context_provider_);

    format_changed |= (pixel_format_ != format);
    pixel_format_ = format;
  }

  if (format_changed) {
    // We can safely do nothing and let Start to create the buffer lazily if we
    // are not in mid-capture.
    if (video_capture_started_) {
      // Don't tolerate changing to NV12 mid-capture:
      CHECK(format != media::PIXEL_FORMAT_NV12);

      // If we have started with kPreferGpuMemoryBuffer, we set it to kDefault
      // as currently we probably only doing mid-capture change due to crash
      // downgrade, and we should not try using GMB anymore.
      // TODO: We may move buffer_format_preference from Start to SetFormat.
      buffer_format_preference_ = mojom::BufferFormatPreference::kDefault;

      TRACE_EVENT_INSTANT("gpu.capture", "SetFormat", "format", format);

      MarkFrame(nullptr);

      frame_pool_ = GetVideoFramePoolForFormat(
          pixel_format_, kFramePoolCapacity, buffer_format_preference_,
          gmb_video_frame_pool_context_provider_);

      RefreshEntireSourceNow();
    }
  }
}

void FrameSinkVideoCapturerImpl::SetMinCapturePeriod(
    base::TimeDelta min_capture_period) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  constexpr base::TimeDelta kMinMinCapturePeriod = base::Microseconds(
      base::Time::kMicrosecondsPerSecond / media::limits::kMaxFramesPerSecond);
  if (min_capture_period < kMinMinCapturePeriod) {
    min_capture_period = kMinMinCapturePeriod;
  }

  // On machines without high-resolution clocks, limit the maximum frame rate to
  // 30 FPS. This avoids a potential issue where the system clock may not
  // advance between two successive frames.
  if (!base::TimeTicks::IsHighResolution()) {
    constexpr base::TimeDelta kMinLowResCapturePeriod =
        base::Microseconds(base::Time::kMicrosecondsPerSecond / 30);
    if (min_capture_period < kMinLowResCapturePeriod) {
      min_capture_period = kMinLowResCapturePeriod;
    }
  }

  TRACE_EVENT_INSTANT("gpu.capture", "SetMinCapturePeriod",
                      "min_capture_period", min_capture_period);

  oracle_->SetMinCapturePeriod(min_capture_period);
  if (refresh_frame_retry_timer_->IsRunning()) {
    // With the change in the minimum capture period, a pending refresh might
    // be ready to execute now (or sooner than it would have been).
    RefreshNow();
  }
}

void FrameSinkVideoCapturerImpl::SetMinSizeChangePeriod(
    base::TimeDelta min_period) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  TRACE_EVENT_INSTANT("gpu.capture", "SetMinSizeChangePeriod",
                      "min_size_change_period", min_period);

  oracle_->SetMinSizeChangePeriod(min_period);
}

void FrameSinkVideoCapturerImpl::SetResolutionConstraints(
    const gfx::Size& min_size,
    const gfx::Size& max_size,
    bool use_fixed_aspect_ratio) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT_INSTANT("gpu.capture", "SetResolutionConstraints", "min_size",
                      min_size.ToString(), "max_size", max_size.ToString());

  if (min_size.width() <= 0 || min_size.height() <= 0 ||
      max_size.width() > media::limits::kMaxDimension ||
      max_size.height() > media::limits::kMaxDimension ||
      min_size.width() > max_size.width() ||
      min_size.height() > max_size.height()) {
    LOG(DFATAL) << "Invalid resolutions constraints: " << min_size.ToString()
                << " must not be greater than " << max_size.ToString()
                << "; and also within media::limits.";
    return;
  }

  oracle_->SetCaptureSizeConstraints(min_size, max_size,
                                     use_fixed_aspect_ratio);
  RefreshEntireSourceNow();
}

void FrameSinkVideoCapturerImpl::SetAutoThrottlingEnabled(bool enabled) {
  TRACE_EVENT_INSTANT("gpu.capture", "SetAutoThrottlingEnabled",
                      "autothrottling_enabled", enabled);

  oracle_->SetAutoThrottlingEnabled(enabled);
}

void FrameSinkVideoCapturerImpl::ChangeTarget(
    const std::optional<VideoCaptureTarget>& target,
    uint32_t sub_capture_target_version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_GE(sub_capture_target_version, sub_capture_target_version_);

  target_ = target;

  if (sub_capture_target_version_ != sub_capture_target_version) {
    sub_capture_target_version_ = sub_capture_target_version;

    if (consumer_) {
      consumer_->OnNewSubCaptureTargetVersion(sub_capture_target_version);
    }
  }

  ResolveTarget();
}

void FrameSinkVideoCapturerImpl::Start(
    mojo::PendingRemote<mojom::FrameSinkVideoConsumer> consumer,
    mojom::BufferFormatPreference buffer_format_preference) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(consumer);

  if (video_capture_started_) {
    Stop();
  }

  buffer_format_preference_ = buffer_format_preference;

  TRACE_EVENT_INSTANT("gpu.capture", "Start", "buffer_format_preference",
                      buffer_format_preference);

  // Clear any marked frame if the capturer was restarted.
  MarkFrame(nullptr);

  frame_pool_ = GetVideoFramePoolForFormat(
      pixel_format_, kFramePoolCapacity, buffer_format_preference_,
      gmb_video_frame_pool_context_provider_);

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN2(
      "gpu.capture", "FrameSinkVideoCapturerImpl::Start", this, "pixel_format_",
      pixel_format_, "buffer_format_preference_", buffer_format_preference_);

  // If we should start capture for NV12 format, we can only hand out GMBs so
  // the caller must tolerate them:
  CHECK(pixel_format_ != media::PIXEL_FORMAT_NV12 ||
        buffer_format_preference_ ==
            mojom::BufferFormatPreference::kPreferGpuMemoryBuffer);

  // If we are using ARGB format with GMB, we must have the pool context
  CHECK(pixel_format_ != media::PIXEL_FORMAT_ARGB ||
        buffer_format_preference_ !=
            mojom::BufferFormatPreference::kPreferGpuMemoryBuffer ||
        gmb_video_frame_pool_context_provider_);

  video_capture_started_ = true;

  if (resolved_target_) {
    resolved_target_->OnClientCaptureStarted();
  }

  consumer_.Bind(std::move(consumer));
  // In the future, if the connection to the consumer is lost before a call to
  // Stop(), make that call on its behalf.
  consumer_.set_disconnect_handler(base::BindOnce(
      &FrameSinkVideoCapturerImpl::Stop, base::Unretained(this)));
  RefreshEntireSourceNow();
}

void FrameSinkVideoCapturerImpl::Stop() {
  if (!video_capture_started_) {
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  refresh_frame_retry_timer_->Stop();

  // Clear any marked frame.
  MarkFrame(nullptr);

  // Cancel any captures in-flight and any captured frames pending delivery.
  capture_weak_factory_.InvalidateWeakPtrs();
  oracle_->CancelAllCaptures();
  while (!delivery_queue_.empty()) {
    delivery_queue_.pop();
  }
  next_delivery_frame_number_ = next_capture_frame_number_;

  if (consumer_) {
    consumer_->OnStopped();
    consumer_.reset();
    consumer_informed_of_empty_region_ = false;
  }

  if (resolved_target_) {
    resolved_target_->OnClientCaptureStopped();
  }

  TRACE_EVENT_NESTABLE_ASYNC_END0("gpu.capture",
                                  "FrameSinkVideoCapturerImpl::Start", this);

  video_capture_started_ = false;
  buffer_format_preference_ = mojom::BufferFormatPreference::kDefault;
}

void FrameSinkVideoCapturerImpl::RequestRefreshFrame() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!TryResolveTarget()) {
    return;
  }

  refresh_frame_retry_timer_->Stop();
  RefreshNow();
}

void FrameSinkVideoCapturerImpl::CreateOverlay(
    int32_t stacking_index,
    mojo::PendingReceiver<mojom::FrameSinkVideoCaptureOverlay> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // This will cause an existing overlay with the same stacking index to be
  // dropped, per mojom-documented behavior.
  overlays_.emplace(stacking_index, std::make_unique<VideoCaptureOverlay>(
                                        *this, std::move(receiver)));
}

gfx::Size FrameSinkVideoCapturerImpl::GetSourceSize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return oracle_->source_size();
}

void FrameSinkVideoCapturerImpl::InvalidateRect(const gfx::Rect& rect) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  gfx::Rect positive_rect = rect;
  positive_rect.Intersect(kMaxRect);
  dirty_rect_.Union(positive_rect);
  content_version_++;
}

void FrameSinkVideoCapturerImpl::OnOverlayConnectionLost(
    VideoCaptureOverlay* overlay) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::EraseIf(overlays_,
                [&overlay](const decltype(overlays_)::value_type& entry) {
                  return entry.second.get() == overlay;
                });
}

void FrameSinkVideoCapturerImpl::RefreshNow() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RefreshInternal(VideoCaptureOracle::kRefreshDemand);
}

gfx::Rect FrameSinkVideoCapturerImpl::GetContentRectangle(
    const gfx::Rect& visible_rect,
    const gfx::Size& source_size,
    media::VideoPixelFormat pixel_format) {
  CHECK(pixel_format == media::PIXEL_FORMAT_I420 ||
        pixel_format == media::PIXEL_FORMAT_NV12 ||
        pixel_format == media::PIXEL_FORMAT_ARGB);

  if (pixel_format == media::PIXEL_FORMAT_I420 ||
      pixel_format == media::PIXEL_FORMAT_NV12) {
    return media::ComputeLetterboxRegionForI420(visible_rect, source_size);
  } else {
    CHECK_EQ(media::PIXEL_FORMAT_ARGB, pixel_format);
    const gfx::Rect content_rect =
        media::ComputeLetterboxRegion(visible_rect, source_size);

    // The media letterboxing computation explicitly allows for off-by-one
    // errors due to computation, so we address those here.
    return content_rect.ApproximatelyEqual(visible_rect, 1) ? visible_rect
                                                            : content_rect;
  }
}

void FrameSinkVideoCapturerImpl::MaybeScheduleRefreshFrame() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!refresh_frame_retry_timer_->IsRunning()) {
    // NOTE: base::Unretained is used here safely because if `this` is invalid
    // then the retry timer should have already been destructed.
    refresh_frame_retry_timer_->Start(
        FROM_HERE, GetDelayBeforeNextRefreshAttempt(),
        base::BindOnce(&FrameSinkVideoCapturerImpl::RefreshInternal,
                       base::Unretained(this),
                       media::VideoCaptureOracle::Event::kRefreshRequest));
  }
}

void FrameSinkVideoCapturerImpl::InvalidateEntireSource() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  dirty_rect_ = kMaxRect;
  content_version_++;
}

base::TimeDelta FrameSinkVideoCapturerImpl::GetDelayBeforeNextRefreshAttempt()
    const {
  // The delay should be long enough to prevent interrupting the smooth timing
  // of frame captures that are expected to take place due to compositor update
  // events. However, the delay should not be excessively long either. Two frame
  // periods should be "just right."
  //
  // NOTE: if a source is idle, the oracle may end up providing a frame duration
  // equal to the time since the last refresh frame was called. In practice,
  // this has the potential for this delay to end up being multiple seconds
  // with no upper limit, so it is instead bounded at `kMaxRefreshDelay`.
  return std::min(kMaxRefreshDelay,
                  2 * std::max(oracle_->estimated_frame_duration(),
                               oracle_->min_capture_period()));
}

void FrameSinkVideoCapturerImpl::RefreshEntireSourceNow() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  InvalidateEntireSource();
  RefreshNow();
}

void FrameSinkVideoCapturerImpl::RefreshInternal(
    VideoCaptureOracle::Event event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If consumption is stopped, cancel the refresh.
  if (!consumer_) {
    return;
  }

  // If the capture target has not yet been resolved, first try changing the
  // target since it may be available now.
  if (!TryResolveTarget()) {
    MaybeScheduleRefreshFrame();
    return;
  }

  // Detect whether the source size changed before attempting capture.
  CHECK(target_);
  const std::optional<CapturableFrameSink::RegionProperties> region_properties =
      resolved_target_->GetRequestRegionProperties(target_->sub_target);
  if (!region_properties) {
    MaybeInformConsumerOfEmptyRegion();
    // If the capture region is empty, it means one of two things: the first
    // frame has not been composited yet or the current region selected for
    // capture has a current size of zero. We schedule a frame refresh here,
    // although it's not useful in all circumstances.
    MaybeScheduleRefreshFrame();
    return;
  }

  const gfx::Size& size = region_properties->render_pass_subrect.size();
  if (size != oracle_->source_size()) {
    oracle_->SetSourceSize(size);
    InvalidateEntireSource();
    OnLog(
        base::StringPrintf("FrameSinkVideoCapturerImpl::RefreshInternal() "
                           "changed active frame size: %s",
                           size.ToString().c_str()));
  }

  MaybeCaptureFrame(event, gfx::Rect(), clock_->NowTicks(),
                    *resolved_target_->GetLastActivatedFrameMetadata());
}

void FrameSinkVideoCapturerImpl::OnFrameDamaged(
    const gfx::Size& root_render_pass_size,
    const gfx::Rect& damage_rect,
    base::TimeTicks expected_display_time,
    const CompositorFrameMetadata& frame_metadata) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!root_render_pass_size.IsEmpty());
  CHECK(!damage_rect.IsEmpty());
  CHECK(!expected_display_time.is_null());
  CHECK(resolved_target_);
  CHECK(target_);

  const std::optional<CapturableFrameSink::RegionProperties> region_properties =
      resolved_target_->GetRequestRegionProperties(target_->sub_target);
  if (!region_properties) {
    MaybeInformConsumerOfEmptyRegion();
    return;
  }

  const gfx::Size& size = region_properties->render_pass_subrect.size();
  if (size == oracle_->source_size()) {
    if (!IsEntireTabCapture(target_->sub_target)) {
      // The damage_rect may not be in the same coordinate space when we have
      // a valid request subtree identifier, so to be safe we just invalidate
      // the entire source.
      InvalidateEntireSource();
    } else {
      InvalidateRect(damage_rect);
    }
  } else {
    oracle_->SetSourceSize(size);
    InvalidateEntireSource();
    OnLog(base::StringPrintf(
        "FrameSinkVideoCapturerImpl::OnFrameDamaged() changed frame size: %s",
        size.ToString().c_str()));
  }

  MaybeCaptureFrame(VideoCaptureOracle::kCompositorUpdate, damage_rect,
                    expected_display_time, frame_metadata);
}

bool FrameSinkVideoCapturerImpl::IsVideoCaptureStarted() {
  return video_capture_started_;
}

std::vector<VideoCaptureOverlay*>
FrameSinkVideoCapturerImpl::GetOverlaysInOrder() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<VideoCaptureOverlay*> list;
  list.reserve(overlays_.size());
  for (const auto& entry : overlays_) {
    list.push_back(entry.second.get());
  }
  return list;
}

FrameSinkVideoCapturerImpl::FrameCapture::FrameCapture(
    int64_t capture_frame_number,
    OracleFrameNumber oracle_frame_number,
    int64_t content_version,
    gfx::Rect content_rect,
    CapturableFrameSink::RegionProperties region_properties,
    scoped_refptr<media::VideoFrame> frame,
    base::TimeTicks request_time)
    : capture_frame_number(capture_frame_number),
      oracle_frame_number(oracle_frame_number),
      content_version(content_version),
      content_rect(content_rect),
      region_properties(region_properties),
      frame(std::move(frame)),
      request_time(request_time) {}

FrameSinkVideoCapturerImpl::FrameCapture::FrameCapture() = default;
FrameSinkVideoCapturerImpl::FrameCapture::FrameCapture(
    const FrameSinkVideoCapturerImpl::FrameCapture&) = default;
FrameSinkVideoCapturerImpl::FrameCapture::FrameCapture(
    FrameSinkVideoCapturerImpl::FrameCapture&&) = default;
FrameSinkVideoCapturerImpl::FrameCapture&
FrameSinkVideoCapturerImpl::FrameCapture::operator=(
    const FrameSinkVideoCapturerImpl::FrameCapture&) = default;
FrameSinkVideoCapturerImpl::FrameCapture&
FrameSinkVideoCapturerImpl::FrameCapture::operator=(
    FrameSinkVideoCapturerImpl::FrameCapture&&) = default;
FrameSinkVideoCapturerImpl::FrameCapture::~FrameCapture() = default;

bool FrameSinkVideoCapturerImpl::FrameCapture::operator<(
    const FrameSinkVideoCapturerImpl::FrameCapture& other) const {
  // Reverse the sort order; so std::priority_queue<FrameCapture> becomes a
  // min-heap instead of a max-heap.
  return other.capture_frame_number < capture_frame_number;
}

void FrameSinkVideoCapturerImpl::FrameCapture::CaptureSucceeded() {
  CHECK_EQ(result_, CaptureResult::kPending);
  result_ = CaptureResult::kSuccess;
}

void FrameSinkVideoCapturerImpl::FrameCapture::CaptureFailed(
    CaptureResult result) {
  CHECK(result_ == CaptureResult::kPending ||
        result_ == CaptureResult::kSuccess);
  result_ = result;
  frame_metadata_ = frame->metadata();
  frame = nullptr;
}

const media::VideoFrameMetadata&
FrameSinkVideoCapturerImpl::FrameCapture::frame_metadata() const {
  return frame ? frame->metadata() : frame_metadata_.value();
}

void FrameSinkVideoCapturerImpl::MaybeCaptureFrame(
    VideoCaptureOracle::Event event,
    const gfx::Rect& damage_rect,
    base::TimeTicks event_time,
    const CompositorFrameMetadata& frame_metadata) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(resolved_target_);
  CHECK(target_);

  // Consult the oracle to determine whether this frame should be captured.
  if (oracle_->ObserveEventAndDecideCapture(event, damage_rect, event_time)) {
    // Regardless of the type of `event`, there is no longer a need for the
    // refresh frame retry timer to fire. The following is a no-op, if the timer
    // was not running.
    refresh_frame_retry_timer_->Stop();
  } else {
    TRACE_EVENT_INSTANT("gpu.capture", "FpsRateLimited", "trigger",
                        VideoCaptureOracle::EventAsString(event));

    // Whether the oracle rejected a compositor update or a refresh event,
    // the consumer needs to be provided an update in the near future.
    MaybeScheduleRefreshFrame();
    return;
  }

  // If there is no `consumer_` present, punt. This check is being done after
  // consulting the oracle because it helps to "prime" the oracle in the short
  // period of time where the capture target is known but the `consumer_` has
  // not yet been provided in the call to Start().
  if (!consumer_) {
    TRACE_EVENT_INSTANT("gpu.capture", "NoConsumer", "trigger",
                        VideoCaptureOracle::EventAsString(event));
    return;
  }

  const std::optional<CapturableFrameSink::RegionProperties> region_properties =
      resolved_target_->GetRequestRegionProperties(target_->sub_target);
  if (!region_properties) {
    // We should have valid properties even if there is no sub target. There is
    // nothing to capture right now.
    TRACE_EVENT_INSTANT("gpu.capture", "NoRegionProperties", "trigger",
                        VideoCaptureOracle::EventAsString(event));
    return;
  }

  const gfx::Rect render_pass_in_root_space =
      region_properties->transform_to_root.MapRect(
          region_properties->render_pass_subrect);

  // Sanity check: the subsection of the render pass selected for capture should
  // be within the size of the compositor frame. Otherwise, this likely means
  // that there is a mismatch between the last aggregated surface and the last
  // activated surface sizes. To be cautious, we refresh the frame although a
  // frame damage event should happen shortly.
  //
  // TODO(crbug.com/40824508): we should likely just get the frame
  // region from the last aggregated surface.
  if (!gfx::Rect(region_properties->root_render_pass_size)
           .Contains(render_pass_in_root_space)) {
    TRACE_EVENT_INSTANT("gpu.capture", "DroppingFrameWithUncontainedRegion",
                        "root_render_pass_size",
                        region_properties->root_render_pass_size.ToString(),
                        "render_pass_subrect_in_root_space",
                        render_pass_in_root_space.ToString());
    MaybeScheduleRefreshFrame();
    return;
  }

  // If we end up capturing a frame, consider this point to be the beginning of
  // the capture for this frame. This is so that we include the time spent
  // reserving a video frame from the frame pool in the total capture duration
  // histogram.
  const base::TimeTicks capture_begin_time = clock_->NowTicks();

  // Reserve a buffer from the pool for the next frame.
  const OracleFrameNumber oracle_frame_number = oracle_->next_frame_number();

  // Size of the video frames that we are supposed to produce. Depends on the
  // pixel format and the capture size as determined by the oracle (which in
  // turn depends on the capture constraints).
  const gfx::Size capture_size =
      AdjustSizeForPixelFormat(oracle_->capture_size());

  // Size of the source that we are capturing.
  const gfx::Size source_size = oracle_->source_size();
  CHECK(!source_size.IsEmpty());
  CHECK_EQ(region_properties->render_pass_subrect.size(), source_size);

  const bool can_resurrect_content = CanResurrectFrame(capture_size);
  scoped_refptr<VideoFrame> frame;
  if (can_resurrect_content) {
    TRACE_EVENT_INSTANT("gpu.capture", "UsingResurrectedFrame");
    frame = ResurrectFrame();
  } else {
    TRACE_EVENT_INSTANT("gpu.capture", "ReservingVideoFrame",
                        "root_render_pass_size",
                        region_properties->root_render_pass_size.ToString(),
                        "render_pass_subrect",
                        region_properties->render_pass_subrect.ToString());
    auto reserve_start_time = base::TimeTicks::Now();

    frame = frame_pool_->ReserveVideoFrame(pixel_format_, capture_size);

    UMA_HISTOGRAM_CUSTOM_TIMES(
        "Viz.FrameSinkVideoCapturer.ReserveFrameDuration",
        base::TimeTicks::Now() - reserve_start_time, base::Milliseconds(1),
        base::Milliseconds(250), 50);
  }

  UMA_HISTOGRAM_BOOLEAN("Viz.FrameSinkVideoCapturer.FrameResurrected",
                        can_resurrect_content);

  const float utilization = GetPipelineUtilization();
  const int utilization_pct = AsPercent(utilization);

  // Do not proceed if the pool did not provide a frame: This indicates the
  // pipeline is full.
  if (!frame) {
    TRACE_EVENT_INSTANT("gpu.capture", "PipelineLimited", "trigger",
                        VideoCaptureOracle::EventAsString(event),
                        "utilization_pct", utilization_pct);
    oracle_->RecordWillNotCapture(utilization);
    if (next_capture_frame_number_ == 0) {
      // The pool was unable to provide a buffer for the very first capture, and
      // so there is no expectation of recovery. Thus, treat this as a fatal
      // resource allocation issue instead of a transient one.
      LOG(ERROR) << "Unable to allocate frame for first frame capture: OOM?";
      Stop();
    } else {
      MaybeScheduleRefreshFrame();
    }
    return;
  }

  // If frame was resurrected / allocated from the pool, its visible rectangle
  // should match what we requested:
  CHECK_EQ(frame->visible_rect().size(), capture_size);
  // The pool should return a frame with visible rectangle that is compatible
  // with the capture format.
  DCHECK(IsCompatibleWithFormat(frame->visible_rect(), pixel_format_));

  // Record a trace event if the capture pipeline is redlining, but capture will
  // still proceed.
  if (utilization >= 1.0) {
    TRACE_EVENT_INSTANT("gpu.capture", "NearlyPipelineLimited", "trigger",
                        VideoCaptureOracle::EventAsString(event),
                        "utilization_pct", utilization_pct);
  }

  // At this point, the capture is going to proceed. Populate the VideoFrame's
  // metadata, and notify the oracle.
  const int64_t capture_frame_number = next_capture_frame_number_++;

  // !WARNING: now that the frame number has been incremented, returning without
  // adding the frame to the `delivery_queue_` or decrementing the frame number
  // will cause the queue to be permanently stuck.
  VideoFrameMetadata& metadata = frame->metadata();
  metadata.capture_begin_time = capture_begin_time;
  metadata.capture_counter = capture_frame_number;
  metadata.frame_duration = oracle_->estimated_frame_duration();
  metadata.frame_rate = 1.0 / oracle_->min_capture_period().InSecondsF();
  metadata.reference_time = event_time;
  metadata.frame_sequence =
      frame_metadata.begin_frame_ack.frame_id.sequence_number;
  metadata.device_scale_factor = frame_metadata.device_scale_factor;
  metadata.page_scale_factor = frame_metadata.page_scale_factor;
  metadata.root_scroll_offset_x = frame_metadata.root_scroll_offset.x();
  metadata.root_scroll_offset_y = frame_metadata.root_scroll_offset.y();
  metadata.source_size = source_size;
  if (frame_metadata.top_controls_visible_height.has_value()) {
    last_top_controls_visible_height_ =
        *frame_metadata.top_controls_visible_height;
  }
  metadata.top_controls_visible_height = last_top_controls_visible_height_;

  // Record that the frame has been reserved for capture.
  TRACE_EVENT_BEGIN("gpu.capture", "FrameInUse", FrameInUseTrack(metadata),
                    "frame_number", capture_frame_number, "utilization_pct",
                    utilization_pct);

  oracle_->RecordCapture(utilization);

  // `content_rect` is the region of the `frame` that we would like to populate.
  // We know our source is of size `source_size`, and we have
  // `frame->visible_rect()` to fill out - find the largest centered rectangle
  // that will fit within the frame and maintains the aspect ratio of the
  // source.
  // TODO(https://crbug.com/1323342): currently, both the frame's visible
  // rectangle and source size are controlled by oracle
  // (`frame->visible_rect().size() == `capture_size`). Oracle also knows if we
  // need to maintain fixed aspect ratio, so it should compute both the
  // `capture_size` and `content_rect` for us, thus ensuring that letterboxing
  // happens only when it needs to (i.e. when we allocate a frame and know that
  // aspect ratio does not have to be maintained, we should use a size that we
  // know would not require letterboxing).
  const gfx::Rect content_rect =
      GetContentRectangle(frame->visible_rect(), source_size, pixel_format_);
  TRACE_EVENT_INSTANT("gpu.capture", "ContentRectDeterminedForCapture",
                      "content_rect", content_rect.ToString(), "source_size",
                      source_size.ToString());

  TRACE_EVENT_BEGIN("gpu.capture", "Capture", CaptureTrack(metadata),
                    "frame_number", capture_frame_number, "trigger",
                    VideoCaptureOracle::EventAsString(event));

  // Determine what rectangular region has changed since the last captured
  // frame.
  gfx::Rect update_rect;
  if (dirty_rect_ == kMaxRect ||
      frame->visible_rect() != last_frame_visible_rect_) {
    // Source or VideoFrame size change: Assume entire frame (including
    // letterboxed regions) have changed.
    update_rect = last_frame_visible_rect_ = frame->visible_rect();
  } else {
    update_rect = copy_output::ComputeResultRect(
        dirty_rect_, gfx::Vector2d(source_size.width(), source_size.height()),
        gfx::Vector2d(content_rect.width(), content_rect.height()));
    update_rect.Offset(content_rect.OffsetFromOrigin());
    if (pixel_format_ == media::PIXEL_FORMAT_I420 ||
        pixel_format_ == media::PIXEL_FORMAT_NV12) {
      update_rect = ExpandRectToI420SubsampleBoundaries(update_rect);
    }
  }
  metadata.capture_update_rect = update_rect;

  // If the frame is a resurrected one, just deliver it since it already
  // contains the most up-to-date capture of the source content.
  if (can_resurrect_content) {
    if (log_to_webrtc_) {
      std::string strides = "";
      switch (frame->format()) {
        case media::PIXEL_FORMAT_I420:
          strides = base::StringPrintf("strideY:%d StrideU:%d StrideV:%d",
                                       frame->stride(VideoFrame::Plane::kY),
                                       frame->stride(VideoFrame::Plane::kU),
                                       frame->stride(VideoFrame::Plane::kV));
          break;
        case media::PIXEL_FORMAT_ARGB:
          strides = base::StringPrintf("strideARGB:%d",
                                       frame->stride(VideoFrame::Plane::kARGB));
          break;
        case media::PIXEL_FORMAT_NV12:
          strides = base::StringPrintf("strideY:%d StrideUV:%d",
                                       frame->stride(VideoFrame::Plane::kY),
                                       frame->stride(VideoFrame::Plane::kUV));
          break;
        default:
          strides = "strides:???";
      }
      consumer_->OnLog(base::StringPrintf(
          "FrameSinkVideoCapturerImpl: Resurrecting frame format=%s "
          "frame_coded_size: %s "
          "frame_visible_rect: %s frame_natural_size: %s %s",
          VideoPixelFormatToString(frame->format()).c_str(),
          frame->coded_size().ToString().c_str(),
          frame->visible_rect().ToString().c_str(),
          frame->natural_size().ToString().c_str(), strides.c_str()));
    }

    FrameCapture frame_capture(
        capture_frame_number, oracle_frame_number, content_version_,
        content_rect, *region_properties, std::move(frame), capture_begin_time);
    frame_capture.CaptureSucceeded();
    OnFrameReadyForDelivery(frame_capture);
    return;
  }

  // At this point, we know the frame is not resurrected, so there will be only
  // one reference to it (held by us). It means that we are free to mutate the
  // frame's pixel content however we want.
  CHECK(frame->HasOneRef());

  // Extreme edge-case: If somehow the source size is so tiny that the content
  // region becomes empty, just deliver a frame filled with black.
  if (content_rect.IsEmpty()) {
    media::LetterboxVideoFrame(frame.get(), gfx::Rect());

    if (pixel_format_ == media::PIXEL_FORMAT_I420 ||
        pixel_format_ == media::PIXEL_FORMAT_NV12) {
      frame->set_color_space(gfx::ColorSpace::CreateREC709());
    } else {
      CHECK_EQ(pixel_format_, media::PIXEL_FORMAT_ARGB);
      frame->set_color_space(gfx::ColorSpace::CreateSRGB());
    }

    dirty_rect_ = gfx::Rect();
    FrameCapture frame_capture(
        capture_frame_number, oracle_frame_number, content_version_,
        gfx::Rect(), *region_properties, std::move(frame), capture_begin_time);
    frame_capture.CaptureSucceeded();
    OnFrameReadyForDelivery(frame_capture);
    return;
  }

  // If the target is in a different renderer than the root renderer (indicated
  // by having a different frame sink ID), we currently cannot provide
  // reasonable metadata about the region capture rect. For more context, see
  // https://crbug.com/1327560.
  //
  // TODO(crbug.com/40228439): Provide accurate bounds for elements
  // embedded in different renderers.
  const bool is_same_frame_sink_as_requested =
      resolved_target_->GetFrameSinkId() == target_->frame_sink_id;
  if (IsRegionCapture(target_->sub_target) && is_same_frame_sink_as_requested) {
    const float scale_factor = frame_metadata.device_scale_factor;
    metadata.region_capture_rect =
        scale_factor
            ? ScaleToEnclosingRect(region_properties->render_pass_subrect,
                                   1.0f / scale_factor)
            : region_properties->render_pass_subrect;
    metadata.source_size = source_size;
  }
  // Note that this is done unconditionally, as a new sub-capture-target version
  // may indicate that the stream has been successfully uncropped.
  metadata.sub_capture_target_version = sub_capture_target_version_;
  FrameCapture frame_capture(capture_frame_number, oracle_frame_number,
                             content_version_, content_rect, *region_properties,
                             std::move(frame), capture_begin_time);

  // TODO(crbug.com/346799708): The condition to check `pixel_format_` shouldn't
  // be necessary but video capture is started with I420+GMB in tests. That
  // still captures software I420 frames and not textures.
  const bool capture_texture_results =
      buffer_format_preference_ ==
          mojom::BufferFormatPreference::kPreferGpuMemoryBuffer &&
      (pixel_format_ == media::PIXEL_FORMAT_NV12 ||
       pixel_format_ == media::PIXEL_FORMAT_ARGB);

  std::optional<BlitRequest> blit_request;
  if (capture_texture_results) {
    TRACE_EVENT("gpu.capture", "PopulateBlitRequest");

    auto sync_token = frame_capture.frame->acquire_sync_token();
    auto mailbox = frame_capture.frame->shared_image()->mailbox();

    // TODO(crbug.com/41350322): change the capturer to only request the
    // parts of the frame that have changed whenever possible.
    blit_request =
        BlitRequest(content_rect.origin(), LetterboxingBehavior::kLetterbox,
                    mailbox, sync_token, true);

    // We haven't captured the frame yet, but let's pretend that we did for
    // the sake of blend information computation. We will be asking for an
    // entire frame (not just dirty part - for that, we'd need to know what
    // the diff between the frame we got and current content version is).
    VideoCaptureOverlay::CapturedFrameProperties frame_properties =
        VideoCaptureOverlay::CapturedFrameProperties{
            frame_capture.region_properties, frame_capture.content_rect,
            pixel_format_};

    for (const VideoCaptureOverlay* overlay : GetOverlaysInOrder()) {
      std::optional<VideoCaptureOverlay::BlendInformation> blend_information =
          overlay->CalculateBlendInformation(frame_properties);
      if (!blend_information) {
        continue;
      }

      // Blend in Skia happens from the unscaled bitmap, into the destination
      // region expressed in content's (aka VideoFrame's) space:
      blit_request->AppendBlendBitmap(
          blend_information->source_region,
          blend_information->destination_region_content,
          overlay->bitmap().asImage());
    }
  }

  // Request a copy of the next frame from the frame sink.
  auto request = std::make_unique<CopyOutputRequest>(
      VideoPixelFormatToCopyOutputRequestFormat(pixel_format_),
      capture_texture_results
          ? CopyOutputRequest::ResultDestination::kNativeTextures
          : CopyOutputRequest::ResultDestination::kSystemMemory,
      base::BindOnce(&FrameSinkVideoCapturerImpl::DidCopyFrame,
                     capture_weak_factory_.GetWeakPtr(),
                     std::move(frame_capture)));

  request->set_result_task_runner(
      base::SequencedTaskRunner::GetCurrentDefault());
  request->set_source(copy_request_source_);
  request->set_area(region_properties->render_pass_subrect);
  request->SetScaleRatio(
      gfx::Vector2d(source_size.width(), source_size.height()),
      gfx::Vector2d(content_rect.width(), content_rect.height()));
  // TODO(crbug.com/41350322): As an optimization, set the result
  // selection to just the part of the result that would have changed due to
  // aggregated damage over all the frames that weren't captured. This is
  // only possible if we kept track of the damage between contents stored
  // in `frame`, and the current contents.
  request->set_result_selection(gfx::Rect(content_rect.size()));

  if (blit_request) {
    request->set_blit_request(std::move(*blit_request));
  }

  // Clear the `dirty_rect_`, to indicate all changes at the source are now
  // being captured.
  dirty_rect_ = gfx::Rect();

  if (log_to_webrtc_) {
    const std::string format = media::VideoPixelFormatToString(pixel_format_);
    // NV12 is currently supported only via GpuMemoryBuffers, everything else is
    // returned as a bitmap:
    const bool is_bitmap =
        buffer_format_preference_ == mojom::BufferFormatPreference::kDefault;
    consumer_->OnLog(base::StringPrintf(
        "FrameSinkVideoCapturerImpl: Sending CopyRequest: "
        "format=%s (%s) area:%s "
        "scale_from: %s "
        "scale_to: %s "
        "frame pool utilization: %d",
        format.c_str(), is_bitmap ? "bitmap" : "GPU memory buffer",
        request->area().ToString().c_str(),
        request->scale_from().ToString().c_str(),
        request->scale_to().ToString().c_str(), utilization_pct));
  }

  const SubtreeCaptureId subtree_id =
      IsSubtreeCapture(target_->sub_target)
          ? absl::get<SubtreeCaptureId>(target_->sub_target)
          : SubtreeCaptureId();

  resolved_target_->RequestCopyOfOutput(
      {LocalSurfaceId(), subtree_id, std::move(request)});
}

void FrameSinkVideoCapturerImpl::DidCopyFrame(
    FrameCapture frame_capture,
    std::unique_ptr<CopyOutputResult> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_GE(frame_capture.capture_frame_number, next_delivery_frame_number_);
  CHECK(frame_capture.frame);
  CHECK(result);

  scoped_refptr<media::VideoFrame>& frame = frame_capture.frame;
  const gfx::Rect& content_rect = frame_capture.content_rect;

  if (log_to_webrtc_ && consumer_) {
    std::string format = "";
    std::string strides = "";
    switch (result->format()) {
      case CopyOutputResult::Format::I420_PLANES:
        format = "I420";
        strides = base::StringPrintf("strideY:%d StrideU:%d StrideV:%d",
                                     frame->stride(VideoFrame::Plane::kY),
                                     frame->stride(VideoFrame::Plane::kU),
                                     frame->stride(VideoFrame::Plane::kV));
        break;
      case CopyOutputResult::Format::NV12:
        format = "NV12";
        strides = base::StringPrintf("strideY:%d StrideUV:%d",
                                     frame->stride(VideoFrame::Plane::kY),
                                     frame->stride(VideoFrame::Plane::kUV));
        break;
      case CopyOutputResult::Format::RGBA:
        strides = base::StringPrintf("strideARGB:%d",
                                     frame->stride(VideoFrame::Plane::kARGB));

        switch (result->destination()) {
          case CopyOutputResult::Destination::kSystemMemory:
            format = "ARGB_Bitmap";
            break;
          case CopyOutputResult::Destination::kNativeTextures:
            format = "ARGB_Texture";
            break;
        }
        break;
    }
    consumer_->OnLog(base::StringPrintf(
        "FrameSinkVideoCapturerImpl: got CopyOutputResult: format=%s size:%s "
        "frame_coded_size: %s frame_visible_rect: %s frame_natural_size: %s "
        "content_rect: %s %s",
        format.c_str(), result->size().ToString().c_str(),
        frame->coded_size().ToString().c_str(),
        frame->visible_rect().ToString().c_str(),
        frame->natural_size().ToString().c_str(),
        content_rect.ToString().c_str(), strides.c_str()));
  }

  // Stop() should have canceled any outstanding copy requests. So, by reaching
  // this point, `consumer_` should be bound.
  CHECK(consumer_);

  if (pixel_format_ == media::PIXEL_FORMAT_I420) {
    CHECK_EQ(content_rect.x() % 2, 0);
    CHECK_EQ(content_rect.y() % 2, 0);
    CHECK_EQ(content_rect.width() % 2, 0);
    CHECK_EQ(content_rect.height() % 2, 0);
    // Populate the VideoFrame from the CopyOutputResult.
    const int y_stride = frame->stride(VideoFrame::Plane::kY);
    uint8_t* const y = frame->GetWritableVisibleData(VideoFrame::Plane::kY) +
                       content_rect.y() * y_stride + content_rect.x();
    const int u_stride = frame->stride(VideoFrame::Plane::kU);
    uint8_t* const u = frame->GetWritableVisibleData(VideoFrame::Plane::kU) +
                       (content_rect.y() / 2) * u_stride +
                       (content_rect.x() / 2);
    const int v_stride = frame->stride(VideoFrame::Plane::kV);
    uint8_t* const v = frame->GetWritableVisibleData(VideoFrame::Plane::kV) +
                       (content_rect.y() / 2) * v_stride +
                       (content_rect.x() / 2);
    bool success =
        result->ReadI420Planes(y, y_stride, u, u_stride, v, v_stride);
    if (success) {
      // Per CopyOutputResult header comments, I420_PLANES results are always in
      // the Rec.709 color space.
      frame->set_color_space(gfx::ColorSpace::CreateREC709());
      UMA_HISTOGRAM_CAPTURE_DURATION(
          "I420", base::TimeTicks::Now() - frame_capture.request_time);
      frame_capture.CaptureSucceeded();
    } else {
      frame_capture.CaptureFailed(CaptureResult::kI420ReadbackFailed);
    }
    UMA_HISTOGRAM_CAPTURE_SUCCEEDED("I420", success);
  } else if (pixel_format_ == media::PIXEL_FORMAT_ARGB) {
    if (buffer_format_preference_ == mojom::BufferFormatPreference::kDefault) {
      int stride = frame->stride(VideoFrame::Plane::kARGB);
      // Note: ResultFormat::RGBA CopyOutputResult's format currently is
      // kN32_SkColorType, which can be RGBA or BGRA depending on the platform.
      uint8_t* const pixels =
          frame->GetWritableVisibleData(VideoFrame::Plane::kARGB) +
          content_rect.y() * stride + content_rect.x() * 4;
      bool success = result->ReadRGBAPlane(pixels, stride);
      if (success) {
        frame->set_color_space(result->GetRGBAColorSpace());
        UMA_HISTOGRAM_CAPTURE_DURATION(
            "RGBA", base::TimeTicks::Now() - frame_capture.request_time);
        frame_capture.CaptureSucceeded();
      } else {
        frame_capture.CaptureFailed(CaptureResult::kARGBReadbackFailed);
      }
    } else {
      CHECK_EQ(buffer_format_preference_,
               mojom::BufferFormatPreference::kPreferGpuMemoryBuffer);
      // GMB ARGB results are written to the existing pool texture.
      if (result->IsEmpty()) {
        frame_capture.CaptureFailed(
            CaptureResult::kGpuMemoryBufferReadbackFailed);
      } else {
        UMA_HISTOGRAM_CAPTURE_DURATION(
            "RGBA", base::TimeTicks::Now() - frame_capture.request_time);
        frame_capture.CaptureSucceeded();
      }
      UMA_HISTOGRAM_CAPTURE_SUCCEEDED("RGBA", !result->IsEmpty());
    }
  } else {
    CHECK_EQ(pixel_format_, media::PIXEL_FORMAT_NV12);
    // NV12 is only supported for GMBs for now, in which case there is nothing
    // for us to do since the CopyOutputResults are already available in the
    // video frame (assuming that we got the results).

    if (result->IsEmpty()) {
      frame_capture.CaptureFailed(CaptureResult::kNV12ReadbackFailed);
    } else {
      frame_capture.CaptureSucceeded();
    }

    UMA_HISTOGRAM_CAPTURE_DURATION(
        "NV12", base::TimeTicks::Now() - frame_capture.request_time);

    UMA_HISTOGRAM_CAPTURE_SUCCEEDED("NV12", !result->IsEmpty());
  }

  if (frame_capture.success()) {
    // The result may be smaller than what was requested, if unforeseen
    // clamping to the source boundaries occurred by the executor of the
    // CopyOutputRequest. However, the result should never contain more than
    // what was requested.
    CHECK_LE(result->size().width(), content_rect.width());
    CHECK_LE(result->size().height(), content_rect.height());

    if (!frame->HasMappableGpuBuffer()) {
      const VideoCaptureOverlay::CapturedFrameProperties frame_properties{
          frame_capture.region_properties, content_rect, frame->format()};

      // For GMB-backed video frames, overlays were already applied by
      // CopyOutputRequest API. For in-memory frames, apply overlays here:
      auto overlay_renderer = VideoCaptureOverlay::MakeCombinedRenderer(
          GetOverlaysInOrder(), frame_properties);

      if (overlay_renderer) {
        TRACE_EVENT("gpu.capture", "BlendVideoCaptureOverlays",
                    "frame_properties", frame_properties.ToString());
        std::move(overlay_renderer).Run(frame.get());
      }
    }

    const gfx::Rect result_rect =
        gfx::Rect(content_rect.origin(), result->size());
    DCHECK(IsCompatibleWithFormat(result_rect, pixel_format_));
    if (frame->visible_rect() != result_rect &&
        !frame->HasMappableGpuBuffer()) {
      // If there are parts of the frame that are visible but we have not wrote
      // into them, letterbox them. This is not needed for GMB-backed frames as
      // the letterboxing happens on GPU.
      media::LetterboxVideoFrame(frame.get(), result_rect);
    }

    if (ShouldMark(*frame, frame_capture.content_version)) {
      MarkFrame(frame, frame_capture.content_version);
    }
  }

  OnFrameReadyForDelivery(frame_capture);
}

void FrameSinkVideoCapturerImpl::OnFrameReadyForDelivery(
    const FrameCapture& frame_capture) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_GE(frame_capture.capture_frame_number, next_delivery_frame_number_);
  CHECK(frame_capture.finished());

  // From this point onward, we're not allowed to mutate `frame`'s pixels as we
  // may be operating on a resurrected frame.

  if (frame_capture.success()) {
    const scoped_refptr<media::VideoFrame>& frame = frame_capture.frame;
    frame->metadata().capture_end_time = clock_->NowTicks();
    base::TimeDelta sample = *frame->metadata().capture_end_time -
                             *frame->metadata().capture_begin_time;

    if (frame->format() == media::PIXEL_FORMAT_I420) {
      UMA_HISTOGRAM_CUSTOM_TIMES(
          "Viz.FrameSinkVideoCapturer.I420.TotalDuration", sample,
          base::Milliseconds(1), base::Seconds(1), 50);
    } else if (frame->format() == media::PIXEL_FORMAT_NV12) {
      UMA_HISTOGRAM_CUSTOM_TIMES(
          "Viz.FrameSinkVideoCapturer.NV12.TotalDuration", sample,
          base::Milliseconds(1), base::Seconds(1), 50);
    }

    UMA_HISTOGRAM_CUSTOM_TIMES("Viz.FrameSinkVideoCapturer.TotalDuration",
                               sample, base::Milliseconds(1), base::Seconds(1),
                               50);
  }

  // Ensure frames are delivered in-order by using a min-heap, and only
  // deliver the next frame(s) in-sequence when they are found at the top.
  delivery_queue_.emplace(std::move(frame_capture));
  while (delivery_queue_.top().capture_frame_number ==
         next_delivery_frame_number_) {
    auto& next = delivery_queue_.top();
    MaybeDeliverFrame(next);
    ++next_delivery_frame_number_;
    delivery_queue_.pop();
    if (delivery_queue_.empty()) {
      break;
    }
  }
}

void FrameSinkVideoCapturerImpl::MaybeDeliverFrame(FrameCapture frame_capture) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::TimeTicks media_ticks;

  if (frame_capture.success()) {
    // TODO(crbug.com/40227755): When capture fails because the
    // sub-capture-target version has changed, expedite the capture/delivery of
    // a new frame.
    if (frame_capture.frame->metadata().sub_capture_target_version !=
        sub_capture_target_version_) {
      frame_capture.CaptureFailed(CaptureResult::kSubCaptureTargetChanged);
    } else if (!oracle_->CompleteCapture(frame_capture.oracle_frame_number,
                                         frame_capture.success(),
                                         &media_ticks)) {
      // The Oracle has the final say in whether frame delivery will proceed. It
      // also rewrites the media timestamp in terms of the smooth flow of the
      // original source content.
      frame_capture.CaptureFailed(CaptureResult::kOracleRejectedFrame);
    }
  }

  if (!frame_capture.success()) {
    const media::VideoFrameMetadata& metadata = frame_capture.frame_metadata();
    TRACE_EVENT_END("gpu.capture", CaptureTrack(metadata), "result",
                    frame_capture.result());
    TRACE_EVENT_END("gpu.capture", FrameInUseTrack(metadata), "frame_number",
                    metadata.capture_counter, "utilization_pct",
                    AsPercent(GetPipelineUtilization()));
    MaybeScheduleRefreshFrame();
    return;
  }

  scoped_refptr<media::VideoFrame>& frame = frame_capture.frame;

  // Set media timestamp in terms of the time offset since the first frame.
  if (!first_frame_media_ticks_) {
    first_frame_media_ticks_ = media_ticks;
  }
  frame->set_timestamp(media_ticks - *first_frame_media_ticks_);

  TRACE_EVENT_END("gpu.capture", CaptureTrack(frame_capture.frame_metadata()),
                  "result", frame_capture.result(), "timestamp_micros",
                  frame->timestamp().InMicroseconds());

  // Clone a handle to the shared memory backing the populated video frame, to
  // send to the consumer.
  auto handle = frame_pool_->CloneHandleForDelivery(*frame);
  CHECK(handle);
  CHECK(!handle->is_read_only_shmem_region() ||
        handle->get_read_only_shmem_region().IsValid());

  // Assemble frame layout, format, and metadata into a mojo struct to send to
  // the consumer.
  media::mojom::VideoFrameInfoPtr info = media::mojom::VideoFrameInfo::New();
  info->timestamp = frame->timestamp();
  info->metadata = frame->metadata();
  info->pixel_format = frame->format();
  info->coded_size = frame->coded_size();
  info->visible_rect = frame->visible_rect();
  DCHECK(frame->ColorSpace().IsValid());  // Ensure it was set by this point.
  info->color_space = frame->ColorSpace();

  // Create an InFlightFrameDelivery for this frame, owned by its mojo receiver.
  // It responds to the consumer's Done() notification by returning the video
  // frame to the `frame_pool_`. It responds to the optional ProvideFeedback()
  // by forwarding the measurement to the `oracle_`.
  mojo::PendingRemote<mojom::FrameSinkVideoConsumerFrameCallbacks> callbacks;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<InFlightFrameDelivery>(
          base::BindOnce(&FrameSinkVideoCapturerImpl::NotifyFrameReleased,
                         capture_weak_factory_.GetWeakPtr(), std::move(frame)),
          base::BindOnce(&VideoCaptureOracle::RecordConsumerFeedback,
                         feedback_weak_factory_.GetWeakPtr(),
                         frame_capture.oracle_frame_number)),
      callbacks.InitWithNewPipeAndPassReceiver());

  num_frames_in_flight_++;

  TRACE_COUNTER("gpu.capture", "NumFramesInFlight", num_frames_in_flight_);

  // Send the frame to the consumer.
  consumer_->OnFrameCaptured(std::move(handle), std::move(info),
                             frame_capture.content_rect, std::move(callbacks));
  consumer_informed_of_empty_region_ = false;
}

gfx::Size FrameSinkVideoCapturerImpl::AdjustSizeForPixelFormat(
    const gfx::Size& raw_size) const {
  if (pixel_format_ == media::PIXEL_FORMAT_ARGB) {
    gfx::Size result(raw_size);
    if (result.width() <= 0) {
      result.set_width(1);
    }
    if (result.height() <= 0) {
      result.set_height(1);
    }
    return result;
  }
  CHECK(media::PIXEL_FORMAT_I420 == pixel_format_ ||
        media::PIXEL_FORMAT_NV12 == pixel_format_);
  gfx::Size result(raw_size.width() & ~1, raw_size.height() & ~1);
  if (result.width() <= 0) {
    result.set_width(2);
  }
  if (result.height() <= 0) {
    result.set_height(2);
  }
  return result;
}

// static
gfx::Rect FrameSinkVideoCapturerImpl::ExpandRectToI420SubsampleBoundaries(
    const gfx::Rect& rect) {
  const int x = rect.x() & ~1;
  const int y = rect.y() & ~1;
  const int r = rect.right() + (rect.right() & 1);
  const int b = rect.bottom() + (rect.bottom() & 1);
  return gfx::Rect(x, y, r - x, b - y);
}

void FrameSinkVideoCapturerImpl::OnLog(const std::string& message) {
  if (log_to_webrtc_ && consumer_) {
    consumer_->OnLog(message);
  }
}

bool FrameSinkVideoCapturerImpl::ShouldMark(const media::VideoFrame& frame,
                                            int64_t content_version) const {
  return !marked_frame_ || content_version > content_version_in_marked_frame_ ||
         frame.coded_size() != marked_frame_->coded_size();
}

void FrameSinkVideoCapturerImpl::MarkFrame(
    scoped_refptr<media::VideoFrame> frame,
    int64_t content_version) {
  marked_frame_ = frame;
  content_version_in_marked_frame_ = marked_frame_ ? content_version : -1;
}

bool FrameSinkVideoCapturerImpl::CanResurrectFrame(
    const gfx::Size& size) const {
  return content_version_ == content_version_in_marked_frame_ &&
         marked_frame_ && marked_frame_->coded_size() == size;
}

void FrameSinkVideoCapturerImpl::NotifyFrameReleased(
    scoped_refptr<media::VideoFrame> frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  num_frames_in_flight_--;
  const media::VideoFrameMetadata metadata = frame->metadata();
  TRACE_EVENT_END("gpu.capture", FrameInUseTrack(metadata), "frame_number",
                  metadata.capture_counter, "utilization_pct",
                  AsPercent(GetPipelineUtilization()));
}

float FrameSinkVideoCapturerImpl::GetPipelineUtilization() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return num_frames_in_flight_ /
         (kDesignLimitMaxFrames * kTargetPipelineUtilization);
}

void FrameSinkVideoCapturerImpl::MaybeInformConsumerOfEmptyRegion() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!consumer_ || !target_ || !IsRegionCapture(target_->sub_target) ||
      consumer_informed_of_empty_region_) {
    return;
  }

  consumer_->OnFrameWithEmptyRegionCapture();
  consumer_informed_of_empty_region_ = true;
}

}  // namespace viz
