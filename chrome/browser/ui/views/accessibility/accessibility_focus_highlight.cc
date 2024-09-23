// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/accessibility/accessibility_focus_highlight.h"

#include <algorithm>

#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/focused_node_details.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_animation_observer.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

namespace ui {
class Compositor;
}

namespace {

// The number of pixels of padding between the outer edge of the focused
// element's bounding box and the inner edge of the inner focus ring.
constexpr int kPadding = 8;

// The size of the border radius of the innermost focus highlight ring.
constexpr int kBorderRadius = 4;

// The stroke width, in , of the innermost focus ring, and each line drawn
// as part of the focus ring gradient effect.
constexpr int kStrokeWidth = 2;

// The thickness, in px, of the outer focus ring gradient.
constexpr int kGradientWidth = 9;

// The padding between the bounds of the layer and the bounds of the
// drawn focus ring, in px. If it's zero the focus ring might be clipped.
constexpr int kLayerPadding = 2;

// Total px between the edge of the node and the edge of the layer.
constexpr int kTotalLayerPadding =
    kPadding + kStrokeWidth + kGradientWidth + kLayerPadding;

// The amount of time it should take for the highlight to fade in.
constexpr auto kFadeInTime = base::Milliseconds(100);

// The amount of time the highlight should persist before beginning to fade.
constexpr auto kHighlightPersistTime = base::Seconds(1);

// The amount of time it should take for the highlight to fade out.
constexpr auto kFadeOutTime = base::Milliseconds(600);

}  // namespace

// static
base::TimeDelta AccessibilityFocusHighlight::fade_in_time_;

// static
base::TimeDelta AccessibilityFocusHighlight::persist_time_;

// static
base::TimeDelta AccessibilityFocusHighlight::fade_out_time_;

// static
bool AccessibilityFocusHighlight::skip_activation_check_for_testing_ = false;

// static
bool AccessibilityFocusHighlight::use_default_color_for_testing_ = false;

// static
bool AccessibilityFocusHighlight::no_fade_for_testing_ = false;

AccessibilityFocusHighlight::AccessibilityFocusHighlight(
    BrowserView* browser_view)
    : browser_view_(browser_view) {
  DCHECK(browser_view);

  // Listen for preference changes.
  profile_pref_registrar_.Init(browser_view_->browser()->profile()->GetPrefs());
  profile_pref_registrar_.Add(
      prefs::kAccessibilityFocusHighlightEnabled,
      base::BindRepeating(&AccessibilityFocusHighlight::AddOrRemoveObservers,
                          base::Unretained(this)));

  // Initialise focus and tab strip model observers based on current
  // preferences.
  AddOrRemoveObservers();

  // One-time initialization of statics the first time an instance is created.
  if (fade_in_time_.is_zero()) {
    fade_in_time_ = kFadeInTime;
    persist_time_ = kHighlightPersistTime;
    fade_out_time_ = kFadeOutTime;
  }
}

AccessibilityFocusHighlight::~AccessibilityFocusHighlight() {
  if (compositor_ && compositor_->HasAnimationObserver(this))
    compositor_->RemoveAnimationObserver(this);
}

// static
void AccessibilityFocusHighlight::SetNoFadeForTesting() {
  no_fade_for_testing_ = true;
}

// static
void AccessibilityFocusHighlight::SkipActivationCheckForTesting() {
  skip_activation_check_for_testing_ = true;
}

// static
void AccessibilityFocusHighlight::UseDefaultColorForTesting() {
  use_default_color_for_testing_ = true;
}

// static
ui::Layer* AccessibilityFocusHighlight::GetLayerForTesting() {
  return layer_.get();
}

SkColor AccessibilityFocusHighlight::GetHighlightColor() {
  const ui::ColorProvider* color_provider = browser_view_->GetColorProvider();
#if !BUILDFLAG(IS_MAC)
  // Match behaviour with renderer_preferences_util::UpdateFromSystemSettings
  // setting prefs->focus_ring_color
  return color_provider->GetColor(kColorFocusHighlightDefault);
#else
  SkColor theme_color =
      color_provider->GetColor(ui::kColorFocusableBorderFocused);

  if (theme_color == SK_ColorTRANSPARENT || use_default_color_for_testing_)
    return color_provider->GetColor(kColorFocusHighlightDefault);

  return theme_color;
#endif
}

void AccessibilityFocusHighlight::CreateOrUpdateLayer(gfx::Rect node_bounds) {
  // Find the layer of our owning BrowserView.
  views::Widget* widget = browser_view_->GetWidget();
  DCHECK(widget);
  ui::Layer* root_layer = widget->GetLayer();

  // Create the layer if needed.
  if (!layer_) {
    layer_ = std::make_unique<ui::Layer>(ui::LAYER_TEXTURED);
    layer_->SetName("AccessibilityFocusHighlight");
    layer_->SetFillsBoundsOpaquely(false);
    root_layer->Add(layer_.get());
    // Initially transparent so it can fade in.
    layer_->SetOpacity(0.0f);
    layer_->set_delegate(this);
    layer_created_time_ = base::TimeTicks::Now();
  }

  // Each time this is called, move it to the top in case new layers
  // have been added since we created this layer.
  layer_->parent()->StackAtTop(layer_.get());

  // Update the bounds.
  // Outset the bounds of the layer by the total width of the focus highlight,
  // plus the extra padding to ensure the highlight isn't clipped.
  gfx::Rect layer_bounds = node_bounds;
  int padding = kTotalLayerPadding;
  layer_bounds.Inset(-padding);

  layer_->SetBounds(layer_bounds);

  // Set node_bounds_ and make their position relative to the layer, instead of
  // the page.
  node_bounds_ = node_bounds;
  node_bounds_.set_x(padding);
  node_bounds_.set_y(padding);

  // Update the timestamp of the last time the layer changed.
  focus_last_changed_time_ = base::TimeTicks::Now();

  // Ensure it's repainted.
  gfx::Rect bounds(0, 0, layer_bounds.width(), layer_bounds.height());
  layer_->SchedulePaint(bounds);

  // Schedule the animation observer, or update it if needed.
  display::Display display =
      display::Screen::GetScreen()->GetDisplayMatching(layer_bounds);
  ui::Compositor* compositor = root_layer->GetCompositor();
  if (compositor != compositor_) {
    if (compositor_ && compositor_->HasAnimationObserver(this))
      compositor_->RemoveAnimationObserver(this);
    compositor_ = compositor;
    if (compositor_ && !compositor_->HasAnimationObserver(this))
      compositor_->AddAnimationObserver(this);
  }
}

void AccessibilityFocusHighlight::RemoveLayer() {
  if (no_fade_for_testing_)
    return;

  layer_.reset();
  if (compositor_) {
    compositor_->RemoveAnimationObserver(this);
    compositor_ = nullptr;
  }
}

void AccessibilityFocusHighlight::AddOrRemoveObservers() {
  Browser* browser = browser_view_->browser();
  PrefService* prefs = browser->profile()->GetPrefs();
  TabStripModel* tab_strip_model = browser->tab_strip_model();

  if (prefs->GetBoolean(prefs::kAccessibilityFocusHighlightEnabled)) {
    // Listen for focus changes. Automatically deregisters when destroyed,
    // or when the preference toggles off.
    // TODO(crbug.com/40758630): This will fire even for focused-element changes
    // in windows other than browser_view_, which might not be ideal behavior.
    focus_changed_subscription_ =
        content::BrowserAccessibilityState::GetInstance()
            ->RegisterFocusChangedCallback(base::BindRepeating(
                &AccessibilityFocusHighlight::OnFocusChangedInPage,
                base::Unretained(this)));

    tab_strip_model->AddObserver(this);
    return;
  } else {
    focus_changed_subscription_.reset();
    tab_strip_model->RemoveObserver(this);
  }
}

void AccessibilityFocusHighlight::OnFocusChangedInPage(
    const content::FocusedNodeDetails& details) {
  // Unless this is a test, only draw the focus ring if this BrowserView is
  // the active one.
  // TODO(crbug.com/40758630): Even if this BrowserView is active, it doesn't
  // necessarily own the node we're about to highlight.
  if (!browser_view_->IsActive() && !skip_activation_check_for_testing_)
    return;

  // Get the bounds of the focused node from the web page.
  gfx::Rect node_bounds = details.node_bounds_in_screen;

  // This happens if e.g. we focus on <body>. Don't show a confusing highlight.
  if (node_bounds.IsEmpty())
    return;

  // Convert it to the local coordinates of this BrowserView's widget.
  node_bounds.Offset(-gfx::ToFlooredVector2d(browser_view_->GetWidget()
                                                 ->GetClientAreaBoundsInScreen()
                                                 .OffsetFromOrigin()));

  // Create the layer if needed, and move/resize it.
  CreateOrUpdateLayer(node_bounds);
}

void AccessibilityFocusHighlight::OnPaintLayer(
    const ui::PaintContext& context) {
  ui::PaintRecorder recorder(context, layer_->size());
  SkColor highlight_color = GetHighlightColor();

  cc::PaintFlags original_flags;
  original_flags.setAntiAlias(true);
  original_flags.setStyle(cc::PaintFlags::kStroke_Style);
  original_flags.setColor(highlight_color);
  original_flags.setStrokeWidth(kStrokeWidth);

  gfx::RectF bounds(node_bounds_);

  // Apply padding
  bounds.Inset(-kPadding);

  // Draw gradient first, so other lines will be drawn over the top.
  gfx::RectF gradient_bounds(bounds);
  int gradient_border_radius = kBorderRadius;
  gradient_bounds.Inset(-kStrokeWidth);
  gradient_border_radius += kStrokeWidth;
  cc::PaintFlags gradient_flags(original_flags);
  gradient_flags.setStrokeWidth(1);
  int original_alpha = std::min(SkColorGetA(highlight_color), 192u);

  // Create a gradient effect by drawing the path outline multiple
  // times with increasing insets from 0 to kGradientWidth, and
  // with increasing transparency.
  for (int remaining = kGradientWidth; remaining > 0; remaining -= 1) {
    // Decrease alpha as distance remaining decreases.
    int alpha = (original_alpha * remaining * remaining) /
                (kGradientWidth * kGradientWidth);
    gradient_flags.setAlphaf(alpha / 255.0f);

    recorder.canvas()->DrawRoundRect(gradient_bounds, gradient_border_radius,
                                     gradient_flags);

    gradient_bounds.Inset(-1);
    gradient_border_radius += 1;
  }

  // Draw the white ring before the inner ring, so that the inner ring is
  // partially over the top, rather than drawing a 1px white ring. A 1px ring
  // would be antialiased to look semi-transparent, which is not what we want.

  // Resize bounds and border radius around inner ring
  gfx::RectF white_ring_bounds(bounds);
  white_ring_bounds.Inset(-(kStrokeWidth / 2));
  int white_ring_border_radius = kBorderRadius + (kStrokeWidth / 2);

  cc::PaintFlags white_ring_flags(original_flags);
  white_ring_flags.setColor(SK_ColorWHITE);

  recorder.canvas()->DrawRoundRect(white_ring_bounds, white_ring_border_radius,
                                   white_ring_flags);

  // Finally, draw the inner ring
  recorder.canvas()->DrawRoundRect(bounds, kBorderRadius, original_flags);
}

float AccessibilityFocusHighlight::ComputeOpacity(
    base::TimeDelta time_since_layer_create,
    base::TimeDelta time_since_focus_move) {
  float opacity = 1.0f;

  if (no_fade_for_testing_)
    return opacity;

  if (time_since_layer_create < fade_in_time_) {
    // We're fading in.
    opacity = time_since_layer_create / fade_in_time_;
  }

  if (time_since_focus_move > persist_time_) {
    // Fading out.
    base::TimeDelta time_since_began_fading =
        time_since_focus_move - (fade_in_time_ + persist_time_);
    opacity = 1.0f - (time_since_began_fading / fade_out_time_);
  }

  return std::clamp(opacity, 0.0f, 1.0f);
}

void AccessibilityFocusHighlight::OnAnimationStep(base::TimeTicks timestamp) {
  if (!layer_)
    return;

  // It's quite possible for the first 1 or 2 animation frames to be
  // for a timestamp that's earlier than the time we received the
  // focus change, so we just treat those as a delta of zero.
  if (timestamp < layer_created_time_)
    timestamp = layer_created_time_;

  // The time since the layer was created is used for fading in.
  base::TimeDelta time_since_layer_create = timestamp - layer_created_time_;

  // For fading out, we look at the time since focus last moved,
  // but we adjust it so that this "clock" doesn't start until after
  // the first fade in completes.
  base::TimeDelta time_since_focus_move =
      std::min(timestamp - focus_last_changed_time_,
               timestamp - layer_created_time_ - fade_in_time_);

  // If the fade out has completed, remove the layer and remove the
  // animation observer.
  if (time_since_focus_move > persist_time_ + fade_out_time_) {
    RemoveLayer();
    return;
  }

  float opacity =
      ComputeOpacity(time_since_layer_create, time_since_focus_move);
  layer_->SetOpacity(opacity);
}

void AccessibilityFocusHighlight::OnCompositingShuttingDown(
    ui::Compositor* compositor) {
  DCHECK(compositor);
  DCHECK_EQ(compositor, compositor_);
  if (compositor == compositor_) {
    compositor->RemoveAnimationObserver(this);
    compositor_ = nullptr;
  }
}

void AccessibilityFocusHighlight::OnTabStripModelChanged(
    TabStripModel*,
    const TabStripModelChange&,
    const TabStripSelectionChange&) {
  RemoveLayer();
}
