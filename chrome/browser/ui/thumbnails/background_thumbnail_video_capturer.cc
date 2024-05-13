// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/thumbnails/background_thumbnail_video_capturer.h"

#include <stdint.h>
#include <utility>

#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_id_helper.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "media/capture/mojom/video_capture_buffer.mojom.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "ui/gfx/geometry/skia_conversions.h"

BackgroundThumbnailVideoCapturer::BackgroundThumbnailVideoCapturer(
    content::WebContents* contents,
    GotFrameCallback got_frame_callback)
    : contents_(contents), got_frame_callback_(std::move(got_frame_callback)) {
  DCHECK(contents_);
  DCHECK(got_frame_callback_);
}

BackgroundThumbnailVideoCapturer::~BackgroundThumbnailVideoCapturer() {
  if (video_capturer_)
    Stop();
}

void BackgroundThumbnailVideoCapturer::Start(
    const ThumbnailCaptureInfo& capture_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (video_capturer_)
    return;

  content::RenderWidgetHostView* const source_view =
      contents_->GetPrimaryMainFrame()
          ->GetRenderViewHost()
          ->GetWidget()
          ->GetView();
  if (!source_view)
    return;

  capture_info_ = capture_info;

  {
    // Assign IDs to each capture session for tracing. IDs are unique
    // throughout the lifetime of this process. Using a static here is
    // safe since this is only invoked from the UI thread.
    static uint64_t capture_num GUARDED_BY_CONTEXT(sequence_checker_) = 0;
    cur_capture_num_ = ++capture_num;
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("ui", "Tab.Preview.VideoCapture",
                                      TRACE_ID_LOCAL(cur_capture_num_));
  }

  start_time_ = base::TimeTicks::Now();
  num_received_frames_ = 0;

  constexpr int kMaxFrameRate = 2;
  video_capturer_ = source_view->CreateVideoCapturer();
  video_capturer_->SetResolutionConstraints(capture_info_.target_size,
                                            capture_info_.target_size, false);
  video_capturer_->SetAutoThrottlingEnabled(false);
  video_capturer_->SetMinSizeChangePeriod(base::TimeDelta());
  video_capturer_->SetFormat(media::PIXEL_FORMAT_ARGB);
  video_capturer_->SetMinCapturePeriod(base::Seconds(1) / kMaxFrameRate);
  video_capturer_->Start(this, viz::mojom::BufferFormatPreference::kDefault);
}

void BackgroundThumbnailVideoCapturer::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!video_capturer_)
    return;

  video_capturer_->Stop();
  video_capturer_.reset();

  TRACE_EVENT_NESTABLE_ASYNC_END0("ui", "Tab.Preview.VideoCapture",
                                  TRACE_ID_LOCAL(cur_capture_num_));
  start_time_ = base::TimeTicks();
  cur_capture_num_ = 0;
}

void BackgroundThumbnailVideoCapturer::OnFrameCaptured(
    ::media::mojom::VideoBufferHandlePtr data,
    ::media::mojom::VideoFrameInfoPtr info,
    const gfx::Rect& content_rect,
    mojo::PendingRemote<::viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
        callbacks) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK(video_capturer_);

  mojo::Remote<::viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
      callbacks_remote(std::move(callbacks));

  CHECK(data->is_read_only_shmem_region());
  base::ReadOnlySharedMemoryRegion& shmem_region =
      data->get_read_only_shmem_region();

  // The |data| parameter is not nullable and mojo type mapping for
  // `base::ReadOnlySharedMemoryRegion` defines that nullable version of it is
  // the same type, with null check being equivalent to IsValid() check. Given
  // the above, we should never be able to receive a read only shmem region that
  // is not valid - mojo will enforce it for us.
  DCHECK(shmem_region.IsValid());

  // Process captured image.
  base::ReadOnlySharedMemoryMapping mapping = shmem_region.Map();
  if (!mapping.IsValid()) {
    DLOG(ERROR) << "Shared memory mapping failed.";
    return;
  }
  if (mapping.size() <
      media::VideoFrame::AllocationSize(info->pixel_format, info->coded_size)) {
    DLOG(ERROR) << "Shared memory size was less than expected.";
    return;
  }

  TRACE_EVENT_INSTANT1("ui", "Tab.Preview.VideoCaptureFrameReceived",
                       TRACE_EVENT_SCOPE_THREAD, "frame_number",
                       num_received_frames_);
  ++num_received_frames_;

  uint64_t frame_id = base::trace_event::GetNextGlobalTraceId();
  TRACE_EVENT_WITH_FLOW0("ui", "Tab.Preview.ProcessVideoCaptureFrame", frame_id,
                         TRACE_EVENT_FLAG_FLOW_OUT);

  // The SkBitmap's pixels will be marked as immutable, but the installPixels()
  // API requires a non-const pointer. So, cast away the const.
  void* const pixels = const_cast<void*>(mapping.memory());

  // Call installPixels() with a |releaseProc| that: 1) notifies the capturer
  // that this consumer has finished with the frame, and 2) releases the shared
  // memory mapping.
  struct FramePinner {
    // Keeps the shared memory that backs |frame_| mapped.
    base::ReadOnlySharedMemoryMapping mapping;
    // Prevents FrameSinkVideoCapturer from recycling the shared memory that
    // backs |frame_|.
    mojo::PendingRemote<viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
        releaser;
  };

  // Subtract back out the scroll bars if we decided there was enough canvas to
  // account for them and still have a decent preview image.
  const float scale_ratio = static_cast<float>(content_rect.width()) /
                            capture_info_.copy_rect.width();

  const gfx::Insets original_scroll_insets = capture_info_.scrollbar_insets;
  const auto scroll_insets = gfx::Insets::TLBR(
      0, 0, std::round(original_scroll_insets.width() * scale_ratio),
      std::round(original_scroll_insets.height() * scale_ratio));
  gfx::Rect effective_content_rect = content_rect;
  effective_content_rect.Inset(scroll_insets);

  const gfx::Size bitmap_size(content_rect.right(), content_rect.bottom());
  SkBitmap frame;
  frame.installPixels(
      SkImageInfo::MakeN32(bitmap_size.width(), bitmap_size.height(),
                           kPremul_SkAlphaType,
                           info->color_space.ToSkColorSpace()),
      pixels,
      media::VideoFrame::RowBytes(media::VideoFrame::Plane::kARGB,
                                  info->pixel_format, info->coded_size.width()),
      [](void* addr, void* context) {
        delete static_cast<FramePinner*>(context);
      },
      new FramePinner{std::move(mapping), callbacks_remote.Unbind()});
  frame.setImmutable();

  SkBitmap cropped_frame;
  if (!frame.extractSubset(&cropped_frame,
                           gfx::RectToSkIRect(effective_content_rect))) {
    return;
  }

  got_frame_callback_.Run(cropped_frame, frame_id);
}

void BackgroundThumbnailVideoCapturer::OnNewSubCaptureTargetVersion(
    uint32_t sub_capture_target_version) {}

void BackgroundThumbnailVideoCapturer::OnFrameWithEmptyRegionCapture() {}

void BackgroundThumbnailVideoCapturer::OnStopped() {}

void BackgroundThumbnailVideoCapturer::OnLog(const std::string& /*message*/) {}
