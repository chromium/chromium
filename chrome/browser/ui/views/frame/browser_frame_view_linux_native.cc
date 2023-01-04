// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_frame_view_linux_native.h"

#include "base/notreached.h"
#include "chrome/browser/ui/views/frame/browser_frame_view_layout_linux_native.h"
#include "ui/linux/linux_ui.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/window/frame_background.h"

namespace {

ui::NavButtonProvider::ButtonState ButtonStateToNavButtonProviderState(
    views::Button::ButtonState state) {
  switch (state) {
    case views::Button::STATE_NORMAL:
      return ui::NavButtonProvider::ButtonState::kNormal;
    case views::Button::STATE_HOVERED:
      return ui::NavButtonProvider::ButtonState::kHovered;
    case views::Button::STATE_PRESSED:
      return ui::NavButtonProvider::ButtonState::kPressed;
    case views::Button::STATE_DISABLED:
      return ui::NavButtonProvider::ButtonState::kDisabled;

    case views::Button::STATE_COUNT:
    default:
      NOTREACHED();
      return ui::NavButtonProvider::ButtonState::kNormal;
  }
}

}  // namespace

bool BrowserFrameViewLinuxNative::DrawFrameButtonParams::operator==(
    const DrawFrameButtonParams& other) const {
  return top_area_height == other.top_area_height &&
         maximized == other.maximized && active == other.active;
}

BrowserFrameViewLinuxNative::BrowserFrameViewLinuxNative(
    BrowserFrame* frame,
    BrowserView* browser_view,
    BrowserFrameViewLayoutLinux* layout,
    std::unique_ptr<ui::NavButtonProvider> nav_button_provider,
    ui::WindowFrameProvider* window_frame_provider)
    : BrowserFrameViewLinux(frame, browser_view, layout),
      nav_button_provider_(std::move(nav_button_provider)),
      window_frame_provider_(window_frame_provider) {}

BrowserFrameViewLinuxNative::~BrowserFrameViewLinuxNative() = default;

float BrowserFrameViewLinuxNative::GetRestoredCornerRadiusDip() const {
  return window_frame_provider_->GetTopCornerRadiusDip();
}

int BrowserFrameViewLinuxNative::GetTranslucentTopAreaHeight() const {
  return window_frame_provider_->IsTopFrameTranslucent() ? GetTopAreaHeight()
                                                         : 0;
}

void BrowserFrameViewLinuxNative::Layout() {
  // Calling MaybeUpdateCachedFrameButtonImages() from Layout() is sufficient to
  // catch all cases that could update the appearance, since
  // DesktopWindowTreeHostPlatform::On{Window,Activation}StateChanged() does a
  // layout any time the maximized and activation state changes, respectively.
  MaybeUpdateCachedFrameButtonImages();
  OpaqueBrowserFrameView::Layout();
}

BrowserFrameViewLinuxNative::FrameButtonStyle
BrowserFrameViewLinuxNative::GetFrameButtonStyle() const {
  return FrameButtonStyle::kImageButton;
}

void BrowserFrameViewLinuxNative::PaintRestoredFrameBorder(
    gfx::Canvas* canvas) const {
  window_frame_provider_->PaintWindowFrame(
      canvas, GetLocalBounds(), GetTopAreaHeight(), ShouldPaintAsActive(),
      GetTiledEdges());
}

void BrowserFrameViewLinuxNative::MaybeUpdateCachedFrameButtonImages() {
  DrawFrameButtonParams params{
      GetTopAreaHeight() - layout()->FrameEdgeInsets(!IsMaximized()).top(),
      IsMaximized(), ShouldPaintAsActive()};
  if (cache_ == params)
    return;
  cache_ = params;
  nav_button_provider_->RedrawImages(params.top_area_height, params.maximized,
                                     params.active);
  for (auto type : {
           ui::NavButtonProvider::FrameButtonDisplayType::kMinimize,
           IsMaximized()
               ? ui::NavButtonProvider::FrameButtonDisplayType::kRestore
               : ui::NavButtonProvider::FrameButtonDisplayType::kMaximize,
           ui::NavButtonProvider::FrameButtonDisplayType::kClose,
       }) {
    for (size_t state = 0; state < views::Button::STATE_COUNT; state++) {
      views::Button::ButtonState button_state =
          static_cast<views::Button::ButtonState>(state);
      views::Button* button = GetButtonFromDisplayType(type);
      DCHECK_EQ(std::string(views::ImageButton::kViewClassName),
                button->GetClassName());
      static_cast<views::ImageButton*>(button)->SetImage(
          button_state,
          nav_button_provider_->GetImage(
              type, ButtonStateToNavButtonProviderState(button_state)));
    }
  }
}

views::Button* BrowserFrameViewLinuxNative::GetButtonFromDisplayType(
    ui::NavButtonProvider::FrameButtonDisplayType type) {
  switch (type) {
    case ui::NavButtonProvider::FrameButtonDisplayType::kMinimize:
      return minimize_button();
    case ui::NavButtonProvider::FrameButtonDisplayType::kMaximize:
      return maximize_button();
    case ui::NavButtonProvider::FrameButtonDisplayType::kRestore:
      return restore_button();
    case ui::NavButtonProvider::FrameButtonDisplayType::kClose:
      return close_button();
    default:
      NOTREACHED();
      return nullptr;
  }
}
