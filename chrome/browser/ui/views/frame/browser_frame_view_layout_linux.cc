// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_frame_view_layout_linux.h"

#include "base/i18n/rtl.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/frame/browser_frame_view_linux.h"
#include "chrome/browser/ui/views/frame/browser_frame_view_paint_utils_linux.h"
#include "chrome/browser/ui/views/frame/opaque_browser_frame_view_layout.h"
#include "ui/base/ui_base_features.h"

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

gfx::Insets BrowserFrameViewLayoutLinux::RestoredMirroredFrameBorderInsets()
    const {
  auto border = RestoredFrameBorderInsets();
  return base::i18n::IsRTL() ? gfx::Insets::TLBR(border.top(), border.right(),
                                                 border.bottom(), border.left())
                             : border;
}

gfx::Insets BrowserFrameViewLayoutLinux::GetInputInsets() const {
  bool showing_shadow = delegate_->ShouldDrawRestoredFrameShadow() &&
                        !delegate_->IsFrameCondensed();
  return gfx::Insets(showing_shadow ? kResizeBorder : 0);
}

int BrowserFrameViewLayoutLinux::CaptionButtonY(views::FrameButton button_id,
                                                bool restored) const {
  return FrameEdgeInsets(restored).top();
}

gfx::Insets BrowserFrameViewLayoutLinux::RestoredFrameBorderInsets() const {
  // Borderless mode only has a minimal frame to be able to resize it from the
  // borders.
  if (delegate_->GetBorderlessModeEnabled()) {
    return gfx::Insets(
        OpaqueBrowserFrameViewLayout::RestoredFrameBorderInsets());
  }

#if BUILDFLAG(IS_LINUX)
  const bool tiled = delegate_->IsTiled();
#else
  const bool tiled = false;
#endif
  auto shadow_values =
      tiled ? gfx::ShadowValues() : view_->GetShadowValues(true);
  return GetRestoredFrameBorderInsetsLinux(
      delegate_->ShouldDrawRestoredFrameShadow(),
      OpaqueBrowserFrameViewLayout::RestoredFrameBorderInsets(), shadow_values,
      kResizeBorder);
}

gfx::Insets BrowserFrameViewLayoutLinux::RestoredFrameEdgeInsets() const {
  return delegate_->ShouldDrawRestoredFrameShadow()
             ? RestoredFrameBorderInsets()
             : gfx::Insets();
}

int BrowserFrameViewLayoutLinux::NonClientExtraTopThickness() const {
  return delegate_->IsTabStripVisible() ? 0 : kExtraTopBorder;
}
