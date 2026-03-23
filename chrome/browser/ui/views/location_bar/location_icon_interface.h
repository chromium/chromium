// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_ICON_INTERFACE_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_ICON_INTERFACE_H_

#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

// An abstract interface for manipulating the location bar's icon (typically
// the security status or "tune" icon). This is implemented by both the
// native Views-based LocationIconView and the upcoming WebUI equivalent.
class LocationIconInterface {
 public:
  virtual ~LocationIconInterface() = default;

  // Determines whether or not text should be shown (e.g., Insecure/Secure).
  virtual bool GetShowText() const = 0;

  // Updates the icon's ink drop mode, focusable behavior, text and security
  // status. `suppress_animations` indicates whether this update should suppress
  // the text change animation (e.g. when swapping tabs).
  // `force_hide_background` hides the background color. This is useful in
  // situations like where the popup is shown.
  virtual void Update(bool suppress_animations, bool force_hide_background) = 0;

  // Returns true if the icon's security state has changed since the last call
  // to Update().
  virtual bool HasSecurityStateChanged() const = 0;

  // Sets whether the location icon view is currently visible.
  virtual void SetVisible(bool visible) = 0;

  // Returns the anchor for bubbles.
  virtual views::BubbleAnchor GetAnchor() = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_ICON_INTERFACE_H_
