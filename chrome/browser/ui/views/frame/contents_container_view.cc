// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/contents_container_view.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/contents_web_view.h"
#include "chrome/browser/ui/views/frame/multi_contents_view_mini_toolbar.h"
#include "chrome/browser/ui/views/frame/scrim_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/border.h"
#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/layout/proposed_layout.h"

namespace {

constexpr int kContentCornerRadius = 6;
constexpr int kContentOutlineCornerRadius = 8;
constexpr int kContentOutlineThickness = 1;
constexpr int kSplitViewContentPadding = 4;

}  // namespace

ContentsContainerView::ContentsContainerView(BrowserView* browser_view) {
  SetLayoutManager(std::make_unique<views::DelegatingLayoutManager>(this));

  contents_view_ = AddChildView(
      std::make_unique<ContentsWebView>(browser_view->GetProfile()));
  scrim_view_ = AddChildView(std::make_unique<ScrimView>(kColorSplitViewScrim));
  mini_toolbar_ = AddChildView(std::make_unique<MultiContentsViewMiniToolbar>(
      browser_view, contents_view_));
}

void ContentsContainerView::UpdateBorderAndOverlay(bool is_in_split,
                                                   bool is_active,
                                                   bool show_scrim) {
  // The border, mini toolbar, and scrim should not be visible if not in a
  // split.
  if (!is_in_split) {
    SetBorder(nullptr);
    contents_view_->layer()->SetRoundedCornerRadius(gfx::RoundedCornersF{0});
    mini_toolbar_->SetVisible(false);
    scrim_view_->SetVisible(false);
    return;
  }

  // Draw active/inactive outlines around the contents areas and updates mini
  // toolbar visibility.
  const SkColor color =
      is_active ? GetColorProvider()->GetColor(
                      kColorMulitContentsViewActiveContentOutline)
                : GetColorProvider()->GetColor(
                      kColorMulitContentsViewInactiveContentOutline);
  SetBorder(views::CreatePaddedBorder(
      views::CreateRoundedRectBorder(kContentOutlineThickness,
                                     kContentOutlineCornerRadius, color),
      gfx::Insets(kSplitViewContentPadding)));
  contents_view_->layer()->SetRoundedCornerRadius(
      gfx::RoundedCornersF{kContentCornerRadius});
  // Mini toolbar should only be visible for the inactive contents
  // container view or both depending on configuration.
  mini_toolbar_->UpdateState(is_active);
  // Scrim should only be allowed to show the scrim for inactive contents
  // container view.
  scrim_view_->SetVisible(!is_active && show_scrim);
}

views::ProposedLayout ContentsContainerView::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  views::ProposedLayout layouts;
  if (!size_bounds.is_fully_bounded()) {
    return layouts;
  }

  int height = size_bounds.height().value();
  int width = size_bounds.width().value();

  // |contents_view_| should fill the contents bounds.
  gfx::Rect contents_rect = GetContentsBounds();
  layouts.child_layouts.emplace_back(
      contents_view_.get(), contents_view_->GetVisible(), contents_rect);

  // The scrim view should cover and take up the same space as the contents
  // view.
  layouts.child_layouts.emplace_back(scrim_view_.get(),
                                     scrim_view_->GetVisible(), contents_rect);

  // |mini_toolbar_| should be offset in the bottom right corner, overlapping
  // the outline.
  gfx::Size mini_toolbar_size = mini_toolbar_->GetPreferredSize(
      views::SizeBounds(width - kContentOutlineCornerRadius, height));
  const int offset_x =
      width - mini_toolbar_size.width() + (kContentOutlineThickness / 2.0f);
  const int offset_y =
      height - mini_toolbar_size.height() + (kContentOutlineThickness / 2.0f);
  const gfx::Rect mini_toolbar_rect =
      gfx::Rect(offset_x, offset_y, mini_toolbar_size.width(),
                mini_toolbar_size.height());
  layouts.child_layouts.emplace_back(
      mini_toolbar_.get(), mini_toolbar_->GetVisible(), mini_toolbar_rect);

  layouts.host_size = gfx::Size(width, height);
  return layouts;
}

BEGIN_METADATA(ContentsContainerView)
END_METADATA
