// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/custom_corners.h"

#include "base/check_op.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_background.h"
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

void CustomCorners::PaintPath(gfx::Canvas* canvas,
                              const SkPath& path,
                              ColorChoice color_choice,
                              bool anti_alias) const {
  if (std::holds_alternative<TopContainerTheme>(color_choice)) {
    gfx::ScopedCanvas scoped(canvas);
    canvas->ClipPath(path, anti_alias);
    TopContainerBackground::PaintBackground(canvas, &GetView(),
                                            &browser_view());
    return;
  }

  ui::ColorVariant color;
  if (std::holds_alternative<FrameColor>(color_choice)) {
    color = browser_view().GetWidget()->ShouldPaintAsActive()
                ? ui::kColorFrameActive
                : ui::kColorFrameInactive;
  } else {
    color = std::get<ui::ColorId>(color_choice);
  }

  cc::PaintFlags flags;
  flags.setAntiAlias(anti_alias);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(color.ResolveToSkColor(GetView().GetColorProvider()));
  canvas->DrawPath(path, flags);
}
