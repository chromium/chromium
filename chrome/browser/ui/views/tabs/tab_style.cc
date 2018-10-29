// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_style.h"

#include <algorithm>
#include <utility>

#include "base/numerics/ranges.h"
#include "cc/paint/paint_record.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/tabs/glow_hover_controller.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_close_button.h"
#include "chrome/browser/ui/views/tabs/tab_controller.h"
#include "chrome/grit/theme_resources.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "third_party/skia/include/pathops/SkPathOps.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/widget/widget.h"

namespace {

// Cache of pre-painted backgrounds for tabs.
class BackgroundCache {
 public:
  BackgroundCache() = default;
  ~BackgroundCache() = default;

  // Updates the cache key with the new values.
  // Returns true if any of the values changed.
  bool UpdateCacheKey(float scale,
                      const gfx::Size& size,
                      SkColor active_color,
                      SkColor inactive_color,
                      SkColor stroke_color,
                      float stroke_thickness);

  const sk_sp<cc::PaintRecord>& fill_record() const { return fill_record_; }
  void set_fill_record(sk_sp<cc::PaintRecord>&& record) {
    fill_record_ = record;
  }

  const sk_sp<cc::PaintRecord>& stroke_record() const { return stroke_record_; }
  void set_stroke_record(sk_sp<cc::PaintRecord>&& record) {
    stroke_record_ = record;
  }

 private:
  // Parameters used to construct the PaintRecords.
  float scale_ = 0.f;
  gfx::Size size_;
  SkColor active_color_ = 0;
  SkColor inactive_color_ = 0;
  SkColor stroke_color_ = 0;
  float stroke_thickness_ = 0.f;

  sk_sp<cc::PaintRecord> fill_record_;
  sk_sp<cc::PaintRecord> stroke_record_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundCache);
};

// Tab style implementation for the GM2 refresh (Chrome 69).
class GM2TabStyle : public TabStyle {
 public:
  explicit GM2TabStyle(const Tab* tab);

 protected:
  // TabStyle:
  gfx::Path GetPath(
      PathType path_type,
      float scale,
      bool force_active = false,
      RenderUnits render_units = RenderUnits::kPixels) const override;
  SeparatorBounds GetSeparatorBounds(float scale) const override;
  gfx::Insets GetContentsInsets() const override;
  int GetStrokeThickness(bool should_paint_as_active = false) const override;
  SeparatorOpacities GetSeparatorOpacities(bool for_layout) const override;
  void PaintTab(gfx::Canvas* canvas, const gfx::Path& clip) const override;

 private:
  // Returns whether we shoould extend the hit test region for Fitts' Law.
  bool ShouldExtendHitTest() const;

  // Painting helper functions:
  void PaintInactiveTabBackground(gfx::Canvas* canvas,
                                  const gfx::Path& clip) const;
  void PaintTabBackground(gfx::Canvas* canvas,
                          bool active,
                          int fill_id,
                          int y_inset,
                          const gfx::Path* clip) const;
  void PaintTabBackgroundFill(gfx::Canvas* canvas,
                              bool active,
                              bool paint_hover_effect,
                              SkColor active_color,
                              SkColor inactive_color,
                              int fill_id,
                              int y_inset) const;
  void PaintBackgroundStroke(gfx::Canvas* canvas,
                             bool active,
                             SkColor stroke_color) const;
  void PaintSeparators(gfx::Canvas* canvas) const;

  // Given a tab of width |width|, returns the radius to use for the corners.
  static float GetTopCornerRadiusForWidth(int width);

  // Scales |bounds| by scale and aligns so that adjacent tabs meet up exactly
  // during painting.
  static gfx::RectF ScaleAndAlignBounds(const gfx::Rect& bounds,
                                        float scale,
                                        int stroke_thickness);

  const Tab* const tab_;

  // Cache of the paint output for tab backgrounds.
  mutable BackgroundCache background_active_cache_;
  mutable BackgroundCache background_inactive_cache_;
};

// Thickness in DIPs of the separator painted on the left and right edges of
// the tab.
constexpr int kSeparatorThickness = 1;

// Returns the radius of the outer corners of the tab shape.
int GetCornerRadius() {
  return ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
      views::EMPHASIS_HIGH);
}

// Returns how far from the leading and trailing edges of a tab the contents
// should actually be laid out.
int GetContentsHorizontalInsetSize() {
  return GetCornerRadius() * 2;
}

// Returns the height of the separator between tabs.
int GetSeparatorHeight() {
  return ui::MaterialDesignController::touch_ui() ? 24 : 20;
}

void DrawHighlight(gfx::Canvas* canvas,
                   const SkPoint& p,
                   SkScalar radius,
                   SkColor color) {
  const SkColor colors[2] = {color, SkColorSetA(color, 0)};
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setShader(cc::PaintShader::MakeRadialGradient(
      p, radius, colors, nullptr, 2, SkShader::kClamp_TileMode));
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

// BackgroundCache -------------------------------------------------------------

bool BackgroundCache::UpdateCacheKey(float scale,
                                     const gfx::Size& size,
                                     SkColor active_color,
                                     SkColor inactive_color,
                                     SkColor stroke_color,
                                     float stroke_thickness) {
  // Use | instead of || to prevent lazy evaluation.
  return UpdateValue(&scale_, scale) | UpdateValue(&size_, size) |
         UpdateValue(&active_color_, active_color) |
         UpdateValue(&inactive_color_, inactive_color) |
         UpdateValue(&stroke_color_, stroke_color) |
         UpdateValue(&stroke_thickness_, stroke_thickness);
}

// GM2TabStyle -----------------------------------------------------------------

GM2TabStyle::GM2TabStyle(const Tab* tab) : tab_(tab) {}

gfx::Path GM2TabStyle::GetPath(PathType path_type,
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
    aligned_bounds.Inset(gfx::InsetsF(0.0f, kChildClipPadding + opacities.left,
                                      0.0f,
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
  } else if (path_type == PathType::kHitTest ||
             path_type == PathType::kExteriorClip) {
    // Outside border needs to draw its bottom line a stroke width above the
    // bottom of the tab, to line up with the stroke that runs across the rest
    // of the bottom of the tab bar (when strokes are enabled).
    tab_bottom -= stroke_adjustment;
    bottom_radius -= stroke_adjustment;
  }
  const bool extend_to_top =
      (path_type == PathType::kHitTest) && ShouldExtendHitTest();

  // When the radius shrinks, it leaves a gap between the bottom corners and the
  // edge of the tab. Make sure we account for this - and for any adjustment we
  // may have made to the location of the tab!
  const float corner_gap = (right - tab_right) - bottom_radius;

  gfx::Path path;

  if (path_type == PathType::kInteriorClip) {
    // Clip path is a simple rectangle.
    path.addRect(tab_left, tab_top, tab_right, tab_bottom);
  } else if (path_type == PathType::kHighlight) {
    // The path is a round rect inset by the focus ring thickness. The
    // radius is also adjusted by the inset.
    const float inset = views::PlatformStyle::kFocusHaloThickness +
                        views::PlatformStyle::kFocusHaloInset;
    SkRRect rrect = SkRRect::MakeRectXY(
        SkRect::MakeLTRB(tab_left + inset, tab_top + inset, tab_right - inset,
                         tab_bottom - inset),
        radius - inset, radius - inset);
    path.addRRect(rrect);
  } else {
    // We will go clockwise from the lower left. We start in the overlap region,
    // preventing a gap between toolbar and tabstrip.
    // TODO(dfried): verify that the we actually want to start the stroke for
    // the exterior path outside the region; we might end up rendering an
    // extraneous descending pixel on displays with odd scaling and nonzero
    // stroke width.

    // Start with the left side of the shape.

    // Draw everything left of the bottom-left corner of the tab.
    //   ╭─────────╮
    //   │ Content │
    // ┏━╯         ╰─┐
    path.moveTo(left, extended_bottom);
    path.lineTo(left, tab_bottom);
    path.lineTo(left + corner_gap, tab_bottom);

    // Draw the bottom-left arc.
    //   ╭─────────╮
    //   │ Content │
    // ┌─╝         ╰─┐
    path.arcTo(bottom_radius, bottom_radius, 0, SkPath::kSmall_ArcSize,
               SkPath::kCCW_Direction, tab_left, tab_bottom - bottom_radius);

    // Draw the ascender and top arc, if present.
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
                 SkPath::kCW_Direction, tab_left + top_radius, tab_top);
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
                 SkPath::kCW_Direction, tab_right, tab_top + top_radius);
    }

    // Draw the descender and bottom-right arc.
    //   ╭─────────╮
    //   │ Content ┃
    // ┌─╯         ╚─┐
    path.lineTo(tab_right, tab_bottom - bottom_radius);
    path.arcTo(bottom_radius, bottom_radius, 0, SkPath::kSmall_ArcSize,
               SkPath::kCCW_Direction, right - corner_gap, tab_bottom);

    // Draw everything right of the bottom-right corner of the tab.
    //   ╭─────────╮
    //   │ Content │
    // ┌─╯         ╰━┓
    path.lineTo(right, tab_bottom);
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
    path.transform(SkMatrix::MakeScale(1.f / scale));

  return path;
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

gfx::Insets GM2TabStyle::GetContentsInsets() const {
  const int stroke_thickness = GetStrokeThickness();
  const int horizontal_inset = GetContentsHorizontalInsetSize();
  return gfx::Insets(
      stroke_thickness, horizontal_inset,
      stroke_thickness + GetLayoutConstant(TABSTRIP_TOOLBAR_OVERLAP),
      horizontal_inset);
}

int GM2TabStyle::GetStrokeThickness(bool should_paint_as_active) const {
  return (tab_->IsActive() || should_paint_as_active)
             ? tab_->controller()->GetStrokeThickness()
             : 0;
}

TabStyle::SeparatorOpacities GM2TabStyle::GetSeparatorOpacities(
    bool for_layout) const {
  // Something should visually separate tabs from each other and any adjacent
  // new tab button.  Normally, active and hovered tabs draw distinct shapes
  // (via different background colors) and thus need no separators, while
  // background tabs need separators between them.  In single-tab mode, the
  // active tab has no visible shape and thus needs separators on any side with
  // an adjacent new tab button.  (The other sides will be faded out below.)
  float leading_opacity, trailing_opacity;
  if (tab_->controller()->SingleTabMode()) {
    leading_opacity = trailing_opacity = 1.f;
  } else if (tab_->IsActive()) {
    leading_opacity = trailing_opacity = 0;
  } else {
    // Fade out the trailing separator while this tab or the subsequent tab is
    // hovered.  If the subsequent tab is active, don't consider its hover
    // animation value, lest the trailing separator on this tab disappear while
    // the subsequent tab is being dragged.
    const float hover_value = tab_->hover_controller()->GetAnimationValue();
    const Tab* subsequent_tab = tab_->controller()->GetAdjacentTab(tab_, 1);
    const float subsequent_hover =
        !for_layout && subsequent_tab && !subsequent_tab->IsActive()
            ? float{subsequent_tab->hover_controller()->GetAnimationValue()}
            : 0;
    trailing_opacity = 1.f - std::max(hover_value, subsequent_hover);

    // The leading separator need not consider the previous tab's hover value,
    // since if there is a previous tab that's hovered and not being dragged,
    // it will draw atop this tab.
    leading_opacity = 1.f - hover_value;

    const Tab* previous_tab = tab_->controller()->GetAdjacentTab(tab_, -1);
    if (tab_->IsSelected()) {
      // Since this tab is selected, its shape will be visible against adjacent
      // unselected tabs, so remove the separator in those cases.
      if (previous_tab && !previous_tab->IsSelected())
        leading_opacity = 0;
      if (subsequent_tab && !subsequent_tab->IsSelected())
        trailing_opacity = 0;
    } else if (tab_->controller()->HasVisibleBackgroundTabShapes()) {
      // Since this tab is unselected, adjacent selected tabs will normally
      // paint atop it, covering the separator.  But if the user drags those
      // selected tabs away, the exposed region looks like the window frame; and
      // since background tab shapes are visible, there should be no separator.
      // TODO(pkasting): https://crbug.com/876599  When a tab is animating
      // into this gap, we should adjust its separator opacities as well.
      if (previous_tab && previous_tab->IsSelected())
        leading_opacity = 0;
      if (subsequent_tab && subsequent_tab->IsSelected())
        trailing_opacity = 0;
    }
  }

  // For the first or last tab in the strip, fade the leading or trailing
  // separator based on the NTB position and how close to the target bounds this
  // tab is.  In the steady state, this hides separators on the opposite end of
  // the strip from the NTB; it fades out the separators as tabs animate into
  // these positions, after they pass by the other tabs; and it snaps the
  // separators to full visibility immediately when animating away from these
  // positions, which seems desirable.
  const NewTabButtonPosition ntb_position =
      tab_->controller()->GetNewTabButtonPosition();
  const gfx::Rect target_bounds =
      tab_->controller()->GetTabAnimationTargetBounds(tab_);
  const int tab_width = std::max(tab_->width(), target_bounds.width());
  const float target_opacity =
      float{std::min(std::abs(tab_->x() - target_bounds.x()), tab_width)} /
      tab_width;
  // If the tab shapes are visible, never draw end separators.
  const bool always_hide_separators_on_ends =
      tab_->controller()->HasVisibleBackgroundTabShapes();
  if (tab_->controller()->IsFirstVisibleTab(tab_) &&
      (ntb_position != LEADING || always_hide_separators_on_ends))
    leading_opacity = target_opacity;
  if (tab_->controller()->IsLastVisibleTab(tab_) &&
      (ntb_position != AFTER_TABS || always_hide_separators_on_ends))
    trailing_opacity = target_opacity;

  // Return the opacities in physical order, rather than logical.
  if (base::i18n::IsRTL())
    std::swap(leading_opacity, trailing_opacity);
  return {leading_opacity, trailing_opacity};
}

void GM2TabStyle::PaintTab(gfx::Canvas* canvas, const gfx::Path& clip) const {
  int active_tab_fill_id = 0;
  int active_tab_y_inset = 0;
  if (tab_->GetThemeProvider()->HasCustomImage(IDR_THEME_TOOLBAR)) {
    active_tab_fill_id = IDR_THEME_TOOLBAR;
    active_tab_y_inset = GetStrokeThickness(true);
  }

  if (tab_->IsActive()) {
    PaintTabBackground(canvas, true /* active */, active_tab_fill_id,
                       active_tab_y_inset, nullptr /* clip */);
  } else {
    PaintInactiveTabBackground(canvas, clip);

    const float throb_value = tab_->GetThrobValue();
    if (throb_value > 0) {
      canvas->SaveLayerAlpha(gfx::ToRoundedInt(throb_value * 0xff),
                             tab_->GetLocalBounds());
      PaintTabBackground(canvas, true /* active */, active_tab_fill_id,
                         active_tab_y_inset, nullptr /* clip */);
      canvas->Restore();
    }
  }
}

bool GM2TabStyle::ShouldExtendHitTest() const {
  const views::Widget* widget = tab_->GetWidget();
  return widget->IsMaximized() || widget->IsFullscreen();
}

void GM2TabStyle::PaintInactiveTabBackground(gfx::Canvas* canvas,
                                             const gfx::Path& clip) const {
  bool has_custom_image;
  int fill_id = tab_->controller()->GetBackgroundResourceId(&has_custom_image);
  if (!has_custom_image)
    fill_id = 0;

  PaintTabBackground(canvas, false /* active */, fill_id, 0,
                     tab_->controller()->MaySetClip() ? &clip : nullptr);
}

void GM2TabStyle::PaintTabBackground(gfx::Canvas* canvas,
                                     bool active,
                                     int fill_id,
                                     int y_inset,
                                     const gfx::Path* clip) const {
  // |y_inset| is only set when |fill_id| is being used.
  DCHECK(!y_inset || fill_id);

  const SkColor active_color =
      tab_->controller()->GetTabBackgroundColor(TAB_ACTIVE);
  const SkColor inactive_color =
      tab_->GetThemeProvider()->GetDisplayProperty(
          ThemeProperties::SHOULD_FILL_BACKGROUND_TAB_COLOR)
          ? tab_->controller()->GetTabBackgroundColor(TAB_INACTIVE)
          : SK_ColorTRANSPARENT;
  const SkColor stroke_color =
      tab_->controller()->GetToolbarTopSeparatorColor();
  const bool paint_hover_effect =
      !active && tab_->hover_controller()->ShouldDraw();
  const float scale = canvas->image_scale();
  const float stroke_thickness = GetStrokeThickness(active);

  // If there is a |fill_id| we don't try to cache. This could be improved but
  // would require knowing then the image from the ThemeProvider had been
  // changed, and invalidating when the tab's x-coordinate or background_offset_
  // changed.
  //
  // If |paint_hover_effect|, we don't try to cache since hover effects change
  // on every invalidation and we would need to invalidate the cache based on
  // the hover states.
  //
  // Finally, we don't cache for non-integral scale factors, since tabs draw
  // with slightly different offsets so as to pixel-align the layout rect (see
  // ScaleAndAlignBounds()).
  if (fill_id || paint_hover_effect || (std::trunc(scale) != scale)) {
    PaintTabBackgroundFill(canvas, active, paint_hover_effect, active_color,
                           inactive_color, fill_id, y_inset);
    if (stroke_thickness > 0) {
      gfx::ScopedCanvas scoped_canvas(clip ? canvas : nullptr);
      if (clip)
        canvas->sk_canvas()->clipPath(*clip, SkClipOp::kDifference, true);
      PaintBackgroundStroke(canvas, active, stroke_color);
    }
  } else {
    const gfx::Size& size = tab_->size();
    BackgroundCache& cache =
        active ? background_active_cache_ : background_inactive_cache_;

    // If any of the cache key values have changed, update the cached records.
    if (cache.UpdateCacheKey(scale, size, active_color, inactive_color,
                             stroke_color, stroke_thickness)) {
      cc::PaintRecorder recorder;
      {
        gfx::Canvas cache_canvas(
            recorder.beginRecording(size.width(), size.height()), scale);
        PaintTabBackgroundFill(&cache_canvas, active, paint_hover_effect,
                               active_color, inactive_color, fill_id, y_inset);
        cache.set_fill_record(recorder.finishRecordingAsPicture());
      }
      if (stroke_thickness > 0) {
        gfx::Canvas cache_canvas(
            recorder.beginRecording(size.width(), size.height()), scale);
        PaintBackgroundStroke(&cache_canvas, active, stroke_color);
        cache.set_stroke_record(recorder.finishRecordingAsPicture());
      }
    }

    canvas->sk_canvas()->drawPicture(cache.fill_record());
    if (stroke_thickness > 0) {
      gfx::ScopedCanvas scoped_canvas(clip ? canvas : nullptr);
      if (clip)
        canvas->sk_canvas()->clipPath(*clip, SkClipOp::kDifference, true);
      canvas->sk_canvas()->drawPicture(cache.stroke_record());
    }
  }

  PaintSeparators(canvas);
}

void GM2TabStyle::PaintTabBackgroundFill(gfx::Canvas* canvas,
                                         bool active,
                                         bool paint_hover_effect,
                                         SkColor active_color,
                                         SkColor inactive_color,
                                         int fill_id,
                                         int y_inset) const {
  const gfx::Path fill_path =
      GetPath(PathType::kFill, canvas->image_scale(), active);
  gfx::ScopedCanvas scoped_canvas(canvas);
  const float scale = canvas->UndoDeviceScaleFactor();

  canvas->ClipPath(fill_path, true);

  // In the active case, always fill the tab with its bg color first in case the
  // image is transparent. In the inactive case, the image is guaranteed to be
  // opaque, so it's only necessary to fill the color when there's no image.
  if (active || !fill_id) {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(active ? active_color : inactive_color);
    canvas->DrawRect(gfx::ScaleToEnclosingRect(tab_->GetLocalBounds(), scale),
                     flags);
  }

  if (fill_id) {
    gfx::ScopedCanvas scale_scoper(canvas);
    canvas->sk_canvas()->scale(scale, scale);
    canvas->TileImageInt(*tab_->GetThemeProvider()->GetImageSkiaNamed(fill_id),
                         tab_->GetMirroredX() + tab_->background_offset(), 0, 0,
                         y_inset, tab_->width(), tab_->height());
  }

  if (paint_hover_effect) {
    SkPoint hover_location(
        gfx::PointToSkPoint(tab_->hover_controller()->location()));
    hover_location.scale(SkFloatToScalar(scale));
    const SkScalar kMinHoverRadius = 16;
    const SkScalar radius =
        std::max(SkFloatToScalar(tab_->width() / 4.f), kMinHoverRadius);
    DrawHighlight(
        canvas, hover_location, radius * scale,
        SkColorSetA(active_color, tab_->hover_controller()->GetAlpha()));
  }
}

void GM2TabStyle::PaintBackgroundStroke(gfx::Canvas* canvas,
                                        bool active,
                                        SkColor stroke_color) const {
  gfx::Path outer_path =
      GetPath(TabStyle::PathType::kBorder, canvas->image_scale(), active);
  gfx::ScopedCanvas scoped_canvas(canvas);
  float scale = canvas->UndoDeviceScaleFactor();
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(stroke_color);
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(GetStrokeThickness(active) * scale);
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
  return base::ClampToRange<float>(radius, 0, ideal_radius);
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
  gfx::InsetsF layout_insets(stroke_thickness, corner_radius, stroke_thickness,
                             corner_radius + kSeparatorThickness);
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
  aligned_bounds.Inset(-layout_insets.Scale(scale));
  return aligned_bounds;
}

}  // namespace

// TabStyle --------------------------------------------------------------------

TabStyle::~TabStyle() = default;

// static
TabStyle* TabStyle::CreateForTab(const Tab* tab) {
  return new GM2TabStyle(tab);
}

// static
int TabStyle::GetMinimumActiveWidth() {
  return TabCloseButton::GetWidth() + GetContentsHorizontalInsetSize() * 2;
}

// static
int TabStyle::GetMinimumInactiveWidth() {
  // Allow tabs to shrink until they appear to be 16 DIP wide excluding
  // outer corners.
  constexpr int kInteriorWidth = 16;
  // The overlap contains the trailing separator that is part of the interior
  // width; avoid double-counting it.
  return kInteriorWidth - kSeparatorThickness + GetTabOverlap();
}

// static
int TabStyle::GetStandardWidth() {
  // The standard tab width is 240 DIP including both separators.
  constexpr int kTabWidth = 240;
  // The overlap includes one separator, so subtract it here.
  return kTabWidth + GetTabOverlap() - kSeparatorThickness;
}

// static
int TabStyle::GetPinnedWidth() {
  constexpr int kTabPinnedContentWidth = 23;
  return kTabPinnedContentWidth + GetContentsHorizontalInsetSize() * 2;
}

// static
int TabStyle::GetTabOverlap() {
  return GetCornerRadius() * 2 + kSeparatorThickness;
}

// static
int TabStyle::GetDragHandleExtension(int height) {
  return (height - GetSeparatorHeight()) / 2 - 1;
}

// static
gfx::Insets TabStyle::GetTabInternalPadding() {
  return gfx::Insets(0, GetCornerRadius());
}

// static
gfx::Size TabStyle::GetSeparatorSize() {
  return gfx::Size(kSeparatorThickness, GetSeparatorHeight());
}
