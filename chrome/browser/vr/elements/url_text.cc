// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/url_text.h"

#include "base/bind.h"
#include "cc/paint/skia_paint_canvas.h"
#include "chrome/browser/vr/elements/omnibox_formatting.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/render_text.h"

namespace vr {

namespace {

// Elision measurements are multiples of font height.
constexpr float kFadeWidthFactor = 1.5f;
constexpr float kMinimumPathWidthFactor = 1.5;

void ApplyUrlFading(SkCanvas* canvas,
                    const gfx::Rect& text_bounds,
                    float fade_width,
                    bool fade_left,
                    bool fade_right) {
  if (!fade_left && !fade_right)
    return;

  SkPoint fade_points[2] = {SkPoint::Make(0.0f, 0.0f),
                            SkPoint::Make(fade_width, 0.0f)};
  SkColor fade_colors[2] = {SK_ColorTRANSPARENT, SK_ColorBLACK};

  SkPaint overlay;
  overlay.setShader(SkGradientShader::MakeLinear(
      fade_points, fade_colors, nullptr, 2, SkTileMode::kClamp, 0, nullptr));
  if (fade_left) {
    canvas->save();
    canvas->translate(text_bounds.x(), 0);
    canvas->clipRect(SkRect::MakeWH(fade_width, text_bounds.height()));
    overlay.setBlendMode(SkBlendMode::kDstIn);
    canvas->drawPaint(overlay);
    canvas->restore();
  }

  if (fade_right) {
    canvas->save();
    canvas->translate(text_bounds.right() - fade_width, 0);
    canvas->clipRect(SkRect::MakeWH(fade_width, text_bounds.height()));
    overlay.setBlendMode(SkBlendMode::kDstOut);
    canvas->drawPaint(overlay);
    canvas->restore();
  }
}

}  // namespace

UrlText::UrlText(float font_height_dmm)
    : Text(font_height_dmm), font_height_dmm_(font_height_dmm) {
  SetLayoutMode(kSingleLineFixedWidth);

  SetOnRenderTextCreated(base::BindRepeating(&UrlText::OnRenderTextCreated,
                                             base::Unretained(this)));
  SetOnRenderTextRendered(base::BindRepeating(&UrlText::OnRenderTextRendered,
                                              base::Unretained(this)));
}

UrlText::~UrlText() = default;

void UrlText::SetUrl(const GURL& url) {
  gurl_ = url;
  UpdateText();
}

void UrlText::SetColor(const SkColor color) {
  NOTREACHED();
}

void UrlText::SetEmphasizedColor(const SkColor color) {
  emphasized_color_ = color;
  UpdateText();
}

void UrlText::SetDeemphasizedColor(const SkColor color) {
  deemphasized_color_ = color;
  UpdateText();
}

void UrlText::UpdateText() {
  const base::string16 text = FormatUrlForVr(gurl_, &url_parsed_);
  SetText(text);
  SetFormatting(CreateUrlFormatting(text, url_parsed_, emphasized_color_,
                                    deemphasized_color_));
}

void UrlText::OnRenderTextCreated(gfx::RenderText* render_text) {
  render_text->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  render_text->SetDirectionalityMode(gfx::DIRECTIONALITY_AS_URL);

  // Turn off elision, so that RenderText reports the true size of the URL.
  // This is needed to compute URL-specific offset and elision parameters.
  render_text->SetElideBehavior(gfx::NO_ELIDE);

  elision_parameters_ = GetElisionParameters(
      gurl_, url_parsed_, render_text,
      MetersToPixels(font_height_dmm_ * kMinimumPathWidthFactor));
  render_text->SetDisplayOffset(elision_parameters_.offset);
}

void UrlText::OnRenderTextRendered(const gfx::RenderText& render_text,
                                   SkCanvas* canvas) {
  float fade_width = MetersToPixels(font_height_dmm_ * kFadeWidthFactor);
  ApplyUrlFading(canvas, render_text.display_rect(), fade_width,
                 elision_parameters_.fade_left, elision_parameters_.fade_right);
}

}  // namespace vr
