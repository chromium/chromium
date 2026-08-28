// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_view_ash.h"

#include <algorithm>

#include "base/check.h"
#include "chrome/browser/glic/browser_ui/context_sharing_border_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/multi_contents_view.h"
#include "chrome/browser/ui/views/frame/scrim_view.h"
#include "components/search/ntp_features.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/controls/webview/webview.h"

BrowserViewAsh::BrowserViewAsh(BrowserWindowInterface* browser)
    : BrowserView(browser) {}

void BrowserViewAsh::Layout(PassKey) {
  LayoutSuperclass<BrowserView>(this);

  // In ChromeOS ash we round the bottom two corners of the browser frame.
  DCHECK(GetWidget());
  GetWidget()->non_client_view()->frame_view()->UpdateWindowRoundedCorners();
}

// TODO(crbug.com/529379168) to investigate handling window rounded corners in
// browser view layout instead since it allows us to support features
// like glass frame and rounded windows in a unified way. This currently
// works because these features don't overlap this.
void BrowserViewAsh::UpdateWindowRoundedCorners(
    const gfx::RoundedCornersF& window_radii) {
  const bool vertical_tabstrip = ShouldDrawVerticalTabStrip();

  window_scrim_view()->SetRoundedCorners(window_radii);

  // With window controls overlay enabled, the web content extends over the
  // entire window height, overlapping the window's top-two rounded corners.
  // Consequently, we need to make the top two corners of the web_view
  // rounded as well.
  const bool round_content_webview_top_corner =
      IsWindowControlsOverlayEnabled();

  const gfx::RoundedCornersF multi_contents_radii(
      round_content_webview_top_corner ? window_radii.upper_left() : 0,
      round_content_webview_top_corner ? window_radii.upper_right() : 0,
      window_radii.lower_right(),
      vertical_tabstrip ? 0 : window_radii.lower_left());

  multi_contents_view()->SetBackgroundRadii(multi_contents_radii);
}
