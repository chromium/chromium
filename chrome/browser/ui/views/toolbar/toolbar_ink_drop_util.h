// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_INK_DROP_UTIL_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_INK_DROP_UTIL_H_

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace views {
class Button;
class View;
}  // namespace views

constexpr float kToolbarInkDropVisibleOpacity = 0.06f;

// Creates insets for a host view so that when insetting from the host view
// the resulting mask or inkdrop has the desired inkdrop size.
gfx::Insets GetToolbarInkDropInsets(const views::View* host_view);

// Returns the ink drop base color that should be used by all toolbar buttons.
// This is only needed if you can't use ConfigureInkDropForToolbar().
SkColor GetToolbarInkDropBaseColor(const views::View* host_view);

// Installs `highlight_generator` and configures the inkdrop for `host` in
// toolbar. Creates a highlight generator when it's not provided.
void ConfigureInkDropForToolbar(views::Button* host,
                                std::unique_ptr<views::HighlightPathGenerator>
                                    highlight_generator = nullptr);

// Sets the highlight color callback and ripple color callback for inkdrop when
// the chrome refresh flag is on.
void ConfigureToolbarInkdropForRefresh2023(views::View* host,
                                           ChromeColorIds hover_color_id,
                                           ChromeColorIds ripple_color_id);

// Sets the highlight color callback and ripple color callback for the inkdrop
// of the host.
void CreateToolbarInkdropCallbacks(views::View* host,
                                   ChromeColorIds hover_color_id,
                                   ChromeColorIds ripple_color_id);

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_INK_DROP_UTIL_H_
