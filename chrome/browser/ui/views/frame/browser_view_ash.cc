// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_view_ash.h"

#include <algorithm>

#include "base/check.h"
#include "chrome/browser/ui/sad_tab_helper.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/contents_container_view.h"
#include "chrome/browser/ui/views/frame/multi_contents_view.h"
#include "chrome/browser/ui/views/frame/scrim_view.h"
#include "chrome/browser/ui/views/new_tab_footer/footer_web_view.h"
#include "chrome/browser/ui/views/sad_tab_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "components/search/ntp_features.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/controls/webview/webview.h"

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/browser_ui/context_sharing_border_view.h"
#endif  // BUILDFLAG(ENABLE_GLIC)

namespace {

void SetRoundedCornersOnHost(views::NativeViewHost* host,
                             const gfx::RoundedCornersF& radii) {
  auto* layer = host->GetUILayer();
  if (layer && layer->rounded_corner_radii() != radii) {
    host->SetCornerRadii(radii);
  }
}

}  // namespace

BrowserViewAsh::BrowserViewAsh(Browser* browser) : BrowserView(browser) {}

void BrowserViewAsh::Layout(PassKey) {
  LayoutSuperclass<BrowserView>(this);

  // In ChromeOS ash we round the bottom two corners of the browser frame by
  // rounding the respective corners of visible client contents i.e main web
  // contents, devtools web contents and side panel. When ever there is change
  // in the layout or visibility of these contents (devtools opened, devtools
  // docked placement change, side panel open etc), we might need to update
  // which corners are currently rounded. See
  // `BrowserFrameViewChromeOS::UpdateWindowRoundedCorners()` for more
  // details.
  DCHECK(GetWidget());
  GetWidget()->non_client_view()->frame_view()->UpdateWindowRoundedCorners();
}

void BrowserViewAsh::UpdateWindowRoundedCorners(
    const gfx::RoundedCornersF& window_radii) {
  SidePanel* side_panel = contents_height_side_panel();
  const bool right_aligned_side_panel_showing =
      side_panel->GetVisible() && side_panel->IsRightAligned();
  const bool left_aligned_side_panel_showing =
      side_panel->GetVisible() && !side_panel->IsRightAligned();

  // If side panel is visible, round one of the bottom two corners of the side
  // panel based on its alignment w.r.t to web contents.
  const gfx::RoundedCornersF side_panel_radii(
      0, 0, right_aligned_side_panel_showing ? window_radii.lower_right() : 0,
      left_aligned_side_panel_showing ? window_radii.lower_left() : 0);

  if (side_panel_radii != side_panel->background_radii()) {
    side_panel->SetBackgroundRadii(side_panel_radii);
  }

  window_scrim_view()->SetRoundedCorners(window_radii);

  if (base::FeatureList::IsEnabled(features::kSideBySide)) {
    const gfx::RoundedCornersF multi_contents_radii(
        0, 0, right_aligned_side_panel_showing ? 0 : window_radii.lower_right(),
        left_aligned_side_panel_showing ? 0 : window_radii.lower_left());

    if (multi_contents_view()->background_radii() != multi_contents_radii) {
      multi_contents_view()->SetBackgroundRadii(multi_contents_radii);
    }

    if (IsInSplitView()) {
      // In a non-split view, browser's content (main web content, DevTools, NTP
      // footer, etc.) extends into the rounded corners. However, in split view,
      // the content is bordered, making it sufficient to simply round the
      // background painted by multi_contents_view().
      return;
    }
  }

  views::WebView *devtools_webview =
      GetActiveContentsContainerView()->devtools_web_view();
  CHECK(devtools_webview);
  CHECK(devtools_webview->holder());

  // If devtools are visible, round one of the bottom two corners of the
  // the devtools context based on the alignment of the side panel. Since
  // devtools cover the full bounds of the web contents container, if the side
  // panel is not visible, we have to round the bottom two corners of side panel
  // irrespective of its docked placement.
  const gfx::RoundedCornersF devtools_webview_radii(
      0, 0, right_aligned_side_panel_showing ? 0 : window_radii.lower_right(),
      left_aligned_side_panel_showing ? 0 : window_radii.lower_left());

  SetRoundedCornersOnHost(devtools_webview->holder(), devtools_webview_radii);
  GetActiveContentsContainerView()->devtools_scrim_view()->SetRoundedCorners(
      devtools_webview_radii);

  const ContentsContainerView::DevToolsDockedPlacement devtools_placement =
      GetActiveContentsContainerView()->devtools_docked_placement();
  CHECK_NE(devtools_placement,
           ContentsContainerView::DevToolsDockedPlacement::kUnknown);

  // Rounded the contents webview.
  std::vector<ContentsWebView*> contents_views =
      GetAllVisibleContentsWebViews();
  ContentsWebView* contents_webview = contents_views.front();

  const gfx::Rect contents_bounds =
      GetActiveContentsContainerView()->GetContentsViewBounds();
  const gfx::Rect container_bounds =
      GetActiveContentsContainerView()->GetLocalBounds();
  const bool devtools_showing = contents_bounds != container_bounds;

  // With window controls overlay enabled, the web content extends over the
  // entire window height, overlapping the window's top-two rounded corners.
  // Consequently, we need to make the top two corners of the web_view
  // rounded as well.
  const bool round_content_webview_top_corner =
      IsWindowControlsOverlayEnabled();

  auto* ntp_footer = GetActiveContentsContainerView()->new_tab_footer_view();
  bool is_ntp_footer_showing = false;
  if (base::FeatureList::IsEnabled(ntp_features::kNtpFooter)) {
    is_ntp_footer_showing = ntp_footer->GetVisible();
  }

  if (is_ntp_footer_showing) {
    const gfx::RoundedCornersF ntp_footer_radii(
        0, 0,
        right_aligned_side_panel_showing ||
                (devtools_showing &&
                 devtools_placement !=
                     ContentsContainerView::DevToolsDockedPlacement::kLeft)
            ? 0
            : window_radii.lower_right(),
        left_aligned_side_panel_showing ||
                (devtools_showing &&
                 devtools_placement !=
                     ContentsContainerView::DevToolsDockedPlacement::kRight)
            ? 0
            : window_radii.lower_left());
    SetRoundedCornersOnHost(ntp_footer->holder(), ntp_footer_radii);
  }

  const gfx::RoundedCornersF contents_webview_radii(
      round_content_webview_top_corner ? window_radii.upper_left() : 0,
      round_content_webview_top_corner ? window_radii.upper_right() : 0,
      is_ntp_footer_showing || right_aligned_side_panel_showing ||
              (devtools_showing &&
               devtools_placement !=
                   ContentsContainerView::DevToolsDockedPlacement::kLeft)
          ? 0
          : window_radii.lower_right(),
      is_ntp_footer_showing || left_aligned_side_panel_showing ||
              (devtools_showing &&
               devtools_placement !=
                   ContentsContainerView::DevToolsDockedPlacement::kRight)
          ? 0
          : window_radii.lower_left());

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
      SetRoundedCornersOnHost(contents_webview->holder(),
                              contents_webview_radii);
    }
  }

  if (contents_webview->GetBackgroundRadii() != contents_webview_radii) {
    contents_webview->SetBackgroundRadii(contents_webview_radii);
  }

#if BUILDFLAG(ENABLE_GLIC)
  if (auto* glic_border = GetActiveContentsContainerView()->glic_border_view();
      glic_border) {
    glic_border->SetRoundedCorners(contents_webview_radii);
  }
#endif  // BUILDFLAG(ENABLE_GLIC)

  const gfx::RoundedCornersF contents_scrim_radii(
      round_content_webview_top_corner ? window_radii.upper_left() : 0,
      round_content_webview_top_corner ? window_radii.upper_right() : 0,
      right_aligned_side_panel_showing ? 0 : window_radii.lower_right(),
      left_aligned_side_panel_showing ? 0 : window_radii.lower_left());
  GetActiveContentsContainerView()->contents_scrim_view()->SetRoundedCorners(
      contents_scrim_radii);
}
