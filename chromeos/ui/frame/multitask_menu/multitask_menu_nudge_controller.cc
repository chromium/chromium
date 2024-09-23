// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/frame/multitask_menu/multitask_menu_nudge_controller.h"

#include "ash/constants/notifier_catalogs.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "chromeos/ui/base/nudge_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/display/screen.h"
#include "ui/display/tablet_state.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "components/user_manager/user_manager.h"
#endif

namespace chromeos {

namespace {

constexpr base::TimeDelta kNudgeDismissTimeout = base::Seconds(6);

// The nudge will not be shown if it already been shown 3 times, or if 24 hours
// have not yet passed since it was last shown.
constexpr int kNudgeMaxShownCount = 3;
constexpr base::TimeDelta kNudgeTimeBetweenShown = base::Hours(24);

constexpr base::TimeDelta kFadeDuration = base::Milliseconds(50);

constexpr gfx::Insets kLabelInsets = gfx::Insets::VH(8, 16);
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

// May be null in tests.
MultitaskMenuNudgeController::Delegate* g_delegate_instance = nullptr;

base::Time GetTime() {
  return g_clock_override ? g_clock_override->Now() : base::Time::Now();
}

std::unique_ptr<views::Widget> CreateWidget(aura::Window* window) {
  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_POPUP);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.name = "MultitaskNudgeWidget";
  params.accept_events = false;
  params.parent = window->parent();
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // This widget must not set `use_accelerated_widget_override` b/c this
  // widget's window will be reparented to `window`.
  params.use_accelerated_widget_override = false;
#endif

  auto widget = std::make_unique<views::Widget>(std::move(params));
  const int message_id = display::Screen::GetScreen()->InTabletMode()
                             ? IDS_TABLET_MULTITASK_MENU_NUDGE_TEXT
                             : IDS_MULTITASK_MENU_NUDGE_TEXT;

  // The contents are a label with a background that has padding, background
  // color and highlight border.
  auto contents_view =
      views::Builder<views::BoxLayoutView>()
          .SetInsideBorderInsets(kLabelInsets)
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
  const float corner_radius =
      contents_view->GetPreferredSize({}).height() / 2.0f;
  contents_view->SetBackground(views::CreateThemedRoundedRectBackground(
      ui::kColorSysSurface3, corner_radius));
  contents_view->SetBorder(std::make_unique<views::HighlightBorder>(
      corner_radius, views::HighlightBorder::Type::kHighlightBorderOnShadow));

  widget->SetContentsView(std::move(contents_view));
  return widget;
}

}  // namespace

MultitaskMenuNudgeController::Delegate::~Delegate() {
  CHECK_EQ(this, g_delegate_instance);
  g_delegate_instance = nullptr;
}

MultitaskMenuNudgeController::Delegate::Delegate() {
  CHECK_EQ(nullptr, g_delegate_instance);
  g_delegate_instance = this;
}

bool MultitaskMenuNudgeController::Delegate::IsUserNewOrGuest() const {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!user_manager::UserManager::IsInitialized()) {
    return false;
  }

  return user_manager::UserManager::Get()->IsCurrentUserNew() ||
         user_manager::UserManager::Get()->IsLoggedInAsGuest();
#else
  return false;
#endif
}

MultitaskMenuNudgeController::MultitaskMenuNudgeController() = default;

MultitaskMenuNudgeController::~MultitaskMenuNudgeController() {
  DismissNudge();
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// static
void MultitaskMenuNudgeController::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      ash::prefs::kMultitaskMenuNudgeClamshellShownCount, 0);
  registry->RegisterIntegerPref(ash::prefs::kMultitaskMenuNudgeTabletShownCount,
                                0);
  registry->RegisterTimePref(ash::prefs::kMultitaskMenuNudgeClamshellLastShown,
                             base::Time());
  registry->RegisterTimePref(ash::prefs::kMultitaskMenuNudgeTabletLastShown,
                             base::Time());
}
#endif

void MultitaskMenuNudgeController::MaybeShowNudge(aura::Window* window) {
  MaybeShowNudge(window, /*anchor_view=*/nullptr);
}

void MultitaskMenuNudgeController::MaybeShowNudge(aura::Window* window,
                                                  views::View* anchor_view) {
  // Delegate could be null if the associated window was created during OOBE.
  if (!g_delegate_instance || g_delegate_instance->IsUserNewOrGuest()) {
    return;
  }

  if (g_suppress_nudge_for_testing || nudge_widget_) {
    return;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kAshNoNudges)) {
    return;
  }
#endif

  // If the window is not visible, do not show the nudge.
  if (!window->IsVisible()) {
    return;
  }

  // `window` and `anchor_view` can be passed safely on clamshell because they
  // are owned by the frame which also owns `this`. They can be passed safely on
  // tablet since tablet is controlled by ash which is sync.
  g_delegate_instance->GetNudgePreferences(
      display::Screen::GetScreen()->InTabletMode(),
      base::BindOnce(&MultitaskMenuNudgeController::OnGetPreferences,
                     weak_ptr_factory_.GetWeakPtr(), window, anchor_view));
}

void MultitaskMenuNudgeController::DismissNudge() {
  clamshell_nudge_dismiss_timer_.Stop();
  weak_ptr_factory_.InvalidateWeakPtrs();

  window_ = nullptr;
  window_observation_.Reset();
  widget_observation_.Reset();

  anchor_view_ = nullptr;
  pulse_layer_.reset();
  if (nudge_widget_ && !nudge_widget_->IsClosed()) {
    nudge_widget_->GetLayer()->GetAnimator()->AbortAllAnimations();
    nudge_widget_->CloseNow();
  }
}

void MultitaskMenuNudgeController::OnMenuOpened(bool tablet_mode) {
  if (!nudge_shown_time_.is_null()) {
    base::UmaHistogramEnumeration(
        GetNudgeTimeToActionHistogramName(GetTime() - nudge_shown_time_),
        tablet_mode ? ash::NudgeCatalogName::kMultitaskMenuTablet
                    : ash::NudgeCatalogName::kMultitaskMenuClamshell);
    nudge_shown_time_ = base::Time();
  }

  // Avoid sending prefs through the cros API or recording user actions if the
  // nudge isn't shown.
  if (!nudge_widget_ || nudge_widget_->IsClosed()) {
    return;
  }

  base::RecordAction(
      base::UserMetricsAction("Nudge_Active_When_MultitaskMenu_Opened"));
  DismissNudge();

  if (g_delegate_instance) {
    g_delegate_instance->SetNudgePreferences(tablet_mode, kNudgeMaxShownCount,
                                             GetTime());
  }
}

void MultitaskMenuNudgeController::OnWindowParentChanged(aura::Window* window,
                                                         aura::Window* parent) {
  if (!parent) {
    return;
  }
  CHECK_EQ(window_, window);
  UpdateWidgetAndPulse();
}

void MultitaskMenuNudgeController::OnWindowVisibilityChanged(
    aura::Window* window,
    bool visible) {
  if (window == window_ && !visible) {
    DismissNudge();
  }
}

void MultitaskMenuNudgeController::OnWindowTargetTransformChanging(
    aura::Window* window,
    const gfx::Transform& new_transform) {
  CHECK_EQ(window_, window);
  // Prevent unintended behaviour in situations that use transforms such as
  // overview mode.
  // TODO(hewer): Decide how the cue behaves when adjusting the split view
  // bounds in tablet mode.
  DismissNudge();
}

void MultitaskMenuNudgeController::OnWindowStackingChanged(
    aura::Window* window) {
  CHECK_EQ(window_, window);

  // Stacking may change during the construction of the widget, at which
  // `nudge_widget_` would still be null.
  if (!nudge_widget_) {
    return;
  }

  // Ensure the `nudge_widget_` is always above `window_`. We dont worry about
  // the pulse layer since it is not a window, and won't get stacked on top of
  // during window activation for example. When moving across displays, it is
  // possible the window parent differs for a bit. In this case we cannot
  // restack and we need to wait for `UpdateWidgetAndPulse` to place the nudge
  // in the correct spot.
  if (window_->parent() == nudge_widget_->GetNativeWindow()->parent()) {
    window_->parent()->StackChildAbove(nudge_widget_->GetNativeWindow(),
                                       window);
  }
}

void MultitaskMenuNudgeController::OnWindowDestroying(aura::Window* window) {
  CHECK_EQ(window_, window);
  DismissNudge();
}

void MultitaskMenuNudgeController::OnWidgetBoundsChanged(
    views::Widget* widget,
    const gfx::Rect& new_bounds) {
  CHECK_EQ(window_, widget->GetNativeWindow());
  UpdateWidgetAndPulse();
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
      // Entering tablet mode will call the `TabletModeMultitaskCueController`
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

void MultitaskMenuNudgeController::OnGetPreferences(
    aura::Window* window,
    views::View* anchor_view,
    bool tablet_mode,
    std::optional<PrefValues> values) {
  if (!values) {
    LOG(WARNING) << "Unable to fetch preferences.";
    return;
  }

  // Tablet state has changed since we fetched preferences.
  if (tablet_mode != display::Screen::GetScreen()->InTabletMode()) {
    return;
  }

  // The nudge is already been shown for this window. This can happen in
  // lacros, where prefs are read and written to async. In ash, the prefs will
  // be updated before the next read, so this cannot happen.
  if (window_) {
    return;
  }

  // Nudge has already been shown three times. No need to educate anymore.
  if (values->show_count >= kNudgeMaxShownCount) {
    return;
  }

  // Nudge has been shown within the last 24 hours already.
  if ((GetTime() - values->last_shown_time) < kNudgeTimeBetweenShown) {
    return;
  }

  // If the anchor is passed and hidden or offscreen, we cannot show the nudge.
  if (anchor_view) {
    if (!anchor_view->IsDrawn() ||
        !display::Screen::GetScreen()
             ->GetDisplayNearestWindow(window)
             .bounds()
             .Contains(anchor_view->GetBoundsInScreen())) {
      return;
    }
  }

  window_ = window;

  nudge_widget_ = CreateWidget(window_);
  anchor_view_ = anchor_view;

  nudge_widget_->Show();

  base::UmaHistogramEnumeration(
      kNotifierFrameworkNudgeShownCountHistogram,
      tablet_mode ? ash::NudgeCatalogName::kMultitaskMenuTablet
                  : ash::NudgeCatalogName::kMultitaskMenuClamshell);
  nudge_shown_time_ = GetTime();

  // Note that order matters because in some cases, creating the widget may
  // trigger some window observations.
  window_observation_.Observe(window_.get());

  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window_);
  CHECK(widget);
  widget_observation_.Observe(widget);

  if (!tablet_mode) {
    // Create the layer which pulses on the maximize/restore button.
    pulse_layer_ = std::make_unique<ui::Layer>(ui::LAYER_SOLID_COLOR);
    pulse_layer_->SetColor(nudge_widget_->GetColorProvider()->GetColor(
        ui::kColorMultitaskMenuNudgePulse));
    window_->parent()->layer()->Add(pulse_layer_.get());
  }

  UpdateWidgetAndPulse();

  // It is possible `UpdateWidgetAndPulse` could not find a good bounds to place
  // the nudge. In that case the widget and pulse and observations would have
  // been cleaned up.
  if (!nudge_widget_) {
    return;
  }

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
  g_delegate_instance->SetNudgePreferences(tablet_mode, values->show_count + 1,
                                           GetTime());

  // No need to update pulse or start timer in tablet mode.
  if (!tablet_mode) {
    PerformPulseAnimation(/*pulse_count=*/0);

    clamshell_nudge_dismiss_timer_.Start(
        FROM_HERE, kNudgeDismissTimeout, this,
        &MultitaskMenuNudgeController::OnDismissTimerEnded);
  }
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
  CHECK(window_);
  CHECK(nudge_widget_);

  const bool tablet_mode = display::Screen::GetScreen()->InTabletMode();
  if (!tablet_mode) {
    CHECK(pulse_layer_);
    CHECK(anchor_view_);
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

  const gfx::Size size = nudge_widget_->GetContentsView()->GetPreferredSize({});

  if (tablet_mode) {
    // The nudge is placed in the top center of the window, just below the cue.
    const int tablet_nudge_y_offset =
        g_delegate_instance->GetTabletNudgeYOffset();
    nudge_widget_->SetBounds(gfx::Rect(
        (window_->bounds().width() - size.width()) / 2 + window_->bounds().x(),
        tablet_nudge_y_offset + window_->bounds().y(), size.width(),
        size.height()));
    return;
  }

  // The nudge is placed right below the anchor.
  const gfx::Rect anchor_bounds_in_screen = anchor_view_->GetBoundsInScreen();
  gfx::Rect bounds_in_screen(
      anchor_bounds_in_screen.CenterPoint().x() - size.width() / 2,
      anchor_bounds_in_screen.bottom() + kNudgeDistanceFromAnchor, size.width(),
      size.height());
  bool adjust_to_fit = false;
  const display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestView(window_);
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Lacros always needs adjustment since the child cannot go outside the
  // parents bounds currently. See https://crbug.com/1416919.
  adjust_to_fit = true;
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  // If the nudge is going to be offscreen, make sure it is within the window
  // bounds.
  adjust_to_fit = !display.work_area().Contains(bounds_in_screen);
#endif
  if (adjust_to_fit) {
    // The nudge should be within the window bounds.
    bounds_in_screen.AdjustToFit(window_->GetBoundsInScreen());
  }
  nudge_widget_->SetBounds(bounds_in_screen);

  // If setting bounds on the nudge causes it to move to another display (this
  // can happen while dragging across displays), dismiss the nudge.
  if (nudge_widget_->GetNativeWindow()->parent() != window_->parent()) {
    DismissNudge();
    return;
  }

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

  CHECK(pulse_layer_);

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
