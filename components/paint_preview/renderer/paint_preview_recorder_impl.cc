// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/renderer/paint_preview_recorder_impl.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_event.h"
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

// Returns a bound origin value centered about `offset` or clamped to the start
// or end of the document if centering would result in capturing less area.
// NOTE: this centers about a scroll offset ignoring the size of the viewport.
int GetBoundOrigin(int document_dimension, int bounds_dimension, int offset) {
  // `offset` must be less than the `document_dimension` or if the
  // `bounds_dimension` is 0 capture the whole document.
  if (document_dimension <= offset || bounds_dimension == 0) {
    return 0;
  }

  const int half_bounds = bounds_dimension / 2;
  if (document_dimension != 0 && document_dimension - offset < half_bounds) {
    // `document_dimension - offset` is the distance remaining from the current
    // scroll offset to the document end. If this is less than `half_bounds`
    // then the capture would be smaller than requested due to the position of
    // the offset relative to the document end.
    //
    // In this case, capture starting `bounds_dimension` from the document end
    // or the document start (0) whichever is greater.
    return std::max(document_dimension - bounds_dimension, 0);
  }
  // Center about `offset` if it is further than `half_bounds` from the document
  // start (0). Otherwise start the capture from the start of the document.
  return std::max(offset - half_bounds, 0);
}

// Represents a finished recording of the page represented by the response and
// status of the mojo message.
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

using CapturePaintPreviewCallback =
    base::OnceCallback<void(mojom::PaintPreviewStatus,
                            mojom::PaintPreviewCaptureResponsePtr)>;

// Finishes building the PaintPreviewCaptureResponse mojo message and sends it
// or sends an error if the status is not `PaintPreviewStatus::kOK`
void BuildAndSendResponse(std::unique_ptr<PaintPreviewTracker> tracker,
                          FinishedRecording out,
                          CapturePaintPreviewCallback callback) {
  if (out.status == mojom::PaintPreviewStatus::kOk) {
    BuildResponse(tracker.get(), out.response.get());
  }
  std::move(callback).Run(out.status, std::move(out.response));
}

// Records `skp` to `skp_file` on the threadpool to avoid blocking the main
// thread.
void RecordToFileOnThreadPool(sk_sp<const SkPicture> skp,
                              base::File skp_file,
                              std::unique_ptr<PaintPreviewTracker> tracker,
                              std::optional<size_t> max_capture_size,
                              FinishedRecording out,
                              CapturePaintPreviewCallback callback) {
  TRACE_EVENT0("paint_preview", "RecordToFileOnThreadPool");
  size_t serialized_size = 0;
  tracker->GetImageSerializationContext()->skip_texture_backed = true;
  bool success = RecordToFile(std::move(skp_file), skp, tracker.get(),
                              max_capture_size, &serialized_size);
  out.status = success ? mojom::PaintPreviewStatus::kOk
                       : mojom::PaintPreviewStatus::kCaptureFailed;
  out.response->serialized_size = serialized_size;

  BuildAndSendResponse(std::move(tracker), std::move(out), std::move(callback));
}

// Handles file persistence storage.
void SerializeFileRecording(sk_sp<const SkPicture> skp,
                            base::File skp_file,
                            std::unique_ptr<PaintPreviewTracker> tracker,
                            std::optional<size_t> max_capture_size,
                            FinishedRecording out,
                            CapturePaintPreviewCallback callback) {
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
       base::WithBaseSyncPrimitives()},
      BindOnce(&RecordToFileOnThreadPool, skp, std::move(skp_file),
               std::move(tracker), max_capture_size, std::move(out),
               base::BindPostTaskToCurrentDefault(std::move(callback))));
}

// Handles memory buffer persistence storage.
void SerializeMemoryBufferRecording(
    sk_sp<const SkPicture> skp,
    std::unique_ptr<PaintPreviewTracker> tracker,
    std::optional<size_t> max_capture_size,
    FinishedRecording out,
    CapturePaintPreviewCallback callback) {
  TRACE_EVENT0("paint_preview", "SerializeMemoryBufferRecording");
  size_t serialized_size = 0;
  std::optional<mojo_base::BigBuffer> buffer =
      RecordToBuffer(skp, tracker.get(), max_capture_size, &serialized_size);
  out.status = buffer.has_value() ? mojom::PaintPreviewStatus::kOk
                                  : mojom::PaintPreviewStatus::kCaptureFailed;
  if (buffer.has_value()) {
    out.response->skp.emplace(std::move(buffer.value()));
  }
  out.response->serialized_size = serialized_size;

  BuildAndSendResponse(std::move(tracker), std::move(out), std::move(callback));
}

// Finishes the recording process by converting the `recording` to an SkPicture.
// Serialization is then delegated based on the type of `persistence`.
void FinishRecordingOnUIThread(cc::PaintRecord recording,
                               const gfx::Rect& bounds,
                               std::unique_ptr<PaintPreviewTracker> tracker,
                               RecordingPersistence persistence,
                               base::File skp_file,
                               std::optional<size_t> max_capture_size,
                               mojom::PaintPreviewCaptureResponsePtr response,
                               CapturePaintPreviewCallback callback) {
  TRACE_EVENT0("paint_preview", "FinishRecordingOnUIThread");
  DCHECK(tracker);
  if (!tracker) {
    std::move(callback).Run(mojom::PaintPreviewStatus::kCaptureFailed,
                            std::move(response));
    return;
  }

  // This cannot be done async if the recording contains a GPU accelerated
  // image.
  TRACE_EVENT_BEGIN0("paint_preview", "ConvertToSkPicture");
  auto skp =
      PaintRecordToSkPicture(std::move(recording), tracker.get(), bounds);
  if (!skp) {
    std::move(callback).Run(mojom::PaintPreviewStatus::kCaptureFailed,
                            std::move(response));
    return;
  }
  TRACE_EVENT_BEGIN0("paint_preview", "ConvertToSkPicture");

  FinishedRecording out(mojom::PaintPreviewStatus::kOk, std::move(response));
  switch (persistence) {
    case RecordingPersistence::kFileSystem:
      SerializeFileRecording(skp, std::move(skp_file), std::move(tracker),
                             max_capture_size, std::move(out),
                             std::move(callback));
      break;
    case RecordingPersistence::kMemoryBuffer:
      SerializeMemoryBufferRecording(skp, std::move(tracker), max_capture_size,
                                     std::move(out), std::move(callback));
      break;
  }
}

}  // namespace

PaintPreviewRecorderImpl::PaintPreviewRecorderImpl(
    content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame),
      is_painting_preview_(false),
      is_main_frame_(render_frame->IsMainFrame() &&
                     !render_frame->IsInFencedFrameTree()) {
  render_frame->GetAssociatedInterfaceRegistry()
      ->AddInterface<mojom::PaintPreviewRecorder>(base::BindRepeating(
          &PaintPreviewRecorderImpl::BindPaintPreviewRecorder,
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
  const base::AutoReset<bool> resetter(&is_painting_preview_, true);
  CapturePaintPreviewInternal(params, std::move(response), std::move(callback));
}

void PaintPreviewRecorderImpl::OnDestruct() {
  paint_preview_recorder_receiver_.reset();
  base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE,
                                                                this);
}

void PaintPreviewRecorderImpl::BindPaintPreviewRecorder(
    mojo::PendingAssociatedReceiver<mojom::PaintPreviewRecorder> receiver) {
  // Capture requests can occur multiple times on the same frame. If the browser
  // has released its endpoint and creates a new one this needs to be reset.
  paint_preview_recorder_receiver_.reset();
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
  gfx::Rect bounds = params->clip_rect;

  auto offset = frame->GetScrollOffset();
  auto document_size = frame->DocumentSize();
  // If the special values of -1 are used for the initial position center about
  // the scroll offset.
  if (bounds.x() == -1) {
    bounds.set_x(
        GetBoundOrigin(document_size.width(), bounds.width(), offset.x()));
  }
  if (bounds.y() == -1) {
    bounds.set_y(
        GetBoundOrigin(document_size.height(), bounds.height(), offset.y()));
  }

  gfx::Rect document_rect = gfx::Rect(document_size);
  if (bounds.IsEmpty() || params->clip_rect_is_hint) {
    // If the clip rect is empty or only a hint try to use the document size.
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
  } else {
    // Overflow check and confirm that that the document has a size.
    if (document_rect.IsEmpty() || bounds.x() >= document_rect.width() ||
        bounds.y() >= document_rect.height() || bounds.x() < 0 ||
        bounds.y() < 0) {
      std::move(callback).Run(mojom::PaintPreviewStatus::kCaptureFailed,
                              std::move(response));
      return;
    }

    // If either width or height is 0 capture the full extent in that dimension
    // while still respecting the provided x and y.
    if (bounds.width() == 0) {
      bounds.set_width(document_rect.width());
    }
    if (bounds.height() == 0) {
      bounds.set_height(document_rect.height());
    }

    // Clamp width and height to be the document size.
    bounds.set_width(
        std::min(document_rect.width() - bounds.x(), bounds.width()));
    bounds.set_height(
        std::min(document_rect.height() - bounds.y(), bounds.height()));
  }

  auto tracker = std::make_unique<PaintPreviewTracker>(
      params->guid, frame->GetEmbeddingToken(), is_main_frame_);
  // Ensure `offset_x` and `offset_y` point to a value in the range
  // [0, bounds.dimension() - 1]. This means the offsets are valid for the
  // captured region.
  int offset_x = std::max(
      std::min(static_cast<int>(offset.x() - bounds.x()), bounds.width() - 1),
      0);
  int offset_y = std::max(
      std::min(static_cast<int>(offset.y() - bounds.y()), bounds.height() - 1),
      0);
  response->scroll_offsets = gfx::Point(offset_x, offset_y);
  response->frame_offsets = gfx::Point(bounds.x(), bounds.y());

  cc::PaintRecorder recorder;
  cc::PaintCanvas* canvas = recorder.beginRecording();
  canvas->save();
  canvas->translate(-bounds.x(), -bounds.y());
  canvas->SetPaintPreviewTracker(tracker.get());

  // Use time ticks manually rather than a histogram macro so as to;
  // 1. Account for main frames and subframes separately.
  // 2. Mitigate binary size as this won't be used that often.
  // 3. Record only on successes as failures are likely to be outliers (fast or
  //    slow).
  base::TimeTicks start_time = base::TimeTicks::Now();
  TRACE_EVENT_BEGIN0("paint_preview", "WebLocalFrame::CapturePaintPreview");
  bool success = frame->CapturePaintPreview(
      bounds, canvas, /*include_linked_destinations=*/params->capture_links,
      /*skip_accelerated_content=*/params->skip_accelerated_content);
  TRACE_EVENT_END0("paint_preview", "WebLocalFrame::CapturePaintPreview");
  canvas->restore();
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

  // Convert the special value |0| to |std::nullopt|.
  std::optional<size_t> max_capture_size;
  if (params->max_capture_size == 0) {
    max_capture_size = std::nullopt;
  } else {
    max_capture_size = params->max_capture_size;
    auto* image_ctx = tracker->GetImageSerializationContext();
    image_ctx->remaining_image_size = params->max_capture_size;
  }

  auto* image_ctx = tracker->GetImageSerializationContext();
  image_ctx->max_decoded_image_size_bytes =
      params->max_decoded_image_size_bytes;

  FinishRecordingOnUIThread(recorder.finishRecordingAsPicture(), bounds,
                            std::move(tracker), params->persistence,
                            std::move(params->file), max_capture_size,
                            std::move(response), std::move(callback));
}

}  // namespace paint_preview
