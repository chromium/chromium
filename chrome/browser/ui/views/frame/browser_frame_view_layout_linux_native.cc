// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_frame_view_layout_linux_native.h"

#include <algorithm>

#include "chrome/browser/ui/views/frame/opaque_browser_frame_view_layout.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/text_constants.h"
#include "ui/linux/nav_button_provider.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/window/frame_view_utils_linux.h"

namespace {

gfx::FontList GetTitleFontList() {
  return views::TypographyProvider::Get()
      .GetWindowTitleFontList()
      .DeriveWithWeight(gfx::Font::Weight::BOLD);
}

}  // namespace

BrowserFrameViewLayoutLinuxNative::BrowserFrameViewLayoutLinuxNative(
    ui::NavButtonProvider* nav_button_provider,
    FrameProviderGetter frame_provider_getter)
    : nav_button_provider_(nav_button_provider),
      frame_provider_getter_(frame_provider_getter) {}

BrowserFrameViewLayoutLinuxNative::~BrowserFrameViewLayoutLinuxNative() =
    default;

ui::WindowFrameProvider* BrowserFrameViewLayoutLinuxNative::GetFrameProvider()
    const {
  const bool tiled = delegate_->IsTiled();
  const bool maximized = delegate_->IsMaximized();
  return frame_provider_getter_.Run(tiled, maximized);
}

int BrowserFrameViewLayoutLinuxNative::NonClientTopHeight(bool restored) const {
  if (!delegate_->ShouldShowWindowTitle() || !ShouldUseNativeHeaderBar()) {
    return BrowserFrameViewLayoutLinux::NonClientTopHeight(restored);
  }

  const gfx::Insets insets = FrameBorderInsets(restored);
  const int title_font_height = GetTitleFontList().GetHeight();

  const bool tiled = !restored && delegate_->IsTiled();
  const bool maximized = !restored && delegate_->IsMaximized();
  ui::WindowFrameProvider* frame_provider =
      frame_provider_getter_.Run(tiled, maximized);
  const int titlebar_min_height = frame_provider->GetTopAreaMinHeightDip();
  const gfx::Insets titlebar_padding = frame_provider->GetTopAreaPaddingDip();
  const gfx::Insets titlebar_border = frame_provider->GetTopAreaBorderDip();

  const int button_content_height =
      nav_button_provider_->GetNavButtonHeight(maximized);
  const int content_height =
      std::max({title_font_height, titlebar_min_height, button_content_height});

  return insets.top() + titlebar_border.height() + titlebar_padding.height() +
         content_height;
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
  // Unframed mode only has a minimal frame to be able to resize it from the
  // borders.
  if (delegate_->GetUnframedModeEnabled()) {
    return gfx::Insets(
        OpaqueBrowserFrameViewLayout::RestoredFrameBorderInsets());
  }

  // Always use the restored frame provider. These insets are used to convert
  // between window geometry and bounds for the restored state, which may be
  // computed while the window is still maximized (e.g. when the compositor
  // unmaximizes the window).
  // Like BrowserFrameViewLayoutLinux, tiled windows have no border insets so
  // that the window geometry stays aligned to the pixel grid.
  if (delegate_->IsTiled()) {
    return gfx::Insets();
  }
  gfx::Insets insets =
      frame_provider_getter_.Run(/*tiled=*/false, /*maximized=*/false)
          ->GetFrameThicknessDip();
  insets.SetToMax(GetRestoredInputInsets());
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

void BrowserFrameViewLayoutLinuxNative::LayoutTitleBar(views::View* host) {
  if (!ShouldUseNativeHeaderBar()) {
    BrowserFrameViewLayoutLinux::LayoutTitleBar(host);
    return;
  }

  if (window_icon()) {
    SetViewVisibility(window_icon(), false);
  }
  set_window_icon_bounds(gfx::Rect());

  views::Label* title = window_title();
  if (!title) {
    return;
  }

  const bool should_show_title = delegate_->ShouldShowWindowTitle();
  SetViewVisibility(title, should_show_title);
  if (!should_show_title) {
    return;
  }

  title->SetText(delegate_->GetWindowTitle());
  const gfx::FontList font_list = GetTitleFontList();
  title->SetFontList(font_list);
  title->SetHorizontalAlignment(gfx::ALIGN_CENTER);

  const int min_x = available_space_leading_x_ + kCaptionSpacing;
  const int max_x = available_space_trailing_x_ - kCaptionSpacing;
  const int available_width = std::max(0, max_x - min_x);
  if (available_width == 0) {
    title->SetBoundsRect(gfx::Rect());
    return;
  }

  const gfx::Insets insets = FrameBorderInsets(false);
  const gfx::Insets titlebar_border = GetFrameProvider()->GetTopAreaBorderDip();
  const int title_height = font_list.GetHeight();
  const int titlebar_content_top = insets.top() + titlebar_border.top();
  const int titlebar_content_height =
      NonClientTopHeight(false) - insets.top() - titlebar_border.height();
  const int title_y =
      titlebar_content_top + (titlebar_content_height - title_height) / 2;

  const int preferred_width =
      title->GetPreferredSize(views::SizeBounds(available_width, title_height))
          .width();
  const int title_width = std::min(preferred_width, available_width);
  const int centered_x = (host->width() - title_width) / 2;
  const int title_x = std::clamp(centered_x, min_x, max_x - title_width);

  title->SetBounds(title_x, title_y, title_width, title_height);
}

bool BrowserFrameViewLayoutLinuxNative::ShouldUseNativeHeaderBar() const {
  return !delegate_->IsTabStripVisible() &&
         delegate_->WebAppButtonHeight() == 0 &&
         !delegate_->GetUnframedModeEnabled();
}

ui::NavButtonProvider::FrameButtonDisplayType
BrowserFrameViewLayoutLinuxNative::GetButtonDisplayType(
    views::FrameButton button_id) const {
  return views::GetFrameButtonDisplayType(button_id, delegate_->IsMaximized());
}
