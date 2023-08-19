// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/renderer/phishing_classifier/phishing_visual_feature_extractor.h"

#include <memory>

#include "base/metrics/histogram_functions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
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

  return bitmap;
}

}  // namespace

namespace safe_browsing {

PhishingVisualFeatureExtractor::PhishingVisualFeatureExtractor() = default;

PhishingVisualFeatureExtractor::~PhishingVisualFeatureExtractor() = default;

void PhishingVisualFeatureExtractor::ExtractFeatures(
    blink::WebLocalFrame* frame,
    DoneCallback done_callback) {
  done_callback_ = std::move(done_callback);
  base::TimeTicks start_time = base::TimeTicks::Now();
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
                                  /*skip_accelerated_content=*/true)) {
    RunCallback(nullptr);
    return;
  }

  cc::PaintRecord paint_record = recorder.finishRecordingAsPicture();

  base::UmaHistogramTimes("SBClientPhishing.VisualFeatureTime",
                          base::TimeTicks::Now() - start_time);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::WithBaseSyncPrimitives()},
      base::BindOnce(&PlaybackOnBackgroundThread, std::move(paint_record),
                     bounds),
      base::BindOnce(&PhishingVisualFeatureExtractor::RunCallback,
                     weak_factory_.GetWeakPtr()));
}

void PhishingVisualFeatureExtractor::RunCallback(
    std::unique_ptr<SkBitmap> bitmap) {
  DCHECK(!done_callback_.is_null());
  std::move(done_callback_).Run(std::move(bitmap));
}

}  // namespace safe_browsing
