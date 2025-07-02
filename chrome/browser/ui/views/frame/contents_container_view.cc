// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/contents_container_view.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/ui_features.h"
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
constexpr gfx::RoundedCornersF kContentCornerRadius{6};
constexpr int kContentOutlineCornerRadius = 8;
constexpr int kContentOutlineThickness = 1;
constexpr int kSplitViewContentPadding = 4;
}  // namespace

ContentsContainerView::ContentsContainerView(BrowserView* browser_view) {
  SetLayoutManager(std::make_unique<views::DelegatingLayoutManager>(this));

  contents_view_ = AddChildView(
      std::make_unique<ContentsWebView>(browser_view->GetProfile()));

  contents_scrim_view_ = AddChildView(std::make_unique<ScrimView>());
  contents_scrim_view_->layer()->SetName("ContentsScrimView");

  if (base::FeatureList::IsEnabled(features::kSideBySide)) {
    inactive_split_scrim_view_ =
        AddChildView(std::make_unique<ScrimView>(kColorSplitViewScrim));
    inactive_split_scrim_view_->SetRoundedCorners(kContentCornerRadius);
    mini_toolbar_ = AddChildView(std::make_unique<MultiContentsViewMiniToolbar>(
        browser_view, contents_view_));
  }
}

void ContentsContainerView::UpdateBorderAndOverlay(bool is_in_split,
                                                   bool is_active,
                                                   bool show_scrim) {
  // The border, mini toolbar, and scrim should not be visible if not in a
  // split.
  if (!is_in_split) {
    SetBorder(nullptr);
    contents_view_->SetBackgroundRadii(gfx::RoundedCornersF{0});
    contents_scrim_view_->SetRoundedCorners(gfx::RoundedCornersF{0});
    mini_toolbar_->SetVisible(false);
    inactive_split_scrim_view_->SetVisible(false);
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

  if (contents_view_->GetBackgroundRadii() != kContentCornerRadius) {
    contents_view_->SetBackgroundRadii(kContentCornerRadius);
  }
  if (contents_scrim_view_->layer()->rounded_corner_radii() !=
      kContentCornerRadius) {
    contents_scrim_view_->SetRoundedCorners(kContentCornerRadius);
  }
  // Mini toolbar should only be visible for the inactive contents
  // container view or both depending on configuration.
  mini_toolbar_->UpdateState(is_active);
  // Scrim should only be allowed to show the scrim for inactive contents
  // container view.
  inactive_split_scrim_view_->SetVisible(!is_active && show_scrim);
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

  CHECK(contents_scrim_view_);
  layouts.child_layouts.emplace_back(contents_scrim_view_.get(),
                                     contents_scrim_view_->GetVisible(),
                                     contents_rect);

  // The scrim view should cover and take up the same space as the contents
  // view.
  if (inactive_split_scrim_view_) {
    layouts.child_layouts.emplace_back(inactive_split_scrim_view_.get(),
                                       inactive_split_scrim_view_->GetVisible(),
                                       contents_rect);
  }

  if (mini_toolbar_) {
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
  }

  layouts.host_size = gfx::Size(width, height);
  return layouts;
}

BEGIN_METADATA(ContentsContainerView)
END_METADATA
