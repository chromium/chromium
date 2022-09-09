// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CHROME_WIDGET_SUBLEVEL_H_
#define CHROME_BROWSER_UI_VIEWS_CHROME_WIDGET_SUBLEVEL_H_

// Semantic z-order sublevels for secondary UI views::Widget.
// Use these values for InitParams::sublevel or SetZOrderSublevel().
enum ChromeWidgetSublevel {
  // Default value.
  kSublevelNormal = 0,
  // Transient hoverables, e.g. tab preview popup.
  // This intentionally uses the same value as `kSublevelSecurity`,
  // so that whichever comes later will be at the top.
  kSublevelHoverable = 1,
  // Security bubble, e.g. permission prompt bubble.
  kSublevelSecurity = 1,
};

#endif  // CHROME_BROWSER_UI_VIEWS_CHROME_WIDGET_SUBLEVEL_H_
