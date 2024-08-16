// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_frame_view_layout_linux_native.h"

#include "chrome/browser/ui/views/frame/opaque_browser_frame_view_layout.h"
#include "ui/linux/nav_button_provider.h"

BrowserFrameViewLayoutLinuxNative::BrowserFrameViewLayoutLinuxNative(
    ui::NavButtonProvider* nav_button_provider,
    FrameProviderGetter frame_provider_getter)
    : nav_button_provider_(nav_button_provider),
      frame_provider_getter_(frame_provider_getter) {}

BrowserFrameViewLayoutLinuxNative::~BrowserFrameViewLayoutLinuxNative() =
    default;

ui::WindowFrameProvider* BrowserFrameViewLayoutLinuxNative::GetFrameProvider()
    const {
#if BUILDFLAG(IS_LINUX)
  const bool tiled = delegate_->IsTiled();
#else
  const bool tiled = false;
#endif
  return frame_provider_getter_.Run(tiled);
}

int BrowserFrameViewLayoutLinuxNative::CaptionButtonY(
    views::FrameButton button_id,
    bool restored) const {
  auto button_type = GetButtonDisplayType(button_id);
  gfx::Insets insets = nav_button_provider_->GetNavButtonMargin(button_type);
  return insets.top() + FrameEdgeInsets(false).top();
}

gfx::Insets BrowserFrameViewLayoutLinuxNative::RestoredFrameBorderInsets()
    const {
  // Borderless mode only has a minimal frame to be able to resize it from the
  // borders.
  if (delegate_->GetBorderlessModeEnabled()) {
    return gfx::Insets(
        OpaqueBrowserFrameViewLayout::RestoredFrameBorderInsets());
  }

  gfx::Insets insets = GetFrameProvider()->GetFrameThicknessDip();
  insets.SetToMax(GetInputInsets());
  return insets;
}

OpaqueBrowserFrameViewLayout::TopAreaPadding
BrowserFrameViewLayoutLinuxNative::GetTopAreaPadding(
    bool has_leading_buttons,
    bool has_trailing_buttons) const {
  gfx::Insets spacing = nav_button_provider_->GetTopAreaSpacing();
  gfx::Insets insets = spacing + FrameEdgeInsets(false);
  const auto padding = FrameBorderInsets(false);
  const auto leading = has_leading_buttons ? insets : padding;
  const auto trailing = has_trailing_buttons ? insets : padding;
  return TopAreaPadding{leading.left(), trailing.right()};
}

int BrowserFrameViewLayoutLinuxNative::GetWindowCaptionSpacing(
    views::FrameButton button_id,
    bool leading_spacing,
    bool is_leading_button) const {
  gfx::Insets insets =
      nav_button_provider_->GetNavButtonMargin(GetButtonDisplayType(button_id));
  if (!leading_spacing) {
    return insets.right();
  }
  int spacing = insets.left();
  if (!is_leading_button) {
    spacing += nav_button_provider_->GetInterNavButtonSpacing();
  }
  return spacing;
}

ui::NavButtonProvider::FrameButtonDisplayType
BrowserFrameViewLayoutLinuxNative::GetButtonDisplayType(
    views::FrameButton button_id) const {
  switch (button_id) {
    case views::FrameButton::kMinimize:
      return ui::NavButtonProvider::FrameButtonDisplayType::kMinimize;
    case views::FrameButton::kMaximize:
      return delegate_->IsMaximized()
                 ? ui::NavButtonProvider::FrameButtonDisplayType::kRestore
                 : ui::NavButtonProvider::FrameButtonDisplayType::kMaximize;
    case views::FrameButton::kClose:
      return ui::NavButtonProvider::FrameButtonDisplayType::kClose;
    default:
      NOTREACHED();
  }
}
