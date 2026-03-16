// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/frame/default_highlight_border_overlay_delegate.h"

#include "chromeos/ui/base/window_properties.h"
#include "ui/aura/window.h"

namespace chromeos {

DefaultHighlightBorderOverlayDelegate::DefaultHighlightBorderOverlayDelegate() =
    default;
DefaultHighlightBorderOverlayDelegate::
    ~DefaultHighlightBorderOverlayDelegate() = default;

bool DefaultHighlightBorderOverlayDelegate::ShouldRoundHighlightBorderForWindow(
    const aura::Window* window) {
  return window->GetProperty(kWindowHasRoundedCornersKey);
}

}  // namespace chromeos
