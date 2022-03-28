// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_frame_view_layout_linux.h"

#include "base/i18n/rtl.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/frame/browser_frame_view_linux.h"
#include "chrome/browser/ui/views/frame/opaque_browser_frame_view.h"
#include "ui/gfx/geometry/rect.h"

namespace {

// This is the same thickness as the resize border on ChromeOS.
constexpr unsigned int kResizeBorder = 10;

// The "extra top border" is only 1dip in OpaqueBrowserFrameViewLayout, but that
// does not include the 2dip 3D frame border at the top, which
// BrowserFrameViewLayoutLinux doesn't have.  We need to add that back here so
// that the tabstrip area maintains the same height.
constexpr unsigned int kExtraTopBorder = 3;

}  // namespace

BrowserFrameViewLayoutLinux::BrowserFrameViewLayoutLinux() = default;
BrowserFrameViewLayoutLinux::~BrowserFrameViewLayoutLinux() = default;

gfx::Insets BrowserFrameViewLayoutLinux::MirroredFrameBorderInsets() const {
  auto border = FrameBorderInsets(false);
  return base::i18n::IsRTL() ? gfx::Insets::TLBR(border.top(), border.right(),
                                                 border.bottom(), border.left())
                             : border;
}

gfx::Insets BrowserFrameViewLayoutLinux::GetInputInsets() const {
  bool showing_shadow = delegate_->ShouldDrawRestoredFrameShadow() &&
                        !delegate_->IsFrameCondensed();
  return gfx::Insets(showing_shadow ? -kResizeBorder : 0);
}

int BrowserFrameViewLayoutLinux::CaptionButtonY(views::FrameButton button_id,
                                                bool restored) const {
  return FrameEdgeInsets(restored).top();
}

gfx::Insets BrowserFrameViewLayoutLinux::RestoredFrameBorderInsets() const {
  if (!delegate_->ShouldDrawRestoredFrameShadow()) {
    gfx::Insets insets =
        OpaqueBrowserFrameViewLayout::RestoredFrameBorderInsets();
    insets.set_top(0);
    return insets;
  }

  // The border must be at least as large as the shadow.
  gfx::Rect frame_extents;
  for (const auto& shadow_value : view_->GetShadowValues()) {
    auto shadow_radius = shadow_value.blur() / 4;
    gfx::RectF shadow_extents;
    shadow_extents.Inset(-gfx::InsetsF(shadow_radius));
    shadow_extents.set_y(shadow_extents.y() + shadow_value.y());
    frame_extents.Union(gfx::ToEnclosingRect(shadow_extents));
  }

  // The border must be at least as large as the input region.
  gfx::Rect input_extents;
  input_extents.Inset(-gfx::Insets(kResizeBorder));
  frame_extents.Union(input_extents);

  return gfx::Insets::TLBR(-frame_extents.y(), -frame_extents.x(),
                           frame_extents.bottom(), frame_extents.right());
}

gfx::Insets BrowserFrameViewLayoutLinux::RestoredFrameEdgeInsets() const {
  return delegate_->ShouldDrawRestoredFrameShadow()
             ? RestoredFrameBorderInsets()
             : gfx::Insets();
}

int BrowserFrameViewLayoutLinux::NonClientExtraTopThickness() const {
  return kExtraTopBorder;
}
