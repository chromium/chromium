// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/contents_border_controller.h"

#include <optional>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/contents_container_view.h"
#include "chrome/browser/ui/views/tab_sharing/tab_capture_contents_border_helper.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_WIN)
#include "ui/views/widget/native_widget_aura.h"
#endif

ContentsBorderController::ContentsBorderController(BrowserView* browser_view) {
  for (ContentsContainerView* contents_container_view :
       browser_view->GetContentsContainerViews()) {
    border_controllers_.push_back(
        std::make_unique<ContentsContainerViewBorderController>(
            contents_container_view, browser_view));
  }
}

ContentsBorderController::~ContentsBorderController() = default;

ContentsBorderController::ContentsContainerViewBorderController::
    ContentsContainerViewBorderController(
        ContentsContainerView* contents_container_view,
        BrowserView* browser_view)
    : contents_container_view_(contents_container_view),
      browser_view_(browser_view) {
  web_contents_attached_subscription_ =
      contents_container_view_->contents_view()->AddWebContentsAttachedCallback(
          base::BindRepeating(
              &ContentsContainerViewBorderController::OnWebContentsAttached,
              base::Unretained(this)));

  web_contents_detached_subscription_ =
      contents_container_view_->contents_view()->AddWebContentsDetachedCallback(
          base::BindRepeating(
              &ContentsContainerViewBorderController::OnWebContentsDetached,
              base::Unretained(this)));
}

ContentsBorderController::ContentsContainerViewBorderController::
    ~ContentsContainerViewBorderController() = default;

void ContentsBorderController::ContentsContainerViewBorderController::
    OnWebContentsAttached(views::WebView* web_view) {
  TabCaptureContentsBorderHelper* const contents_border_helper =
      TabCaptureContentsBorderHelper::FromWebContents(
          web_view->GetWebContents());

  if (!contents_border_helper) {
    return;
  }

  tab_capture_change_subscription_ =
      contents_border_helper->AddOnTabCaptureChangeCallback(base::BindRepeating(
          &ContentsContainerViewBorderController::OnTabCaptureChange,
          base::Unretained(this)));

  tab_capture_location_change_subscription_ =
      contents_border_helper->AddOnTabCaptureLocationChangeCallback(
          base::BindRepeating(&ContentsContainerViewBorderController::
                                  OnTabCaptureLocationChange,
                              base::Unretained(this)));

  const bool should_show_border =
      contents_border_helper->ShouldShowBlueBorder();
  OnTabCaptureLocationChange(
      should_show_border ? contents_border_helper->GetBlueBorderLocation()
                         : std::nullopt);
  OnTabCaptureChange(should_show_border);
}

void ContentsBorderController::ContentsContainerViewBorderController::
    OnWebContentsDetached(views::WebView* web_view) {
  tab_capture_change_subscription_ = base::CallbackListSubscription();
  OnTabCaptureChange(false);
}

void ContentsBorderController::ContentsContainerViewBorderController::
    OnTabCaptureChange(bool is_capturing) {
  if (is_capturing) {
    contents_container_view_->ShowCaptureContentsBorder();
  } else {
    contents_container_view_->HideCaptureContentsBorder();
  }
}

void ContentsBorderController::ContentsContainerViewBorderController::
    OnTabCaptureLocationChange(std::optional<gfx::Rect> border_location) {
  contents_container_view_->SetCaptureContentsBorderLocation(border_location);
}
