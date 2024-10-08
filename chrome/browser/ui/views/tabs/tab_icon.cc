// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_icon.h"

#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_renderer_data.h"
#include "chrome/browser/ui/views/dotted_icon.h"
#include "chrome/common/webui_url_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/grit/components_scaled_resources.h"
#include "content/public/common/url_constants.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_throbber.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/native_theme/native_theme.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/border.h"
#include "ui/views/cascading_property.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace {

constexpr int kAttentionIndicatorRadius = 3;
constexpr int kLoadingAnimationStrokeWidthDp = 2;

bool NetworkStateIsAnimated(TabNetworkState network_state) {
  return network_state != TabNetworkState::kNone &&
         network_state != TabNetworkState::kError;
}

}  // namespace

DEFINE_CUSTOM_ELEMENT_EVENT_TYPE(kDiscardAnimationFinishes);

// Helper class that manages the favicon crash animation.
class TabIcon::CrashAnimation : public gfx::LinearAnimation,
                                public gfx::AnimationDelegate {
 public:
  explicit CrashAnimation(TabIcon* target)
      : gfx::LinearAnimation(base::Seconds(1), 25, this), target_(target) {}
  CrashAnimation(const CrashAnimation&) = delete;
  CrashAnimation& operator=(const CrashAnimation&) = delete;
  ~CrashAnimation() override = default;

  // gfx::Animation overrides:
  void AnimateToState(double state) override {
    if (state < .5) {
      // Animate the normal icon down.
      target_->hiding_fraction_ = state * 2.0;
    } else {
      // Animate the crashed icon up.
      target_->should_display_crashed_favicon_ = true;
      target_->hiding_fraction_ = 1.0 - (state - 0.5) * 2.0;
    }
    target_->SchedulePaint();
  }

 private:
  raw_ptr<TabIcon> target_;
};

TabIcon::TabIcon()
    : AnimationDelegateViews(this),
      clock_(base::DefaultTickClock::GetInstance()),
      favicon_size_animation_(this),
      tab_discard_animation_(base::Seconds(1),
                             gfx::LinearAnimation::kDefaultFrameRate,
                             this) {
  favicon_size_animation_.SetSlideDuration(base::Milliseconds(250));

  SetCanProcessEventsWithinSubtree(false);

  // Add padding to avoid clipping the attention indicator and the increased
  // discard ring radius when kDiscardRingImprovements is enabled. Padding must
  // be symmetric on each side so that elements will anchor to the center of the
  // favicon.
  SetBorder(views::CreateEmptyBorder(kAttentionIndicatorRadius));

  const int preferred_width = gfx::kFaviconSize + GetInsets().width();
  SetPreferredSize(gfx::Size(preferred_width, preferred_width));

  // Initial state (before any data) should not be animating.
  DCHECK(!GetShowingLoadingAnimation());

  if (!gfx::Animation::ShouldRenderRichAnimation()) {
    tab_discard_animation_.SetDuration(base::TimeDelta());
    favicon_size_animation_.SetSlideDuration(base::TimeDelta());
  }

  SetProperty(views::kElementIdentifierKey, kTabIconElementId);
}

TabIcon::~TabIcon() = default;

void TabIcon::SetData(const TabRendererData& data) {
  const bool was_showing_load = GetShowingLoadingAnimation();

  inhibit_loading_animation_ = data.should_hide_throbber;
  is_monochrome_favicon_ = data.is_monochrome_favicon;
  SetIcon(data.favicon, data.should_themify_favicon);
  SetNetworkState(data.network_state);
  SetCrashed(data.IsCrashed());
  SetDiscarded(data.should_show_discard_status);
  has_tab_renderer_data_ = true;

  const bool showing_load = GetShowingLoadingAnimation();

  RefreshLayer();

  if (was_showing_load && !showing_load) {
    // Loading animation transitioning from on to off.
    loading_animation_start_time_ = base::TimeTicks();
    SchedulePaint();
  } else if (!was_showing_load && showing_load) {
    // Loading animation transitioning from off to on. The animation painting
    // function will lazily initialize the data.
    SchedulePaint();
  }
}

void TabIcon::SetActiveState(bool is_active) {
  if (is_active_tab_ != is_active) {
    is_active_tab_ = is_active;
    UpdateThemedFavicon();
  }
}

void TabIcon::SetAttention(AttentionType type, bool enabled) {
  int previous_attention_type = attention_types_;
  if (enabled) {
    attention_types_ |= static_cast<int>(type);
  } else {
    attention_types_ &= ~static_cast<int>(type);
  }

  if (attention_types_ != previous_attention_type) {
    SchedulePaint();
  }
}

bool TabIcon::GetShowingLoadingAnimation() const {
  if (inhibit_loading_animation_) {
    return false;
  }

  return NetworkStateIsAnimated(network_state_);
}

bool TabIcon::GetShowingAttentionIndicator() const {
  return attention_types_ > 0;
}

bool TabIcon::GetShowingDiscardIndicator() const {
  return was_discard_indicator_shown_;
}

void TabIcon::SetCanPaintToLayer(bool can_paint_to_layer) {
  if (can_paint_to_layer == can_paint_to_layer_) {
    return;
  }
  can_paint_to_layer_ = can_paint_to_layer;
  RefreshLayer();
}

void TabIcon::StepLoadingAnimation(const base::TimeDelta& elapsed_time) {
  if (GetShowingLoadingAnimation()) {
    SchedulePaint();
  }
}

void TabIcon::EnlargeDiscardIndicatorRadius(int radius) {
  CHECK(radius <= GetInsets().left());
  increased_discard_indicator_radius_ = radius;
}

void TabIcon::SetShouldShowDiscardIndicator(bool enabled) {
  should_show_discard_indicator_ = enabled;
  bool show_discard_indicator = is_discarded_ && should_show_discard_indicator_;
  if (was_discard_indicator_shown_ != show_discard_indicator) {
    was_discard_indicator_shown_ = show_discard_indicator;

    // Directly set animations to their end states and do not animate.
    if (show_discard_indicator) {
      tab_discard_animation_.SetCurrentValue(1);
      favicon_size_animation_.Reset(0);
    } else {
      tab_discard_animation_.SetCurrentValue(0);
      favicon_size_animation_.Reset(1);
    }
    SchedulePaint();
  }
}

void TabIcon::OnPaint(gfx::Canvas* canvas) {
  // Compute the bounds adjusted for the hiding fraction.
  gfx::Rect contents_bounds = GetContentsBounds();

  if (contents_bounds.IsEmpty()) {
    return;
  }

  gfx::Rect icon_bounds(
      GetMirroredXWithWidthInView(contents_bounds.x(), gfx::kFaviconSize),
      contents_bounds.y() +
          static_cast<int>(contents_bounds.height() * hiding_fraction_),
      std::min(gfx::kFaviconSize, contents_bounds.width()),
      std::min(gfx::kFaviconSize, contents_bounds.height()));

  // Don't paint the attention indicator during the loading animation.
  if (!GetShowingLoadingAnimation() && GetShowingAttentionIndicator() &&
      !should_display_crashed_favicon_) {
    PaintAttentionIndicatorAndIcon(canvas, GetIconToPaint(), icon_bounds);
  } else if (was_discard_indicator_shown_) {
    PaintDiscardRingAndIcon(canvas, GetIconToPaint(), icon_bounds);
  } else {
    MaybePaintFavicon(canvas, GetIconToPaint(), icon_bounds);
  }

  if (GetShowingLoadingAnimation()) {
    PaintLoadingAnimation(canvas, icon_bounds);
  }
}

void TabIcon::OnThemeChanged() {
  views::View::OnThemeChanged();
  crashed_icon_ = gfx::ImageSkia();  // Force recomputation if crashed.
  UpdateThemedFavicon();
}

void TabIcon::AnimationProgressed(const gfx::Animation* animation) {
  SchedulePaint();
}

void TabIcon::AnimationEnded(const gfx::Animation* animation) {
  RefreshLayer();
  SchedulePaint();
}

void TabIcon::PaintAttentionIndicatorAndIcon(gfx::Canvas* canvas,
                                             const gfx::ImageSkia& icon,
                                             const gfx::Rect& bounds) {
  TRACE_EVENT0("views", "TabIcon::PaintAttentionIndicatorAndIcon");

  gfx::Point circle_center(
      bounds.x() + (base::i18n::IsRTL() ? 0 : gfx::kFaviconSize),
      bounds.y() + gfx::kFaviconSize);

  // The attention indicator consists of two parts:
  // . a clear (totally transparent) part over the bottom right (or left in rtl)
  //   of the favicon. This is done by drawing the favicon to a layer, then
  //   drawing the clear part on top of the favicon.
  // . a circle in the bottom right (or left in rtl) of the favicon.
  if (!icon.isNull()) {
    canvas->SaveLayerAlpha(0xff);
    canvas->DrawImageInt(icon, 0, 0, bounds.width(), bounds.height(),
                         bounds.x(), bounds.y(), bounds.width(),
                         bounds.height(), false);
    cc::PaintFlags clear_flags;
    clear_flags.setAntiAlias(true);
    clear_flags.setBlendMode(SkBlendMode::kClear);
    const float kIndicatorCropRadius = 4.5f;
    canvas->DrawCircle(circle_center, kIndicatorCropRadius, clear_flags);
    canvas->Restore();
  }

  // Draws the actual attention indicator.
  cc::PaintFlags indicator_flags;
  indicator_flags.setColor(views::GetCascadingAccentColor(this));
  indicator_flags.setAntiAlias(true);
  canvas->DrawCircle(circle_center, kAttentionIndicatorRadius, indicator_flags);
}

void TabIcon::PaintDiscardRingAndIcon(gfx::Canvas* canvas,
                                      const gfx::ImageSkia& icon,
                                      const gfx::Rect& icon_bounds) {
  // Fades in the discard ring and smaller favicon
  MaybePaintFavicon(canvas, icon, icon_bounds);

  // Increase the bounds of the discard ring beyond the icon bounds if
  // kDiscardRingImprovements is enabled. This is safe because in the
  // constructor, we have already added insets so that the larger discard ring
  // can expand into them and won't be clipped, and the icon bounds will be
  // inside those insets.
  gfx::Rect discard_ring_bounds = icon_bounds;
  discard_ring_bounds.Outset(increased_discard_indicator_radius_);

  const ui::ColorProvider* color_provider = GetColorProvider();
  const views::Widget* widget = GetWidget();
  SkColor ring_color =
      color_provider->GetColor(widget && widget->ShouldPaintAsActive()
                                   ? kColorTabDiscardRingFrameActive
                                   : kColorTabDiscardRingFrameInactive);

  // Painting Discard Ring
  PaintRingDottedPath(
      canvas, discard_ring_bounds, ring_color,
      /*opacity_ratio=*/tab_discard_animation_.GetCurrentValue());
}

void TabIcon::PaintLoadingAnimation(gfx::Canvas* canvas, gfx::Rect bounds) {
  TRACE_EVENT0("views", "TabIcon::PaintLoadingAnimation");

  const SkColor spinning_color = views::GetCascadingAccentColor(this);
  const base::TimeTicks current_time = clock_->NowTicks();
  if (loading_animation_start_time_.is_null()) {
    loading_animation_start_time_ = current_time;
  }

  gfx::PaintThrobberSpinningWithSweepEasedIn(
      canvas, bounds, spinning_color,
      current_time - loading_animation_start_time_,
      kLoadingAnimationStrokeWidthDp);
}

gfx::ImageSkia TabIcon::GetIconToPaint() {
  CHECK(GetWidget());
  if (should_display_crashed_favicon_) {
    if (crashed_icon_.isNull()) {
      // Lazily create a themed sad tab icon.
      ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
      crashed_icon_ =
          ThemeFavicon(*rb.GetImageSkiaNamed(IDR_CRASH_SAD_FAVICON));
    }
    return crashed_icon_;
  }

  return themed_favicon_.isNull() ? favicon_.Rasterize(GetColorProvider())
                                  : themed_favicon_;
}

void TabIcon::MaybePaintFavicon(gfx::Canvas* canvas,
                                const gfx::ImageSkia& icon,
                                const gfx::Rect& bounds) {
  TRACE_EVENT0("views", "TabIcon::MaybePaintFavicon");

  if (icon.isNull()) {
    return;
  }

  if (GetShowingLoadingAnimation()) {
    // Never paint the favicon during the waiting animation.
    if (network_state_ == TabNetworkState::kWaiting) {
      return;
    }
    // Don't paint the default favicon while we're still loading.
    if (!GetNonDefaultFavicon()) {
      return;
    }
  }

  std::unique_ptr<gfx::ScopedCanvas> scoped_canvas;
  bool use_scale_filter = false;

  if (GetShowingLoadingAnimation() || favicon_size_animation_.is_animating() ||
      was_discard_indicator_shown_) {
    scoped_canvas = std::make_unique<gfx::ScopedCanvas>(canvas);
    use_scale_filter = true;
    // The favicon is initially inset with the width of the loading-animation
    // stroke + an additional dp to create some visual separation.
    const float kInitialFaviconInsetDp = 1 + kLoadingAnimationStrokeWidthDp;
    const float kInitialFaviconDiameterDp =
        gfx::kFaviconSize - 2 * kInitialFaviconInsetDp +
        (was_discard_indicator_shown_ ? 2 * increased_discard_indicator_radius_
                                      : 0);
    // This a full outset circle of the favicon square. The animation ends with
    // the entire favicon shown.
    const float kFinalFaviconDiameterDp = sqrt(2) * gfx::kFaviconSize;

    SkScalar diameter = kInitialFaviconDiameterDp;
    if (was_discard_indicator_shown_ ||
        favicon_size_animation_.is_animating()) {
      // Animate the icon based on the favicon size animation.
      diameter = gfx::Tween::FloatValueBetween(
          gfx::Tween::CalculateValue(gfx::Tween::EASE_IN,
                                     favicon_size_animation_.GetCurrentValue()),
          kInitialFaviconDiameterDp, kFinalFaviconDiameterDp);
    }
    SkPath path;
    gfx::PointF center = gfx::RectF(bounds).CenterPoint();
    path.addCircle(center.x(), center.y(), diameter / 2);
    canvas->ClipPath(path, true);
    // This scales and offsets painting so that the drawn favicon is downscaled
    // to fit in the cropping area.
    const float offset =
        (gfx::kFaviconSize -
         std::min(diameter, SkFloatToScalar(gfx::kFaviconSize))) /
        2;
    const float scale = std::min(diameter, SkFloatToScalar(gfx::kFaviconSize)) /
                        gfx::kFaviconSize;
    // Translating to/from bounds offset is done to scale around the center
    // point. This fixes RTL issues where bounds.x() is non-zero. See
    // https://crbug.com/1147408
    canvas->Translate(gfx::Vector2d(bounds.x(), bounds.y()));
    canvas->Translate(gfx::Vector2d(offset, offset));
    canvas->Scale(scale, scale);
    canvas->Translate(gfx::Vector2d(-bounds.x(), -bounds.y()));
  }

  canvas->DrawImageInt(icon, 0, 0, bounds.width(), bounds.height(), bounds.x(),
                       bounds.y(), bounds.width(), bounds.height(),
                       use_scale_filter);

  // Emits a custom event when the favicon finishes shrinking and the discard
  // ring gets painted
  if (favicon_size_animation_.GetCurrentValue() == 0.0 &&
      tab_discard_animation_.GetCurrentValue() == 1.0) {
    views::ElementTrackerViews::GetInstance()->NotifyCustomEvent(
        kDiscardAnimationFinishes, this);
  }
}

bool TabIcon::GetNonDefaultFavicon() const {
  const auto default_favicon =
      favicon::GetDefaultFaviconModel(kColorTabBackgroundActiveFrameActive);
  return !favicon_.IsEmpty() && favicon_ != default_favicon;
}

void TabIcon::SetIcon(const ui::ImageModel& icon, bool should_themify_favicon) {
  // Detect when updating to the same icon. This avoids re-theming and
  // re-painting.
  if (favicon_ == icon) {
    return;
  }

  favicon_ = icon;
  should_themify_favicon_ = should_themify_favicon;
  UpdateThemedFavicon();
}

void TabIcon::SetDiscarded(bool discarded) {
  is_discarded_ = discarded;
  bool show_discard_indicator = is_discarded_ && should_show_discard_indicator_;
  if (was_discard_indicator_shown_ != show_discard_indicator) {
    was_discard_indicator_shown_ = show_discard_indicator;
    if (show_discard_indicator) {
      tab_discard_animation_.Start();
      favicon_size_animation_.Hide();

      // Potentially show an IPH if a tab was discarded.
      Browser* browser = chrome::FindBrowserWithUiElementContext(
          views::ElementTrackerViews::GetInstance()->GetContextForView(this));
      browser->window()->MaybeShowFeaturePromo(
          feature_engagement::kIPHDiscardRingFeature);
    } else {
      tab_discard_animation_.Stop();
      favicon_size_animation_.Show();
    }
  }
}

void TabIcon::SetNetworkState(TabNetworkState network_state) {
  const bool was_animated = NetworkStateIsAnimated(network_state_);
  network_state_ = network_state;
  const bool is_animated = NetworkStateIsAnimated(network_state_);
  if (was_animated != is_animated) {
    if (was_animated && GetNonDefaultFavicon()) {
      favicon_size_animation_.Show();
    } else {
      favicon_size_animation_.Hide();
    }
  }
}

void TabIcon::SetCrashed(bool crashed) {
  if (crashed == crashed_) {
    return;
  }
  crashed_ = crashed;

  if (!crashed_) {
    // Transitioned from crashed to non-crashed.
    if (crash_animation_) {
      crash_animation_->Stop();
    }
    should_display_crashed_favicon_ = false;
    hiding_fraction_ = 0.0;
  } else {
    // Transitioned from non-crashed to crashed.
    if (!has_tab_renderer_data_) {
      // This is the initial SetData(), so show the crashed icon directly
      // without animating.
      should_display_crashed_favicon_ = true;
    } else {
      if (!crash_animation_) {
        crash_animation_ = std::make_unique<CrashAnimation>(this);
      }
      if (!crash_animation_->is_animating()) {
        crash_animation_->Start();
      }
    }
  }
  OnPropertyChanged(&crashed_, views::kPropertyEffectsPaint);
}

bool TabIcon::GetCrashed() const {
  return crashed_;
}

void TabIcon::RefreshLayer() {
  // Since the loading animation can run for a long time, paint animation to a
  // separate layer when possible to reduce repaint overhead.
  bool should_paint_to_layer =
      can_paint_to_layer_ &&
      (GetShowingLoadingAnimation() || favicon_size_animation_.is_animating() ||
       tab_discard_animation_.is_animating());
  if (should_paint_to_layer == !!layer()) {
    return;
  }

  // Change layer mode.
  if (should_paint_to_layer) {
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
  } else {
    DestroyLayer();
  }
}

gfx::ImageSkia TabIcon::ThemeFavicon(const gfx::ImageSkia& source) {
  const auto* cp = GetColorProvider();
  return favicon::ThemeFavicon(
      source, cp->GetColor(kColorToolbarButtonIcon),
      cp->GetColor(kColorTabBackgroundActiveFrameActive),
      cp->GetColor(kColorTabBackgroundInactiveFrameActive));
}

gfx::ImageSkia TabIcon::ThemeMonochromeFavicon(const gfx::ImageSkia& source) {
  const auto* cp = GetColorProvider();
  return favicon::ThemeMonochromeFavicon(
      source, is_active_tab_
                  ? cp->GetColor(kColorTabBackgroundActiveFrameActive)
                  : cp->GetColor(kColorTabBackgroundInactiveFrameActive));
}

void TabIcon::UpdateThemedFavicon() {
  if (!GetWidget()) {
    return;
  }

  if (!GetNonDefaultFavicon() || should_themify_favicon_) {
    themed_favicon_ = ThemeFavicon(favicon_.Rasterize(GetColorProvider()));
  } else if (is_monochrome_favicon_) {
    themed_favicon_ =
        ThemeMonochromeFavicon(favicon_.Rasterize(GetColorProvider()));
  } else {
    themed_favicon_ = gfx::ImageSkia();
  }

  SchedulePaint();
}

BEGIN_METADATA(TabIcon)
ADD_READONLY_PROPERTY_METADATA(bool, ShowingLoadingAnimation)
ADD_READONLY_PROPERTY_METADATA(bool, ShowingAttentionIndicator)
ADD_READONLY_PROPERTY_METADATA(bool, NonDefaultFavicon)
ADD_PROPERTY_METADATA(bool, Crashed)
END_METADATA
