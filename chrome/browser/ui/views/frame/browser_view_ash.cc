// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_view_ash.h"

#include <algorithm>

#include "base/check.h"
#include "chrome/browser/ui/sad_tab_helper.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/sad_tab_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/controls/webview/webview.h"

BrowserViewAsh::BrowserViewAsh(std::unique_ptr<Browser> browser)
    : BrowserView(std::move(browser)) {}

void BrowserViewAsh::Layout(PassKey) {
  LayoutSuperclass<BrowserView>(this);

  // In ChromeOS ash we round the bottom two corners of the browser frame by
  // rounding the respective corners of visible client contents i.e main web
  // contents, devtools web contents and side panel. When ever there is change
  // in the layout or visibility of these contents (devtools opened, devtools
  // docked placement change, side panel open etc), we might need to update
  // which corners are currently rounded. See
  // `BrowserNonClientFrameViewChromeOS::UpdateWindowRoundedCorners()` for more
  // details.
  DCHECK(GetWidget());
  GetWidget()->non_client_view()->frame_view()->UpdateWindowRoundedCorners();
}

void BrowserViewAsh::UpdateWindowRoundedCorners(int corner_radius) {
  SidePanel* side_panel = unified_side_panel();
  const bool right_aligned_side_panel_showing =
      side_panel->GetVisible() && side_panel->IsRightAligned();
  const bool left_aligned_side_panel_showing =
      side_panel->GetVisible() && !side_panel->IsRightAligned();

  // If side panel is visible, round one of the bottom two corners of the side
  // panel based on its alignment w.r.t to web contents.
  const gfx::RoundedCornersF side_panel_radii(
      0, 0, right_aligned_side_panel_showing ? corner_radius : 0,
      left_aligned_side_panel_showing ? corner_radius : 0);

  if (side_panel_radii != side_panel->background_radii()) {
    side_panel->SetBackgroundRadii(side_panel_radii);
  }

  views::WebView* devtools_webview = devtools_web_view();
  CHECK(devtools_webview);
  CHECK(devtools_webview->holder());

  // If devtools are visible, round one of the bottom two corners of the
  // the devtools context based on the alignment of the side panel. Since
  // devtools cover the full bounds of the web contents container, if the side
  // panel is not visible, we have to round the bottom two corners of side panel
  // irrespective of its docked placement.
  const gfx::RoundedCornersF devtools_webview_radii(
      0, 0, right_aligned_side_panel_showing ? 0 : corner_radius,
      left_aligned_side_panel_showing ? 0 : corner_radius);

  if (devtools_webview_radii_ != devtools_webview_radii) {
    devtools_webview_radii_ = devtools_webview_radii;
    devtools_webview->holder()->SetCornerRadii(devtools_webview_radii_);
  }

  const DevToolsDockedPlacement devtools_placement =
      devtools_docked_placement();
  CHECK_NE(devtools_placement, DevToolsDockedPlacement::kUnknown);

  // Rounded the contents webview.
  ContentsWebView* contents_webview = contents_web_view();
  const views::View* container = contents_container();

  const bool devtools_showing =
      contents_webview->bounds() != container->GetLocalBounds();

  // With window controls overlay enabled, the web content extends over the
  // entire window height, overlapping the window's top-two rounded corners.
  // Consequently, we need to make the top two corners of the web_view
  // rounded as well.
  const bool round_content_webview_top_corner =
      IsWindowControlsOverlayEnabled();

  const gfx::RoundedCornersF contents_webview_radii(
      round_content_webview_top_corner ? corner_radius : 0,
      round_content_webview_top_corner ? corner_radius : 0,
      right_aligned_side_panel_showing ||
              (devtools_showing &&
               devtools_placement != DevToolsDockedPlacement::kLeft)
          ? 0
          : corner_radius,
      left_aligned_side_panel_showing ||
              (devtools_showing &&
               devtools_placement != DevToolsDockedPlacement::kRight)
          ? 0
          : corner_radius);

  CHECK(contents_webview);
  CHECK(contents_webview->holder());

  if (contents_webview->web_contents()) {
    // SideTabView is shown when the renderer crashes. Initially the SabTabView
    // gets the same corners as the contents webview it gets attached to but its
    // radii needs to be updated as it is unaware of the client view layout
    // changes.
    if (auto* sad_tab_helper =
            SadTabHelper::FromWebContents(contents_webview->web_contents());
        sad_tab_helper && sad_tab_helper->sad_tab()) {
      SadTabView* sad_tab_view =
          static_cast<SadTabView*>(sad_tab_helper->sad_tab());
      if (sad_tab_view->GetBackgroundRadii() != contents_webview_radii) {
        sad_tab_view->SetBackgroundRadii(contents_webview_radii);
      }
    } else {
      // We only round contents_webview, if SadTabView is not showing.
      if (contents_webview_radii_ != contents_webview_radii) {
        contents_webview_radii_ = contents_webview_radii;
        contents_webview->holder()->SetCornerRadii(contents_webview_radii);
      }
    }
  }

  if (contents_webview->background_radii() != contents_webview_radii) {
    contents_webview->SetBackgroundRadii(contents_webview_radii);
  }
}
