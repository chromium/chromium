// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_icon.h"

#include "base/metrics/histogram_macros.h"
#include "base/time/default_tick_clock.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "cc/paint/paint_flags.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_renderer_data.h"
#include "chrome/common/webui_url_constants.h"
#include "components/grit/components_scaled_resources.h"
#include "content/public/common/url_constants.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/native_theme/native_theme.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/border.h"
#include "url/gurl.h"

namespace {

constexpr int kAttentionIndicatorRadius = 3;
constexpr int kLoadingAnimationStrokeWidthDp = 2;

// Returns whether the favicon for the given URL should be colored according to
// the browser theme.
bool ShouldThemifyFaviconForUrl(const GURL& url) {
  return url.SchemeIs(content::kChromeUIScheme) &&
         url.host_piece() != chrome::kChromeUIHelpHost &&
         url.host_piece() != chrome::kChromeUIAppLauncherPageHost;
}

bool NetworkStateIsAnimated(TabNetworkState network_state) {
  return network_state != TabNetworkState::kNone &&
         network_state != TabNetworkState::kError;
}

}  // namespace

// Helper class that manages the favicon crash animation.
class TabIcon::CrashAnimation : public gfx::LinearAnimation,
                                public gfx::AnimationDelegate {
 public:
  explicit CrashAnimation(TabIcon* target)
      : gfx::LinearAnimation(base::TimeDelta::FromSeconds(1), 25, this),
        target_(target) {}
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
  TabIcon* target_;

  DISALLOW_COPY_AND_ASSIGN(CrashAnimation);
};

TabIcon::TabIcon()
    : AnimationDelegateViews(this),
      clock_(base::DefaultTickClock::GetInstance()),
      favicon_fade_in_animation_(base::TimeDelta::FromMilliseconds(250),
                                 gfx::LinearAnimation::kDefaultFrameRate,
                                 this) {
  set_can_process_events_within_subtree(false);

  // The minimum size to avoid clipping the attention indicator.
  const int preferred_width =
      gfx::kFaviconSize + kAttentionIndicatorRadius + GetInsets().width();
  SetPreferredSize(gfx::Size(preferred_width, preferred_width));

  // Initial state (before any data) should not be animating.
  DCHECK(!ShowingLoadingAnimation());
}

TabIcon::~TabIcon() = default;

void TabIcon::SetData(const TabRendererData& data) {
  const bool was_showing_load = ShowingLoadingAnimation();

  inhibit_loading_animation_ = data.should_hide_throbber;
  SetIcon(data.visible_url, data.favicon);
  SetNetworkState(data.network_state);
  SetIsCrashed(data.IsCrashed());
  has_tab_renderer_data_ = true;

  const bool showing_load = ShowingLoadingAnimation();

  RefreshLayer();

  if (was_showing_load && !showing_load) {
    // Loading animation transitioning from on to off.
    loading_animation_start_time_ = base::TimeTicks();
    waiting_state_ = gfx::ThrobberWaitingState();
    SchedulePaint();
  } else if (!was_showing_load && showing_load) {
    // Loading animation transitioning from off to on. The animation painting
    // function will lazily initialize the data.
    SchedulePaint();
  }
}

void TabIcon::SetAttention(AttentionType type, bool enabled) {
  int previous_attention_type = attention_types_;
  if (enabled)
    attention_types_ |= static_cast<int>(type);
  else
    attention_types_ &= ~static_cast<int>(type);

  if (attention_types_ != previous_attention_type)
    SchedulePaint();
}

bool TabIcon::ShowingLoadingAnimation() const {
  if (inhibit_loading_animation_)
    return false;

  return NetworkStateIsAnimated(network_state_);
}

bool TabIcon::ShowingAttentionIndicator() const {
  return attention_types_ > 0;
}

void TabIcon::SetCanPaintToLayer(bool can_paint_to_layer) {
  if (can_paint_to_layer == can_paint_to_layer_)
    return;
  can_paint_to_layer_ = can_paint_to_layer;
  RefreshLayer();
}

void TabIcon::StepLoadingAnimation(const base::TimeDelta& elapsed_time) {
  // Only update elapsed time in the kWaiting state. This is later used as a
  // starting point for PaintThrobberSpinningAfterWaiting().
  if (network_state_ == TabNetworkState::kWaiting)
    waiting_state_.elapsed_time = elapsed_time;
  if (ShowingLoadingAnimation())
    SchedulePaint();
}

void TabIcon::OnPaint(gfx::Canvas* canvas) {
  // This is used to log to UMA. NO EARLY RETURNS!
  base::ElapsedTimer paint_timer;

  // Compute the bounds adjusted for the hiding fraction.
  gfx::Rect contents_bounds = GetContentsBounds();

  if (contents_bounds.IsEmpty())
    return;

  gfx::Rect icon_bounds(
      GetMirroredXWithWidthInView(contents_bounds.x(), gfx::kFaviconSize),
      contents_bounds.y() +
          static_cast<int>(contents_bounds.height() * hiding_fraction_),
      std::min(gfx::kFaviconSize, contents_bounds.width()),
      std::min(gfx::kFaviconSize, contents_bounds.height()));

  // Don't paint the attention indicator during the loading animation.
  if (!ShowingLoadingAnimation() && ShowingAttentionIndicator() &&
      !should_display_crashed_favicon_) {
    PaintAttentionIndicatorAndIcon(canvas, GetIconToPaint(), icon_bounds);
  } else {
    MaybePaintFavicon(canvas, GetIconToPaint(), icon_bounds);
  }

  if (ShowingLoadingAnimation())
    PaintLoadingAnimation(canvas, icon_bounds);

  UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
      "TabStrip.Tab.Icon.PaintDuration", paint_timer.Elapsed(),
      base::TimeDelta::FromMicroseconds(1),
      base::TimeDelta::FromMicroseconds(10000), 50);
}

void TabIcon::OnThemeChanged() {
  crashed_icon_ = gfx::ImageSkia();  // Force recomputation if crashed.
  if (!themed_favicon_.isNull())
    themed_favicon_ = ThemeImage(favicon_);
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
  indicator_flags.setColor(GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_ProminentButtonColor));
  indicator_flags.setAntiAlias(true);
  canvas->DrawCircle(circle_center, kAttentionIndicatorRadius, indicator_flags);
}

void TabIcon::PaintLoadingAnimation(gfx::Canvas* canvas, gfx::Rect bounds) {
  TRACE_EVENT0("views", "TabIcon::PaintLoadingAnimation");

  const ui::ThemeProvider* tp = GetThemeProvider();

  if (network_state_ == TabNetworkState::kWaiting) {
    gfx::PaintThrobberWaiting(
        canvas, bounds,
        tp->GetColor(ThemeProperties::COLOR_TAB_THROBBER_WAITING),
        waiting_state_.elapsed_time, kLoadingAnimationStrokeWidthDp);
  } else {
    const base::TimeTicks current_time = clock_->NowTicks();
    if (loading_animation_start_time_.is_null())
      loading_animation_start_time_ = current_time;

    waiting_state_.color =
        tp->GetColor(ThemeProperties::COLOR_TAB_THROBBER_WAITING);
    gfx::PaintThrobberSpinningAfterWaiting(
        canvas, bounds,
        tp->GetColor(ThemeProperties::COLOR_TAB_THROBBER_SPINNING),
        current_time - loading_animation_start_time_, &waiting_state_,
        kLoadingAnimationStrokeWidthDp);
  }
}

const gfx::ImageSkia& TabIcon::GetIconToPaint() {
  if (should_display_crashed_favicon_) {
    if (crashed_icon_.isNull()) {
      // Lazily create a themed sad tab icon.
      ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
      crashed_icon_ = ThemeImage(*rb.GetImageSkiaNamed(IDR_CRASH_SAD_FAVICON));
    }
    return crashed_icon_;
  }
  return themed_favicon_.isNull() ? favicon_ : themed_favicon_;
}

void TabIcon::MaybePaintFavicon(gfx::Canvas* canvas,
                                const gfx::ImageSkia& icon,
                                const gfx::Rect& bounds) {
  TRACE_EVENT0("views", "TabIcon::MaybePaintFavicon");

  if (icon.isNull())
    return;

  if (ShowingLoadingAnimation()) {
    // Never paint the favicon during the waiting animation.
    if (network_state_ == TabNetworkState::kWaiting)
      return;
    // Don't paint the default favicon while we're still loading.
    if (!HasNonDefaultFavicon())
      return;
  }

  std::unique_ptr<gfx::ScopedCanvas> scoped_canvas;
  bool use_scale_filter = false;

  if (ShowingLoadingAnimation() || favicon_fade_in_animation_.is_animating()) {
    scoped_canvas = std::make_unique<gfx::ScopedCanvas>(canvas);
    use_scale_filter = true;
    // The favicon is initially inset with the width of the loading-animation
    // stroke + an additional dp to create some visual separation.
    const float kInitialFaviconInsetDp = 1 + kLoadingAnimationStrokeWidthDp;
    const float kInitialFaviconDiameterDp =
        gfx::kFaviconSize - 2 * kInitialFaviconInsetDp;
    // This a full outset circle of the favicon square. The animation ends with
    // the entire favicon shown.
    const float kFinalFaviconDiameterDp = sqrt(2) * gfx::kFaviconSize;

    SkScalar diameter = kInitialFaviconDiameterDp;
    if (favicon_fade_in_animation_.is_animating()) {
      diameter += gfx::Tween::CalculateValue(
                      gfx::Tween::EASE_OUT,
                      favicon_fade_in_animation_.GetCurrentValue()) *
                  (kFinalFaviconDiameterDp - kInitialFaviconDiameterDp);
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
    canvas->Translate(gfx::Vector2d(offset, offset));
    canvas->Scale(scale, scale);
  }

  canvas->DrawImageInt(icon, 0, 0, bounds.width(), bounds.height(), bounds.x(),
                       bounds.y(), bounds.width(), bounds.height(),
                       use_scale_filter);
}

bool TabIcon::HasNonDefaultFavicon() const {
  return !favicon_.isNull() && !favicon_.BackedBySameObjectAs(
                                   favicon::GetDefaultFavicon().AsImageSkia());
}

void TabIcon::SetIcon(const GURL& url, const gfx::ImageSkia& icon) {
  // Detect when updating to the same icon. This avoids re-theming and
  // re-painting.
  if (favicon_.BackedBySameObjectAs(icon))
    return;

  favicon_ = icon;

  if (!HasNonDefaultFavicon() || ShouldThemifyFaviconForUrl(url)) {
    themed_favicon_ = ThemeImage(icon);
  } else {
    themed_favicon_ = gfx::ImageSkia();
  }

  SchedulePaint();
}

void TabIcon::SetNetworkState(TabNetworkState network_state) {
  const bool was_animated = NetworkStateIsAnimated(network_state_);
  network_state_ = network_state;
  const bool is_animated = NetworkStateIsAnimated(network_state_);
  if (was_animated != is_animated) {
    if (was_animated && HasNonDefaultFavicon()) {
      favicon_fade_in_animation_.Start();
    } else {
      favicon_fade_in_animation_.Stop();
      favicon_fade_in_animation_.SetCurrentValue(0.0);
    }
  }
}

void TabIcon::SetIsCrashed(bool is_crashed) {
  if (is_crashed == is_crashed_)
    return;
  is_crashed_ = is_crashed;

  if (!is_crashed_) {
    // Transitioned from crashed to non-crashed.
    if (crash_animation_)
      crash_animation_->Stop();
    should_display_crashed_favicon_ = false;
    hiding_fraction_ = 0.0;
  } else {
    // Transitioned from non-crashed to crashed.
    if (!has_tab_renderer_data_) {
      // This is the initial SetData(), so show the crashed icon directly
      // without animating.
      should_display_crashed_favicon_ = true;
    } else {
      if (!crash_animation_)
        crash_animation_ = std::make_unique<CrashAnimation>(this);
      if (!crash_animation_->is_animating())
        crash_animation_->Start();
    }
  }
  SchedulePaint();
}

void TabIcon::RefreshLayer() {
  // Since the loading animation can run for a long time, paint animation to a
  // separate layer when possible to reduce repaint overhead.
  bool should_paint_to_layer =
      can_paint_to_layer_ &&
      (ShowingLoadingAnimation() || favicon_fade_in_animation_.is_animating());
  if (should_paint_to_layer == !!layer())
    return;

  // Change layer mode.
  if (should_paint_to_layer) {
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
  } else {
    DestroyLayer();
  }
}

gfx::ImageSkia TabIcon::ThemeImage(const gfx::ImageSkia& source) {
  if (!GetThemeProvider()->HasCustomColor(
          ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON))
    return source;

  return gfx::ImageSkiaOperations::CreateColorMask(
      source,
      GetThemeProvider()->GetColor(ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON));
}
