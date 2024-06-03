// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_FRAME_MULTITASK_MENU_MULTITASK_MENU_NUDGE_CONTROLLER_H_
#define CHROMEOS_UI_FRAME_MULTITASK_MENU_MULTITASK_MENU_NUDGE_CONTROLLER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/clock.h"
#include "base/timer/timer.h"
#include "ui/aura/window_observer.h"
#include "ui/display/display_observer.h"
#include "ui/views/view.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget_observer.h"

class PrefRegistrySimple;

namespace ash {
class MultitaskMenuNudgeControllerTest;
class MultitaskMenuNudgeTest;
}

namespace ui {
class Layer;
}

namespace chromeos {

// Controller for showing the user education nudge for the multitask menu.
class COMPONENT_EXPORT(CHROMEOS_UI_FRAME) MultitaskMenuNudgeController
    : public aura::WindowObserver,
      public views::WidgetObserver,
      public display::DisplayObserver {
 public:
  // `tablet_mode` refers to the tablet state when the prefs are fetched. If
  // the state changes while fetching prefs, we do not show the nudge. The
  // callback may fail on lacros; in this case `values` will be null.
  // `shown_count` and `last_shown_time` are the values fetched from the pref
  // service regarding how many times the nudge has been shown, and when it was
  // last shown.
  struct PrefValues {
    int show_count;
    base::Time last_shown_time;
  };
  using GetPreferencesCallback =
      base::OnceCallback<void(bool tablet_mode,
                              std::optional<PrefValues> values)>;

  // A delegate to provide platform specific implementation (ash, lacros).
  class Delegate {
   public:
    virtual ~Delegate();

    virtual int GetTabletNudgeYOffset() const = 0;
    virtual void GetNudgePreferences(bool tablet_mode,
                                     GetPreferencesCallback callback) = 0;
    virtual void SetNudgePreferences(bool tablet_mode,
                                     int count,
                                     base::Time time) = 0;
    // Returns true if the user has logged in for the first time, or is a guest
    // user. We don't want to show the nudge in this case.
    virtual bool IsUserNewOrGuest() const;

   protected:
    Delegate();
  };

  MultitaskMenuNudgeController();
  MultitaskMenuNudgeController(const MultitaskMenuNudgeController&) = delete;
  MultitaskMenuNudgeController& operator=(const MultitaskMenuNudgeController&) =
      delete;
  ~MultitaskMenuNudgeController() override;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);
#endif

  // Attempts to show the nudge. Reads preferences and then calls
  // `OnGetPreferences()`.
  void MaybeShowNudge(aura::Window* window);
  void MaybeShowNudge(aura::Window* window, views::View* anchor_view);

  // Closes the widget and cleans up all pointers in this class.
  void DismissNudge();

  // Called when the menu is opened. Marks the pref as seen so it does not show
  // up again.
  void OnMenuOpened(bool tablet_mode);

  // aura::WindowObserver:
  void OnWindowParentChanged(aura::Window* window,
                             aura::Window* parent) override;
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override;
  void OnWindowTargetTransformChanging(
      aura::Window* window,
      const gfx::Transform& new_transform) override;
  void OnWindowStackingChanged(aura::Window* window) override;
  void OnWindowDestroying(aura::Window* window) override;

  // views::WidgetObserver:
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;

  // display::DisplayObserver:
  void OnDisplayTabletStateChanged(display::TabletState state) override;

  static void SetSuppressNudgeForTesting(bool val);

 private:
  friend class ::ash::MultitaskMenuNudgeControllerTest;
  friend class ::ash::MultitaskMenuNudgeTest;

  // Used to control the clock in a test setting.
  static void SetOverrideClockForTesting(base::Clock* test_clock);

  // Callback function after fetching preferences. Shows the nudge if it can be
  // shown. The nudge can be shown if it hasn't been shown 3 times already, or
  // shown in the last 24 hours. `window` and `anchor_view` are the associated
  // window and the anchor for the nudge. `anchor_view` will be null in tablet
  // mode as the nudge shows in the top center of the window and is not anchored
  // to anything. `values` will return `std::nullopt` if fetching the pref
  // failed in lacros, it will return a value in ash.
  void OnGetPreferences(aura::Window* window,
                        views::View* anchor_view,
                        bool tablet_mode,
                        std::optional<PrefValues> values);

  // Runs when the nudge dismiss timer expires. Dismisses the nudge if it is
  // being shown.
  void OnDismissTimerEnded();

  // Dismisses the widget and pulse if `window_` is not visible, or if
  // `anchor_view_` is not drawn in clamshell mode. Otherwise updates the bounds
  // and reparents the two if necessary.
  void UpdateWidgetAndPulse();

  // The animation associated with `pulse_layer_`. Runs until `pulse_layer_` is
  // destroyed or `pulse_count` reaches 3.
  void PerformPulseAnimation(int pulse_count);

  // Dismisses the clamshell nudge at the end of the timer if it is still
  // visible. Tablet nudge is handled by the `TabletModeMultitaskCueController`
  // timer.
  base::OneShotTimer clamshell_nudge_dismiss_timer_;

  views::UniqueWidgetPtr nudge_widget_;
  std::unique_ptr<ui::Layer> pulse_layer_;

  // The time the nudge was shown. Null if it hasn't been shown this session.
  base::Time nudge_shown_time_;

  // The app window that the nudge is associated with.
  raw_ptr<aura::Window> window_ = nullptr;

  // The view that the nudge will be anchored to. It is the maximize or resize
  // button on `window_`'s frame. Null in tablet mode.
  raw_ptr<views::View> anchor_view_ = nullptr;

  base::ScopedObservation<aura::Window, aura::WindowObserver>
      window_observation_{this};
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};

  display::ScopedDisplayObserver display_observer_{this};

  base::WeakPtrFactory<MultitaskMenuNudgeController> weak_ptr_factory_{this};
};

}  // namespace chromeos

#endif  // CHROMEOS_UI_FRAME_MULTITASK_MENU_MULTITASK_MENU_NUDGE_CONTROLLER_H_
