// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_GAME_MODE_GAME_MODE_CONTROLLER_H_
#define CHROMEOS_ASH_COMPONENTS_GAME_MODE_GAME_MODE_CONTROLLER_H_

#include "ash/wm/window_state.h"
#include "ash/wm/window_state_observer.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "base/timer/elapsed_timer.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/dbus/resourced/resourced_client.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/aura/client/focus_client.h"

namespace game_mode {

namespace {

using GameMode = ash::ResourcedClient::GameMode;

typedef base::RepeatingCallback<void(GameMode, ash::WindowState*)>
    NotifySetGameModeCallback;

}  // namespace

inline constexpr char kTimeInGameModeHistogramName[] =
    "GameMode.TimeInGameMode.Borealis";
inline constexpr char kGameModeResultHistogramName[] =
    "GameMode.Result.Borealis";

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class GameModeResult {
  kAttempted = 0,
  kFailed = 1,
  kMaxValue = kFailed,
};

// When a Borealis game app game enters full screen, game mode is
// enabled. Game Mode is actually enabled as a result of multiple sets of
// criteria being fulfilled, each checked in sequence. The GameModeController
// additionally exposes an Observer interface which allows clients to subscribe
// to changes in the game mode state.
//
// When one criteria set is met, a new criteria object is constructed which
// is responsible for checking the next criteria, and is owned by the prior
// criteria object. An owner destroys its direct and indirect owned (subsequent)
// criteria objects as soon as itself becomes invalid.
//
// The criteria objects are constructed in this order:
//
// Criteria object         Conditions checked      Causes invalidation (x)
// -----------------------------------------------------------------------------
// GameModeController      Window is focused
// WindowTracker *         Window is fullscreen    Window is destroyed
// GameModeEnabler **      None
//
// (x) Indicates the responsible criteria object makes itself inactive and
//     discards its child criteria, if any.
// *   WindowTracker is responsible for determining the type of window
// **  GameModeEnabler starts game mode on construction, and stops game mode on
//     destruction.
//
// More concretely, this is the logical flow:
//
//          +"GameMode off"+<---------------------------------------------+
//          |              ^ focus                                  focus ^
//          |              | lost                                   lost  |
//          V   focused    |                                   Y          |
// "Watch focus"------->"Watch state"--------->"Game window?"---->"GameMode on"
//                          ^         full           |                    |
//                          |       screen'd         | N       fullscreen |
//                          |                        V               lost V
//                     "GameMode off"<------------------------------------+
//
class GameModeController : public aura::client::FocusChangeObserver {
 public:
  GameModeController();
  GameModeController(const GameModeController&) = delete;
  GameModeController& operator=(const GameModeController&) = delete;
  ~GameModeController() override;

  // Overridden from FocusChangeObserver
  void OnWindowFocused(aura::Window* gained_focus,
                       aura::Window* lost_focus) override;

  // Maintains GameMode in an ON state until destroyed.
  class GameModeEnabler {
   public:
    // |window_state| indicates the window which currently meets the criteria to
    // enable game mode.
    GameModeEnabler(ash::WindowState* window_state,
                    NotifySetGameModeCallback notify_set_game_mode_callback);
    ~GameModeEnabler();

    // Updates the window which is causing the game mode to be enabled, and
    // notifies observers.
    void SetWindowState(ash::WindowState* window_state);

   private:
    static void OnSetGameMode(std::optional<GameMode> refresh_of,
                              std::optional<GameMode> previous);
    void RefreshGameMode();

    // Used to determine if it's the first instance of game mode failing.
    static bool should_record_failure;
    base::RepeatingTimer timer_;
    base::ElapsedTimer began_;

    // Not owned. |window_state_| is observed by the WindowTracker which owns
    // this GameModeEnabler, and this enabler will always be destroyed before
    // the window is destroyed.
    raw_ptr<ash::WindowState> window_state_;

    const NotifySetGameModeCallback notify_set_game_mode_callback_;
  };

  class WindowTracker : public ash::WindowStateObserver,
                        public aura::WindowObserver {
   public:
    WindowTracker(ash::WindowState* window_state,
                  std::unique_ptr<WindowTracker> previous_focused,
                  NotifySetGameModeCallback notify_set_game_mode_callback);
    ~WindowTracker() override;

    // Overridden from WindowObserver
    void OnWindowDestroying(aura::Window* window) override;

    // Overridden from WindowStateObserver
    void OnPostWindowStateTypeChange(
        ash::WindowState* window_state,
        chromeos::WindowStateType old_type) override;

    void UpdateGameModeStatus(ash::WindowState* window_state);

   private:
    base::ScopedObservation<ash::WindowState, ash::WindowStateObserver>
        window_state_observer_{this};
    base::ScopedObservation<aura::Window, aura::WindowObserver>
        window_observer_{this};
    std::unique_ptr<GameModeEnabler> game_mode_enabler_;
    const NotifySetGameModeCallback notify_set_game_mode_callback_;
  };

  using GameModeChangedCallback =
      base::RepeatingCallback<void(aura::Window*, GameMode)>;

  void set_game_mode_changed_callback(GameModeChangedCallback callback) {
    callback_ = callback;
  }

  void NotifySetGameMode(GameMode game_mode, ash::WindowState* window_state);

 private:
  std::unique_ptr<WindowTracker> focused_;
  GameModeChangedCallback callback_;
};

}  // namespace game_mode

#endif  // CHROMEOS_ASH_COMPONENTS_GAME_MODE_GAME_MODE_CONTROLLER_H_
