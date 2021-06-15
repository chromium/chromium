// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_frame_view_layout_linux_native.h"

#include "ui/views/linux_ui/nav_button_provider.h"

BrowserFrameViewLayoutLinuxNative::BrowserFrameViewLayoutLinuxNative(
    views::NavButtonProvider* nav_button_provider)
    : nav_button_provider_(nav_button_provider) {}

BrowserFrameViewLayoutLinuxNative::~BrowserFrameViewLayoutLinuxNative() =
    default;

int BrowserFrameViewLayoutLinuxNative::CaptionButtonY(
    views::FrameButton button_id,
    bool restored) const {
  auto button_type = GetButtonDisplayType(button_id);
  gfx::Insets insets = nav_button_provider_->GetNavButtonMargin(button_type);
  return insets.top() + FrameTopThickness(!delegate_->IsMaximized());
}

OpaqueBrowserFrameViewLayout::TopAreaPadding
BrowserFrameViewLayoutLinuxNative::GetTopAreaPadding(
    bool has_leading_buttons,
    bool has_trailing_buttons) const {
  gfx::Insets insets =
      nav_button_provider_->GetTopAreaSpacing() +
      gfx::Insets(0, FrameSideThickness(!delegate_->IsMaximized()));
  const int leading = base::i18n::IsRTL() ? insets.right() : insets.left();
  const int trailing = base::i18n::IsRTL() ? insets.left() : insets.right();
  const int padding = FrameBorderThickness(false);
  return {has_leading_buttons ? leading : padding,
          has_trailing_buttons ? trailing : padding};
}

int BrowserFrameViewLayoutLinuxNative::GetWindowCaptionSpacing(
    views::FrameButton button_id,
    bool leading_spacing,
    bool is_leading_button) const {
  gfx::Insets insets =
      nav_button_provider_->GetNavButtonMargin(GetButtonDisplayType(button_id));
  if (!leading_spacing)
    return insets.right();
  int spacing = insets.left();
  if (!is_leading_button)
    spacing += nav_button_provider_->GetInterNavButtonSpacing();
  return spacing;
}

views::NavButtonProvider::FrameButtonDisplayType
BrowserFrameViewLayoutLinuxNative::GetButtonDisplayType(
    views::FrameButton button_id) const {
  switch (button_id) {
    case views::FrameButton::kMinimize:
      return views::NavButtonProvider::FrameButtonDisplayType::kMinimize;
    case views::FrameButton::kMaximize:
      return delegate_->IsMaximized()
                 ? views::NavButtonProvider::FrameButtonDisplayType::kRestore
                 : views::NavButtonProvider::FrameButtonDisplayType::kMaximize;
    case views::FrameButton::kClose:
      return views::NavButtonProvider::FrameButtonDisplayType::kClose;
    default:
      NOTREACHED();
      return views::NavButtonProvider::FrameButtonDisplayType::kClose;
  }
}
