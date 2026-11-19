// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/system_web_dialog/system_web_dialog_view.h"

#include <algorithm>
#include <utility>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/check_op.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/webview/web_dialog_view.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

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

SystemWebDialogView::~SystemWebDialogView() {
  ResetModalDialogHost();
}

void SystemWebDialogView::UpdateWindowRoundedCorners(
    const gfx::RoundedCornersF& window_radii) {
  gfx::RoundedCornersF radii;

  if (GetWebDialogFrameKind() == FrameKind::kDialog && GetBubbleFrameView()) {
    const auto* frame_view = GetBubbleFrameView();

    const bool round_top_corners =
        frame_view->GetClientViewInsets().top() < window_radii.upper_left();

    // If the frame kind of a dialog is FrameKind::kDialog, BubbleFrameView is
    // used as a frame for the dialog. For windows with BubbleFrameView, we draw
    // a window border, along with optional decorations like header, footnote,
    // and control buttons. If the decorations are drawn above the client view,
    // the rounded border will round the top corners and client view will be
    // straight line. If there are no decorations above client_view (i.e no
    // insets from frame), the rounded border will be just below the webview and
    // the top corners  of webview must match the radius of rounded border.
    radii.set_upper_left(round_top_corners ? window_radii.upper_left() : 0);
    radii.set_upper_right(round_top_corners ? window_radii.upper_right() : 0);

    const bool round_bottom_corners = frame_view->GetFootnoteView() == nullptr;

    // For BubbleFrameView, if footnote view is present, the footnote view will
    // be rounded alongside the border by BubbleFrameView. The client_view will
    // be a straight line and in turn webview should not have rounded corners.
    radii.set_lower_left(round_bottom_corners ? window_radii.lower_left() : 0);
    radii.set_lower_right(round_bottom_corners ? window_radii.lower_right()
                                               : 0);
  } else {
    // If the frame kind of a dialog is FrameKind::kDialog,
    // FrameViewAsh is used as a frame for the dialog. For windows with
    // FrameViewAsh, there is no window border. The top corners are
    // rounded by the header in FrameViewAsh, therefore to round the
    // bottom corners of the dialog, we need to round the bottom corners of the
    // webview.
    radii.set_lower_left(window_radii.lower_left());
    radii.set_lower_right(window_radii.lower_right());
  }

  SetWebViewCornersRadii(radii);
}

void SystemWebDialogView::AddedToWidget() {
  views::WebDialogView::AddedToWidget();
  CHECK(web_contents());
  web_modal::WebContentsModalDialogManager::CreateForWebContents(
      web_contents());
  auto* manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(web_contents());
  if (!manager->delegate()) {
    manager->SetDelegate(this);
  }
}

void SystemWebDialogView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  views::WebDialogView::OnBoundsChanged(previous_bounds);
  modal_dialog_host_observer_list_.Notify(
      &web_modal::ModalDialogHostObserver::OnPositionRequiresUpdate);
}

void SystemWebDialogView::WindowClosing() {
  ResetModalDialogHost();
  views::WebDialogView::WindowClosing();
}

web_modal::WebContentsModalDialogHost*
SystemWebDialogView::GetWebContentsModalDialogHost(
    content::WebContents* web_contents) {
  return this;
}

bool SystemWebDialogView::IsWebContentsVisible(
    content::WebContents* web_contents) {
  return CHECK_DEREF(web_contents->GetNativeView()).IsVisible();
}

gfx::NativeView SystemWebDialogView::GetHostView() const {
  return CHECK_DEREF(GetWidget()).GetNativeView();
}

gfx::Point SystemWebDialogView::GetDialogPosition(
    const gfx::Size& dialog_size) {
  const gfx::Size host_size = GetMaximumDialogSize();
  gfx::Point position(
      std::max(0, (host_size.width() - dialog_size.width()) / 2),
      std::max(0, (host_size.height() - dialog_size.height()) / 2));
  views::View::ConvertPointToWidget(this, &position);
  return position;
}

gfx::Size SystemWebDialogView::GetMaximumDialogSize() {
  return size().IsEmpty() ? CalculatePreferredSize({}) : size();
}

void SystemWebDialogView::AddObserver(
    web_modal::ModalDialogHostObserver* observer) {
  modal_dialog_host_observer_list_.AddObserver(observer);
}

void SystemWebDialogView::RemoveObserver(
    web_modal::ModalDialogHostObserver* observer) {
  modal_dialog_host_observer_list_.RemoveObserver(observer);
}

void SystemWebDialogView::ResetModalDialogHost() {
  if (modal_dialog_host_reset_) {
    return;
  }
  modal_dialog_host_reset_ = true;
  modal_dialog_host_observer_list_.Notify(
      &web_modal::ModalDialogHostObserver::OnHostDestroying);
  if (web_contents()) {
    if (auto* manager =
            web_modal::WebContentsModalDialogManager::FromWebContents(
                web_contents());
        manager && manager->delegate() == this) {
      manager->SetDelegate(nullptr);
    }
  }
}

}  // namespace ash
