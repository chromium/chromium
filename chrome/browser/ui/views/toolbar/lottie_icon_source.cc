// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/lottie_icon_source.h"

#include "cc/paint/paint_flags.h"
#include "ui/gfx/canvas.h"

LottieIconSource::LottieIconSource(lottie::Animation* animation,
                                   float progress,
                                   int size,
                                   SkColor color)
    : gfx::CanvasImageSource(gfx::Size(size, size)),
      animation_(animation),
      progress_(progress),
      color_(color) {}

LottieIconSource::~LottieIconSource() = default;

void LottieIconSource::Draw(gfx::Canvas* canvas) {
  cc::PaintFlags flags;
  flags.setColorFilter(cc::ColorFilter::MakeBlend(SkColor4f::FromColor(color_),
                                                  SkBlendMode::kSrcIn));
  canvas->SaveLayerWithFlags(flags);
  animation_->PaintFrame(canvas, progress_, size());
  canvas->Restore();
}
