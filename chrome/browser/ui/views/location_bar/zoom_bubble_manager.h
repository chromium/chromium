// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ZOOM_BUBBLE_MANAGER_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ZOOM_BUBBLE_MANAGER_H_

#include <string>

#include "ui/gfx/native_ui_types.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

// An interface implemented by a class that provides browser-window specific
// support required by zoom bubble components.
class ZoomBubbleManager {
 public:
  virtual ~ZoomBubbleManager() = default;

  // Returns the bubble anchor for the zoom bubble. Returns nullptr if no anchor
  // is appropriate (e.g. in fullscreen).
  virtual views::BubbleAnchor GetZoomBubbleAnchor() = 0;

  // Returns the native view for parent window assignment.
  virtual gfx::NativeView GetNativeView() = 0;

  // Updates the legacy (pre-migration) page action icon for zoom.
  virtual void UpdateLegacyPageActionIcon() = 0;

  // Returns the accessible name for the zoom action in the toolbar, used as
  // the accessible window title for the zoom bubble.
  virtual std::u16string GetZoomActionAccessibleName() = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ZOOM_BUBBLE_MANAGER_H_
