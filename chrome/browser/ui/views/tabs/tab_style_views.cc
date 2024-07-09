// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_style_views.h"

#include <algorithm>
#include <utility>

#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "cc/paint/paint_record.h"
#include "cc/paint/paint_shader.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/tabs/tab_types.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_background.h"
#include "chrome/browser/ui/views/tabs/glow_hover_controller.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_close_button.h"
#include "chrome/browser/ui/views/tabs/tab_group_underline.h"
#include "chrome/browser/ui/views/tabs/tab_slot_controller.h"
#include "chrome/browser/ui/views/tabs/tab_slot_view.h"
#include "chrome/grit/theme_resources.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "third_party/skia/include/pathops/SkPathOps.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/widget/widget.h"

namespace {
// How the tab shape path is modified for selected tabs.
using ShapeModifier = int;
// No modification should be done.
constexpr ShapeModifier kNone = 0x00;
// Exclude the lower left arc.
constexpr ShapeModifier kNoLowerLeftArc = 0x01;
// Exclude the lower right arc.
constexpr ShapeModifier kNoLowerRightArc = 0x01 << 1;
// shrink the left arc to fit the reduced space without frame
// controls/tabsearch.
constexpr ShapeModifier kCompactLeftArc = 0x01 << 2;

// Updates a target value, returning true if it changed.
template <class T>
bool UpdateValue(T* dest, const T& src) {
  if (*dest == src) {
    return false;
  }
  *dest = src;
  return true;
}

class TabStyleViewsImpl : public TabStyleViews {
 public:
  explicit TabStyleViewsImpl(Tab* tab);
  ~TabStyleViewsImpl() override = default;
  TabStyleViewsImpl(const TabStyleViewsImpl&) = delete;
  TabStyleViewsImpl& operator=(const TabStyleViewsImpl&) = delete;

  const Tab* tab() const { return tab_; }

 protected:
  // TabStyle:
  SkPath GetPath(TabStyle::PathType path_type,
                 float scale,
                 bool force_active,
                 TabStyle::RenderUnits render_units) const override;
  gfx::Insets GetContentsInsets() const override;
  float GetZValue() const override;
  float GetTargetActiveOpacity() const override;
  float GetCurrentActiveOpacity() const override;
  TabActive GetApparentActiveState() const override;
  TabStyle::TabColors CalculateTargetColors() const override;
  void PaintTab(gfx::Canvas* canvas) const override;
  void SetHoverLocation(const gfx::Point& location) override;
  void ShowHover(TabStyle::ShowHoverStyle style) override;
  void HideHover(TabStyle::HideHoverStyle style) override;

  // Returns the color for the separator.
  virtual SkColor GetTabSeparatorColor() const;

  // Painting helper functions:
  virtual SkColor GetTargetTabBackgroundColor(
      TabStyle::TabSelectionState selection_state,
      bool hovered) const;
  virtual SkColor GetCurrentTabBackgroundColor(
      TabStyle::TabSelectionState selection_state,
      bool hovered) const;

  // Returns the thickness of the stroke drawn around the top and sides of the
  // tab. Only active tabs may have a stroke, and not in all cases. If there
  // is no stroke, returns 0. If |should_paint_as_active| is true, the tab is
  // treated as an active tab regardless of its true current state.
  virtual int GetStrokeThickness(bool should_paint_as_active = false) const;

  virtual bool ShouldPaintTabBackgroundColor(
      TabStyle::TabSelectionState selection_state,
      bool has_custom_background) const;

  // Returns the progress (0 to 1) of the hover animation.
  double GetHoverAnimationValue() const override;

  // Scales |bounds| by scale and aligns so that adjacent tabs meet up exactly
  // during painting.
  gfx::RectF ScaleAndAlignBounds(const gfx::Rect& bounds,
                                 float scale,
                                 int stroke_thickness) const;

  // Given a tab of width |width|, returns the radius to use for the corners.
  float GetTopCornerRadiusForWidth(int width) const;

  // Returns a single separator's opacity based on whether it is the
  // logically `leading` separator. `for_layout` has the same meaning as in
  // GetSeparatorOpacities().
  virtual float GetSeparatorOpacity(bool for_layout, bool leading) const;

  // Helper that returns an interpolated opacity if the tab or its neighbor
  // `other_tab` is mid-hover-animation. Used in almost all cases when a
  // separator is shown, since hovering is independent of tab state.
  // `for_layout` has the same meaning as in GetSeparatorOpacities().
  float GetHoverInterpolatedSeparatorOpacity(bool for_layout,
                                             const Tab* other_tab) const;

  TabStyle::TabSelectionState GetSelectionState() const;

 private:
  // Gets the bounds for the leading and trailing separators for a tab.
  TabStyle::SeparatorBounds GetSeparatorBounds(float scale) const;

  // Returns the opacities of the separators. If |for_layout| is true, returns
  // the "layout" opacities, which ignore the effects of surrounding tabs' hover
  // effects and consider only the current tab's state.
  TabStyle::SeparatorOpacities GetSeparatorOpacities(bool for_layout) const;

  // Returns whether we shoould extend the hit test region for Fitts' Law.
  bool ShouldExtendHitTest() const;

  // Returns whether the mouse is currently hovering this tab.
  bool IsHovering() const;

  // Returns whether the hover animation is being shown.
  bool IsHoverAnimationActive() const;

  // Returns the opacity of the hover effect that should be drawn, which may not
  // be the same as GetHoverAnimationValue.
  float GetHoverOpacity() const;

  // When selected, non-active, non-hovered tabs are adjacent to each other,
  // there are anti-aliasing artifacts in the overlapped lower arc region. This
  // returns how to modify the tab shape to eliminate the lower arcs on the
  // right or left based on the state of the adjacent tab(s).
  ShapeModifier GetShapeModifier(TabStyle::PathType path_type) const;

  // Painting helper functions:
  void PaintInactiveTabBackground(gfx::Canvas* canvas) const;
  void PaintTabBackground(gfx::Canvas* canvas,
                          TabStyle::TabSelectionState selection_state,
                          bool hovered,
                          std::optional<int> fill_id,
                          int y_inset) const;
  void PaintTabBackgroundWithImages(
      gfx::Canvas* canvas,
      std::optional<int> active_tab_fill_id,
      std::optional<int> inactive_tab_fill_id) const;
  void PaintTabBackgroundFill(gfx::Canvas* canvas,
                              TabStyle::TabSelectionState selection_state,
                              bool hovered,
                              std::optional<int> fill_id,
                              int y_inset) const;
  virtual void PaintBackgroundHover(gfx::Canvas* canvas, float scale) const;
  void PaintBackgroundStroke(gfx::Canvas* canvas,
                             TabStyle::TabSelectionState selection_state,
                             SkColor stroke_color) const;
  void PaintSeparators(gfx::Canvas* canvas) const;

  const raw_ptr<const Tab> tab_;

  std::unique_ptr<GlowHoverController> hover_controller_;
};

// TabStyleViewsImpl ----------------------------------------------------------

TabStyleViewsImpl::TabStyleViewsImpl(Tab* tab)
    : tab_(tab),
      hover_controller_((tab && gfx::Animation::ShouldRenderRichAnimation())
                            ? new GlowHoverController(tab)
                            : nullptr) {
  // `tab_` must not be nullptr.
  CHECK(tab_);
  // TODO(dfried): create a new STYLE_PROMINENT or similar to use instead of
  // repurposing CONTEXT_BUTTON_MD.
}

SkPath TabStyleViewsImpl::GetPath(TabStyle::PathType path_type,
                                  float scale,
                                  bool force_active,
                                  TabStyle::RenderUnits render_units) const {
  CHECK(tab());
  const int stroke_thickness = GetStrokeThickness(force_active);

  const TabStyle::TabSelectionState state = GetSelectionState();

  // We'll do the entire path calculation in aligned pixels.
  // TODO(dfried): determine if we actually want to use |stroke_thickness| as
  // the inset in this case.
  gfx::RectF aligned_bounds =
      ScaleAndAlignBounds(tab()->bounds(), scale, stroke_thickness);

  // Calculate the corner radii. Note that corner radius is based on original
  // tab width (in DIP), not our new, scaled-and-aligned bounds.
  float content_corner_radius =
      GetTopCornerRadiusForWidth(tab()->width()) * scale;
  float extension_corner_radius = tab_style()->GetBottomCornerRadius() * scale;

  // Selected, hover, and inactive tab fills are a detached squarcle tab.
  if ((path_type == TabStyle::PathType::kFill &&
       state != TabStyle::TabSelectionState::kActive) ||
      path_type == TabStyle::PathType::kHighlight ||
      path_type == TabStyle::PathType::kInteriorClip ||
      path_type == TabStyle::PathType::kHitTest) {
    // TODO (crbug.com/1451400): This constant should be unified with
    // kCRtabstripRegionViewControlPadding in tab_strip_region_view.

    float top_content_corner_radius = content_corner_radius;
    float bottom_content_corner_radius = content_corner_radius;
    float tab_height = GetLayoutConstant(TAB_HEIGHT) * scale;

    // The tab displays favicon animations that can emerge from the toolbar. The
    // interior clip needs to extend the entire height of the toolbar to support
    // this. Detached tab shapes do not need to respect this.
    if (path_type != TabStyle::PathType::kInteriorClip &&
        path_type != TabStyle::PathType::kHitTest) {
      tab_height -= GetLayoutConstant(TAB_STRIP_PADDING) * scale;
      tab_height -= GetLayoutConstant(TABSTRIP_TOOLBAR_OVERLAP) * scale;
    }

    // Don't round the bottom corners to avoid creating dead space between tabs.
    if (path_type == TabStyle::PathType::kHitTest) {
      bottom_content_corner_radius = 0;
    }

    int left = aligned_bounds.x() + extension_corner_radius;
    int top = aligned_bounds.y() + GetLayoutConstant(TAB_STRIP_PADDING) * scale;
    int right = aligned_bounds.right() - extension_corner_radius;
    const int bottom = top + tab_height;

    // For maximized and full screen windows, extend the tab hit test to the top
    // of the tab, encompassing the top padding. This makes it easy to click on
    // tabs by moving the mouse to the top of the screen.
    if (path_type == TabStyle::PathType::kHitTest &&
        tab()->controller()->IsFrameCondensed()) {
      top -= GetLayoutConstant(TAB_STRIP_PADDING) * scale;
      // Don't round the top corners to avoid creating dead space between tabs.
      top_content_corner_radius = 0;
    }

    // if the size of the space for the path is smaller than the size of a
    // favicon or if we are building a path for the hit test, expand to take the
    // entire width of the separator margins AND the separator.
    if ((right - left) < (gfx::kFaviconSize * scale) ||
        path_type == TabStyle::PathType::kHitTest) {
      // Take the entire size of the separator. in odd separator size cases, the
      // right side will take the remaining space.
      const int left_separator_overlap =
          tab_style()->GetSeparatorSize().width() / 2;
      const int right_separator_overlap =
          tab_style()->GetSeparatorSize().width() - left_separator_overlap;

      // If there is a tab before this one, then expand into its overlap.
      const Tab* const previous_tab =
          tab()->controller()->GetAdjacentTab(tab(), -1);
      if (previous_tab) {
        left -= (tab_style()->GetSeparatorMargins().right() +
                 left_separator_overlap) *
                scale;
      }

      // If there is a tab after this one, then expand into its overlap.
      const Tab* const next_tab = tab()->controller()->GetAdjacentTab(tab(), 1);
      if (next_tab) {
        right += (tab_style()->GetSeparatorMargins().left() +
                  right_separator_overlap) *
                 scale;
      }
    }

    // Radii are clockwise from top left.
    const SkVector radii[4] = {
        SkVector(top_content_corner_radius, top_content_corner_radius),
        SkVector(top_content_corner_radius, top_content_corner_radius),
        SkVector(bottom_content_corner_radius, bottom_content_corner_radius),
        SkVector(bottom_content_corner_radius, bottom_content_corner_radius)};
    SkRRect rrect;
    rrect.setRectRadii(SkRect::MakeLTRB(left, top, right, bottom), radii);
    SkPath path;
    path.addRRect(rrect);

    // Convert path to be relative to the tab origin.
    gfx::PointF origin(tab()->origin());
    origin.Scale(scale);
    path.offset(-origin.x(), -origin.y());

    // Possibly convert back to DIPs.
    if (render_units == TabStyle::RenderUnits::kDips && scale != 1.0f) {
      path.transform(SkMatrix::Scale(1.0f / scale, 1.0f / scale));
    }

    return path;
  }

  // Compute |extension| as the width outside the separators.  This is a fixed
  // value equal to the normal corner radius.
  const float extension = extension_corner_radius;

  // Calculate the bounds of the actual path.
  const float left = aligned_bounds.x();
  const float right = aligned_bounds.right();
  float tab_top =
      aligned_bounds.y() + GetLayoutConstant(TAB_STRIP_PADDING) * scale;
  float tab_left = left + extension;
  float tab_right = right - extension;

  // Overlap the toolbar below us so that gaps don't occur when rendering at
  // non-integral display scale factors.
  const float extended_bottom = aligned_bounds.bottom();
  const float bottom_extension =
      GetLayoutConstant(TABSTRIP_TOOLBAR_OVERLAP) * scale;
  float tab_bottom = extended_bottom - bottom_extension;

  // Path-specific adjustments:
  const float stroke_adjustment = stroke_thickness * scale;
  bool extend_to_top = false;
  if (path_type == TabStyle::PathType::kFill ||
      path_type == TabStyle::PathType::kBorder) {
    tab_left += 0.5f * stroke_adjustment;
    tab_right -= 0.5f * stroke_adjustment;
    tab_top += 0.5f * stroke_adjustment;
    content_corner_radius -= 0.5f * stroke_adjustment;
    tab_bottom -= 0.5f * stroke_adjustment;
    extension_corner_radius -= 0.5f * stroke_adjustment;
  }
  const ShapeModifier shape_modifier = GetShapeModifier(path_type);
  const bool extend_left_to_bottom = shape_modifier & kNoLowerLeftArc;
  const bool extend_right_to_bottom = shape_modifier & kNoLowerRightArc;
  const bool compact_left_to_bottom =
      !extend_left_to_bottom && (shape_modifier & kCompactLeftArc);

  SkPath path;

  float left_extension_corner_radius = extension_corner_radius;
  if (compact_left_to_bottom) {
    left_extension_corner_radius = (tab_style()->GetBottomCornerRadius() -
                                    GetLayoutConstant(TOOLBAR_CORNER_RADIUS)) *
                                   scale;
  }

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

  // Start with the left side of the shape.
  path.moveTo(left, extended_bottom);

  if (tab_left != left) {
    // Draw the left edge of the extension.
    //   ╭─────────╮
    //   │ Content │
    // ┏─╯         ╰─┐
    if (tab_bottom != extended_bottom) {
      path.lineTo(left, tab_bottom);
    }

    // Draw the bottom-left corner.
    //   ╭─────────╮
    //   │ Content │
    // ┌━╝         ╰─┐
    if (extend_left_to_bottom) {
      path.lineTo(tab_left, tab_bottom);
    } else {
      path.lineTo(tab_left - left_extension_corner_radius, tab_bottom);
      path.arcTo(left_extension_corner_radius, left_extension_corner_radius, 0,
                 SkPath::kSmall_ArcSize, SkPathDirection::kCCW, tab_left,
                 tab_bottom - left_extension_corner_radius);
    }
  }

  // Draw the ascender and top-left curve, if present.
  if (extend_to_top) {
    //   ┎─────────╮
    //   ┃ Content │
    // ┌─╯         ╰─┐
    path.lineTo(tab_left, tab_top);
  } else {
    //   ╔─────────╮
    //   ┃ Content │
    // ┌─╯         ╰─┐
    path.lineTo(tab_left, tab_top + content_corner_radius);
    path.arcTo(content_corner_radius, content_corner_radius, 0,
               SkPath::kSmall_ArcSize, SkPathDirection::kCW,
               tab_left + content_corner_radius, tab_top);
  }

  // Draw the top crossbar and top-right curve, if present.
  if (extend_to_top) {
    //   ┌━━━━━━━━━┑
    //   │ Content │
    // ┌─╯         ╰─┐
    path.lineTo(tab_right, tab_top);
  } else {
    //   ╭━━━━━━━━━╗
    //   │ Content │
    // ┌─╯         ╰─┐
    path.lineTo(tab_right - content_corner_radius, tab_top);
    path.arcTo(content_corner_radius, content_corner_radius, 0,
               SkPath::kSmall_ArcSize, SkPathDirection::kCW, tab_right,
               tab_top + content_corner_radius);
  }

  if (tab_right != right) {
    // Draw the descender and bottom-right corner.
    //   ╭─────────╮
    //   │ Content ┃
    // ┌─╯         ╚━┐
    if (extend_right_to_bottom) {
      path.lineTo(tab_right, tab_bottom);
    } else {
      path.lineTo(tab_right, tab_bottom - extension_corner_radius);
      path.arcTo(extension_corner_radius, extension_corner_radius, 0,
                 SkPath::kSmall_ArcSize, SkPathDirection::kCCW,
                 tab_right + extension_corner_radius, tab_bottom);
    }
    if (tab_bottom != extended_bottom) {
      path.lineTo(right, tab_bottom);
    }
  }

  // Draw anything remaining: the descender, the bottom right horizontal
  // stroke, or the right edge of the extension, depending on which
  // conditions fired above.
  //   ╭─────────╮
  //   │ Content │
  // ┌─╯         ╰─┓
  path.lineTo(right, extended_bottom);

  if (path_type != TabStyle::PathType::kBorder) {
    path.close();
  }

  // Convert path to be relative to the tab origin.
  gfx::PointF origin(tab_->origin());
  origin.Scale(scale);
  path.offset(-origin.x(), -origin.y());

  // Possibly convert back to DIPs.
  if (render_units == TabStyle::RenderUnits::kDips && scale != 1.0f) {
    path.transform(SkMatrix::Scale(1.0f / scale, 1.0f / scale));
  }

  return path;
}

gfx::Insets TabStyleViewsImpl::GetContentsInsets() const {
  const int stroke_thickness = GetStrokeThickness();
  gfx::Insets base_style_insets = tab_style()->GetContentsInsets();
  return gfx::Insets::TLBR(
             stroke_thickness, 0,
             stroke_thickness + GetLayoutConstant(TABSTRIP_TOOLBAR_OVERLAP),
             0) +
         base_style_insets;
}

float TabStyleViewsImpl::GetZValue() const {
  // This will return values so that inactive tabs can be sorted in the
  // following order:
  //
  // o Unselected tabs, in ascending hover animation value order.
  // o The single unselected tab being hovered by the mouse, if present.
  // o Selected tabs, in ascending hover animation value order.
  // o The single selected tab being hovered by the mouse, if present.
  //
  // Representing the above groupings is accomplished by adding a "weight" to
  // the current hover animation value.
  //
  // 0.0 == z-value         Unselected/non hover animating.
  // 0.0 <  z-value <= 1.0  Unselected/hover animating.
  // 2.0 <= z-value <= 3.0  Unselected/mouse hovered tab.
  // 4.0 == z-value         Selected/non hover animating.
  // 4.0 <  z-value <= 5.0  Selected/hover animating.
  // 6.0 <= z-value <= 7.0  Selected/mouse hovered tab.
  //
  // This function doesn't handle active tabs, as they are normally painted by a
  // different code path (with z-value infinity).
  float sort_value = GetHoverAnimationValue();
  if (tab_->IsSelected())
    sort_value += 4.f;
  if (IsHovering()) {
    sort_value += 2.f;
  }

  DCHECK_GE(sort_value, 0.0f);
  DCHECK_LE(sort_value, TabStyle::kMaximumZValue);

  return sort_value;
}

float TabStyleViewsImpl::GetTargetActiveOpacity() const {
  const TabStyle::TabSelectionState selection_state = GetSelectionState();
  if (selection_state == TabStyle::TabSelectionState::kActive) {
    return 1.0f;
  }
  if (IsHovering()) {
    return GetHoverOpacity();
  }
  return selection_state == TabStyle::TabSelectionState::kSelected
             ? tab_style()->GetSelectedTabOpacity()
             : 0.0f;
}

float TabStyleViewsImpl::GetCurrentActiveOpacity() const {
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

TabActive TabStyleViewsImpl::GetApparentActiveState() const {
  // In some cases, inactive tabs may have background more like active tabs than
  // inactive tabs, so colors should be adapted to ensure appropriate contrast.
  // In particular, text should have plenty of contrast in all cases, so switch
  // to using foreground color designed for active tabs if the tab looks more
  // like an active tab than an inactive tab.
  return GetTargetActiveOpacity() > 0.5f ? TabActive::kActive
                                         : TabActive::kInactive;
}

TabStyle::TabColors TabStyleViewsImpl::CalculateTargetColors() const {
  // TODO(crbug.com/347086815): Using GetApparentActiveState no longer makes
  // sense after migration and should be cleaned up.
  const TabActive active = GetApparentActiveState();
  const SkColor foreground_color =
      tab_->controller()->GetTabForegroundColor(active);
  const SkColor background_color =
      GetTargetTabBackgroundColor(GetSelectionState(), IsHovering());
  const ui::ColorId focus_ring_color = (active == TabActive::kActive)
                                           ? kColorTabFocusRingActive
                                           : kColorTabFocusRingInactive;
  const ui::ColorId close_button_focus_ring_color =
      (active == TabActive::kActive) ? kColorTabCloseButtonFocusRingActive
                                     : kColorTabCloseButtonFocusRingInactive;
  return {foreground_color, background_color, focus_ring_color,
          close_button_focus_ring_color};
}

void TabStyleViewsImpl::PaintTab(gfx::Canvas* canvas) const {
  std::optional<int> active_tab_fill_id;
  if (tab_->GetThemeProvider()->HasCustomImage(IDR_THEME_TOOLBAR)) {
    active_tab_fill_id = IDR_THEME_TOOLBAR;
  }
  const std::optional<int> inactive_tab_fill_id =
      tab_->controller()->GetCustomBackgroundId(
          BrowserFrameActiveState::kUseCurrent);

  if (active_tab_fill_id.has_value() || inactive_tab_fill_id.has_value()) {
    PaintTabBackgroundWithImages(canvas, active_tab_fill_id,
                                 inactive_tab_fill_id);
  } else {
    PaintTabBackground(canvas, GetSelectionState(), IsHoverAnimationActive(),
                       std::nullopt, 0);
  }
}

void TabStyleViewsImpl::PaintTabBackgroundWithImages(
    gfx::Canvas* canvas,
    std::optional<int> active_tab_fill_id,
    std::optional<int> inactive_tab_fill_id) const {
  // When at least one of the active or inactive tab backgrounds have an image,
  // we must paint them with the previous method of layering the active and
  // inactive images with two paint calls.

  const int active_tab_y_inset = GetStrokeThickness(true);
  const TabStyle::TabSelectionState current_state = GetSelectionState();

  if (current_state == TabStyle::TabSelectionState::kActive) {
    PaintTabBackground(canvas, TabStyle::TabSelectionState::kActive,
                       /*hovered=*/false, active_tab_fill_id,
                       active_tab_y_inset);
  } else {
    PaintTabBackground(canvas, TabStyle::TabSelectionState::kInactive,
                       /*hovered=*/false, inactive_tab_fill_id, 0);

    const float opacity = GetCurrentActiveOpacity();
    if (opacity > 0) {
      canvas->SaveLayerAlpha(base::ClampRound<uint8_t>(opacity * 0xff),
                             tab_->GetLocalBounds());
      PaintTabBackground(canvas, TabStyle::TabSelectionState::kActive,
                         /*hovered=*/false, active_tab_fill_id,
                         active_tab_y_inset);
      canvas->Restore();
    }
  }
}

void TabStyleViewsImpl::SetHoverLocation(const gfx::Point& location) {
  // There's a "glow" that gets drawn over inactive tabs based on the mouse's
  // location. There is no glow for the active tab so don't update the hover
  // controller and incur a redraw.
  if (hover_controller_ && !tab_->IsActive())
    hover_controller_->SetLocation(location);
}

void TabStyleViewsImpl::ShowHover(TabStyle::ShowHoverStyle style) {
  if (!hover_controller_)
    return;

  if (style == TabStyle::ShowHoverStyle::kSubtle) {
    hover_controller_->SetSubtleOpacityScale(
        tab_->controller()->GetHoverOpacityForRadialHighlight());
  }
  hover_controller_->Show(style);
}

void TabStyleViewsImpl::HideHover(TabStyle::HideHoverStyle style) {
  if (hover_controller_)
    hover_controller_->Hide(style);
}

TabStyle::SeparatorBounds TabStyleViewsImpl::GetSeparatorBounds(
    float scale) const {
  const gfx::RectF aligned_bounds =
      ScaleAndAlignBounds(tab_->bounds(), scale, GetStrokeThickness());
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

  gfx::PointF origin(tab_->bounds().origin());
  origin.Scale(scale);
  separator_bounds.leading.Offset(-origin.x(), -origin.y());
  separator_bounds.trailing.Offset(-origin.x(), -origin.y());

  return separator_bounds;
}

TabStyle::SeparatorOpacities TabStyleViewsImpl::GetSeparatorOpacities(
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
  if (base::i18n::IsRTL())
    std::swap(leading_opacity, trailing_opacity);
  return {leading_opacity, trailing_opacity};
}

float TabStyleViewsImpl::GetSeparatorOpacity(bool for_layout,
                                             bool leading) const {
  const auto has_visible_background = [](const Tab* const tab) {
    return tab->IsActive() || tab->IsSelected() || tab->IsMouseHovered();
  };

  // These tab states all have visible backgrounds. Separators must not
  // be shown between tabs if that is the case;
  if (has_visible_background(tab())) {
    return 0.0f;
  }

  // Check the adjacent tab/group header to see if there's a visible shapes.
  const Tab* const adjacent_tab =
      tab()->controller()->GetAdjacentTab(tab(), leading ? -1 : 1);

  const Tab* const left_tab = leading ? adjacent_tab : tab();
  const Tab* const right_tab = leading ? tab() : adjacent_tab;
  const bool adjacent_to_header =
      right_tab && right_tab->group().has_value() &&
      (!left_tab || left_tab->group() != right_tab->group());

  const float shown_separator_opacity =
      GetHoverInterpolatedSeparatorOpacity(for_layout, adjacent_tab);

  // Show the separator unless this tab is the first in the group and is next
  // to it's own header.
  if (adjacent_to_header) {
    return (tab()->group().has_value() && leading) ? 0.0f
                                                   : shown_separator_opacity;
  }

  // If there isn't an adjacent tab, the tab is at the beginning or end of the
  // tab strip. for the first tab, we shouldn't not show the leading separator,
  // for the last tab, we should show the separator between the new tab button
  // and the tab strip IF the tab isn't selected, hovered, or active.
  if (!adjacent_tab) {
    return leading ? 0.0f : shown_separator_opacity;
  }

  // Do not show when the adjacent tab is displaying a visible shape.
  if (has_visible_background(adjacent_tab)) {
    return 0.0f;
  }

  // Otherwise, default to showing the separator.
  return shown_separator_opacity;
}

float TabStyleViewsImpl::GetHoverInterpolatedSeparatorOpacity(
    bool for_layout,
    const Tab* other_tab) const {
  // Fade out the intervening separator while this tab or an adjacent tab is
  // hovered, which prevents sudden opacity changes when scrubbing the mouse
  // across the tabstrip. If that adjacent tab is active, don't consider its
  // hover animation value, otherwise the separator on this tab will disappear
  // while that tab is being dragged.
  auto adjacent_hover_value = [for_layout](const Tab* other_tab) {
    if (for_layout || !other_tab || other_tab->IsActive()) {
      return 0.0f;
    }
    return static_cast<float>(
        other_tab->tab_style_views()->GetHoverAnimationValue());
  };
  const float hover_value = GetHoverAnimationValue();
  return 1.0f - std::max(hover_value, adjacent_hover_value(other_tab));
}

bool TabStyleViewsImpl::ShouldExtendHitTest() const {
  const views::Widget* widget = tab_->GetWidget();
  return widget->IsMaximized() || widget->IsFullscreen();
}

bool TabStyleViewsImpl::IsHovering() const {
  return tab_->mouse_hovered();
}

bool TabStyleViewsImpl::IsHoverAnimationActive() const {
  return IsHovering() || (hover_controller_ && hover_controller_->ShouldDraw());
}

double TabStyleViewsImpl::GetHoverAnimationValue() const {
  if (!hover_controller_) {
    return IsHoverAnimationActive() ? 1.0 : 0.0;
  }
  return hover_controller_->GetAnimationValue();
}

float TabStyleViewsImpl::GetHoverOpacity() const {
  // Opacity boost varies on tab width.  The interpolation is nonlinear so
  // that most tabs will fall on the low end of the opacity range, but very
  // narrow tabs will still stand out on the high end.
  const float range_start = static_cast<float>(tab_style()->GetStandardWidth());
  constexpr float kWidthForMaxHoverOpacity = 32.0f;
  const float value_in_range = static_cast<float>(tab_->width());
  const float t = std::clamp(
      (value_in_range - range_start) / (kWidthForMaxHoverOpacity - range_start),
      0.0f, 1.0f);
  return tab_->controller()->GetHoverOpacityForTab(t * t);
}

int TabStyleViewsImpl::GetStrokeThickness(bool should_paint_as_active) const {
  std::optional<tab_groups::TabGroupId> group = tab_->group();
  if (group.has_value() && tab_->IsActive())
    return TabGroupUnderline::kStrokeThickness;

  if (tab_->IsActive() || should_paint_as_active)
    return tab_->controller()->GetStrokeThickness();

  return 0;
}

bool TabStyleViewsImpl::ShouldPaintTabBackgroundColor(
    TabStyle::TabSelectionState selection_state,
    bool has_custom_background) const {
  // In the active case, always paint the tab background. The fill image may be
  // transparent.
  if (selection_state == TabStyle::TabSelectionState::kActive) {
    return true;
  }

  // In the inactive case, the fill image is guaranteed to be opaque, so it's
  // not necessary to paint the background when there is one.
  if (has_custom_background) {
    return false;
  }

  return tab_->GetThemeProvider()->GetDisplayProperty(
      ThemeProperties::SHOULD_FILL_BACKGROUND_TAB_COLOR);
}

SkColor TabStyleViewsImpl::GetTabSeparatorColor() const {
  const auto* cp = tab()->GetWidget()->GetColorProvider();
  DCHECK(cp);
  if (!cp) {
    return gfx::kPlaceholderColor;
  }

  return cp->GetColor(tab()->GetWidget()->ShouldPaintAsActive()
                          ? kColorTabDividerFrameActive
                          : kColorTabDividerFrameInactive);
}

SkColor TabStyleViewsImpl::GetTargetTabBackgroundColor(
    const TabStyle::TabSelectionState selection_state,
    bool hovered) const {
  // Tests may not have a color provider or a widget.
  const bool active_widget =
      tab()->GetWidget() ? tab()->GetWidget()->ShouldPaintAsActive() : true;
  if (!tab()->GetColorProvider()) {
    return gfx::kPlaceholderColor;
  }

  return tab_style()->GetTabBackgroundColor(
      selection_state, hovered, active_widget, *tab()->GetColorProvider());
}

SkColor TabStyleViewsImpl::GetCurrentTabBackgroundColor(
    const TabStyle::TabSelectionState selection_state,
    bool hovered) const {
  const SkColor color = GetTargetTabBackgroundColor(selection_state, hovered);
  if (!hovered) {
    return color;
  }

  const SkColor unhovered_color =
      GetTargetTabBackgroundColor(selection_state, /*hovered=*/false);
  return color_utils::AlphaBlend(color, unhovered_color,
                                 static_cast<float>(GetHoverAnimationValue()));
}

TabStyle::TabSelectionState TabStyleViewsImpl::GetSelectionState() const {
  if (tab_->IsActive()) {
    return TabStyle::TabSelectionState::kActive;
  }
  if (tab_->IsSelected()) {
    return TabStyle::TabSelectionState::kSelected;
  }
  return TabStyle::TabSelectionState::kInactive;
}

ShapeModifier TabStyleViewsImpl::GetShapeModifier(
    TabStyle::PathType path_type) const {
  ShapeModifier shape_modifier = kNone;
  if (path_type == TabStyle::PathType::kFill && tab_->IsSelected() &&
      !IsHoverAnimationActive() && !tab_->IsActive()) {
    auto check_adjacent_tab = [](const Tab* tab, int offset,
                                 ShapeModifier modifier) {
      const Tab* adjacent_tab = tab->controller()->GetAdjacentTab(tab, offset);
      if (adjacent_tab && adjacent_tab->IsSelected() &&
          !adjacent_tab->IsMouseHovered())
        return modifier;
      return kNone;
    };
    shape_modifier |= check_adjacent_tab(tab_, -1, kNoLowerLeftArc);
    shape_modifier |= check_adjacent_tab(tab_, 1, kNoLowerRightArc);
  }

  // If the tab is the first in the list
  if (tab_->controller()->tab_at(0) == tab_ &&
      tab_->controller()->ShouldCompactLeadingEdge()) {
    shape_modifier |= kCompactLeftArc;
  }

  return shape_modifier;
}

void TabStyleViewsImpl::PaintTabBackground(
    gfx::Canvas* canvas,
    TabStyle::TabSelectionState selection_state,
    bool hovered,
    std::optional<int> fill_id,
    int y_inset) const {
  // |y_inset| is only set when |fill_id| is being used.
  DCHECK(!y_inset || fill_id.has_value());

  std::optional<SkColor> group_color = tab_->GetGroupColor();

  PaintTabBackgroundFill(canvas, selection_state, hovered, fill_id, y_inset);

  const auto* widget = tab_->GetWidget();
  DCHECK(widget);
  const SkColor tab_stroke_color = widget->GetColorProvider()->GetColor(
      tab_->GetWidget()->ShouldPaintAsActive() ? kColorTabStrokeFrameActive
                                               : kColorTabStrokeFrameInactive);

  PaintBackgroundStroke(canvas, selection_state,
                        group_color.value_or(tab_stroke_color));
  PaintSeparators(canvas);
}

void TabStyleViewsImpl::PaintTabBackgroundFill(
    gfx::Canvas* canvas,
    TabStyle::TabSelectionState selection_state,
    bool hovered,
    std::optional<int> fill_id,
    int y_inset) const {
  const SkPath fill_path =
      GetPath(TabStyle::PathType::kFill, canvas->image_scale(),
              selection_state == TabStyle::TabSelectionState::kActive,
              TabStyle::RenderUnits::kPixels);
  gfx::ScopedCanvas scoped_canvas(canvas);
  const float scale = canvas->UndoDeviceScaleFactor();

  canvas->ClipPath(fill_path, true);

  if (ShouldPaintTabBackgroundColor(selection_state, fill_id.has_value())) {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(GetCurrentTabBackgroundColor(selection_state, hovered));
    canvas->DrawRect(gfx::ScaleToEnclosingRect(tab_->GetLocalBounds(), scale),
                     flags);
  }

  if (fill_id.has_value()) {
    gfx::ScopedCanvas scale_scoper(canvas);
    canvas->sk_canvas()->scale(scale, scale);
    gfx::ImageSkia* image =
        tab_->GetThemeProvider()->GetImageSkiaNamed(fill_id.value());
    TopContainerBackground::PaintThemeAlignedImage(
        canvas, tab_,
        BrowserView::GetBrowserViewForBrowser(tab_->controller()->GetBrowser()),
        image);
  }

  if (hovered) {
    PaintBackgroundHover(canvas, scale);
  }
}

void TabStyleViewsImpl::PaintBackgroundHover(gfx::Canvas* canvas,
                                             float scale) const {
  const SkPath fill_path =
      GetPath(TabStyle::PathType::kHighlight, canvas->image_scale(), true,
              TabStyle::RenderUnits::kPixels);
  canvas->ClipPath(fill_path, true);

  const SkColor hover_color =
      GetCurrentTabBackgroundColor(GetSelectionState(), /*hovered=*/true);

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(hover_color);
  canvas->DrawRect(gfx::ScaleToEnclosingRect(tab()->GetLocalBounds(), scale),
                   flags);
}

void TabStyleViewsImpl::PaintBackgroundStroke(
    gfx::Canvas* canvas,
    TabStyle::TabSelectionState selection_state,
    SkColor stroke_color) const {
  const bool is_active =
      selection_state == TabStyle::TabSelectionState::kActive;
  const int stroke_thickness = GetStrokeThickness(is_active);
  if (!stroke_thickness)
    return;

  SkPath outer_path =
      GetPath(TabStyle::PathType::kBorder, canvas->image_scale(), is_active,
              TabStyle::RenderUnits::kPixels);
  gfx::ScopedCanvas scoped_canvas(canvas);
  float scale = canvas->UndoDeviceScaleFactor();
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(stroke_color);
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(stroke_thickness * scale);
  canvas->DrawPath(outer_path, flags);
}

void TabStyleViewsImpl::PaintSeparators(gfx::Canvas* canvas) const {
  const auto separator_opacities = GetSeparatorOpacities(false);
  if (!separator_opacities.left && !separator_opacities.right)
    return;

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
  flags.setColor(separator_color(separator_opacities.left));
  canvas->DrawRoundRect(separator_bounds.leading,
                        tab_style()->GetSeparatorCornerRadius() * scale, flags);
  flags.setColor(separator_color(separator_opacities.right));
  canvas->DrawRoundRect(separator_bounds.trailing,
                        tab_style()->GetSeparatorCornerRadius() * scale, flags);
}

float TabStyleViewsImpl::GetTopCornerRadiusForWidth(int width) const {
  // Get the width of the top of the tab by subtracting the width of the outer
  // corners.
  const int ideal_radius = tab_style()->GetTopCornerRadius();
  const int top_width = width - ideal_radius * 2;

  // To maintain a round-rect appearance, ensure at least one third of the top
  // of the tab is flat.
  const float radius = top_width / 3.f;
  return std::clamp<float>(radius, 0, ideal_radius);
}

gfx::RectF TabStyleViewsImpl::ScaleAndAlignBounds(const gfx::Rect& bounds,
                                                  float scale,
                                                  int stroke_thickness) const {
  // Convert to layout bounds.  We must inset the width such that the right edge
  // of one tab's layout bounds is the same as the left edge of the next tab's;
  // this way the two tabs' separators will be drawn at the same coordinate.
  gfx::RectF aligned_bounds(bounds);
  const int bottom_corner_radius = tab_style()->GetBottomCornerRadius();
  // Note: This intentionally doesn't subtract TABSTRIP_TOOLBAR_OVERLAP from the
  // bottom inset, because we want to pixel-align the bottom of the stroke, not
  // the bottom of the overlap.
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

}  // namespace

// static
std::u16string ui::metadata::TypeConverter<TabStyle::TabColors>::ToString(
    ui::metadata::ArgType<TabStyle::TabColors> source_value) {
  return base::ASCIIToUTF16(base::StrCat(
      {"{", color_utils::SkColorToRgbaString(source_value.foreground_color),
       ",", color_utils::SkColorToRgbaString(source_value.background_color),
       ",", color_utils::SkColorToRgbaString(source_value.focus_ring_color),
       ",",
       color_utils::SkColorToRgbaString(
           source_value.close_button_focus_ring_color),
       "}"}));
}

// static
std::optional<TabStyle::TabColors> ui::metadata::TypeConverter<
    TabStyle::TabColors>::FromString(const std::u16string& source_value) {
  std::u16string trimmed_string;
  base::TrimString(source_value, u"{ }", &trimmed_string);
  std::u16string::const_iterator color_pos = trimmed_string.cbegin();
  const auto foreground_color = SkColorConverter::GetNextColor(
      color_pos, trimmed_string.cend(), color_pos);
  const auto background_color = SkColorConverter::GetNextColor(
      color_pos, trimmed_string.cend(), color_pos);
  const auto focus_ring_color = SkColorConverter::GetNextColor(
      color_pos, trimmed_string.cend(), color_pos);
  const auto close_button_focus_ring_color =
      SkColorConverter::GetNextColor(color_pos, trimmed_string.cend());
  return (foreground_color && background_color && focus_ring_color &&
          close_button_focus_ring_color)
             ? std::make_optional<TabStyle::TabColors>(
                   foreground_color.value(), background_color.value(),
                   focus_ring_color.value(),
                   close_button_focus_ring_color.value())
             : std::nullopt;
}

// static
ui::metadata::ValidStrings
ui::metadata::TypeConverter<TabStyle::TabColors>::GetValidStrings() {
  return ValidStrings();
}

// TabStyleViews ---------------------------------------------------------------

TabStyleViews::TabStyleViews() : tab_style_(TabStyle::Get()) {}

TabStyleViews::~TabStyleViews() = default;

// static
std::unique_ptr<TabStyleViews> TabStyleViews::CreateForTab(Tab* tab) {
  return std::make_unique<TabStyleViewsImpl>(tab);
}
