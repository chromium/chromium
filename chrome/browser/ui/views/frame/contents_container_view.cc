// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/contents_container_view.h"

#include <memory>

#include "chrome/browser/devtools/devtools_contents_resizing_strategy.h"
#include "chrome/browser/enterprise/watermark/watermark_view.h"
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
#include "content/public/browser/web_contents.h"
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
  SetProperty(views::kElementIdentifierKey, kContentsContainerViewElementId);

  // The default z-order is the order in which children were added to the
  // parent view. So first added devtools and the devtools scrim view (as it
  // exists behind the content view), then the content view and new tab page
  // footer. This should be followed by scrims, borders and lastly mini-toolbar.

  auto devtools_web_view =
      std::make_unique<views::WebView>(browser_view->GetProfile());
  devtools_web_view->SetID(VIEW_ID_DEV_TOOLS_DOCKED);
  devtools_web_view->SetVisible(false);
  devtools_web_view_ = AddChildView(std::move(devtools_web_view));

  devtools_scrim_view_ = AddChildView(std::make_unique<ScrimView>());
  devtools_scrim_view_->layer()->SetName("DevtoolsScrimView");

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

  watermark_view_ =
      AddChildView(std::make_unique<enterprise_watermark::WatermarkView>());

  contents_scrim_view_ = AddChildView(std::make_unique<ScrimView>());
  contents_scrim_view_->layer()->SetName("ContentsScrimView");

  if (base::FeatureList::IsEnabled(features::kSideBySide)) {
    inactive_split_scrim_view_ =
        AddChildView(std::make_unique<ScrimView>(kColorSplitViewScrim));
    inactive_split_scrim_view_->SetProperty(views::kElementIdentifierKey,
                                            kInactiveSplitScrimViewElementId);
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
    view_bounds_observer_.Observe(contents_view_);
  }
}

ContentsContainerView::~ContentsContainerView() = default;

std::vector<views::View*> ContentsContainerView::GetAccessiblePanes() {
  std::vector<views::View*> accessible_panes;
  if (contents_view_->GetVisible()) {
    accessible_panes.push_back(contents_view_);
  }
  if (devtools_web_view_->GetVisible()) {
    accessible_panes.push_back(devtools_web_view_);
  }
  if (devtools_scrim_view_->GetVisible()) {
    accessible_panes.push_back(devtools_scrim_view_);
  }
  return accessible_panes;
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
  // Update devtools rounded corners. Note, devtools exists behind the contents
  // view so all devtools corners are rounded.
  devtools_web_view_->holder()->SetCornerRadii(kContentRoundedCorners);
  devtools_scrim_view_->SetRoundedCorners(kContentRoundedCorners);

  const bool devtools_in_upper_left =
      devtools_web_view_->GetVisible() &&
      current_devtools_docked_placement_ == DevToolsDockedPlacement::kLeft;
  const bool devtools_in_upper_right =
      devtools_web_view_->GetVisible() &&
      current_devtools_docked_placement_ == DevToolsDockedPlacement::kRight;
  const bool devtools_in_lower_left =
      devtools_web_view_->GetVisible() &&
      (current_devtools_docked_placement_ == DevToolsDockedPlacement::kBottom ||
       current_devtools_docked_placement_ == DevToolsDockedPlacement::kLeft);
  const bool devtools_in_lower_right =
      devtools_web_view_->GetVisible() &&
      (current_devtools_docked_placement_ == DevToolsDockedPlacement::kBottom ||
       current_devtools_docked_placement_ == DevToolsDockedPlacement::kRight);

  const gfx::RoundedCornersF content_upper_rounded_corners =
      gfx::RoundedCornersF{devtools_in_upper_left ? 0 : kContentCornerRadius,
                           devtools_in_upper_right ? 0 : kContentCornerRadius,
                           0, 0};
  const gfx::RoundedCornersF content_lower_rounded_corners =
      gfx::RoundedCornersF{0, 0,
                           devtools_in_lower_right ? 0 : kContentCornerRadius,
                           devtools_in_lower_left ? 0 : kContentCornerRadius};
  const gfx::RoundedCornersF content_rounded_corners =
      gfx::RoundedCornersF{devtools_in_upper_left ? 0 : kContentCornerRadius,
                           devtools_in_upper_right ? 0 : kContentCornerRadius,
                           devtools_in_lower_right ? 0 : kContentCornerRadius,
                           devtools_in_lower_left ? 0 : kContentCornerRadius};

  auto radii = new_tab_footer_view_ && new_tab_footer_view_->GetVisible()
                   ? content_upper_rounded_corners
                   : content_rounded_corners;

  contents_view_->holder()->SetCornerRadii(radii);

  if (new_tab_footer_view_) {
    new_tab_footer_view_->holder()->SetCornerRadii(
        content_lower_rounded_corners);
  }

  if (contents_scrim_view_->layer()->rounded_corner_radii() !=
      kContentRoundedCorners) {
    contents_scrim_view_->SetRoundedCorners(kContentRoundedCorners);
  }

#if BUILDFLAG(ENABLE_GLIC)
  if (glic_border_) {
    glic_border_->SetRoundedCorners(content_rounded_corners);
  }
#endif
}

void ContentsContainerView::ClearBorderRoundedCorners() {
  constexpr gfx::RoundedCornersF kNoRoundedCorners = gfx::RoundedCornersF{0};

  devtools_web_view_->holder()->SetCornerRadii(kNoRoundedCorners);
  devtools_scrim_view_->SetRoundedCorners(kNoRoundedCorners);

  contents_view_->holder()->SetCornerRadii(kNoRoundedCorners);

  if (new_tab_footer_view_) {
    new_tab_footer_view_->holder()->SetCornerRadii(kNoRoundedCorners);
  }

  contents_scrim_view_->SetRoundedCorners(kNoRoundedCorners);

#if BUILDFLAG(ENABLE_GLIC)
  if (glic_border_) {
    glic_border_->SetRoundedCorners(kNoRoundedCorners);
  }
#endif
}

void ContentsContainerView::ChildVisibilityChanged(View* child) {
  if ((child == new_tab_footer_view_ || child == devtools_web_view_) &&
      is_in_split_) {
    UpdateBorderRoundedCorners();
  }
}

void ContentsContainerView::OnViewBoundsChanged(View* observed_view) {
  if (observed_view == contents_view_) {
    UpdateDevToolsDockedPlacement();
    if (is_in_split_) {
      UpdateBorderRoundedCorners();
    }
  }
}

void ContentsContainerView::SetContentsResizingStrategy(
    const DevToolsContentsResizingStrategy& strategy) {
  if (strategy_.Equals(strategy)) {
    return;
  }

  strategy_.CopyFrom(strategy);
  InvalidateLayout();
}

void ContentsContainerView::ApplyWatermarkSettings(
    const std::string& watermark_text,
    SkColor fill_color,
    SkColor outline_color,
    int font_size) {
  watermark_view_->SetString(watermark_text, fill_color, outline_color,
                             font_size);
}

void ContentsContainerView::UpdateDevToolsDockedPlacement() {
  DevToolsDockedPlacement placement = DevToolsDockedPlacement::kUnknown;
  gfx::Rect contents_view_bounds = contents_view_->bounds();
  // Include ntp footer bounds so that we don't mistakenly believe devtools is
  // bottom docked when the footer is showing.
  if (new_tab_footer_view_ && new_tab_footer_view_->GetVisible()) {
    CHECK(new_tab_footer_view_separator_);
    contents_view_bounds.set_height(contents_view_bounds.height() +
                                    new_tab_footer_view_->height() +
                                    new_tab_footer_view_separator_->height());
  }
  const gfx::Rect& container_bounds = GetContentsBounds();
  // If contents_webview has the same bounds as webview_container, it either
  // means that devtools are not open or devtools are open in a separate
  // window (not docked).
  if (contents_view_bounds == container_bounds) {
    placement = DevToolsDockedPlacement::kNone;
  } else if (contents_view_bounds.x() > container_bounds.x() &&
             contents_view_bounds.y() == container_bounds.y() &&
             contents_view_bounds.height() == container_bounds.height()) {
    placement = DevToolsDockedPlacement::kLeft;
  } else if (contents_view_bounds.origin() == container_bounds.origin() &&
             contents_view_bounds.height() == container_bounds.height()) {
    placement = DevToolsDockedPlacement::kRight;
  } else if (contents_view_bounds.origin() == container_bounds.origin() &&
             contents_view_bounds.width() == container_bounds.width()) {
    placement = DevToolsDockedPlacement::kBottom;
  }

  // When browser window is resizing, the contents_container and web_contents
  // bounds can be out of sync, resulting in a state, where it is impossible to
  // infer docked placement based on contents webview bounds. In this case, use
  // the last known docked placement, since resizing a window does not change
  // the devtools dock placement.
  if (placement != DevToolsDockedPlacement::kUnknown) {
    current_devtools_docked_placement_ = placement;
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

  gfx::Rect full_contents_bounds = GetContentsBounds();
  gfx::Rect devtools_bounds;
  // The area contents excluding devtools is drawn (ie |contents_view_|,
  // |new_tab_footer_view_|, etc).
  gfx::Rect non_devtools_contents_bounds;

  ApplyDevToolsContentsResizingStrategy(strategy_, full_contents_bounds,
                                        &devtools_bounds,
                                        &non_devtools_contents_bounds);
  gfx::Rect contents_view_bounds = non_devtools_contents_bounds;

  // DevTools cares about the specific position, so we have to compensate RTL
  // layout here.
  layouts.child_layouts.emplace_back(
      devtools_web_view_.get(), devtools_web_view_->GetVisible(),
      GetMirroredRect(devtools_bounds),
      views::SizeBounds(full_contents_bounds.size()));
  layouts.child_layouts.emplace_back(
      devtools_scrim_view_.get(), devtools_scrim_view_->GetVisible(),
      GetMirroredRect(devtools_bounds),
      views::SizeBounds(full_contents_bounds.size()));

  if (new_tab_footer_view_ && new_tab_footer_view_->GetVisible()) {
    // Shrink the rect for the contents view if the ntp footer is visible.
    contents_view_bounds.set_height(non_devtools_contents_bounds.height() -
                                    kNewTabFooterHeight -
                                    kNewTabFooterSeparatorHeight);

    gfx::Rect footer_separator_rect =
        gfx::Rect(contents_view_bounds.x(), contents_view_bounds.bottom(),
                  contents_view_bounds.width(), kNewTabFooterSeparatorHeight);
    gfx::Rect footer_rect =
        gfx::Rect(contents_view_bounds.x(), contents_view_bounds.bottom(),
                  contents_view_bounds.width(), kNewTabFooterHeight);

    layouts.child_layouts.emplace_back(
        new_tab_footer_view_separator_.get(),
        new_tab_footer_view_separator_->GetVisible(), footer_separator_rect);

    layouts.child_layouts.emplace_back(new_tab_footer_view_.get(),
                                       new_tab_footer_view_->GetVisible(),
                                       footer_rect);
  }

  const auto& contents_rect = GetMirroredRect(contents_view_bounds);
  layouts.child_layouts.emplace_back(
      contents_view_.get(), contents_view_->GetVisible(), contents_rect);

#if BUILDFLAG(ENABLE_GLIC)
  if (glic_border_) {
    // |glic_border_| should not be seen over devtools.
    layouts.child_layouts.emplace_back(glic_border_.get(),
                                       glic_border_->GetVisible(),
                                       non_devtools_contents_bounds);
  }
#endif

  // The content scrim view should cover the entire contents bounds.
  CHECK(contents_scrim_view_);
  layouts.child_layouts.emplace_back(contents_scrim_view_.get(),
                                     contents_scrim_view_->GetVisible(),
                                     full_contents_bounds);

  CHECK(watermark_view_);
  layouts.child_layouts.emplace_back(watermark_view_.get(),
                                     watermark_view_->GetVisible(),
                                     full_contents_bounds);

  // The inactive split scrim view should cover the entire contents bounds
  // including over devtools and other views.
  if (inactive_split_scrim_view_) {
    layouts.child_layouts.emplace_back(inactive_split_scrim_view_.get(),
                                       inactive_split_scrim_view_->GetVisible(),
                                       full_contents_bounds);
  }

  // Actor Overlay view bounds are the same as the contents view.
  if (actor_overlay_view_) {
    layouts.child_layouts.emplace_back(
        actor_overlay_view_.get(), actor_overlay_view_->GetVisible(),
        non_devtools_contents_bounds, size_bounds);
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
