// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BUBBLE_ANCHOR_UTIL_H_
#define CHROME_BROWSER_UI_BUBBLE_ANCHOR_UTIL_H_

#include "build/build_config.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {
class Rect;
}

class Browser;

namespace bubble_anchor_util {

// Indicates which browser component to use as an anchor.
// Used as a parameter of GetPageInfoAnchorView().
enum Anchor {
  kLocationBar,
  kAppMenuButton,
  kCustomTabBar,
};

// Offset from the window edge to show bubbles when there is no location bar.
// E.g., when in fullscreen or in a Hosted App window. Don't center, since that
// could obscure a fullscreen bubble.
constexpr int kNoToolbarLeftOffset = 40;

// Returns the Rect appropriate for anchoring a bubble to |browser|'s Page Info
// icon, or an appropriate fallback when that is not visible. This is used only
// when the platform-specific GetPageInfoAnchorView() is unable to return an
// actual View. This function has separate implementations for Views- and Cocoa-
// based browsers. The anchor rect is in screen coordinates.
gfx::Rect GetPageInfoAnchorRect(Browser* browser);

}  // namespace bubble_anchor_util

#endif  // CHROME_BROWSER_UI_BUBBLE_ANCHOR_UTIL_H_
