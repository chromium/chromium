// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/contents_container_view.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/contents_separator.h"
#include "chrome/browser/ui/views/frame/contents_web_view.h"
#include "chrome/browser/ui/views/frame/multi_contents_view_mini_toolbar.h"
#include "chrome/browser/ui/views/frame/scrim_view.h"
#include "chrome/browser/ui/views/new_tab_footer/footer_web_view.h"
#include "chrome/common/chrome_features.h"
#include "components/search/ntp_features.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/border.h"
#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/view_class_properties.h"

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/browser_ui/glic_border_view.h"
#include "chrome/browser/glic/glic_enabling.h"
#endif

namespace {
constexpr float kContentCornerRadius = 6;
constexpr gfx::RoundedCornersF kContentRoundedCorners{kContentCornerRadius};

constexpr int kContentOutlineCornerRadius = 8;
constexpr int kContentOutlineThickness = 1;
constexpr int kSplitViewContentPadding = 4;

constexpr int kNewTabFooterSeparatorHeight = 1;
constexpr int kNewTabFooterHeight = 56;
}  // namespace

ContentsContainerView::ContentsContainerView(BrowserView* browser_view) {
  SetLayoutManager(std::make_unique<views::DelegatingLayoutManager>(this));

  // The default z-order is the order in which children were added to the
  // parent view. So first added the content view and new tab page footer.
  // This should be followed by scrims, borders and lastly mini-toolbar.

  contents_view_ = AddChildView(
      std::make_unique<ContentsWebView>(browser_view->GetProfile()));
  contents_view_->SetID(VIEW_ID_TAB_CONTAINER);

  if (base::FeatureList::IsEnabled(ntp_features::kNtpFooter)) {
    new_tab_footer_view_separator_ =
        AddChildView(std::make_unique<ContentsSeparator>());
    new_tab_footer_view_separator_->SetProperty(
        views::kElementIdentifierKey, kFooterWebViewSeparatorElementId);

    new_tab_footer_view_ =
        AddChildView(std::make_unique<new_tab_footer::NewTabFooterWebView>(
            browser_view->browser()));
    new_tab_footer_view_->SetVisible(false);
  }

  contents_scrim_view_ = AddChildView(std::make_unique<ScrimView>());
  contents_scrim_view_->layer()->SetName("ContentsScrimView");

  if (base::FeatureList::IsEnabled(features::kSideBySide)) {
    inactive_split_scrim_view_ =
        AddChildView(std::make_unique<ScrimView>(kColorSplitViewScrim));
    inactive_split_scrim_view_->SetRoundedCorners(kContentRoundedCorners);
  }

  if (features::kGlicActorUiOverlay.Get()) {
    auto actor_overlay_view = std::make_unique<views::View>();
    actor_overlay_view->SetID(VIEW_ID_ACTOR_OVERLAY);
    actor_overlay_view->SetVisible(false);
    actor_overlay_view->SetLayoutManager(std::make_unique<views::FillLayout>());
    actor_overlay_view_ = AddChildView(std::move(actor_overlay_view));
  }

#if BUILDFLAG(ENABLE_GLIC)
  if (glic::GlicEnabling::IsProfileEligible(browser_view->GetProfile())) {
    glic_border_ =
        AddChildView(views::Builder<glic::GlicBorderView>(
                         glic::GlicBorderView::Factory::Create(
                             browser_view->browser(), contents_view_))
                         .SetVisible(false)
                         .SetCanProcessEventsWithinSubtree(false)
                         .Build());
  }
#endif

  if (base::FeatureList::IsEnabled(features::kSideBySide)) {
    mini_toolbar_ = AddChildView(std::make_unique<MultiContentsViewMiniToolbar>(
        browser_view, contents_view_));
  }
}

void ContentsContainerView::UpdateBorderAndOverlay(bool is_in_split,
                                                   bool is_active,
                                                   bool show_scrim) {
  is_in_split_ = is_in_split;
  // The border, mini toolbar, and scrim should not be visible if not in a
  // split.
  if (!is_in_split) {
    SetBorder(nullptr);
    ClearBorderRoundedCorners();
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

  UpdateBorderRoundedCorners();

  // Mini toolbar should only be visible for the inactive contents
  // container view or both depending on configuration.
  mini_toolbar_->UpdateState(is_active);
  // Scrim should only be allowed to show the scrim for inactive contents
  // container view.
  inactive_split_scrim_view_->SetVisible(!is_active && show_scrim);
}

void ContentsContainerView::UpdateBorderRoundedCorners() {
  constexpr gfx::RoundedCornersF kContentUpperRoundedCorners =
      gfx::RoundedCornersF{kContentCornerRadius, kContentCornerRadius, 0, 0};
  constexpr gfx::RoundedCornersF kContentLowerRoundedCorners =
      gfx::RoundedCornersF{0, 0, kContentCornerRadius, kContentCornerRadius};

  auto radii = new_tab_footer_view_ && new_tab_footer_view_->GetVisible()
                   ? kContentUpperRoundedCorners
                   : kContentRoundedCorners;

  contents_view_->holder()->SetCornerRadii(radii);

  if (new_tab_footer_view_) {
    new_tab_footer_view_->holder()->SetCornerRadii(kContentLowerRoundedCorners);
  }

  if (contents_scrim_view_->layer()->rounded_corner_radii() !=
      kContentRoundedCorners) {
    contents_scrim_view_->SetRoundedCorners(kContentRoundedCorners);
  }
}

void ContentsContainerView::ClearBorderRoundedCorners() {
  constexpr gfx::RoundedCornersF kNoRoundedCorners = gfx::RoundedCornersF{0};

  contents_view_->holder()->SetCornerRadii(kNoRoundedCorners);

  if (new_tab_footer_view_) {
    new_tab_footer_view_->holder()->SetCornerRadii(kNoRoundedCorners);
  }

  contents_scrim_view_->SetRoundedCorners(kNoRoundedCorners);
}

void ContentsContainerView::ChildVisibilityChanged(View* child) {
  if (child == new_tab_footer_view_ && is_in_split_) {
    UpdateBorderRoundedCorners();
  }
}

views::ProposedLayout ContentsContainerView::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  views::ProposedLayout layouts;
  if (!size_bounds.is_fully_bounded()) {
    return layouts;
  }

  int height = size_bounds.height().value();
  int width = size_bounds.width().value();

  // |contents_view_| and |new_tab_footer_view_| (if it exists) should fill the
  // contents bounds.
  gfx::Rect contents_bounds = GetContentsBounds();
  gfx::Rect contents_rect = contents_bounds;

  if (new_tab_footer_view_ && new_tab_footer_view_->GetVisible()) {
    // Shrink contents rect if the ntp footer is visible.
    contents_rect.set_height(contents_rect.height() - kNewTabFooterHeight -
                             kNewTabFooterSeparatorHeight);

    gfx::Rect footer_separator_rect =
        gfx::Rect(contents_bounds.x(), contents_rect.bottom(),
                  contents_bounds.width(), kNewTabFooterSeparatorHeight);
    gfx::Rect footer_rect =
        gfx::Rect(contents_bounds.x(), footer_separator_rect.bottom(),
                  contents_bounds.width(), kNewTabFooterHeight);

    layouts.child_layouts.emplace_back(
        new_tab_footer_view_separator_.get(),
        new_tab_footer_view_separator_->GetVisible(), footer_separator_rect);

    layouts.child_layouts.emplace_back(new_tab_footer_view_.get(),
                                       new_tab_footer_view_->GetVisible(),
                                       footer_rect);
  }

  layouts.child_layouts.emplace_back(
      contents_view_.get(), contents_view_->GetVisible(), contents_rect);

#if BUILDFLAG(ENABLE_GLIC)
  if (glic_border_) {
    layouts.child_layouts.emplace_back(
        glic_border_.get(), glic_border_->GetVisible(), contents_bounds);
  }
#endif

  // The scrim view should cover the entire contents bounds.
  CHECK(contents_scrim_view_);
  layouts.child_layouts.emplace_back(contents_scrim_view_.get(),
                                     contents_scrim_view_->GetVisible(),
                                     contents_bounds);

  // The scrim view should cover the entire contents bounds.
  if (inactive_split_scrim_view_) {
    layouts.child_layouts.emplace_back(inactive_split_scrim_view_.get(),
                                       inactive_split_scrim_view_->GetVisible(),
                                       contents_bounds);
  }

  // Actor Overlay view bounds are the same as the contents view.
  if (actor_overlay_view_) {
    layouts.child_layouts.emplace_back(actor_overlay_view_.get(),
                                       actor_overlay_view_->GetVisible(),
                                       contents_rect, size_bounds);
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
