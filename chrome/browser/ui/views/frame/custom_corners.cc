// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/custom_corners.h"

#include "base/check_op.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/themed_background.h"
#include "ui/gfx/scoped_canvas.h"

CustomCorners::CustomCorners(BrowserView& browser_view)
    : browser_view_(browser_view) {
  // Browser may not yet have a widget, but we need to track the widget in
  // case we need active/inactive frame color.
  if (browser_view_->GetWidget()) {
    // This hooks up the active/inactive state listener.
    OnViewAddedToWidget(&browser_view);
  } else {
    // This will hook up the listener when the browser view is added to a
    // widget.
    browser_view_observation_.Observe(&browser_view);
  }
}

CustomCorners::~CustomCorners() = default;

void CustomCorners::OnViewAddedToWidget(views::View* view) {
  CHECK_EQ(view, &*browser_view_);
  browser_view_observation_.Reset();
  browser_paint_as_active_subscription_ =
      view->GetWidget()->RegisterPaintAsActiveChangedCallback(
          base::BindRepeating(&CustomCorners::OnBrowserPaintAsActiveChanged,
                              base::Unretained(this)));
}

void CustomCorners::SetFadeBackground(
    std::optional<FadeBackground> fade_background) {
  if (fade_background_ == fade_background) {
    return;
  }

  fade_background_ = std::move(fade_background);
  SchedulePaintHost();
}

void CustomCorners::PaintPath(gfx::Canvas* canvas,
                              const SkPath& path,
                              ColorChoice color_choice,
                              bool anti_alias) const {
  auto paint_color = [&](ColorChoice choice, float alpha) {
    if (std::holds_alternative<ToolbarTheme>(choice) ||
        std::holds_alternative<FrameTheme>(choice)) {
      gfx::ScopedCanvas scoped(canvas);
      canvas->ClipPath(path, anti_alias);
      // If this theme color should have any transparency, we paint it to a
      // layer so we can adjust the layer's transparency.
      bool has_transparency = alpha < 1.0f;
      if (has_transparency) {
        cc::PaintFlags layer_flags;
        layer_flags.setAlphaf(alpha);
        canvas->SaveLayerWithFlags(layer_flags);
      }
      ThemedBackground::PaintBackground(
          canvas, &GetView(), &browser_view(),
          std::holds_alternative<ToolbarTheme>(choice)
              ? ThemedBackground::ThemeChoice::kToolbarTheme
              : ThemedBackground::ThemeChoice::kFrameTheme);
    } else {
      ui::ColorId color_id = std::get<ui::ColorId>(choice);

      cc::PaintFlags flags;
      flags.setAntiAlias(anti_alias);
      flags.setStyle(cc::PaintFlags::kFill_Style);
      flags.setColor(
          SkColorSetA(GetView().GetColorProvider()->GetColor(color_id),
                      std::clamp(static_cast<int>(255 * alpha), 0, 255)));
      canvas->DrawPath(path, flags);
    }
  };

  // A fade background may be drawn with some transparency over the original
  // background. If the fade background is fully transparent, we only draw the
  // original background. If the fade background is fully opaque, we only draw
  // the fade background. If the fade background is partially transparent, we
  // draw the original background at full opacity with the partially transparent
  // fade background on top.
  if (fade_background_.has_value()) {
    if (fade_background_->opacity <= 0.0f) {
      paint_color(color_choice, 1.0f);
    } else if (fade_background_->opacity < 1.0f) {
      paint_color(color_choice, 1.0f);
      paint_color(fade_background_->color, fade_background_->opacity);
    } else {
      paint_color(fade_background_->color, 1.0f);
    }
    return;
  }

  paint_color(color_choice, 1.0f);
}
