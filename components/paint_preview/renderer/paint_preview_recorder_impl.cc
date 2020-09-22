// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/renderer/paint_preview_recorder_impl.h"

#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/optional.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/common/trace_event_common.h"
#include "cc/paint/paint_record.h"
#include "cc/paint/paint_recorder.h"
#include "components/paint_preview/common/paint_preview_tracker.h"
#include "components/paint_preview/common/serialized_recording.h"
#include "components/paint_preview/renderer/paint_preview_recorder_utils.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace paint_preview {

namespace {

struct FinishedRecording {
  FinishedRecording(mojom::PaintPreviewStatus status,
                    mojom::PaintPreviewCaptureResponsePtr response)
      : status(status), response(std::move(response)) {}
  ~FinishedRecording() = default;

  FinishedRecording(FinishedRecording&& other) = default;
  FinishedRecording& operator=(FinishedRecording&& other) = default;

  FinishedRecording(const FinishedRecording& other) = delete;
  FinishedRecording& operator=(const FinishedRecording& other) = delete;

  mojom::PaintPreviewStatus status;
  mojom::PaintPreviewCaptureResponsePtr response;
};

FinishedRecording FinishRecording(
    sk_sp<const cc::PaintRecord> recording,
    const gfx::Rect& bounds,
    std::unique_ptr<PaintPreviewTracker> tracker,
    RecordingPersistence persistence,
    base::File skp_file,
    base::Optional<size_t> max_capture_size,
    mojom::PaintPreviewCaptureResponsePtr response) {
  TRACE_EVENT0("paint_preview", "FinishRecording");
  FinishedRecording out(mojom::PaintPreviewStatus::kOk, std::move(response));
  DCHECK(tracker);
  if (!tracker) {
    out.status = mojom::PaintPreviewStatus::kCaptureFailed;
    return out;
  }

  TRACE_EVENT_BEGIN0("paint_preview", "ParseGlyphsAndLinks");
  ParseGlyphsAndLinks(recording.get(), tracker.get());
  TRACE_EVENT_END0("paint_preview", "ParseGlyphsAndLinks");

  TRACE_EVENT0("paint_preview", "SerializeAsSkPicture");

  bool success = false;
  size_t serialized_size = 0;
  {
    auto skp = PaintRecordToSkPicture(recording, tracker.get(), bounds);
    recording.reset();
    if (!skp) {
      out.status = mojom::PaintPreviewStatus::kCaptureFailed;
      return out;
    }

    switch (persistence) {
      case RecordingPersistence::kFileSystem:
        success = RecordToFile(std::move(skp_file), skp, tracker.get(),
                               max_capture_size, &serialized_size);
        break;
      case RecordingPersistence::kMemoryBuffer:
        base::Optional<mojo_base::BigBuffer> buffer = RecordToBuffer(
            skp, tracker.get(), max_capture_size, &serialized_size);
        success = buffer.has_value();
        out.response->skp.emplace(std::move(buffer.value()));
        break;
    }
  }

  if (!success) {
    out.status = mojom::PaintPreviewStatus::kCaptureFailed;
    return out;
  }

  BuildResponse(tracker.get(), out.response.get(), /*log=*/true);
  out.response->serialized_size = serialized_size;
  return out;
}

}  // namespace

PaintPreviewRecorderImpl::PaintPreviewRecorderImpl(
    content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame),
      is_painting_preview_(false),
      is_main_frame_(render_frame->IsMainFrame()) {
  render_frame->GetAssociatedInterfaceRegistry()->AddInterface(
      base::BindRepeating(&PaintPreviewRecorderImpl::BindPaintPreviewRecorder,
                          weak_ptr_factory_.GetWeakPtr()));
}

PaintPreviewRecorderImpl::~PaintPreviewRecorderImpl() = default;

void PaintPreviewRecorderImpl::CapturePaintPreview(
    mojom::PaintPreviewCaptureParamsPtr params,
    CapturePaintPreviewCallback callback) {
  TRACE_EVENT0("paint_preview",
               "PaintPreviewRecorderImpl::CapturePaintPreview");
  base::ReadOnlySharedMemoryRegion region;
  // This should not be called recursively or multiple times while unfinished
  // (Blink can only run one capture per RenderFrame at a time).
  DCHECK(!is_painting_preview_);
  // DCHECK, but fallback safely as it is difficult to reason about whether this
  // might happen due to it being tied to a RenderFrame rather than
  // RenderWidget and we don't want to crash the renderer as this is
  // recoverable.
  auto response = mojom::PaintPreviewCaptureResponse::New();
  if (is_painting_preview_) {
    std::move(callback).Run(mojom::PaintPreviewStatus::kAlreadyCapturing,
                            std::move(response));
    return;
  }
  base::AutoReset<bool>(&is_painting_preview_, true);
  CapturePaintPreviewInternal(params, std::move(response), std::move(callback));
}

void PaintPreviewRecorderImpl::OnDestruct() {
  paint_preview_recorder_receiver_.reset();
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
}

void PaintPreviewRecorderImpl::BindPaintPreviewRecorder(
    mojo::PendingAssociatedReceiver<mojom::PaintPreviewRecorder> receiver) {
  paint_preview_recorder_receiver_.Bind(std::move(receiver));
}

void PaintPreviewRecorderImpl::CapturePaintPreviewInternal(
    const mojom::PaintPreviewCaptureParamsPtr& params,
    mojom::PaintPreviewCaptureResponsePtr response,
    CapturePaintPreviewCallback callback) {
  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
  // Ensure the a frame actually exists to avoid a possible crash.
  if (!frame) {
    DVLOG(1) << "Error: renderer has no frame yet!";
    std::move(callback).Run(mojom::PaintPreviewStatus::kFailed,
                            std::move(response));
    return;
  }

  DCHECK_EQ(is_main_frame_, params->is_main_frame);
  // Default to using the clip rect.
  gfx::Rect bounds = gfx::Rect(params->clip_rect.size());
  if (bounds.IsEmpty() || params->clip_rect_is_hint) {
    // If the clip rect is empty or only a hint try to use the document size.
    auto size = frame->DocumentSize();
    gfx::Rect document_rect = gfx::Rect(0, 0, size.width, size.height);
    if (!document_rect.IsEmpty())
      bounds = document_rect;

    if (bounds.IsEmpty()) {
      // |bounds| may be empty if a capture is triggered prior to geometry
      // being finalized and no clip rect was provided. If this happens there
      // are no valid dimensions for the canvas and an abort is needed.
      //
      // This should only happen in tests or if a capture is triggered
      // immediately after a navigation finished.
      std::move(callback).Run(mojom::PaintPreviewStatus::kCaptureFailed,
                              std::move(response));
      return;
    }
  }

  auto tracker = std::make_unique<PaintPreviewTracker>(
      params->guid, frame->GetEmbeddingToken(), is_main_frame_);
  auto size = frame->GetScrollOffset();
  response->scroll_offsets = gfx::Size(size.width, size.height);

  cc::PaintRecorder recorder;
  cc::PaintCanvas* canvas =
      recorder.beginRecording(bounds.width(), bounds.height());
  canvas->SetPaintPreviewTracker(tracker.get());

  // Use time ticks manually rather than a histogram macro so as to;
  // 1. Account for main frames and subframes separately.
  // 2. Mitigate binary size as this won't be used that often.
  // 3. Record only on successes as failures are likely to be outliers (fast or
  //    slow).
  base::TimeTicks start_time = base::TimeTicks::Now();
  TRACE_EVENT_BEGIN0("paint_preview", "WebLocalFrame::CapturePaintPreview");
  bool success = frame->CapturePaintPreview(
      bounds, canvas, /*include_linked_destinations=*/params->capture_links);
  TRACE_EVENT_END0("paint_preview", "WebLocalFrame::CapturePaintPreview");
  base::TimeDelta capture_time = base::TimeTicks::Now() - start_time;
  response->blink_recording_time = capture_time;

  if (is_main_frame_) {
    base::UmaHistogramBoolean("Renderer.PaintPreview.Capture.MainFrameSuccess",
                              success);
    if (success) {
      // Main frame should generally be the largest cost and will always run so
      // it is tracked separately.
      base::UmaHistogramTimes(
          "Renderer.PaintPreview.Capture.MainFrameBlinkCaptureDuration",
          capture_time);
    }
  } else {
    base::UmaHistogramBoolean("Renderer.PaintPreview.Capture.SubframeSuccess",
                              success);
    if (success) {
      base::UmaHistogramTimes(
          "Renderer.PaintPreview.Capture.SubframeBlinkCaptureDuration",
          capture_time);
    }
  }

  // Restore to before out-of-lifecycle paint phase.
  if (!success) {
    std::move(callback).Run(mojom::PaintPreviewStatus::kCaptureFailed,
                            std::move(response));
    return;
  }

  // Convert the special value |0| to |base::nullopt|.
  base::Optional<size_t> max_capture_size;
  if (params->max_capture_size == 0) {
    max_capture_size = base::nullopt;
  } else {
    max_capture_size = params->max_capture_size;
  }

  // This cannot be done async if the recording contains a GPU accelerated
  // image.
  FinishedRecording recording = FinishRecording(
      recorder.finishRecordingAsPicture(), bounds, std::move(tracker),
      params->persistence, std::move(params->file), max_capture_size,
      std::move(response));
  std::move(callback).Run(recording.status, std::move(recording.response));
}

}  // namespace paint_preview
