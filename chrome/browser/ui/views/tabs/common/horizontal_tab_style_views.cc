// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/common/horizontal_tab_style_views.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "cc/paint/paint_flags.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/themed_background.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab/glow_hover_controller.h"
#include "chrome/browser/ui/views/tabs/tab_group_underline.h"
#include "chrome/grit/theme_resources.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPathBuilder.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/widget/widget.h"

HorizontalTabStyleViews::HorizontalTabStyleViews(
    std::unique_ptr<TabStyleViewDelegate> delegate)
    : delegate_(std::move(delegate)) {
  CHECK(delegate_);
  // TODO(dfried): create a new STYLE_PROMINENT or similar to use instead of
  // repurposing CONTEXT_BUTTON_MD.
}

HorizontalTabStyleViews::~HorizontalTabStyleViews() = default;

const TabStyleViewDelegate* HorizontalTabStyleViews::delegate() const {
  return delegate_.get();
}

GlowHoverController* HorizontalTabStyleViews::GetHoverControllerForTesting() {
  return delegate_->GetHoverControllerForTesting();  // IN-TEST
}

TabStyle::SeparatorOpacities
HorizontalTabStyleViews::GetSeparatorOpacitiesForTesting() const {  // IN-TEST
  return GetSeparatorOpacities(/*for_layout=*/false);
}

SkPath HorizontalTabStyleViews::GetPath(TabStyle::PathType path_type,
                                        float scale,
                                        const TabPathFlags& flags) const {
  if (delegate_->IsPinned() && !delegate_->IsActive() &&
      tabs::IsNewHorizontalPinnedTabStylingEnabled()) {
    return GetPinnedPath(path_type, scale, flags).value_or(SkPath());
  }

  const int stroke_thickness = GetStrokeThickness();

  // We'll do the entire path calculation in aligned pixels.
  // TODO(dfried): determine if we actually want to use `stroke_thickness` as
  // the inset in this case.
  gfx::RectF aligned_bounds = ScaleAndAlignBounds(
      delegate_->GetView()->bounds(), scale, stroke_thickness);

  // Calculate the corner radii. Note that corner radius is based on original
  // tab width (in DIP), not our new, scaled-and-aligned bounds.
  float content_corner_radius =
      GetTopCornerRadiusForWidth(delegate_->GetView()->width()) * scale;
  float extension_corner_radius = tab_style()->GetBottomCornerRadius() * scale;

  const float separator_overlap = (tab_style()->GetSeparatorMargins().width() +
                                   tab_style()->GetSeparatorSize().width()) *
                                  scale;

  // Selected, hover, and inactive tab fills are a detached squarcle tab.
  if (path_type == TabStyle::PathType::kHighlight ||
      path_type == TabStyle::PathType::kHitTest) {
    float top_left_corner_radius = content_corner_radius;
    float top_right_corner_radius = content_corner_radius;
    float bottom_left_corner_radius = content_corner_radius;
    float bottom_right_corner_radius = content_corner_radius;
    float tab_height = GetLayoutConstant(LayoutConstant::kTabHeight) * scale;

    // The hit test extends the full height to make it easier to select tabs.
    if (path_type != TabStyle::PathType::kHitTest) {
      tab_height -= GetLayoutConstant(LayoutConstant::kTabStripPadding) * scale;
      tab_height -=
          GetLayoutConstant(LayoutConstant::kTabstripToolbarOverlap) * scale;
    }

    // Don't round the bottom corners to avoid creating dead space between tabs.
    if (path_type == TabStyle::PathType::kHitTest) {
      bottom_left_corner_radius = 0;
      bottom_right_corner_radius = 0;
    }

    float left = aligned_bounds.x() + extension_corner_radius;
    float top = aligned_bounds.y() +
                GetLayoutConstant(LayoutConstant::kTabStripPadding) * scale;

    float right = aligned_bounds.right() - extension_corner_radius;
    const int bottom = top + tab_height;

    BrowserFrameView* const browser_frame_view =
        delegate_->GetBrowserFrameView();
    const bool is_frame_condensed =
        browser_frame_view ? browser_frame_view->IsFrameCondensed() : false;
    // For maximized and full screen windows, extend the tab hit test to the top
    // of the tab, encompassing the top padding. This makes it easy to click on
    // tabs by moving the mouse to the top of the screen.
    if (path_type == TabStyle::PathType::kHitTest && is_frame_condensed) {
      top -= GetLayoutConstant(LayoutConstant::kTabStripPadding) * scale;
      // Don't round the top corners to avoid creating dead space between tabs.
      top_left_corner_radius = 0;
      top_right_corner_radius = 0;
    }

    // While the tab is closing do not add extra space as it degrades the close
    // tab animation.
    if (!delegate_->IsClosing()) {
      // If the size of the space for the path is smaller than the size of a
      // favicon, if we are building a path for the hit test, or if we are
      // building a path for a split tab, expand to take the entire width of the
      // separator margins AND the separator.
      const bool limited_tab_space =
          (right - left) < (gfx::kFaviconSize * scale);
      const bool expand_into_left_separator =
          limited_tab_space || path_type == TabStyle::PathType::kHitTest ||
          delegate_->IsRightSplitTab();
      const bool expand_into_right_separator =
          limited_tab_space || path_type == TabStyle::PathType::kHitTest ||
          delegate_->IsLeftSplitTab();
      if (expand_into_left_separator) {
        left -= separator_overlap / 2.0;
      }
      if (expand_into_right_separator) {
        right += separator_overlap / 2.0;
      }
    }

    if (delegate_->IsSplit()) {
      if (delegate_->IsLeftSplitTab()) {
        top_right_corner_radius = 0;
        bottom_right_corner_radius = 0;
      } else if (delegate_->IsRightSplitTab()) {
        top_left_corner_radius = 0;
        bottom_left_corner_radius = 0;
      }
    }

    // Radii are clockwise from top left.
    const SkVector radii[4] = {
        SkVector(top_left_corner_radius, top_left_corner_radius),
        SkVector(top_right_corner_radius, top_right_corner_radius),
        SkVector(bottom_right_corner_radius, bottom_right_corner_radius),
        SkVector(bottom_left_corner_radius, bottom_left_corner_radius)};

    SkPathBuilder path;
    path.addRRect(SkRRect::MakeRectRadii(
        SkRect::MakeLTRB(left, top, right, bottom), radii));

    // Convert path to be relative to the tab origin.
    gfx::PointF origin(delegate_->GetView()->origin());
    origin.Scale(scale);
    path.offset(-origin.x(), -origin.y());

    // Possibly convert back to DIPs.
    if (flags.render_units == TabStyle::RenderUnits::kDips && scale != 1.0f) {
      path.transform(SkMatrix::Scale(1.0f / scale, 1.0f / scale));
    }

    return path.detach();
  }

  // Compute `extension` as the width outside the separators.  This is a fixed
  // value equal to the normal corner radius.
  const float extension = extension_corner_radius;
  float top_left_corner_radius = content_corner_radius;
  float top_right_corner_radius = content_corner_radius;

  // Calculate the bounds of the actual path.
  const float left = aligned_bounds.x();
  const float right = aligned_bounds.right();
  float tab_top = aligned_bounds.y() +
                  GetLayoutConstant(LayoutConstant::kTabStripPadding) * scale;
  float tab_left = left + extension;
  float tab_right = right - extension;

  // Overlap the toolbar below us so that gaps don't occur when rendering at
  // non-integral display scale factors.
  const float extended_bottom = aligned_bounds.bottom();
  const float bottom_extension =
      GetLayoutConstant(LayoutConstant::kTabstripToolbarOverlap) * scale;
  float tab_bottom = extended_bottom - bottom_extension;

  // Path-specific adjustments:
  const float stroke_adjustment = stroke_thickness * scale;
  if (path_type == TabStyle::PathType::kActiveTab ||
      path_type == TabStyle::PathType::kBorder) {
    if (!delegate_->IsRightSplitTab()) {
      tab_left += 0.5f * stroke_adjustment;
    }
    if (!delegate_->IsLeftSplitTab()) {
      tab_right -= 0.5f * stroke_adjustment;
    }
    tab_top += 0.5f * stroke_adjustment;
    content_corner_radius -= 0.5f * stroke_adjustment;
    tab_bottom -= 0.5f * stroke_adjustment;
    extension_corner_radius -= 0.5f * stroke_adjustment;
  }

  float left_extension_corner_radius = extension_corner_radius;
  if (delegate_->IsLeftSplitTab()) {
    top_right_corner_radius = 0;
    // Assign half of the tab overlap to each of the split tabs.
    tab_right = tab_right + extension - separator_overlap / 2.0;
    extension_corner_radius = 0;
  } else if (delegate_->IsRightSplitTab()) {
    top_left_corner_radius = 0;
    tab_left = tab_left - extension + separator_overlap / 2.0;
    left_extension_corner_radius = 0;
  }

  SkPathBuilder path;
  // Avoid mallocs at every new path verb by preallocating an
  // empirically-determined amount of space in the verb and point buffers.
  const int kMaxPathPoints = 20;
  path.incReserve(kMaxPathPoints);

  // We will go clockwise from the lower left. We start in the overlap region,
  // preventing a gap between toolbar and tabstrip.
  // TODO(dfried): verify that the we actually want to start the stroke for
  // the exterior path outside the region; we might end up rendering an
  // extraneous descending pixel on displays with odd scaling and nonzero
  // stroke width.

  if (path_type == TabStyle::PathType::kBorder && delegate_->IsSplit() &&
      !delegate_->IsLeftSplitTab()) {
    // Start with the top left side of the shape.
    path.moveTo(left, tab_top);
  } else {
    // Start with the left side of the shape.
    path.moveTo(left, extended_bottom);

    // Draw the left edge of the extension.
    //   ╭─────────╮
    //   │ Content │
    // ┏─╯         ╰─┐
    if (tab_bottom != extended_bottom) {
      if (flags.should_paint_extension) {
        path.lineTo(left, tab_bottom);
      } else {
        path.moveTo(left, tab_bottom);
      }
    }

    // Draw the bottom-left corner.
    //   ╭─────────╮
    //   │ Content │
    // ┌━╝         ╰─┐
    path.lineTo(tab_left - left_extension_corner_radius, tab_bottom);
    path.arcTo(
        SkVector(left_extension_corner_radius, left_extension_corner_radius), 0,
        SkPathBuilder::kSmall_ArcSize, SkPathDirection::kCCW,
        SkPoint(tab_left, tab_bottom - left_extension_corner_radius));

    // Draw the ascender and top-left curve.
    //   ╔─────────╮
    //   ┃ Content │
    // ┌─╯         ╰─┐
    path.lineTo(tab_left, tab_top + top_left_corner_radius);
    path.arcTo(SkVector(top_left_corner_radius, top_left_corner_radius), 0,
               SkPathBuilder::kSmall_ArcSize, SkPathDirection::kCW,
               SkPoint(tab_left + top_left_corner_radius, tab_top));
  }

  // Draw the top crossbar.
  //   ╭━━━━━━━━━╮
  //   │ Content │
  // ┌─╯         ╰─┐
  path.lineTo(tab_right - top_right_corner_radius, tab_top);

  if (path_type == TabStyle::PathType::kBorder && delegate_->IsSplit() &&
      !delegate_->IsRightSplitTab()) {
    // Finish to the top right corner.
    path.lineTo(right, tab_top);
  } else {
    // Draw the top-right curve.
    //   ╭─────────╗
    //   │ Content │
    // ┌─╯         ╰─┐
    path.arcTo(SkVector(top_right_corner_radius, top_right_corner_radius), 0,
               SkPathBuilder::kSmall_ArcSize, SkPathDirection::kCW,
               SkPoint(tab_right, tab_top + top_right_corner_radius));

    // Draw the descender and bottom-right corner.
    //   ╭─────────╮
    //   │ Content ┃
    // ┌─╯         ╚━┐
    path.lineTo(tab_right, tab_bottom - extension_corner_radius);
    path.arcTo(SkVector(extension_corner_radius, extension_corner_radius), 0,
               SkPathBuilder::kSmall_ArcSize, SkPathDirection::kCCW,
               SkPoint(tab_right + extension_corner_radius, tab_bottom));
    if (tab_bottom != extended_bottom) {
      path.lineTo(right, tab_bottom);
    }

    // Draw anything remaining: the descender, the bottom right horizontal
    // stroke, or the right edge of the extension, depending on which
    // conditions fired above.
    //   ╭─────────╮
    //   │ Content │
    // ┌─╯         ╰─┓
    if (flags.should_paint_extension) {
      path.lineTo(right, extended_bottom);
    }
  }

  if (path_type != TabStyle::PathType::kBorder) {
    path.close();
  }

  // Convert path to be relative to the tab origin.
  gfx::PointF origin(delegate_->GetView()->origin());
  origin.Scale(scale);
  path.offset(-origin.x(), -origin.y());

  // Possibly convert back to DIPs.
  if (flags.render_units == TabStyle::RenderUnits::kDips && scale != 1.0f) {
    path.transform(SkMatrix::Scale(1.0f / scale, 1.0f / scale));
  }

  return path.detach();
}

std::optional<SkPath> HorizontalTabStyleViews::GetChildClipPath(
    float paint_recording_scale) const {
  // Clip children based on the tab's fill path. This has no effect except when
  // the tab is too narrow to completely show even one icon, at which point this
  // serves to clip the favicon.
  return GetPath(TabStyle::PathType::kHighlight, paint_recording_scale, {});
}

void HorizontalTabStyleViews::PaintTab(gfx::Canvas* canvas) const {
  std::optional<int> active_tab_fill_id;
  if (delegate_->GetView()->GetThemeProvider()->HasCustomImage(
          IDR_THEME_TOOLBAR)) {
    active_tab_fill_id = IDR_THEME_TOOLBAR;
  }
  BrowserFrameView* const browser_frame_view = GetBrowserFrameView();
  const std::optional<int> inactive_tab_fill_id =
      browser_frame_view ? browser_frame_view->GetCustomBackgroundId(
                               BrowserFrameActiveState::kUseCurrent)
                         : std::nullopt;

  if (active_tab_fill_id.has_value() || inactive_tab_fill_id.has_value()) {
    PaintTabBackgroundWithImages(canvas, active_tab_fill_id,
                                 inactive_tab_fill_id);
  } else {
    PaintTabBackground(canvas, IsHoverAnimationActive(), std::nullopt);
  }
}

void HorizontalTabStyleViews::PaintTabBackgroundWithImages(
    gfx::Canvas* canvas,
    std::optional<int> active_tab_fill_id,
    std::optional<int> inactive_tab_fill_id) const {
  // When at least one of the active or inactive tab backgrounds have an image,
  // we must paint them with the previous method of layering the active and
  // inactive images with two paint calls.

  const TabStyle::TabSelectionState current_state = GetSelectionState();

  if (current_state == TabStyle::TabSelectionState::kActive) {
    PaintTabBackground(canvas,
                       /*hovered=*/false, active_tab_fill_id);
  } else {
    PaintTabBackground(canvas,
                       /*hovered=*/false, inactive_tab_fill_id);

    const float opacity = GetCurrentActiveOpacity();
    if (opacity > 0) {
      canvas->SaveLayerAlpha(base::ClampRound<uint8_t>(opacity * 0xff),
                             delegate_->GetView()->GetLocalBounds());
      PaintTabBackground(canvas,
                         /*hovered=*/false, active_tab_fill_id);
      canvas->Restore();
    }
  }
}

gfx::Insets HorizontalTabStyleViews::GetContentsInsets() const {
  gfx::Insets base_style_insets = tab_style()->GetContentsInsets();
  gfx::Insets split_insets = gfx::Insets(0);

  // For split tabs, remove insets equal to the total separator width between
  // them.
  const float total_separator_width =
      tab_style()->GetSeparatorMargins().left() +
      tab_style()->GetSeparatorSize().width() +
      tab_style()->GetSeparatorMargins().right();
  if (delegate_->IsRightSplitTab()) {
    split_insets.set_left(total_separator_width / -2);
  }
  if (delegate_->IsLeftSplitTab()) {
    split_insets.set_right(total_separator_width / -2);
  }

  gfx::Insets insets =
      gfx::Insets::TLBR(
          0, 0, GetLayoutConstant(LayoutConstant::kTabstripToolbarOverlap), 0) +
      base_style_insets + split_insets;

  return insets;
}

bool HorizontalTabStyleViews::IsApparentlyActive() const {
  const TabStyle::TabSelectionState selection_state = GetSelectionState();
  if (selection_state == TabStyle::TabSelectionState::kActive) {
    return true;
  }
  if (IsHovering()) {
    return GetHoverOpacity() > 0.5f;
  }
  return selection_state == TabStyle::TabSelectionState::kSelected;
}

float HorizontalTabStyleViews::GetCurrentActiveOpacity() const {
  const TabStyle::TabSelectionState selection_state = GetSelectionState();
  if (selection_state == TabStyle::TabSelectionState::kActive) {
    return 1.0f;
  }
  const float base_opacity =
      selection_state == TabStyle::TabSelectionState::kSelected
          ? tab_style()->GetSelectedTabOpacity()
          : 0.0f;
  if (!IsHoverAnimationActive()) {
    return base_opacity;
  }
  return std::lerp(base_opacity, GetHoverOpacity(), GetHoverAnimationValue());
}

TabStyle::TabColors HorizontalTabStyleViews::CalculateTargetColors() const {
  return tab_style()->CalculateTargetColors(
      GetSelectionState(), IsApparentlyActive(), IsHovering(),
      delegate_->GetView()->GetWidget()
          ? delegate_->GetView()->GetWidget()->ShouldPaintAsActive()
          : true,
      delegate_->GetView()->GetColorProvider());
}

TabStyle::SeparatorBounds HorizontalTabStyleViews::GetSeparatorBounds(
    float scale) const {
  const gfx::Rect original_bounds = delegate_->GetView()->bounds();
  // Factor out the amount of the tab strip that is overlapped by the toolbar.
  const gfx::Rect visible_bounds = gfx::Rect(
      original_bounds.x(), original_bounds.y(), original_bounds.width(),
      original_bounds.height() -
          GetLayoutConstant(LayoutConstant::kTabstripToolbarOverlap));
  const gfx::RectF aligned_bounds =
      ScaleAndAlignBounds(visible_bounds, scale, GetStrokeThickness());
  const int corner_radius = tab_style()->GetBottomCornerRadius() * scale;
  gfx::SizeF separator_size(tab_style()->GetSeparatorSize());
  separator_size.Scale(scale);
  gfx::InsetsF separator_margin =
      gfx::InsetsF(tab_style()->GetSeparatorMargins());
  separator_margin.Scale(scale);

  TabStyle::SeparatorBounds separator_bounds;

  const int extra_vertical_space =
      aligned_bounds.height() -
      (separator_size.height() + separator_margin.bottom() +
       separator_margin.top());

  separator_bounds.leading = gfx::RectF(
      aligned_bounds.x() + corner_radius - separator_margin.right() -
          separator_size.width(),
      aligned_bounds.y() + extra_vertical_space / 2 + separator_margin.top(),
      separator_size.width(), separator_size.height());

  separator_bounds.trailing = separator_bounds.leading;
  separator_bounds.trailing.set_x(aligned_bounds.right() - corner_radius +
                                  separator_margin.left());

  gfx::PointF origin(delegate_->GetView()->bounds().origin());
  origin.Scale(scale);
  separator_bounds.leading.Offset(-origin.x(), -origin.y());
  separator_bounds.trailing.Offset(-origin.x(), -origin.y());

  return separator_bounds;
}

TabStyle::SeparatorOpacities HorizontalTabStyleViews::GetSeparatorOpacities(
    bool for_layout) const {
  // Adjacent slots should be visually separated from each other. This can be
  // achieved in multiple ways:
  //   - Contrasting background colors for tabs, due to:
  //       - Active state
  //       - Selected state
  //       - Hovered state
  //       - Theming (affected by all the above, plus the neutral state)
  //   - Manually painting a separator.
  // The separator should be the last resort, if none of the above states
  // apply. It's also needed if multiple adjacent views are selected, in which
  // case the uniform selected color does not provide enough contrast.
  // In addition, separators should smoothly fade in and out between states,
  // particularly during the hover animation.

  float leading_opacity = GetSeparatorOpacity(for_layout, true);
  float trailing_opacity = GetSeparatorOpacity(for_layout, false);

  // Return the opacities in physical order, rather than logical.
  if (base::i18n::IsRTL()) {
    std::swap(leading_opacity, trailing_opacity);
  }
  return {leading_opacity, trailing_opacity};
}

float HorizontalTabStyleViews::GetSeparatorOpacity(bool for_layout,
                                                   bool leading) const {
  // Do not show separators if the tab strip is in a decluttered state.
  if (features::IsTabStripDeclutterEnabled() &&
      delegate_->GetTabCount() >=
          TabStyle::kTabStripDeclutterMinTabsForSeparatorHide) {
    return 0.0f;
  }

  const auto has_visible_background =
      [](const TabStyleViewDelegate* const delegate) {
        return delegate->IsActive() || delegate->IsSelected() ||
               delegate->IsHovering() ||
               (delegate->IsPinned() &&
                tabs::IsNewHorizontalPinnedTabStylingEnabled());
      };

  // These tab states all have visible backgrounds. Separators must not
  // be shown between tabs if that is the case;
  if (has_visible_background(delegate_.get())) {
    return 0.0f;
  }

  // Check the adjacent tab/group header to see if there's a visible shapes.
  const TabStyleViewDelegate* const adjacent_tab =
      delegate_->GetAdjacentTab(leading);

  const TabStyleViewDelegate* const left_tab =
      leading ? adjacent_tab : delegate_.get();
  const TabStyleViewDelegate* const right_tab =
      leading ? delegate_.get() : adjacent_tab;

  // Separator should never be shown between split tabs.
  if (right_tab && right_tab->IsSplit() && left_tab && left_tab->IsSplit() &&
      right_tab->GetSplit().value() == left_tab->GetSplit().value()) {
    return 0.0f;
  }

  const bool adjacent_to_header =
      right_tab && right_tab->GetGroup().has_value() &&
      (!left_tab || left_tab->GetGroup() != right_tab->GetGroup());

  const float shown_separator_opacity =
      GetHoverInterpolatedSeparatorOpacity(for_layout, adjacent_tab);

  // Show the separator unless this tab is the first in the group and is next
  // to it's own header.
  if (adjacent_to_header) {
    return (delegate_->GetGroup().has_value() && leading)
               ? 0.0f
               : shown_separator_opacity;
  }

  if (base::FeatureList::IsEnabled(tabs::kTabStripUnification)) {
    // When unification is enabled, tabs render trailing separators by default
    // to avoid double separators between tabs. Leading separators are only
    // rendered between the pinned and unpinned tab containers when new pinned
    // tab styling is disabled.
    if (leading) {
      if (!tabs::IsNewHorizontalPinnedTabStylingEnabled() && adjacent_tab &&
          adjacent_tab->IsPinned() && !has_visible_background(adjacent_tab)) {
        return shown_separator_opacity;
      }
      return 0.0f;
    }
  } else {
    // If there isn't an adjacent tab, the tab is at the beginning or end of the
    // tab strip. For the first tab, we shouldn't show the leading separator.
    // For the last tab, we should show the separator between the new tab button
    // and the tab strip IF the tab isn't selected, hovered, or active. If there
    // is a combo button with a non-transparent background in place of the new
    // tab button, we should not show the trailing separator.
    if (!adjacent_tab) {
      return leading ? 0.0f : shown_separator_opacity;
    }
  }

  // Do not show when the adjacent tab is displaying a visible shape.
  if (adjacent_tab && has_visible_background(adjacent_tab)) {
    return 0.0f;
  }

  // Otherwise, default to showing the separator.
  return shown_separator_opacity;
}

float HorizontalTabStyleViews::GetHoverInterpolatedSeparatorOpacity(
    bool for_layout,
    const TabStyleViewDelegate* other_tab) const {
  // Fade out the intervening separator while this tab or an adjacent tab is
  // hovered, which prevents sudden opacity changes when scrubbing the mouse
  // across the tabstrip. If that adjacent tab is active, don't consider its
  // hover animation value, otherwise the separator on this tab will disappear
  // while that tab is being dragged.
  auto adjacent_hover_value =
      [for_layout](const TabStyleViewDelegate* other_tab) {
        if (for_layout || !other_tab || other_tab->IsActive()) {
          return 0.0f;
        }
        return static_cast<float>(other_tab->GetHoverAnimationValue());
      };
  const float hover_value = GetHoverAnimationValue();
  return 1.0f - std::max(hover_value, adjacent_hover_value(other_tab));
}

bool HorizontalTabStyleViews::IsHovering() const {
  return delegate_->IsHovering();
}

bool HorizontalTabStyleViews::IsHoverAnimationActive() const {
  return delegate_->IsHoverAnimationActive();
}

double HorizontalTabStyleViews::GetHoverAnimationValue() const {
  return delegate_->GetHoverAnimationValue();
}

float HorizontalTabStyleViews::GetHoverOpacity() const {
  return delegate_->GetHoverOpacity();
}

int HorizontalTabStyleViews::GetStrokeThickness() const {
  if (delegate_->GetGroup().has_value() && !delegate_->IsInFocusedGroup() &&
      delegate_->IsActive()) {
    return TabGroupUnderline::kStrokeThickness;
  }

  if (delegate_->IsActive()) {
    return delegate_->GetStrokeThickness();
  }

  if (delegate_->IsPinned() && tabs::IsNewHorizontalPinnedTabStylingEnabled()) {
    return 1;
  }

  return 0;
}

SkPath HorizontalTabStyleViews::GetOverlinePath(float scale) const {
  return GetPath(TabStyle::PathType::kBorder, scale,
                 {.should_paint_extension = false});
}

bool HorizontalTabStyleViews::ShouldPaintTabBackgroundColor(
    TabStyle::TabSelectionState selection_state,
    bool has_custom_background) const {
  if (delegate_->IsPinned() && tabs::IsNewHorizontalPinnedTabStylingEnabled() &&
      !delegate_->IsGlassFrame()) {
    return true;
  }

  // In the active and selected cases, always paint the tab background. The fill
  // image may be transparent.
  if (selection_state == TabStyle::TabSelectionState::kActive ||
      selection_state == TabStyle::TabSelectionState::kSelected) {
    return true;
  }

  if (has_custom_background) {
    return false;
  }

  if (delegate_->IsGlassFrame()) {
    return false;
  }

  return delegate_->ShouldPaintTabBackgroundColor();
}

SkColor HorizontalTabStyleViews::GetTabSeparatorColor() const {
  const auto* cp = delegate_->GetView()->GetWidget()->GetColorProvider();
  DCHECK(cp);
  if (!cp) {
    return gfx::kPlaceholderColor;
  }

  return cp->GetColor(delegate_->GetView()->GetWidget()->ShouldPaintAsActive()
                          ? kColorTabDividerFrameActive
                          : kColorTabDividerFrameInactive);
}

SkColor HorizontalTabStyleViews::GetCurrentTabBackgroundColor(
    const TabStyle::TabSelectionState selection_state,
    bool hovered) const {
  const bool frame_active =
      delegate_->GetView()->GetWidget()
          ? delegate_->GetView()->GetWidget()->ShouldPaintAsActive()
          : true;
  const bool frame_glass = delegate_->IsGlassFrame();
  return tab_style()->GetCurrentTabBackgroundColor(
      selection_state, hovered, GetHoverAnimationValue(), frame_active,
      frame_glass, delegate_->GetView()->GetColorProvider());
}

TabStyle::TabSelectionState HorizontalTabStyleViews::GetSelectionState() const {
  if (delegate_->IsActive()) {
    return TabStyle::TabSelectionState::kActive;
  }

  if (delegate_->IsSelected()) {
    return TabStyle::TabSelectionState::kSelected;
  }

  return TabStyle::TabSelectionState::kInactive;
}

void HorizontalTabStyleViews::PaintTabBackground(
    gfx::Canvas* canvas,
    bool hovered,
    std::optional<int> fill_id) const {
  std::optional<SkColor> group_color = delegate_->GetGroupColor();

  PaintTabBackgroundFill(canvas, hovered, fill_id);

  const auto* widget = delegate_->GetView()->GetWidget();
  DCHECK(widget);
  const SkColor tab_stroke_color = widget->GetColorProvider()->GetColor(
      widget->ShouldPaintAsActive() ? kColorTabStrokeFrameActive
                                    : kColorTabStrokeFrameInactive);

  PaintBackgroundStroke(canvas, group_color.value_or(tab_stroke_color));
  PaintSeparators(canvas);
}

void HorizontalTabStyleViews::PaintTabBackgroundFill(
    gfx::Canvas* canvas,
    bool hovered,
    std::optional<int> fill_id) const {
  const TabStyle::TabSelectionState selection_state = GetSelectionState();
  const SkPath fill_path =
      GetPath(selection_state == TabStyle::TabSelectionState::kActive
                  ? TabStyle::PathType::kActiveTab
                  : TabStyle::PathType::kHighlight,
              canvas->image_scale(), {});
  gfx::ScopedCanvas scoped_canvas(canvas);
  const float scale = canvas->UndoDeviceScaleFactor();

  canvas->ClipPath(fill_path, true);

  if (ShouldPaintTabBackgroundColor(selection_state, fill_id.has_value())) {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);

    flags.setColor(GetCurrentTabBackgroundColor(selection_state, false));
    canvas->DrawRect(gfx::ScaleToEnclosingRect(
                         delegate_->GetView()->GetLocalBounds(), scale),
                     flags);
  }

  if (fill_id.has_value()) {
    gfx::ScopedCanvas scale_scoper(canvas);
    canvas->sk_canvas()->scale(scale, scale);
    gfx::ImageSkia* image =
        delegate_->GetView()->GetThemeProvider()->GetImageSkiaNamed(
            fill_id.value());

    BrowserWindowInterface* browser_window =
        delegate_->GetBrowserWindowInterface();
    BrowserView* browser_view =
        browser_window ? BrowserView::GetBrowserViewForBrowser(browser_window)
                       : nullptr;
    ThemedBackground::PaintThemeAlignedImage(canvas, delegate_->GetView(),
                                             browser_view, image);
  }

  if (hovered) {
    PaintBackgroundHover(canvas, scale);
  }
}

void HorizontalTabStyleViews::PaintBackgroundHover(gfx::Canvas* canvas,
                                                   float scale) const {
  const SkPath fill_path =
      GetPath(TabStyle::PathType::kHighlight, canvas->image_scale(), {});
  canvas->ClipPath(fill_path, true);

  SkColor hover_color =
      GetCurrentTabBackgroundColor(GetSelectionState(), /*hovered=*/true);

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(hover_color);
  canvas->DrawRect(
      gfx::ScaleToEnclosingRect(delegate_->GetView()->GetLocalBounds(), scale),
      flags);
}

void HorizontalTabStyleViews::PaintBackgroundStroke(
    gfx::Canvas* canvas,
    SkColor stroke_color) const {
  if (base::FeatureList::IsEnabled(tabs::kTabStripUnification) &&
      delegate_->GetGroup().has_value() && !delegate_->IsInFocusedGroup() &&
      !delegate_->IsDragging()) {
    return;
  }

  const int stroke_thickness = GetStrokeThickness();
  if (!stroke_thickness) {
    return;
  }

  SkPath outer_path =
      GetPath(TabStyle::PathType::kBorder, canvas->image_scale(),
              {.should_paint_extension = false});
  gfx::ScopedCanvas scoped_canvas(canvas);
  float scale = canvas->UndoDeviceScaleFactor();
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(stroke_color);
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(stroke_thickness * scale);
  canvas->DrawPath(outer_path, flags);
}

void HorizontalTabStyleViews::PaintSeparators(gfx::Canvas* canvas) const {
  const auto separator_opacities = GetSeparatorOpacities(false);
  if (!separator_opacities.left && !separator_opacities.right) {
    return;
  }

  gfx::ScopedCanvas scoped_canvas(canvas);
  const float scale = canvas->UndoDeviceScaleFactor();

  TabStyle::SeparatorBounds separator_bounds = GetSeparatorBounds(scale);

  const SkColor separator_base_color = GetTabSeparatorColor();
  const auto separator_color = [separator_base_color](float opacity) {
    return SkColorSetA(separator_base_color,
                       gfx::Tween::IntValueBetween(opacity, SK_AlphaTRANSPARENT,
                                                   SK_AlphaOPAQUE));
  };

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  if (separator_opacities.left > 0) {
    flags.setColor(separator_color(separator_opacities.left));
    canvas->DrawRoundRect(separator_bounds.leading,
                          tab_style()->GetSeparatorCornerRadius() * scale,
                          flags);
  }
  if (separator_opacities.right > 0) {
    flags.setColor(separator_color(separator_opacities.right));
    canvas->DrawRoundRect(separator_bounds.trailing,
                          tab_style()->GetSeparatorCornerRadius() * scale,
                          flags);
  }
}

BrowserFrameView* HorizontalTabStyleViews::GetBrowserFrameView() const {
  return delegate_->GetBrowserFrameView();
}

float HorizontalTabStyleViews::GetTopCornerRadiusForWidth(int width) const {
  // Get the width of the top of the tab by subtracting the width of the outer
  // corners.
  const int ideal_radius = tab_style()->GetTopCornerRadius();
  const int top_width = width - ideal_radius * 2;

  // To maintain a round-rect appearance, ensure at least one third of the top
  // of the tab is flat.
  const float radius = top_width / 3.f;
  return std::clamp<float>(radius, 0, ideal_radius);
}

gfx::RectF HorizontalTabStyleViews::ScaleAndAlignBounds(
    const gfx::Rect& bounds,
    float scale,
    int stroke_thickness) const {
  // Convert to layout bounds.  We must inset the width such that the right edge
  // of one tab's layout bounds is the same as the left edge of the next tab's;
  // this way the two tabs' separators will be drawn at the same coordinate.
  gfx::RectF aligned_bounds(bounds);
  const int bottom_corner_radius = tab_style()->GetBottomCornerRadius();
  // Note: This intentionally doesn't subtract
  // LayoutConstant::kTabstripToolbarOverlap from the bottom inset, because we
  // want to pixel-align the bottom of the stroke, not the bottom of the
  // overlap.
  auto layout_insets = gfx::InsetsF::TLBR(
      stroke_thickness, bottom_corner_radius, stroke_thickness,
      bottom_corner_radius + tab_style()->GetSeparatorSize().width());
  aligned_bounds.Inset(layout_insets);

  // Scale layout bounds from DIP to px.
  aligned_bounds.Scale(scale);

  // Snap layout bounds to nearest pixels so we get clean lines.
  const float x = std::round(aligned_bounds.x());
  const float y = std::round(aligned_bounds.y());
  // It's important to round the right edge and not the width, since rounding
  // both x and width would mean the right edge would accumulate error.
  const float right = std::round(aligned_bounds.right());
  const float bottom = std::round(aligned_bounds.bottom());
  aligned_bounds = gfx::RectF(x, y, right - x, bottom - y);

  // Convert back to full bounds.  It's OK that the outer corners of the curves
  // around the separator may not be snapped to the pixel grid as a result.
  aligned_bounds.Inset(-gfx::ScaleInsets(layout_insets, scale));
  return aligned_bounds;
}

std::optional<SkPath> HorizontalTabStyleViews::GetPinnedPath(
    TabStyle::PathType path_type,
    float scale,
    const TabPathFlags& flags) const {
  CHECK(tabs::IsNewHorizontalPinnedTabStylingEnabled());
  CHECK(delegate()->IsPinned());

  const int stroke_thickness = GetStrokeThickness();
  gfx::RectF aligned_bounds = ScaleAndAlignBounds(
      delegate()->GetView()->bounds(), scale, stroke_thickness);

  float content_corner_radius =
      GetTopCornerRadiusForWidth(delegate()->GetView()->width()) * scale;
  float extension_corner_radius = tab_style()->GetBottomCornerRadius() * scale;

  float top_left_corner_radius = content_corner_radius;
  float top_right_corner_radius = content_corner_radius;
  float bottom_left_corner_radius = content_corner_radius;
  float bottom_right_corner_radius = content_corner_radius;
  float tab_height = GetLayoutConstant(LayoutConstant::kTabHeight) * scale;

  tab_height -= GetLayoutConstant(LayoutConstant::kTabStripPadding) * scale;
  tab_height -=
      GetLayoutConstant(LayoutConstant::kTabstripToolbarOverlap) * scale;

  float left = aligned_bounds.x() + extension_corner_radius;
  float top = aligned_bounds.y() +
              GetLayoutConstant(LayoutConstant::kTabStripPadding) * scale;
  float right = aligned_bounds.right() - extension_corner_radius;
  float bottom = top + tab_height;

  // Apply stroke adjustment if it's active tab or border
  const float stroke_adjustment = stroke_thickness * scale;
  if (path_type == TabStyle::PathType::kActiveTab ||
      path_type == TabStyle::PathType::kBorder) {
    left += 0.5f * stroke_adjustment;
    right -= 0.5f * stroke_adjustment;
    top += 0.5f * stroke_adjustment;
    bottom -= 0.5f * stroke_adjustment;
    top_left_corner_radius -= 0.5f * stroke_adjustment;
    top_right_corner_radius -= 0.5f * stroke_adjustment;
    bottom_left_corner_radius -= 0.5f * stroke_adjustment;
    bottom_right_corner_radius -= 0.5f * stroke_adjustment;
  }

  const SkVector radii[4] = {
      SkVector(top_left_corner_radius, top_left_corner_radius),
      SkVector(top_right_corner_radius, top_right_corner_radius),
      SkVector(bottom_right_corner_radius, bottom_right_corner_radius),
      SkVector(bottom_left_corner_radius, bottom_left_corner_radius)};

  SkPathBuilder path;
  path.addRRect(SkRRect::MakeRectRadii(
      SkRect::MakeLTRB(left, top, right, bottom), radii));

  // Convert path to be relative to the tab origin.
  gfx::PointF origin(delegate_->GetView()->origin());
  origin.Scale(scale);
  path.offset(-origin.x(), -origin.y());

  // Possibly convert back to DIPs.
  if (flags.render_units == TabStyle::RenderUnits::kDips && scale != 1.0f) {
    path.transform(SkMatrix::Scale(1.0f / scale, 1.0f / scale));
  }

  return path.detach();
}
