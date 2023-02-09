// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_INK_DROP_UTIL_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_INK_DROP_UTIL_H_

#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"

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

void ConfigureInkDropForToolbar(views::Button* host);

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_INK_DROP_UTIL_H_
