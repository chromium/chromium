// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/shadow_overlay_view.h"

#include <memory>

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/themed_background.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_panel/side_panel_animation_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_animation_ids.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPathBuilder.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/compositor_extra/shadow.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_shadow.h"

// Implements the opaque corners that overlay the main area of the browser and
// sync with the shadow box.
class ShadowOverlayView::CornerView : public views::View {
  METADATA_HEADER(CornerView, views::View)
 public:
  enum class Corner {
    kTopLeading,
    kTopTrailing,
    kBottomLeading,
    kBottomTrailing
  };

  // Because of subpixel rounding issues between the overlay and the content
  // pane underneath, the corners are drawn starting slightly outside the bounds
  // of the overlay. They will render correctly by virtue of being on a layer.
  static constexpr int kCornerOutset = 1;

  // Additional amount to overpaint the border to prevent subpixel issues with
  // antialiasing and alignment between webcontents and corners.
  static constexpr float kCornerSubpixelOverpaint = 0.5f;

  CornerView(Corner corner, BrowserView& browser_view) : corner_(corner) {
    SetBackground(std::make_unique<ThemedBackground>(&browser_view));
  }
  ~CornerView() override = default;

  // views::View:
  void Layout(PassKey) override {
    LayoutSuperclass<views::View>(this);
    SetClipPath(GetClipPath());
  }

  void OnThemeChanged() override {
    views::View::OnThemeChanged();
    SchedulePaint();
  }

 private:
  // Returns the clip path for the corner.
  //
  // The contents need to be drawn by `ThemedBackground` to ensure that
  // the correct content is drawn in all themes (including themes that e.g. use
  // an image background). However, the corner shape still needs to be drawn; in
  // order to ensure that only the opaque portion of the corner is painted a
  // clip mask is used.
  //
  // The shape of the mask is roughly:
  //
  //      ├─────┤
  //    ┏━━━━━━━┱─┐
  //  ┬ ┃       ┃ │
  //  │ ┃   ╭━━━┛ │
  //  │ ┃   ┃     │
  //  ┴ ┡━━━┛     │
  //    └─────────┘
  // ...where the area of the box extends `kCornerOutset` beyond the basic
  // curve shape in each direction.
  SkPath GetClipPath() const {
    gfx::Rect visible_area = GetLocalBounds();
    visible_area.Inset(kCornerOutset);
    gfx::RectF clip_area = gfx::RectF(GetLocalBounds());
    clip_area.Outset(kCornerSubpixelOverpaint);

    SkPathBuilder path;
    switch (corner_) {
      case Corner::kTopLeading:
        path.moveTo(clip_area.x(), clip_area.y());
        path.lineTo(visible_area.right(), clip_area.y());
        path.lineTo(visible_area.right(), visible_area.y());
        path.arcTo(SkVector(visible_area.width(), visible_area.height()), 0,
                   SkPathBuilder::kSmall_ArcSize, SkPathDirection::kCCW,
                   SkPoint(visible_area.x(), visible_area.bottom()));
        path.lineTo(clip_area.x(), visible_area.bottom());
        break;
      case Corner::kTopTrailing:
        path.moveTo(clip_area.right(), clip_area.y());
        path.lineTo(clip_area.right(), visible_area.bottom());
        path.lineTo(visible_area.right(), visible_area.bottom());
        path.arcTo(SkVector(visible_area.width(), visible_area.height()), 0,
                   SkPathBuilder::kSmall_ArcSize, SkPathDirection::kCCW,
                   SkPoint(visible_area.x(), visible_area.y()));
        path.lineTo(visible_area.x(), clip_area.y());
        break;
      case Corner::kBottomLeading:
        path.moveTo(clip_area.x(), clip_area.bottom());
        path.lineTo(clip_area.x(), visible_area.y());
        path.lineTo(visible_area.x(), visible_area.y());
        path.arcTo(SkVector(visible_area.width(), visible_area.height()), 0,
                   SkPathBuilder::kSmall_ArcSize, SkPathDirection::kCCW,
                   SkPoint(visible_area.right(), visible_area.bottom()));
        path.lineTo(visible_area.right(), clip_area.bottom());
        break;
      case Corner::kBottomTrailing:
        path.moveTo(clip_area.right(), clip_area.bottom());
        path.lineTo(visible_area.x(), clip_area.bottom());
        path.lineTo(visible_area.x(), visible_area.bottom());
        path.arcTo(SkVector(visible_area.width(), visible_area.height()), 0,
                   SkPathBuilder::kSmall_ArcSize, SkPathDirection::kCCW,
                   SkPoint(visible_area.right(), visible_area.y()));
        path.lineTo(clip_area.right(), visible_area.y());
        break;
    }

    path.close();
    return path.detach();
  }

  const Corner corner_;
};

using CornerView = ShadowOverlayView::CornerView;

BEGIN_METADATA(CornerView)
END_METADATA

// Implements the shadow box that surrounds the main area of the browser.
class ShadowOverlayView::ShadowBox : public views::View {
  METADATA_HEADER(ShadowBox, views::View)

 public:
  static constexpr int kShadowElevation = 4;

  ShadowBox() { SetCanProcessEventsWithinSubtree(false); }
  ~ShadowBox() override = default;

  void SetShadowVisible(bool visible) {
    // No-op if visible set is the same as current state.
    if ((visible && layer()) || (!visible && !layer())) {
      return;
    }

    if (visible) {
      const int rounded_corner_radius =
          GetLayoutProvider()->GetCornerRadiusMetric(views::Emphasis::kHigh);
      const int elevation =
          GetLayoutProvider()->GetShadowElevationMetric(views::Emphasis::kHigh);

      view_shadow_ = std::make_unique<views::ViewShadow>(this, elevation);
      view_shadow_->SetRoundedCornerRadius(rounded_corner_radius);
      view_shadow_->shadow()->SetElevation(kShadowElevation);
      UpdateShadowColors();
    } else {
      view_shadow_.reset();
      DestroyLayer();
      was_dark_ = std::nullopt;
    }
  }

  void SetShadowOpacity(double opacity) {
    if (!view_shadow_) {
      return;
    }

    view_shadow_->shadow()->shadow_layer()->SetOpacity(opacity);
  }

  // views::View:
  void OnThemeChanged() override {
    views::View::OnThemeChanged();
    if (view_shadow_) {
      UpdateShadowColors();
    }
  }

 private:
  void UpdateShadowColors() {
    // These are the UX targets:
    // light: 0 0 4px 0 rgba(0, 0, 0, 0.10),
    //        0 2px 6px 0 rgba(0, 0, 0, 0.17)
    // dark:  0 0 4px 0 rgba(0, 0, 0, 0.20),
    //        0 2px 6px 0 rgba(0, 0, 0, 0.40)

    // These are the defaults for the MD shadow at 4 height.
    // box-shadow: 0 0 4px rgba(0, 0, 0, .12),
    //             0 4px 8px rgba(0, 0, 0, .24)

    // This is an attempt to approximate the target values with only color.
    static constexpr std::array<std::pair<SkColor, SkColor>, 2> kShadowColors{
        std::make_pair(SkColorSetARGB(26, 0, 0, 0),
                       SkColorSetARGB(43, 0, 0, 0)),
        std::make_pair(SkColorSetARGB(51, 0, 0, 0),
                       SkColorSetARGB(102, 0, 0, 0))};

    const bool is_dark =
        color_utils::IsDark(GetColorProvider()->GetColor(kColorToolbar));
    if (was_dark_ == is_dark) {
      return;
    }
    was_dark_ = is_dark;
    ui::Shadow::ElevationToColorsMap map;
    map.emplace(kShadowElevation, kShadowColors[is_dark ? 1 : 0]);
    view_shadow_->shadow()->SetElevationToColorsMap(map);
    SchedulePaint();
  }

  // The shadow and elevation around main_container to visually separate the
  // container from MainRegionBackground when the toolbar_height_side_panel is
  // visible.
  std::unique_ptr<views::ViewShadow> view_shadow_;
  std::optional<bool> was_dark_;
};

using ShadowBox = ShadowOverlayView::ShadowBox;

BEGIN_METADATA(ShadowBox)
END_METADATA

ShadowOverlayView::ShadowOverlayView(BrowserView& browser_view)
    : browser_view_(browser_view) {
  SetCanProcessEventsWithinSubtree(false);
  top_leading_corner_ = AddChildView(std::make_unique<CornerView>(
      CornerView::Corner::kTopLeading, browser_view));
  top_trailing_corner_ = AddChildView(std::make_unique<CornerView>(
      CornerView::Corner::kTopTrailing, browser_view));
  bottom_leading_corner_ = AddChildView(std::make_unique<CornerView>(
      CornerView::Corner::kBottomLeading, browser_view));
  bottom_trailing_corner_ = AddChildView(std::make_unique<CornerView>(
      CornerView::Corner::kBottomTrailing, browser_view));
  shadow_box_ = AddChildView(std::make_unique<ShadowBox>());

  SetLayoutManager(std::make_unique<views::DelegatingLayoutManager>(this));

  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  // Starts hidden; visibility set by layout.
  SetVisible(false);
}

ShadowOverlayView::~ShadowOverlayView() = default;

void ShadowOverlayView::VisibilityChanged(View* starting_from, bool visible) {
  if (starting_from == this) {
    shadow_box_->SetShadowVisible(visible);

    // Ensure the opacity matches the current animation value in cases where the
    // panel should not animate but is open such as swapping between tabs.
    if (side_panel_observer_.IsObserving()) {
      shadow_box_->SetShadowOpacity(
          side_panel_observer_.GetSource()
              ->animation_coordinator()
              ->GetAnimationValueFor(kShadowOverlayOpacityAnimation));
    }
  }
}

void ShadowOverlayView::AddedToWidget() {
  side_panel_observer_.Observe(browser_view_->toolbar_height_side_panel());
  side_panel_observer_.GetSource()->animation_coordinator()->AddObserver(
      kShadowOverlayOpacityAnimation, this);
}

void ShadowOverlayView::RemovedFromWidget() {
  if (side_panel_observer_.IsObserving()) {
    side_panel_observer_.GetSource()->animation_coordinator()->RemoveObserver(
        kShadowOverlayOpacityAnimation, this);
    side_panel_observer_.Reset();
  }
}

void ShadowOverlayView::OnViewIsDeleting(views::View* observed_view) {
  CHECK(observed_view == side_panel_observer_.GetSource());

  side_panel_observer_.GetSource()->animation_coordinator()->RemoveObserver(
      kShadowOverlayOpacityAnimation, this);
  side_panel_observer_.Reset();
}

views::ProposedLayout ShadowOverlayView::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  const int corner_radius =
      GetLayoutProvider()->GetCornerRadiusMetric(views::Emphasis::kHigh);

  views::ProposedLayout layout;
  layout.host_size = gfx::Size(size_bounds.width().value_or(corner_radius),
                               size_bounds.height().value_or(corner_radius));

  views::ChildLayout shadow;
  shadow.child_view = shadow_box_;
  shadow.bounds = gfx::Rect({}, layout.host_size);
  shadow.visible = true;
  layout.child_layouts.push_back(shadow);

  views::ChildLayout top_leading;
  top_leading.child_view = top_leading_corner_;
  top_leading.bounds = gfx::Rect(0, 0, corner_radius, corner_radius);
  top_leading.bounds.Outset(CornerView::kCornerOutset);
  top_leading.visible = true;
  layout.child_layouts.push_back(top_leading);

  views::ChildLayout top_trailing;
  top_trailing.child_view = top_trailing_corner_;
  top_trailing.bounds = gfx::Rect(layout.host_size.width() - corner_radius, 0,
                                  corner_radius, corner_radius);
  top_trailing.bounds.Outset(CornerView::kCornerOutset);
  top_trailing.visible = true;
  layout.child_layouts.push_back(top_trailing);

  views::ChildLayout bottom_leading;
  bottom_leading.child_view = bottom_leading_corner_;
  bottom_leading.bounds =
      gfx::Rect(0, layout.host_size.height() - corner_radius, corner_radius,
                corner_radius);
  bottom_leading.bounds.Outset(CornerView::kCornerOutset);
  bottom_leading.visible = true;
  layout.child_layouts.push_back(bottom_leading);

  views::ChildLayout bottom_trailing;
  bottom_trailing.child_view = bottom_trailing_corner_;
  bottom_trailing.bounds = gfx::Rect(layout.host_size.width() - corner_radius,
                                     layout.host_size.height() - corner_radius,
                                     corner_radius, corner_radius);
  bottom_trailing.bounds.Outset(CornerView::kCornerOutset);
  bottom_trailing.visible = true;
  layout.child_layouts.push_back(bottom_trailing);

  return layout;
}

void ShadowOverlayView::OnAnimationSequenceProgressed(
    const SidePanelAnimationCoordinator::SidePanelAnimationId& animation_id,
    double animation_value) {
  CHECK_EQ(kShadowOverlayOpacityAnimation, animation_id);

  shadow_box_->SetShadowOpacity(animation_value);
}

void ShadowOverlayView::OnAnimationSequenceEnded(
    const SidePanelAnimationCoordinator::SidePanelAnimationId& animation_id) {
  // When the animation ends, set the final opacity based on whether the side
  // panel is closing or opening.
  const double ending_opacity =
      side_panel_observer_.GetSource()->animation_coordinator()->IsClosing()
          ? 0.0f
          : 1.0f;
  shadow_box_->SetShadowOpacity(ending_opacity);
}

BEGIN_METADATA(ShadowOverlayView)
END_METADATA
