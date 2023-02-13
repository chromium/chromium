// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/frame/multitask_menu/multitask_menu_nudge_controller.h"

#include "base/check_is_test.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "chromeos/ui/base/tablet_state.h"
#include "chromeos/ui/frame/caption_buttons/frame_caption_button_container_view.h"
#include "chromeos/ui/frame/frame_header.h"
#include "components/prefs/pref_registry_simple.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/highlight_border.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/frame_caption_button.h"
#include "ui/wm/public/activation_client.h"

namespace chromeos {

namespace {

constexpr base::TimeDelta kNudgeDismissTimeout = base::Seconds(6);

// The nudge will not be shown if it already been shown 3 times, or if 24 hours
// have not yet passed since it was last shown.
constexpr int kNudgeMaxShownCount = 3;
constexpr base::TimeDelta kNudgeTimeBetweenShown = base::Hours(24);

constexpr base::TimeDelta kFadeDuration = base::Milliseconds(50);

constexpr gfx::Insets kLabelInsets(10);
constexpr int kLabelRoundingDp = 16;
constexpr int kLabelMaxWidth = 512;

constexpr int kNudgeDistanceFromAnchor = 8;

// The max pulse size will be three times the size of the maximize/restore
// button.
constexpr float kPulseSizeMultiplier = 3.0f;
constexpr base::TimeDelta kPulseDuration = base::Seconds(2);
constexpr int kPulses = 3;

bool g_suppress_nudge_for_testing = false;

// Clock that can be overridden for testing.
base::Clock* g_clock_override = nullptr;

base::Time GetTime() {
  return g_clock_override ? g_clock_override->Now() : base::Time::Now();
}

std::unique_ptr<views::Widget> CreateWidget(aura::Window* window) {
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.name = "MultitaskNudgeWidget";
  params.accept_events = false;
  params.parent = window->parent();

  auto widget = std::make_unique<views::Widget>(std::move(params));
  const int message_id = TabletState::Get()->InTabletMode()
                             ? IDS_TABLET_MULTITASK_MENU_NUDGE_TEXT
                             : IDS_MULTITASK_MENU_NUDGE_TEXT;

  // The contents are a label with a background that has padding, background
  // color and highlight border.
  auto contents_view =
      views::Builder<views::BoxLayoutView>()
          .SetInsideBorderInsets(kLabelInsets)
          .SetBackground(views::CreateThemedRoundedRectBackground(
              ui::kColorSysSurface3, kLabelRoundingDp))
          .SetBorder(std::make_unique<views::HighlightBorder>(
              kLabelRoundingDp, views::HighlightBorder::Type::kHighlightBorder1,
              /*use_light_colors=*/false))
          .AddChildren(
              views::Builder<views::Label>()
                  .SetHorizontalAlignment(gfx::ALIGN_CENTER)
                  .SetAutoColorReadabilityEnabled(false)
                  .SetMultiLine(true)
                  .SetMaximumWidth(kLabelMaxWidth)
                  .SetMaxLines(2)
                  .SetSubpixelRenderingEnabled(false)
                  .SetFontList(views::Label::GetDefaultFontList().Derive(
                      2, gfx::Font::FontStyle::NORMAL,
                      gfx::Font::Weight::NORMAL))
                  .SetText(l10n_util::GetStringUTF16(message_id)))
          .Build();
  widget->SetContentsView(std::move(contents_view));
  return widget;
}

}  // namespace

MultitaskMenuNudgeController::MultitaskMenuNudgeController(
    aura::Window* root_window,
    std::unique_ptr<Delegate> delegate)
    : root_window_(root_window), delegate_(std::move(delegate)) {
  display::Screen::GetScreen()->AddObserver(this);

  ::wm::GetActivationClient(root_window_)->AddObserver(this);
  root_window_observation_.Observe(root_window_);
}

MultitaskMenuNudgeController::~MultitaskMenuNudgeController() {
  DismissNudge();
  display::Screen::GetScreen()->RemoveObserver(this);
  if (root_window_) {
    ::wm::GetActivationClient(root_window_)->RemoveObserver(this);
  }
}

// static
void MultitaskMenuNudgeController::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(kClamshellShownCountPrefName, 0);
  registry->RegisterIntegerPref(kTabletShownCountPrefName, 0);
  registry->RegisterTimePref(kClamshellLastShownPrefName, base::Time());
  registry->RegisterTimePref(kTabletLastShownPrefName, base::Time());
}

void MultitaskMenuNudgeController::MaybeShowNudge(aura::Window* window) {
  if (!chromeos::wm::features::IsWindowLayoutMenuEnabled() ||
      g_suppress_nudge_for_testing || nudge_widget_) {
    return;
  }

  // If the window is not visible, do not show the nudge.
  if (!window->IsVisible()) {
    return;
  }

  if (!delegate_->IsRegularUser()) {
    return;
  }

  const bool tablet_mode = TabletState::Get()->InTabletMode();
  const int shown_count = delegate_->GetShowCount(tablet_mode);
  const base::Time last_shown_time = delegate_->GetLastShownTime(tablet_mode);

  // TODO(b/267787811): When the multitask menu has been opened in tablet
  // mode, don't show the tablet nudge anymore.
  // TODO(sammiequon|hewer): When the multitask menu has been opened in
  // clamshell mode, don't show the clamshell nudge anymore.

  // Nudge has already been shown three times. No need to educate anymore.
  if (shown_count >= kNudgeMaxShownCount) {
    return;
  }

  // Nudge has been shown within the last 24 hours already.
  if ((GetTime() - last_shown_time) < kNudgeTimeBetweenShown) {
    return;
  }

  // The anchor is the button on the header that serves as the maximize or
  // restore button (depending on the window state). Not used in tablet
  // mode.
  views::FrameCaptionButton* anchor_view = nullptr;

  if (!tablet_mode) {
    // Some tests create windows without a backing widget
    // (`CreateTestWindow()`), and some widgets may not have a header, test or
    // custom header.
    auto* widget = views::Widget::GetWidgetForNativeWindow(window);
    if (!widget) {
      CHECK_IS_TEST();
      return;
    }

    auto* frame_header = chromeos::FrameHeader::Get(widget);
    if (!frame_header) {
      return;
    }

    anchor_view = frame_header->caption_button_container()->size_button();
    DCHECK(anchor_view);

    // If the anchor is not visible, do not show the nudge.
    if (!anchor_view->IsDrawn()) {
      return;
    }
  }

  window_ = window;
  window_observation_.Observe(window_);

  nudge_widget_ = CreateWidget(window_);
  nudge_widget_->Show();

  anchor_view_ = anchor_view;

  if (!tablet_mode) {
    // Create the layer which pulses on the maximize/restore button.
    pulse_layer_ = std::make_unique<ui::Layer>(ui::LAYER_SOLID_COLOR);
    // TODO(b/267646118): Update the color to match the theme.
    pulse_layer_->SetColor(SK_ColorGRAY);
    window_->parent()->layer()->Add(pulse_layer_.get());
  }

  UpdateWidgetAndPulse();
  DCHECK(nudge_widget_);

  // Fade the education nudge in.
  ui::Layer* layer = nudge_widget_->GetLayer();
  layer->SetOpacity(0.0f);
  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetDuration(kFadeDuration)
      .SetOpacity(layer, 1.0f, gfx::Tween::LINEAR);

  // Update the preferences.
  delegate_->SetShowCount(shown_count + 1, tablet_mode);
  delegate_->SetLastShownTime(GetTime(), tablet_mode);

  // No need to update pulse or start timer in tablet mode.
  if (tablet_mode) {
    return;
  }

  PerformPulseAnimation(/*pulse_count=*/0);

  clamshell_nudge_dismiss_timer_.Start(
      FROM_HERE, kNudgeDismissTimeout, this,
      &MultitaskMenuNudgeController::OnDismissTimerEnded);
}

void MultitaskMenuNudgeController::DismissNudge() {
  clamshell_nudge_dismiss_timer_.Stop();
  window_observation_.Reset();
  window_ = nullptr;
  anchor_view_ = nullptr;
  pulse_layer_.reset();
  if (nudge_widget_ && !nudge_widget_->IsClosed()) {
    nudge_widget_->GetLayer()->GetAnimator()->AbortAllAnimations();
    nudge_widget_->CloseNow();
  }
}

void MultitaskMenuNudgeController::OnWindowParentChanged(aura::Window* window,
                                                         aura::Window* parent) {
  if (!parent) {
    return;
  }
  DCHECK_EQ(window_, window);
  UpdateWidgetAndPulse();
}

void MultitaskMenuNudgeController::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {
  if (window == window_) {
    UpdateWidgetAndPulse();
  }
}

void MultitaskMenuNudgeController::OnWindowTargetTransformChanging(
    aura::Window* window,
    const gfx::Transform& new_transform) {
  if (window == window_) {
    // Prevent unintended behaviour in situations that use transforms such as
    // overview mode.
    // TODO(hewer): Decide how the cue behaves when adjusting the split view
    // bounds in tablet mode.
    DismissNudge();
  }
}

void MultitaskMenuNudgeController::OnWindowStackingChanged(
    aura::Window* window) {
  if (window != window_) {
    return;
  }

  // Stacking may change during the construction of the widget, at which
  // `nudge_widget_` would still be null.
  if (!nudge_widget_) {
    return;
  }

  // Ensure the `nudge_widget_` is always above `window_`. We dont worry about
  // the pulse layer since it is not a window, and won't get stacked on top of
  // during window activation for example.
  window_->parent()->StackChildAbove(nudge_widget_->GetNativeWindow(), window);
}

void MultitaskMenuNudgeController::OnWindowDestroying(aura::Window* window) {
  if (root_window_ == window) {
    ::wm::GetActivationClient(root_window_)->RemoveObserver(this);
    root_window_ = nullptr;
    root_window_observation_.Reset();
    return;
  }
  DCHECK_EQ(window_, window);
  DismissNudge();
}

void MultitaskMenuNudgeController::OnWindowActivated(
    ActivationReason reason,
    aura::Window* gained_active,
    aura::Window* lost_active) {
  // Tablet mode window activation is handled by `TabletModeMultitaskCue`.
  if (gained_active && !TabletState::Get()->InTabletMode()) {
    MaybeShowNudge(gained_active);
  }
}

void MultitaskMenuNudgeController::OnDisplayTabletStateChanged(
    display::TabletState state) {
  switch (state) {
    case display::TabletState::kEnteringTabletMode:
    case display::TabletState::kExitingTabletMode:
      // Nudge must be dismissed before switching modes, as each nudge is
      // different in each mode and the anchor isn't used in tablet mode.
      DismissNudge();
      break;
    case display::TabletState::kInTabletMode:
      // Entering tablet mode will call the `TabletModeMultitaskCue`
      // constructor so no work needed.
      // TODO(b/267648014): Combine cue and nudge logic so both are activated in
      // the same place when switching modes.
      break;
    case display::TabletState::kInClamshellMode:
      // TODO(b/267648071): Find a way to make the nudge shown after finishing
      // transition to clamshell mode as anchor is not visible yet.
      break;
  }
}

void MultitaskMenuNudgeController::SetSuppressNudgeForTesting(bool val) {
  g_suppress_nudge_for_testing = val;
}

// static
void MultitaskMenuNudgeController::SetOverrideClockForTesting(
    base::Clock* test_clock) {
  g_clock_override = test_clock;
}

void MultitaskMenuNudgeController::OnDismissTimerEnded() {
  if (!nudge_widget_) {
    return;
  }

  ui::Layer* layer = nudge_widget_->GetLayer();
  layer->SetOpacity(1.0f);
  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnEnded(base::BindOnce(&MultitaskMenuNudgeController::DismissNudge,
                              base::Unretained(this)))
      .Once()
      .SetDuration(kFadeDuration)
      .SetOpacity(layer, 0.0f, gfx::Tween::LINEAR);
}

void MultitaskMenuNudgeController::UpdateWidgetAndPulse() {
  DCHECK(window_);
  DCHECK(nudge_widget_);

  const bool tablet_mode = TabletState::Get()->InTabletMode();
  if (!tablet_mode) {
    DCHECK(pulse_layer_);
    DCHECK(anchor_view_);
  }

  // Dismiss the nudge if the window (or anchor in clamshell mode) is not
  // visible, otherwise it will be floating.
  if (!window_->IsVisible() || (!tablet_mode && !anchor_view_->IsDrawn())) {
    DismissNudge();
    return;
  }

  // Reparent the nudge and pulse if necessary.
  aura::Window* new_parent = window_->parent();
  aura::Window* nudge_window = nudge_widget_->GetNativeWindow();

  if (new_parent != nudge_window->parent()) {
    new_parent->AddChild(nudge_window);
    if (pulse_layer_) {
      new_parent->layer()->Add(pulse_layer_.get());
    }
  }

  const gfx::Size size = nudge_widget_->GetContentsView()->GetPreferredSize();

  if (tablet_mode) {
    // The nudge is placed in the top center of the window, just below the cue.
    nudge_widget_->SetBounds(gfx::Rect(
        (window_->bounds().width() - size.width()) / 2 + window_->bounds().x(),
        kTabletNudgeYOffset + window_->bounds().y(), size.width(),
        size.height()));
    return;
  }

  // The nudge is placed right below the anchor.
  // TODO(crbug.com/1329233): Determine what to do if the nudge is offscreen.
  const gfx::Rect anchor_bounds_in_screen = anchor_view_->GetBoundsInScreen();
  const gfx::Rect bounds_in_screen(
      anchor_bounds_in_screen.CenterPoint().x() - size.width() / 2,
      anchor_bounds_in_screen.bottom() + kNudgeDistanceFromAnchor, size.width(),
      size.height());
  nudge_widget_->SetBounds(bounds_in_screen);

  // The circular pulse should be a square that matches the smaller dimension of
  // `anchor_view_`. We use rounded corners to make it look like a circle.
  gfx::Rect pulse_layer_bounds = anchor_bounds_in_screen;
  gfx::Point pulse_layer_origin = pulse_layer_bounds.origin();
  aura::client::GetScreenPositionClient(nudge_window->GetRootWindow())
      ->ConvertPointFromScreen(nudge_window->parent(), &pulse_layer_origin);
  pulse_layer_bounds.set_origin(pulse_layer_origin);
  const int length =
      std::min(pulse_layer_bounds.width(), pulse_layer_bounds.height());
  pulse_layer_bounds.ClampToCenteredSize(gfx::Size(length, length));
  pulse_layer_->SetBounds(pulse_layer_bounds);
  pulse_layer_->SetRoundedCornerRadius(gfx::RoundedCornersF(length / 2.f));
}

void MultitaskMenuNudgeController::PerformPulseAnimation(int pulse_count) {
  if (pulse_count >= kPulses) {
    return;
  }

  DCHECK(pulse_layer_);

  // The pulse animation scales up and fades out on top of the maximize/restore
  // button until the nudge disappears.
  const gfx::Point pivot(
      gfx::Rect(pulse_layer_->GetTargetBounds().size()).CenterPoint());
  const gfx::Transform transform =
      gfx::GetScaleTransform(pivot, kPulseSizeMultiplier);

  pulse_layer_->SetOpacity(1.0f);
  pulse_layer_->SetTransform(gfx::Transform());

  // Note that `views::AnimationBuilder::Repeatedly` works here as well, but
  // causes tests to hang.
  views::AnimationBuilder builder;
  builder
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnEnded(
          base::BindOnce(&MultitaskMenuNudgeController::PerformPulseAnimation,
                         base::Unretained(this), pulse_count + 1))
      .Once()
      .SetDuration(kPulseDuration)
      .SetOpacity(pulse_layer_.get(), 0.0f, gfx::Tween::ACCEL_0_80_DECEL_80)
      .SetTransform(pulse_layer_.get(), transform,
                    gfx::Tween::ACCEL_0_40_DECEL_100);
}

}  // namespace chromeos
