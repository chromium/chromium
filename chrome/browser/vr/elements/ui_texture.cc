// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/ui_texture.h"

#include "base/trace_event/trace_event.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/canvas.h"

namespace vr {

UiTexture::UiTexture() = default;

UiTexture::~UiTexture() = default;

void UiTexture::DrawTexture(SkCanvas* canvas, const gfx::Size& texture_size) {
  TRACE_EVENT0("gpu", "UiTexture::DrawTexture");
  canvas->drawColor(SK_ColorTRANSPARENT);
  Draw(canvas, texture_size);
  dirty_ = false;
}

void UiTexture::DrawEmptyTexture() {
  dirty_ = false;
}

bool UiTexture::LocalHitTest(const gfx::PointF& point) const {
  return false;
}

void UiTexture::OnInitialized() {
  set_dirty();
}

SkColor UiTexture::foreground_color() const {
  DCHECK(foreground_color_);
  return foreground_color_.value();
}

SkColor UiTexture::background_color() const {
  DCHECK(background_color_);
  return background_color_.value();
}

void UiTexture::SetForegroundColor(SkColor color) {
  if (foreground_color_ == color)
    return;
  foreground_color_ = color;
  set_dirty();
}

void UiTexture::SetBackgroundColor(SkColor color) {
  if (background_color_ == color)
    return;
  background_color_ = color;
  set_dirty();
}

}  // namespace vr
