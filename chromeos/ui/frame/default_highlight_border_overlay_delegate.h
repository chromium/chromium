// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_FRAME_DEFAULT_HIGHLIGHT_BORDER_OVERLAY_DELEGATE_H_
#define CHROMEOS_UI_FRAME_DEFAULT_HIGHLIGHT_BORDER_OVERLAY_DELEGATE_H_

#include "base/component_export.h"
#include "chromeos/ui/frame/highlight_border_overlay_delegate.h"

namespace chromeos {

class COMPONENT_EXPORT(CHROMEOS_UI_FRAME) DefaultHighlightBorderOverlayDelegate
    : public HighlightBorderOverlayDelegate {
 public:
  DefaultHighlightBorderOverlayDelegate();

  DefaultHighlightBorderOverlayDelegate(
      const DefaultHighlightBorderOverlayDelegate&) = delete;
  DefaultHighlightBorderOverlayDelegate& operator=(
      const DefaultHighlightBorderOverlayDelegate&) = delete;

  ~DefaultHighlightBorderOverlayDelegate() override;

  // HighlightBorderOverlayDelegate:
  bool ShouldRoundHighlightBorderForWindow(const aura::Window* window) override;
};

}  // namespace chromeos

#endif  // CHROMEOS_UI_FRAME_DEFAULT_HIGHLIGHT_BORDER_OVERLAY_DELEGATE_H_
