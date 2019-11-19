// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/desktop_linux_browser_frame_view.h"

#include "chrome/browser/ui/views/frame/desktop_linux_browser_frame_view_layout.h"
#include "chrome/browser/ui/views/nav_button_provider.h"
#include "ui/views/controls/button/image_button.h"

bool DesktopLinuxBrowserFrameView::DrawFrameButtonParams::operator==(
    const DrawFrameButtonParams& other) const {
  return top_area_height == other.top_area_height &&
         maximized == other.maximized && active == other.active;
}

DesktopLinuxBrowserFrameView::DesktopLinuxBrowserFrameView(
    BrowserFrame* frame,
    BrowserView* browser_view,
    OpaqueBrowserFrameViewLayout* layout,
    std::unique_ptr<views::NavButtonProvider> nav_button_provider)
    : OpaqueBrowserFrameView(frame, browser_view, layout),
      nav_button_provider_(std::move(nav_button_provider)) {}

DesktopLinuxBrowserFrameView::~DesktopLinuxBrowserFrameView() {}

void DesktopLinuxBrowserFrameView::Layout() {
  // Calling MaybeUpdateCachedFrameButtonImages() from Layout() is sufficient to
  // catch all cases that could update the appearance, since
  // DesktopWindowTreeHostPlatform::OnWindowStateChanged() does a layout any
  // time any properties change.
  MaybeUpdateCachedFrameButtonImages();
  OpaqueBrowserFrameView::Layout();
}

DesktopLinuxBrowserFrameView::FrameButtonStyle
DesktopLinuxBrowserFrameView::GetFrameButtonStyle() const {
  return FrameButtonStyle::kImageButton;
}

void DesktopLinuxBrowserFrameView::MaybeUpdateCachedFrameButtonImages() {
  DrawFrameButtonParams params{
      GetTopAreaHeight() - layout()->FrameTopThickness(!IsMaximized()),
      IsMaximized(), ShouldPaintAsActive()};
  if (cache_ == params)
    return;
  cache_ = params;
  nav_button_provider_->RedrawImages(params.top_area_height, params.maximized,
                                     params.active);
  for (auto type : {
           chrome::FrameButtonDisplayType::kMinimize,
           IsMaximized() ? chrome::FrameButtonDisplayType::kRestore
                         : chrome::FrameButtonDisplayType::kMaximize,
           chrome::FrameButtonDisplayType::kClose,
       }) {
    for (size_t state = 0; state < views::Button::STATE_COUNT; state++) {
      views::Button::ButtonState button_state =
          static_cast<views::Button::ButtonState>(state);
      views::Button* button = GetButtonFromDisplayType(type);
      DCHECK_EQ(std::string(views::ImageButton::kViewClassName),
                button->GetClassName());
      static_cast<views::ImageButton*>(button)->SetImage(
          button_state, nav_button_provider_->GetImage(type, button_state));
    }
  }
}

views::Button* DesktopLinuxBrowserFrameView::GetButtonFromDisplayType(
    chrome::FrameButtonDisplayType type) {
  switch (type) {
    case chrome::FrameButtonDisplayType::kMinimize:
      return minimize_button();
    case chrome::FrameButtonDisplayType::kMaximize:
      return maximize_button();
    case chrome::FrameButtonDisplayType::kRestore:
      return restore_button();
    case chrome::FrameButtonDisplayType::kClose:
      return close_button();
    default:
      NOTREACHED();
      return nullptr;
  }
}
