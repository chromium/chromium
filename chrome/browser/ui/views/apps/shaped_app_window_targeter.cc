// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/shaped_app_window_targeter.h"

#include <memory>

#include "chrome/browser/ui/views/apps/chrome_native_app_window_views.h"

ShapedAppWindowTargeter::ShapedAppWindowTargeter(
    ChromeNativeAppWindowViews* app_window)
    : app_window_(app_window) {}

ShapedAppWindowTargeter::~ShapedAppWindowTargeter() {}

std::unique_ptr<aura::WindowTargeter::HitTestRects>
ShapedAppWindowTargeter::GetExtraHitTestShapeRects(aura::Window* target) const {
  if (!app_window_->shape_rects())
    return nullptr;

  auto shape_rects = std::make_unique<aura::WindowTargeter::HitTestRects>(
      *app_window_->shape_rects());
  return shape_rects;
}
