// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_INK_DROP_UTIL_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_INK_DROP_UTIL_H_

#include <memory>

#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"

namespace views {
class InkDropHighlight;
class InkDropHostView;
class View;
struct InstallableInkDropConfig;
}  // namespace views

constexpr float kToolbarInkDropVisibleOpacity = 0.06f;
constexpr float kToolbarInkDropHighlightVisibleOpacity = 0.08f;
constexpr SkAlpha kToolbarButtonBackgroundAlpha = 32;

// Creates insets for a host view so that when insetting from the host view
// the resulting mask or inkdrop has the desired inkdrop size.
gfx::Insets GetToolbarInkDropInsets(const views::View* host_view);

// Creates the default inkdrop highlight but using the toolbar visible opacity.
std::unique_ptr<views::InkDropHighlight> CreateToolbarInkDropHighlight(
    const views::InkDropHostView* host_view);

// Returns the ink drop base color that should be used by all toolbar buttons.
SkColor GetToolbarInkDropBaseColor(const views::View* host_view);

views::InstallableInkDropConfig GetToolbarInstallableInkDropConfig(
    const views::View* host_view);

// Installs a highlight path generator that matches the toolbar button style.
void InstallToolbarButtonHighlightPathGenerator(views::View* host);

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_INK_DROP_UTIL_H_
