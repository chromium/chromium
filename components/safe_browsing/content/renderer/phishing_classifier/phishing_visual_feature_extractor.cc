// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/renderer/phishing_classifier/phishing_visual_feature_extractor.h"

#include <memory>

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "components/crash/core/common/crash_key.h"
#include "components/paint_preview/common/paint_preview_tracker.h"
#include "skia/ext/legacy_display_globals.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace {

std::unique_ptr<SkBitmap> PlaybackOnBackgroundThread(
    cc::PaintRecord paint_record,
    gfx::Rect bounds) {
  base::ElapsedTimer timer;
  std::optional<base::ElapsedThreadTimer> thread_timer;
  if (base::ThreadTicks::IsSupported()) {
    thread_timer.emplace();
  }

  std::unique_ptr<SkBitmap> bitmap = std::make_unique<SkBitmap>();
  // Use the Rec. 2020 color space, in case the user input is wide-gamut.
  sk_sp<SkColorSpace> rec2020 = SkColorSpace::MakeRGB(
      {2.22222f, 0.909672f, 0.0903276f, 0.222222f, 0.0812429f, 0, 0},
      SkNamedGamut::kRec2020);
  SkImageInfo bitmap_info = SkImageInfo::Make(
      bounds.width(), bounds.height(), SkColorType::kN32_SkColorType,
      SkAlphaType::kUnpremul_SkAlphaType, rec2020);
  if (!bitmap->tryAllocPixels(bitmap_info)) {
    return nullptr;
  }

  SkCanvas sk_canvas(*bitmap, skia::LegacyDisplayGlobals::GetSkSurfaceProps());
  paint_record.Playback(&sk_canvas);

  base::UmaHistogramCustomMicrosecondsTimes(
      "SBClientPhishing.VisualFeatureTime.BackgroundPlayback.Duration",
      timer.Elapsed(), base::Microseconds(1), base::Seconds(2), 100);
  if (thread_timer) {
    base::UmaHistogramCustomMicrosecondsTimes(
        "SBClientPhishing.VisualFeatureTime.BackgroundPlayback.ThreadDuration",
        thread_timer->Elapsed(), base::Microseconds(1), base::Seconds(2), 100);
  }
  return bitmap;
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(FrameCaptureState)
enum class FrameCaptureState {
  kReady = 0,
  kMissingView = 1,
  kMissingWidget = 2,
  kProvisionalWidget = 3,
  kMaxValue = kProvisionalWidget,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/sb_client/enums.xml:SBClientPhishingVisualExtractionFrameState)

const char* FrameCaptureStateToString(FrameCaptureState state) {
  switch (state) {
    case FrameCaptureState::kReady:
      return "Ready";
    case FrameCaptureState::kMissingView:
      return "MissingView";
    case FrameCaptureState::kMissingWidget:
      return "MissingWidget";
    case FrameCaptureState::kProvisionalWidget:
      return "ProvisionalWidget";
  }
  NOTREACHED();
}

void RecordFrameCaptureState(FrameCaptureState state) {
  base::UmaHistogramEnumeration("SBClientPhishing.VisualExtractionFrameState",
                                state);
  static crash_reporter::CrashKeyString<32> capture_state_key(
      "visual-extraction-frame-state");
  capture_state_key.Set(FrameCaptureStateToString(state));
}

}  // namespace

namespace safe_browsing {

PhishingVisualFeatureExtractor::PhishingVisualFeatureExtractor() = default;

PhishingVisualFeatureExtractor::~PhishingVisualFeatureExtractor() = default;

void PhishingVisualFeatureExtractor::ExtractFeatures(
    blink::WebLocalFrame* frame,
    DoneCallback done_callback) {
  done_callback_ = std::move(done_callback);
  timer_.emplace();

  // Evaluate and log the frame state before attempting to use it.
  FrameCaptureState capture_state = FrameCaptureState::kReady;
  if (!frame->View()) {
    capture_state = FrameCaptureState::kMissingView;
  } else if (!frame->FrameWidget()) {
    capture_state = FrameCaptureState::kMissingWidget;
  } else if (frame->IsProvisional()) {
    capture_state = FrameCaptureState::kProvisionalWidget;
  }
  RecordFrameCaptureState(capture_state);

  // TODO(crbug.com/551990098): We are currently gathering telemetry to prove
  // this state correlates with the AXObjectCacheImpl::MayHaveHTMLLabel crash.

  gfx::SizeF viewport_size = frame->View()->VisualViewportSize();
  gfx::Rect bounds = ToEnclosingRect(gfx::RectF(viewport_size));

  auto tracker = std::make_unique<paint_preview::PaintPreviewTracker>(
      base::UnguessableToken::Create(), frame->GetEmbeddingToken(),
      /*is_main_frame=*/true);
  cc::PaintRecorder recorder;
  cc::PaintCanvas* canvas = recorder.beginRecording();
  canvas->SetPaintPreviewTracker(tracker.get());

  if (!frame->CapturePaintPreview(bounds, canvas,
                                  /*include_linked_destinations=*/false,
                                  /*skip_accelerated_content=*/true,
                                  /*allow_scrollbars=*/false)) {
    RunCallback(nullptr);
    return;
  }

  cc::PaintRecord paint_record = recorder.finishRecordingAsPicture();

  // This logs only the main thread time.
  base::UmaHistogramTimes("SBClientPhishing.VisualFeatureTime",
                          timer_->Elapsed());

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::WithBaseSyncPrimitives()},
      base::BindOnce(&PlaybackOnBackgroundThread, std::move(paint_record),
                     bounds),
      base::BindOnce(&PhishingVisualFeatureExtractor::RunCallback,
                     weak_factory_.GetWeakPtr()));
}

void PhishingVisualFeatureExtractor::RunCallback(
    std::unique_ptr<SkBitmap> bitmap) {
  CHECK(timer_);
  base::UmaHistogramTimes("SBClientPhishing.VisualFeatureTime.TotalDuration",
                          timer_->Elapsed());
  timer_.reset();

  DCHECK(!done_callback_.is_null());
  std::move(done_callback_).Run(std::move(bitmap));
}

}  // namespace safe_browsing
