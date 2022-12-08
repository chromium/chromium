// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_style_views.h"

#include <algorithm>
#include <utility>

#include "base/cxx17_backports.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "cc/paint/paint_record.h"
#include "cc/paint/paint_shader.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_types.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/tabs/glow_hover_controller.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_close_button.h"
#include "chrome/browser/ui/views/tabs/tab_group_underline.h"
#include "chrome/browser/ui/views/tabs/tab_slot_controller.h"
#include "chrome/grit/theme_resources.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "third_party/skia/include/pathops/SkPathOps.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/style/typography.h"
#include "ui/views/widget/widget.h"

namespace {
// How the tab shape path is modified for selected tabs.
using ShapeModifier = int;
// No modification should be done.
constexpr ShapeModifier kNone = 0x00;
// Exclude the lower left arc.
constexpr ShapeModifier kNoLowerLeftArc = 0x01;
// Exclude the lower right arc.
constexpr ShapeModifier kNoLowerRightArc = 0x02;

// Tab style implementation for the GM2 refresh (Chrome 69).
class GM2TabStyle : public TabStyleViews {
 public:
  explicit GM2TabStyle(Tab* tab);
  GM2TabStyle(const GM2TabStyle&) = delete;
  GM2TabStyle& operator=(const GM2TabStyle&) = delete;

 protected:
  // TabStyle:
  SkPath GetPath(
      PathType path_type,
      float scale,
      bool force_active = false,
      RenderUnits render_units = RenderUnits::kPixels) const override;
  gfx::Insets GetContentsInsets() const override;
  float GetZValue() const override;
  float GetActiveOpacity() const override;
  TabActive GetApparentActiveState() const override;
  TabStyle::TabColors CalculateColors() const override;
  const gfx::FontList& GetFontList() const override;
  void PaintTab(gfx::Canvas* canvas) const override;
  void SetHoverLocation(const gfx::Point& location) override;
  void ShowHover(ShowHoverStyle style) override;
  void HideHover(HideHoverStyle style) override;

 private:
  // Gets the bounds for the leading and trailing separators for a tab.
  SeparatorBounds GetSeparatorBounds(float scale) const;

  // Returns the opacities of the separators. If |for_layout| is true, returns
  // the "layout" opacities, which ignore the effects of surrounding tabs' hover
  // effects and consider only the current tab's state.
  SeparatorOpacities GetSeparatorOpacities(bool for_layout) const;

  // Returns a single separator's opacity based on whether it is the
  // logically |leading| separator. |for_layout| has the same meaning as in
  // GetSeparatorOpacities().
  float GetSeparatorOpacity(bool for_layout, bool leading) const;

  // Helper that returns an interpolated opacity if the tab or its neighbor
  // |other_tab| is mid-hover-animation. Used in almost all cases when a
  // separator is shown, since hovering is independent of tab state.
  // |for_layout| has the same meaning as in GetSeparatorOpacities().
  float GetHoverInterpolatedSeparatorOpacity(bool for_layout,
                                             const Tab* other_tab) const;

  // Returns whether we shoould extend the hit test region for Fitts' Law.
  bool ShouldExtendHitTest() const;

  // Returns whether the hover animation is being shown.
  bool IsHoverActive() const;

  // Returns the progress (0 to 1) of the hover animation.
  double GetHoverAnimationValue() const;

  // Returns the opacity of the hover effect that should be drawn, which may not
  // be the same as GetHoverAnimationValue.
  float GetHoverOpacity() const;

  // Gets the throb value. A value of 0 indicates no throbbing.
  float GetThrobValue() const;

  // Returns the thickness of the stroke drawn around the top and sides of the
  // tab. Only active tabs may have a stroke, and not in all cases. If there
  // is no stroke, returns 0. If |should_paint_as_active| is true, the tab is
  // treated as an active tab regardless of its true current state.
  int GetStrokeThickness(bool should_paint_as_active = false) const;

  bool ShouldPaintTabBackgroundColor(TabActive active,
                                     bool has_custom_background) const;

  SkColor GetTabBackgroundColor(TabActive active) const;

  // When selected, non-active, non-hovered tabs are adjacent to each other,
  // there are anti-aliasing artifacts in the overlapped lower arc region. This
  // returns how to modify the tab shape to eliminate the lower arcs on the
  // right or left based on the state of the adjacent tab(s).
  ShapeModifier GetShapeModifier(PathType path_type) const;

  // Painting helper functions:
  void PaintInactiveTabBackground(gfx::Canvas* canvas) const;
  void PaintTabBackground(gfx::Canvas* canvas,
                          TabActive active,
                          absl::optional<int> fill_id,
                          int y_inset) const;
  void PaintTabBackgroundFill(gfx::Canvas* canvas,
                              TabActive active,
                              bool paint_hover_effect,
                              absl::optional<int> fill_id,
                              int y_inset) const;
  void PaintBackgroundStroke(gfx::Canvas* canvas,
                             TabActive active,
                             SkColor stroke_color) const;
  void PaintSeparators(gfx::Canvas* canvas) const;

  // Given a tab of width |width|, returns the radius to use for the corners.
  static float GetTopCornerRadiusForWidth(int width);

  // Scales |bounds| by scale and aligns so that adjacent tabs meet up exactly
  // during painting.
  static gfx::RectF ScaleAndAlignBounds(const gfx::Rect& bounds,
                                        float scale,
                                        int stroke_thickness);

  const raw_ptr<const Tab> tab_;

  std::unique_ptr<GlowHoverController> hover_controller_;
  gfx::FontList normal_font_;
  gfx::FontList heavy_font_;
};

void DrawHighlight(gfx::Canvas* canvas,
                   const SkPoint& p,
                   SkScalar radius,
                   SkColor color) {
  // TODO(crbug/1308932): Remove FromColor and make all SkColor4f.
  const SkColor4f colors[2] = {
      SkColor4f::FromColor(color),
      SkColor4f::FromColor(SkColorSetA(color, SK_AlphaTRANSPARENT))};
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setShader(cc::PaintShader::MakeRadialGradient(
      p, radius, colors, nullptr, 2, SkTileMode::kClamp));
  canvas->sk_canvas()->drawRect(
      SkRect::MakeXYWH(p.x() - radius, p.y() - radius, radius * 2, radius * 2),
      flags);
}

// Updates a target value, returning true if it changed.
template <class T>
bool UpdateValue(T* dest, const T& src) {
  if (*dest == src)
    return false;
  *dest = src;
  return true;
}

// GM2TabStyle -----------------------------------------------------------------

GM2TabStyle::GM2TabStyle(Tab* tab)
    : tab_(tab),
      hover_controller_(gfx::Animation::ShouldRenderRichAnimation()
                            ? new GlowHoverController(tab)
                            : nullptr),
      normal_font_(views::style::GetFont(views::style::CONTEXT_LABEL,
                                         views::style::STYLE_PRIMARY)),
      heavy_font_(views::style::GetFont(views::style::CONTEXT_BUTTON_MD,
                                        views::style::STYLE_PRIMARY)) {
  // TODO(dfried): create a new STYLE_PROMINENT or similar to use instead of
  // repurposing CONTEXT_BUTTON_MD.
}

SkPath GM2TabStyle::GetPath(PathType path_type,
                            float scale,
                            bool force_active,
                            RenderUnits render_units) const {
  const int stroke_thickness = GetStrokeThickness(force_active);

  // We'll do the entire path calculation in aligned pixels.
  // TODO(dfried): determine if we actually want to use |stroke_thickness| as
  // the inset in this case.
  gfx::RectF aligned_bounds =
      ScaleAndAlignBounds(tab_->bounds(), scale, stroke_thickness);

  if (path_type == PathType::kInteriorClip) {
    // When there is a separator, animate the clip to account for it, in sync
    // with the separator's fading.
    // TODO(pkasting): Consider crossfading the favicon instead of animating
    // the clip, especially if other children get crossfaded.
    const auto opacities = GetSeparatorOpacities(true);
    constexpr float kChildClipPadding = 2.5f;
    aligned_bounds.Inset(
        gfx::InsetsF::TLBR(0.0f, kChildClipPadding + opacities.left, 0.0f,
                           kChildClipPadding + opacities.right));
  }

  // Calculate the corner radii. Note that corner radius is based on original
  // tab width (in DIP), not our new, scaled-and-aligned bounds.
  const float radius = GetTopCornerRadiusForWidth(tab_->width()) * scale;
  float top_radius = radius;
  float bottom_radius = radius;

  // Compute |extension| as the width outside the separators.  This is a fixed
  // value equal to the normal corner radius.
  const float extension = GetCornerRadius() * scale;

  // Calculate the bounds of the actual path.
  const float left = aligned_bounds.x();
  const float right = aligned_bounds.right();
  float tab_top = aligned_bounds.y();
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
  if (path_type == PathType::kInteriorClip) {
    // Inside of the border runs |stroke_thickness| inside the outer edge.
    tab_left += stroke_adjustment;
    tab_right -= stroke_adjustment;
    tab_top += stroke_adjustment;
    top_radius -= stroke_adjustment;
  } else if (path_type == PathType::kFill || path_type == PathType::kBorder) {
    tab_left += 0.5f * stroke_adjustment;
    tab_right -= 0.5f * stroke_adjustment;
    tab_top += 0.5f * stroke_adjustment;
    top_radius -= 0.5f * stroke_adjustment;
    tab_bottom -= 0.5f * stroke_adjustment;
    bottom_radius -= 0.5f * stroke_adjustment;
  } else if (path_type == PathType::kHitTest) {
    // Outside border needs to draw its bottom line a stroke width above the
    // bottom of the tab, to line up with the stroke that runs across the rest
    // of the bottom of the tab bar (when strokes are enabled).
    tab_bottom -= stroke_adjustment;
    bottom_radius -= stroke_adjustment;
    if (ShouldExtendHitTest()) {
      extend_to_top = true;
      if (tab_->controller()->IsTabFirst(tab_)) {
        // The path is not mirrored in RTL and thus we must manually choose the
        // correct "leading" edge.
        if (base::i18n::IsRTL())
          tab_right = right;
        else
          tab_left = left;
      }
    }
  }
  const ShapeModifier shape_modifier = GetShapeModifier(path_type);
  const bool extend_left_to_bottom = shape_modifier & kNoLowerLeftArc;
  const bool extend_right_to_bottom = shape_modifier & kNoLowerRightArc;

  SkPath path;

  if (path_type == PathType::kInteriorClip) {
    // Clip path is a simple rectangle.
    path.addRect(tab_left, tab_top, tab_right, tab_bottom);
  } else if (path_type == PathType::kHighlight) {
    // The path is a round rect inset by the focus ring thickness. The
    // radius is also adjusted by the inset.
    const float inset = views::FocusRing::kDefaultHaloThickness +
                        views::FocusRing::kDefaultHaloInset;
    SkRRect rrect = SkRRect::MakeRectXY(
        SkRect::MakeLTRB(tab_left + inset, tab_top + inset, tab_right - inset,
                         tab_bottom - inset),
        radius - inset, radius - inset);
    path.addRRect(rrect);
  } else {
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
      if (tab_bottom != extended_bottom)
        path.lineTo(left, tab_bottom);

      // Draw the bottom-left corner.
      //   ╭─────────╮
      //   │ Content │
      // ┌━╝         ╰─┐
      if (extend_left_to_bottom) {
        path.lineTo(tab_left, tab_bottom);
      } else {
        path.lineTo(tab_left - bottom_radius, tab_bottom);
        path.arcTo(bottom_radius, bottom_radius, 0, SkPath::kSmall_ArcSize,
                   SkPathDirection::kCCW, tab_left, tab_bottom - bottom_radius);
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
      path.lineTo(tab_left, tab_top + top_radius);
      path.arcTo(top_radius, top_radius, 0, SkPath::kSmall_ArcSize,
                 SkPathDirection::kCW, tab_left + top_radius, tab_top);
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
      path.lineTo(tab_right - top_radius, tab_top);
      path.arcTo(top_radius, top_radius, 0, SkPath::kSmall_ArcSize,
                 SkPathDirection::kCW, tab_right, tab_top + top_radius);
    }

    if (tab_right != right) {
      // Draw the descender and bottom-right corner.
      //   ╭─────────╮
      //   │ Content ┃
      // ┌─╯         ╚━┐
      if (extend_right_to_bottom) {
        path.lineTo(tab_right, tab_bottom);
      } else {
        path.lineTo(tab_right, tab_bottom - bottom_radius);
        path.arcTo(bottom_radius, bottom_radius, 0, SkPath::kSmall_ArcSize,
                   SkPathDirection::kCCW, tab_right + bottom_radius,
                   tab_bottom);
      }
      if (tab_bottom != extended_bottom)
        path.lineTo(right, tab_bottom);
    }

    // Draw anything remaining: the descender, the bottom right horizontal
    // stroke, or the right edge of the extension, depending on which
    // conditions fired above.
    //   ╭─────────╮
    //   │ Content │
    // ┌─╯         ╰─┓
    path.lineTo(right, extended_bottom);

    if (path_type != PathType::kBorder)
      path.close();
  }

  // Convert path to be relative to the tab origin.
  gfx::PointF origin(tab_->origin());
  origin.Scale(scale);
  path.offset(-origin.x(), -origin.y());

  // Possibly convert back to DIPs.
  if (render_units == RenderUnits::kDips && scale != 1.0f)
    path.transform(SkMatrix::Scale(1.0f / scale, 1.0f / scale));

  return path;
}

gfx::Insets GM2TabStyle::GetContentsInsets() const {
  const int stroke_thickness = GetStrokeThickness();
  const int horizontal_inset = GetContentsHorizontalInsetSize();
  return gfx::Insets::TLBR(
      stroke_thickness, horizontal_inset,
      stroke_thickness + GetLayoutConstant(TABSTRIP_TOOLBAR_OVERLAP),
      horizontal_inset);
}

float GM2TabStyle::GetZValue() const {
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
  if (tab_->mouse_hovered())
    sort_value += 2.f;

  DCHECK_GE(sort_value, 0.0f);
  DCHECK_LE(sort_value, TabStyle::kMaximumZValue);

  return sort_value;
}

float GM2TabStyle::GetActiveOpacity() const {
  if (tab_->IsActive())
    return 1.0f;
  if (tab_->IsSelected())
    return kSelectedTabOpacity;
  if (tab_->mouse_hovered())
    return GetHoverOpacity();
  return 0.0f;
}

TabActive GM2TabStyle::GetApparentActiveState() const {
  // In some cases, inactive tabs may have background more like active tabs than
  // inactive tabs, so colors should be adapted to ensure appropriate contrast.
  // In particular, text should have plenty of contrast in all cases, so switch
  // to using foreground color designed for active tabs if the tab looks more
  // like an active tab than an inactive tab.
  return GetActiveOpacity() > 0.5f ? TabActive::kActive : TabActive::kInactive;
}

TabStyle::TabColors GM2TabStyle::CalculateColors() const {
  const TabActive active = GetApparentActiveState();
  const SkColor foreground_color =
      tab_->controller()->GetTabForegroundColor(active);
  const SkColor background_color = color_utils::AlphaBlend(
      GetTabBackgroundColor(TabActive::kActive),
      GetTabBackgroundColor(TabActive::kInactive), GetActiveOpacity());
  const ui::ColorId focus_ring_color = (active == TabActive::kActive)
                                           ? kColorTabFocusRingActive
                                           : kColorTabFocusRingInactive;
  const ui::ColorId close_button_focus_ring_color =
      (active == TabActive::kActive) ? kColorTabCloseButtonFocusRingActive
                                     : kColorTabCloseButtonFocusRingInactive;
  return {foreground_color, background_color, focus_ring_color,
          close_button_focus_ring_color};
}

const gfx::FontList& GM2TabStyle::GetFontList() const {
  // Don't want to have to keep re-computing this value.
  static const bool prominent_dark_mode_title =
      base::FeatureList::IsEnabled(features::kProminentDarkModeActiveTabTitle);

  if (prominent_dark_mode_title && tab_->IsActive() &&
      color_utils::IsDark(GetTabBackgroundColor(TabActive::kActive))) {
    return heavy_font_;
  }

  return normal_font_;
}

void GM2TabStyle::PaintTab(gfx::Canvas* canvas) const {
  absl::optional<int> active_tab_fill_id;
  int active_tab_y_inset = 0;
  if (tab_->GetThemeProvider()->HasCustomImage(IDR_THEME_TOOLBAR)) {
    active_tab_fill_id = IDR_THEME_TOOLBAR;
    active_tab_y_inset = GetStrokeThickness(true);
  }

  if (tab_->IsActive()) {
    PaintTabBackground(canvas, TabActive::kActive, active_tab_fill_id,
                       active_tab_y_inset);
  } else {
    PaintInactiveTabBackground(canvas);

    const float throb_value = GetThrobValue();
    if (throb_value > 0) {
      canvas->SaveLayerAlpha(base::ClampRound<uint8_t>(throb_value * 0xff),
                             tab_->GetLocalBounds());
      PaintTabBackground(canvas, TabActive::kActive, active_tab_fill_id,
                         active_tab_y_inset);
      canvas->Restore();
    }
  }
}

void GM2TabStyle::SetHoverLocation(const gfx::Point& location) {
  // There's a "glow" that gets drawn over inactive tabs based on the mouse's
  // location. There is no glow for the active tab so don't update the hover
  // controller and incur a redraw.
  if (hover_controller_ && !tab_->IsActive())
    hover_controller_->SetLocation(location);
}

void GM2TabStyle::ShowHover(ShowHoverStyle style) {
  if (!hover_controller_)
    return;

  if (style == ShowHoverStyle::kSubtle) {
    hover_controller_->SetSubtleOpacityScale(
        tab_->controller()->GetHoverOpacityForRadialHighlight());
  }
  hover_controller_->Show(style);
}

void GM2TabStyle::HideHover(HideHoverStyle style) {
  if (hover_controller_)
    hover_controller_->Hide(style);
}

TabStyle::SeparatorBounds GM2TabStyle::GetSeparatorBounds(float scale) const {
  const gfx::RectF aligned_bounds =
      ScaleAndAlignBounds(tab_->bounds(), scale, GetStrokeThickness());
  const int corner_radius = GetCornerRadius() * scale;
  gfx::SizeF separator_size(GetSeparatorSize());
  separator_size.Scale(scale);

  SeparatorBounds separator_bounds;

  separator_bounds.leading =
      gfx::RectF(aligned_bounds.x() + corner_radius,
                 aligned_bounds.y() +
                     (aligned_bounds.height() - separator_size.height()) / 2,
                 separator_size.width(), separator_size.height());

  separator_bounds.trailing = separator_bounds.leading;
  separator_bounds.trailing.set_x(aligned_bounds.right() -
                                  (corner_radius + separator_size.width()));

  gfx::PointF origin(tab_->bounds().origin());
  origin.Scale(scale);
  separator_bounds.leading.Offset(-origin.x(), -origin.y());
  separator_bounds.trailing.Offset(-origin.x(), -origin.y());

  return separator_bounds;
}

TabStyle::SeparatorOpacities GM2TabStyle::GetSeparatorOpacities(
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

float GM2TabStyle::GetSeparatorOpacity(bool for_layout, bool leading) const {
  // If the current tab is active, never show the separator.
  if (tab_->IsActive())
    return 0.0f;

  const Tab* adjacent_tab =
      tab_->controller()->GetAdjacentTab(tab_, leading ? -1 : 1);

  const Tab* left_tab = leading ? adjacent_tab : tab_.get();
  const Tab* right_tab = leading ? tab_.get() : adjacent_tab;
  const bool adjacent_to_header =
      right_tab && right_tab->group().has_value() &&
      (!left_tab || left_tab->group() != right_tab->group());

  // If the current tab is selected, default to hiding the separator. Only show
  // the separator if it's adjacent to other selected tabs.
  if (tab_->IsSelected()) {
    // If the adjacent view is actually a group header, hide the separator since
    // group headers normally cannot be selected. Group headers can become
    // selected when dragging groups, but in that case it is always the first
    // view dragging followed by the active tab (which has a group outline
    // instead of a separator). So a separator is still not necessary here.
    if (adjacent_to_header)
      return 0.0f;

    if (adjacent_tab && adjacent_tab->IsSelected())
      return GetHoverInterpolatedSeparatorOpacity(for_layout, adjacent_tab);

    return 0.0f;
  }

  // Otherwise, default to showing the separator, respecting the hover
  // animation. Only hide the separator if it's in the first slot, or in
  // certain cases if the tab has a visible background (see below).

  // If the tab has a visible background even when not selected or active, there
  // are additional cases where the separators can be hidden.
  if (tab_->controller()->HasVisibleBackgroundTabShapes()) {
    // If the tab with a visible background is in an end slot, hide the
    // separator because it doesn't need additional contrast with the tab strip
    // or the new tab button. This value isn't interpolated like the others
    // because the separator was likely already hidden: if it's animating into
    // an end slot, then the tab was probably next to a selected dragging tab
    // (see the condition below).
    if (!adjacent_tab)
      return 0.0f;

    // With visible tab background shapes, a tab next to a group header doesn't
    // need the additional contrast of a separator, because it's the tab
    // background on top of the tab strip background directly, same as if the
    // tab were in an end slot.
    if (adjacent_to_header)
      return 0.0f;

    // If the adjacent tab is selected, any separator on the current tab will be
    // "hidden" beneath the adjacent tab's background. Normally tabs will still
    // have a separator, in case the adjacent tab is dragged away and it reveals
    // an empty gap. However, tabs with visible backgrounds already have
    // sufficient contrast against the empty gap, so this contingency isn't
    // needed. Therefore, the separator is hidden only for tabs with visible
    // backgrounds.
    if (adjacent_tab->IsSelected())
      return 0.0f;
  }

  // Do not show the separator if it is to the right of a group header.
  // Otherwise, show the separator since the following group header takes up a
  // slot.
  if (adjacent_to_header) {
    if (leading)
      return 0.0f;
    return GetHoverInterpolatedSeparatorOpacity(for_layout, nullptr);
  }

  // If the tab does not have a visible background and is in the first slot,
  // do not show the separator. This once was interpolated based on the tab's
  // progress through animating into this slot, but that was removed because the
  // visual impact was minimal and
  if (!adjacent_tab && leading)
    return 0.0f;

  return GetHoverInterpolatedSeparatorOpacity(for_layout, adjacent_tab);
}

float GM2TabStyle::GetHoverInterpolatedSeparatorOpacity(
    bool for_layout,
    const Tab* other_tab) const {
  // Fade out the intervening separator while this tab or an adjacent tab is
  // hovered, which prevents sudden opacity changes when scrubbing the mouse
  // across the tabstrip. If that adjacent tab is active, don't consider its
  // hover animation value, otherwise the separator on this tab will disappear
  // while that tab is being dragged.
  auto adjacent_hover_value = [for_layout](const Tab* other_tab) {
    if (for_layout || !other_tab || other_tab->IsActive())
      return 0.0f;
    auto* tab_style = static_cast<const GM2TabStyle*>(other_tab->tab_style());
    return static_cast<float>(tab_style->GetHoverAnimationValue());
  };
  const float hover_value = GetHoverAnimationValue();
  return 1.0f - std::max(hover_value, adjacent_hover_value(other_tab));
}

bool GM2TabStyle::ShouldExtendHitTest() const {
  const views::Widget* widget = tab_->GetWidget();
  return widget->IsMaximized() || widget->IsFullscreen();
}

bool GM2TabStyle::IsHoverActive() const {
  if (!hover_controller_)
    return false;
  return hover_controller_->ShouldDraw();
}

double GM2TabStyle::GetHoverAnimationValue() const {
  if (!hover_controller_)
    return 0.0;
  return hover_controller_->GetAnimationValue();
}

float GM2TabStyle::GetHoverOpacity() const {
  // Opacity boost varies on tab width.  The interpolation is nonlinear so
  // that most tabs will fall on the low end of the opacity range, but very
  // narrow tabs will still stand out on the high end.
  const float range_start = static_cast<float>(GetStandardWidth());
  constexpr float kWidthForMaxHoverOpacity = 32.0f;
  const float value_in_range = static_cast<float>(tab_->width());
  const float t = base::clamp(
      (value_in_range - range_start) / (kWidthForMaxHoverOpacity - range_start),
      0.0f, 1.0f);
  return tab_->controller()->GetHoverOpacityForTab(t * t);
}

float GM2TabStyle::GetThrobValue() const {
  const bool is_selected = tab_->IsSelected();
  double val = is_selected ? kSelectedTabOpacity : 0;

  if (IsHoverActive()) {
    constexpr float kSelectedTabThrobScale = 0.95f - kSelectedTabOpacity;
    const float opacity = GetHoverOpacity();
    const float offset =
        is_selected ? (kSelectedTabThrobScale * opacity) : opacity;
    val += GetHoverAnimationValue() * offset;
  }

  return val;
}

int GM2TabStyle::GetStrokeThickness(bool should_paint_as_active) const {
  absl::optional<tab_groups::TabGroupId> group = tab_->group();
  if (group.has_value() && tab_->IsActive())
    return TabGroupUnderline::kStrokeThickness;

  if (tab_->IsActive() || should_paint_as_active)
    return tab_->controller()->GetStrokeThickness();

  return 0;
}

bool GM2TabStyle::ShouldPaintTabBackgroundColor(
    TabActive active,
    bool has_custom_background) const {
  // In the active case, always paint the tab background. The fill image may be
  // transparent.
  if (active == TabActive::kActive)
    return true;

  // In the inactive case, the fill image is guaranteed to be opaque, so it's
  // not necessary to paint the background when there is one.
  if (has_custom_background)
    return false;

  return tab_->GetThemeProvider()->GetDisplayProperty(
      ThemeProperties::SHOULD_FILL_BACKGROUND_TAB_COLOR);
}

SkColor GM2TabStyle::GetTabBackgroundColor(TabActive active) const {
  SkColor color = tab_->controller()->GetTabBackgroundColor(
      active, BrowserFrameActiveState::kUseCurrent);

  return color;
}

ShapeModifier GM2TabStyle::GetShapeModifier(PathType path_type) const {
  ShapeModifier shape_modifier = kNone;
  if (path_type == PathType::kFill && tab_->IsSelected() && !IsHoverActive() &&
      !tab_->IsActive()) {
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
  return shape_modifier;
}

void GM2TabStyle::PaintInactiveTabBackground(gfx::Canvas* canvas) const {
  PaintTabBackground(canvas, TabActive::kInactive,
                     tab_->controller()->GetCustomBackgroundId(
                         BrowserFrameActiveState::kUseCurrent),
                     0);
}

void GM2TabStyle::PaintTabBackground(gfx::Canvas* canvas,
                                     TabActive active,
                                     absl::optional<int> fill_id,
                                     int y_inset) const {
  // |y_inset| is only set when |fill_id| is being used.
  DCHECK(!y_inset || fill_id.has_value());

  absl::optional<SkColor> group_color = tab_->GetGroupColor();

  PaintTabBackgroundFill(canvas, active,
                         active == TabActive::kInactive && IsHoverActive(),
                         fill_id, y_inset);

  const auto* widget = tab_->GetWidget();
  DCHECK(widget);
  const SkColor tab_stroke_color = widget->GetColorProvider()->GetColor(
      tab_->controller()->ShouldPaintAsActiveFrame()
          ? kColorTabStrokeFrameActive
          : kColorTabStrokeFrameInactive);

  PaintBackgroundStroke(canvas, active, group_color.value_or(tab_stroke_color));
  PaintSeparators(canvas);
}

void GM2TabStyle::PaintTabBackgroundFill(gfx::Canvas* canvas,
                                         TabActive active,
                                         bool paint_hover_effect,
                                         absl::optional<int> fill_id,
                                         int y_inset) const {
  const SkPath fill_path = GetPath(PathType::kFill, canvas->image_scale(),
                                   active == TabActive::kActive);
  gfx::ScopedCanvas scoped_canvas(canvas);
  const float scale = canvas->UndoDeviceScaleFactor();

  canvas->ClipPath(fill_path, true);

  if (ShouldPaintTabBackgroundColor(active, fill_id.has_value())) {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(GetTabBackgroundColor(active));
    canvas->DrawRect(gfx::ScaleToEnclosingRect(tab_->GetLocalBounds(), scale),
                     flags);
  }

  if (fill_id.has_value()) {
    gfx::ScopedCanvas scale_scoper(canvas);
    canvas->sk_canvas()->scale(scale, scale);
    canvas->TileImageInt(
        *tab_->GetThemeProvider()->GetImageSkiaNamed(fill_id.value()),
        tab_->GetMirroredX() + tab_->controller()->GetBackgroundOffset(), 0, 0,
        y_inset, tab_->width(), tab_->height());
  }

  if (paint_hover_effect) {
    SkPoint hover_location(gfx::PointToSkPoint(hover_controller_->location()));
    hover_location.scale(SkFloatToScalar(scale));
    const SkScalar kMinHoverRadius = 16;
    const SkScalar radius =
        std::max(SkFloatToScalar(tab_->width() / 4.f), kMinHoverRadius);
    DrawHighlight(canvas, hover_location, radius * scale,
                  SkColorSetA(GetTabBackgroundColor(TabActive::kActive),
                              hover_controller_->GetAlpha()));
  }
}

void GM2TabStyle::PaintBackgroundStroke(gfx::Canvas* canvas,
                                        TabActive active,
                                        SkColor stroke_color) const {
  const bool is_active = active == TabActive::kActive;
  const int stroke_thickness = GetStrokeThickness(is_active);
  if (!stroke_thickness)
    return;

  SkPath outer_path =
      GetPath(TabStyle::PathType::kBorder, canvas->image_scale(), is_active);
  gfx::ScopedCanvas scoped_canvas(canvas);
  float scale = canvas->UndoDeviceScaleFactor();
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(stroke_color);
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(stroke_thickness * scale);
  canvas->DrawPath(outer_path, flags);
}

void GM2TabStyle::PaintSeparators(gfx::Canvas* canvas) const {
  const auto separator_opacities = GetSeparatorOpacities(false);
  if (!separator_opacities.left && !separator_opacities.right)
    return;

  gfx::ScopedCanvas scoped_canvas(canvas);
  const float scale = canvas->UndoDeviceScaleFactor();

  TabStyle::SeparatorBounds separator_bounds = GetSeparatorBounds(scale);

  const SkColor separator_base_color =
      tab_->controller()->GetTabSeparatorColor();
  const auto separator_color = [separator_base_color](float opacity) {
    return SkColorSetA(separator_base_color,
                       gfx::Tween::IntValueBetween(opacity, SK_AlphaTRANSPARENT,
                                                   SK_AlphaOPAQUE));
  };

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(separator_color(separator_opacities.left));
  canvas->DrawRect(separator_bounds.leading, flags);
  flags.setColor(separator_color(separator_opacities.right));
  canvas->DrawRect(separator_bounds.trailing, flags);
}

// static
float GM2TabStyle::GetTopCornerRadiusForWidth(int width) {
  // Get the width of the top of the tab by subtracting the width of the outer
  // corners.
  const int ideal_radius = GetCornerRadius();
  const int top_width = width - ideal_radius * 2;

  // To maintain a round-rect appearance, ensure at least one third of the top
  // of the tab is flat.
  const float radius = top_width / 3.f;
  return base::clamp<float>(radius, 0, ideal_radius);
}

// static
gfx::RectF GM2TabStyle::ScaleAndAlignBounds(const gfx::Rect& bounds,
                                            float scale,
                                            int stroke_thickness) {
  // Convert to layout bounds.  We must inset the width such that the right edge
  // of one tab's layout bounds is the same as the left edge of the next tab's;
  // this way the two tabs' separators will be drawn at the same coordinate.
  gfx::RectF aligned_bounds(bounds);
  const int corner_radius = GetCornerRadius();
  // Note: This intentionally doesn't subtract TABSTRIP_TOOLBAR_OVERLAP from the
  // bottom inset, because we want to pixel-align the bottom of the stroke, not
  // the bottom of the overlap.
  auto layout_insets =
      gfx::InsetsF::TLBR(stroke_thickness, corner_radius, stroke_thickness,
                         corner_radius + GetSeparatorSize().width());
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
absl::optional<TabStyle::TabColors> ui::metadata::TypeConverter<
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
             ? absl::make_optional<TabStyle::TabColors>(
                   foreground_color.value(), background_color.value(),
                   focus_ring_color.value(),
                   close_button_focus_ring_color.value())
             : absl::nullopt;
}

// static
ui::metadata::ValidStrings
ui::metadata::TypeConverter<TabStyle::TabColors>::GetValidStrings() {
  return ValidStrings();
}

// TabStyle --------------------------------------------------------------------

TabStyleViews::~TabStyleViews() = default;

// static
std::unique_ptr<TabStyleViews> TabStyleViews::CreateForTab(Tab* tab) {
  return std::make_unique<GM2TabStyle>(tab);
}

// static
int TabStyleViews::GetMinimumActiveWidth() {
  int min_active_width =
      TabCloseButton::GetGlyphSize() + GetContentsHorizontalInsetSize() * 2;
  if (base::FeatureList::IsEnabled(features::kScrollableTabStrip)) {
    return std::max(
        min_active_width,
        base::GetFieldTrialParamByFeatureAsInt(
            features::kScrollableTabStrip,
            features::kMinimumTabWidthFeatureParameterName, min_active_width));
  }
  return min_active_width;
}

// static
int TabStyleViews::GetMinimumInactiveWidth() {
  // Allow tabs to shrink until they appear to be 16 DIP wide excluding
  // outer corners.
  constexpr int kInteriorWidth = 16;
  // The overlap contains the trailing separator that is part of the interior
  // width; avoid double-counting it.
  int min_inactive_width =
      kInteriorWidth - GetSeparatorSize().width() + GetTabOverlap();

  if (base::FeatureList::IsEnabled(features::kScrollableTabStrip)) {
    return std::max(min_inactive_width,
                    base::GetFieldTrialParamByFeatureAsInt(
                        features::kScrollableTabStrip,
                        features::kMinimumTabWidthFeatureParameterName,
                        min_inactive_width));
  }

  return min_inactive_width;
}
