// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/system_web_dialog/system_web_dialog_view.h"

#include "base/check_op.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/webview/web_dialog_view.h"

namespace ash {

SystemWebDialogView::SystemWebDialogView(
    content::BrowserContext* context,
    ui::WebDialogDelegate* delegate,
    std::unique_ptr<WebContentsHandler> handler,
    content::WebContents* web_contents)
    : views::WebDialogView(context,
                           delegate,
                           std::move(handler),
                           web_contents) {}

void SystemWebDialogView::UpdateWindowRoundedCorners(int corner_radius) {
  gfx::RoundedCornersF radii;

  if (GetWebDialogFrameKind() == FrameKind::kDialog && GetBubbleFrameView()) {
    const auto* frame_view = GetBubbleFrameView();

    const bool round_top_corners =
        frame_view->GetClientViewInsets().top() < corner_radius;

    // If the frame kind of a dialog is FrameKind::kDialog, BubbleFrameView is
    // used as a frame for the dialog. For windows with BubbleFrameView, we draw
    // a window border, along with optional decorations like header, footnote,
    // and control buttons. If the decorations are drawn above the client view,
    // the rounded border will round the top corners and client view will be
    // straight line. If there are no decorations above client_view (i.e no
    // insets from frame), the rounded border will be just below the webview and
    // the top corners  of webview must match the radius of rounded border.
    radii.set_upper_left(round_top_corners ? corner_radius : 0);
    radii.set_upper_right(round_top_corners ? corner_radius : 0);

    const bool round_bottom_corners = frame_view->GetFootnoteView() == nullptr;

    // For BubbleFrameView, if footnote view is present, the footnote view will
    // be rounded alongside the border by BubbleFrameView. The client_view will
    // be a straight line and in turn webview should not have rounded corners.
    radii.set_lower_left(round_bottom_corners ? corner_radius : 0);
    radii.set_lower_right(round_bottom_corners ? corner_radius : 0);
  } else {
    // If the frame kind of a dialog is FrameKind::kDialog,
    // NonClientFrameViewAsh is used as a frame for the dialog. For windows with
    // NonClientFrameViewAsh, there is no window border. The top corners are
    // rounded by the header in NonClientFrameViewAsh, therefore to round the
    // bottom corners of the dialog, we need to round the bottom corners of the
    // webview.
    radii.set_lower_left(corner_radius);
    radii.set_lower_right(corner_radius);
  }

  SetWebViewCornersRadii(radii);
}

}  // namespace ash
