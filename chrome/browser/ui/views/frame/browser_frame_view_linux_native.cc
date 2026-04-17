// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_frame_view_linux_native.h"

#include "base/notreached.h"
#include "chrome/browser/ui/views/frame/browser_frame_view_layout_linux_native.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/linux/linux_ui.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/window/frame_background.h"
#include "ui/views/window/frame_view_utils_linux.h"

BrowserFrameViewLinuxNative::BrowserFrameViewLinuxNative(
    BrowserWidget* widget,
    BrowserView* browser_view,
    BrowserFrameViewLayoutLinuxNative* layout,
    std::unique_ptr<ui::NavButtonProvider> nav_button_provider)
    : BrowserFrameViewLinux(widget, browser_view, layout),
      nav_button_provider_(std::move(nav_button_provider)),
      layout_(layout) {}

BrowserFrameViewLinuxNative::~BrowserFrameViewLinuxNative() = default;

void BrowserFrameViewLinuxNative::Layout(PassKey) {
  // Calling MaybeUpdateCachedFrameButtonImages() here is sufficient to catch
  // all cases that could update the appearance, since
  // DesktopWindowTreeHostPlatform::On{Window,Activation}StateChanged() does a
  // layout any time the maximized and activation state changes, respectively.
  MaybeUpdateCachedFrameButtonImages();
  LayoutSuperclass<BrowserFrameViewLinux>(this);
}

BrowserFrameViewLinuxNative::FrameButtonStyle
BrowserFrameViewLinuxNative::GetFrameButtonStyle() const {
  return FrameButtonStyle::kImageButton;
}

int BrowserFrameViewLinuxNative::GetTranslucentTopAreaHeight() const {
  return layout_->GetFrameProvider()->IsTopFrameTranslucent()
             ? GetTopAreaHeight()
             : 0;
}

BrowserLayoutParams BrowserFrameViewLinuxNative::GetBrowserLayoutParams()
    const {
  // Because this can be called before the frame is laid out, we might need to
  // preemptively cache the frame buttons.
  const_cast<BrowserFrameViewLinuxNative*>(this)
      ->MaybeUpdateCachedFrameButtonImages();
  return BrowserFrameViewLinux::GetBrowserLayoutParams();
}

float BrowserFrameViewLinuxNative::GetRestoredCornerRadiusDip() const {
  return layout_->GetFrameProvider()->GetTopCornerRadiusDip();
}

void BrowserFrameViewLinuxNative::PaintRestoredFrameBorder(
    gfx::Canvas* canvas) const {
  layout_->GetFrameProvider()->PaintWindowFrame(
      canvas, GetLocalBounds(), GetTopAreaHeight(), ShouldPaintAsActive(),
      GetInputInsets());
}

void BrowserFrameViewLinuxNative::MaybeUpdateCachedFrameButtonImages() {
  views::DrawFrameButtonParams params{
      GetTopAreaHeight() - layout()->FrameEdgeInsets(!IsMaximized()).top(),
      IsMaximized(), ShouldPaintAsActive()};
  views::MaybeUpdateCachedFrameButtonImages(
      nav_button_provider_.get(), params, cache_,
      [this](ui::NavButtonProvider::FrameButtonDisplayType type) {
        return GetButtonFromDisplayType(type);
      });
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
  }
}

BEGIN_METADATA(BrowserFrameViewLinuxNative)
END_METADATA
