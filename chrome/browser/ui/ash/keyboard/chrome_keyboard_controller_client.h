// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_KEYBOARD_CHROME_KEYBOARD_CONTROLLER_CLIENT_H_
#define CHROME_BROWSER_UI_ASH_KEYBOARD_CHROME_KEYBOARD_CONTROLLER_CLIENT_H_

#include <memory>
#include <optional>
#include <set>
#include <vector>

#include "ash/public/cpp/keyboard/keyboard_config.h"
#include "ash/public/cpp/keyboard/keyboard_controller.h"
#include "ash/public/cpp/keyboard/keyboard_controller_observer.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "url/gurl.h"

class ChromeKeyboardWebContents;
class Profile;

namespace aura {
class Window;
}

// This class implements KeyboardControllerObserver and makes calls
// into the KeyboardController service. It also observes keyboard prefs
// and enables or disables the keyboard accordingly.
class ChromeKeyboardControllerClient
    : public ash::KeyboardControllerObserver,
      public session_manager::SessionManagerObserver {
 public:
  // Convenience observer allowing UI classes to observe the global instance of
  // this class.
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Forwards the 'OnKeyboardVisibilityChanged' observer method.
    // This is used by oobe and login to adjust the UI.
    virtual void OnKeyboardVisibilityChanged(bool visible) {}

    virtual void OnKeyboardVisibleBoundsChanged(
        const gfx::Rect& screen_bounds) {}

    // Forwards the 'OnKeyboardOccludedBoundsChanged' observer method.
    // This is used to update the insets of browser and app windows when the
    // keyboard is shown.
    virtual void OnKeyboardOccludedBoundsChanged(
        const gfx::Rect& screen_bounds) {}

    // Notifies observers when the keyboard content (i.e. the extension) has
    // loaded. Note: if the content is already loaded when the observer is
    // added, this will not be triggered, but see is_keyboard_loaded().
    virtual void OnKeyboardLoaded() {}

    // Forwards the 'OnKeyboardEnabledChanged' observer method.
    virtual void OnKeyboardEnabledChanged(bool enabled) {}
  };

  // Creates the singleton instance for chrome or browser tests.
  // |Init| needs to be called after creation.
  static std::unique_ptr<ChromeKeyboardControllerClient> Create();

  // Creates the singleton instance for unit tests where there is no
  // SessionManager. Prefs will not be observed or affect the enabled state.
  // |Init| needs to be called after creation.
  static std::unique_ptr<ChromeKeyboardControllerClient> CreateForTest();

  // Static getter. The single instance must be instantiated first.
  static ChromeKeyboardControllerClient* Get();

  // Used in tests to determine whether this has been instantiated.
  static bool HasInstance();

  ChromeKeyboardControllerClient(const ChromeKeyboardControllerClient&) =
      delete;
  ChromeKeyboardControllerClient& operator=(
      const ChromeKeyboardControllerClient&) = delete;

  ~ChromeKeyboardControllerClient() override;

  // Called after ash::Shell is created.
  void Init(ash::KeyboardController* keyboard_controller);

  // Called before Shell or the primary profile is destroyed.
  void Shutdown();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // In Classic Ash, notifies this that the contents have loaded, triggering
  // OnKeyboardLoaded.
  void NotifyKeyboardLoaded();

  // Returns the cached KeyboardConfig value.
  keyboard::KeyboardConfig GetKeyboardConfig();

  // Sets the new keyboard configuration and updates the cached config.
  void SetKeyboardConfig(const keyboard::KeyboardConfig& config);

  // Returns the current enabled state. Call this after Set/ClearEnableFlag to
  // get the updated enabled state.
  bool GetKeyboardEnabled();

  // Sets/clears the privided keyboard enable state.
  void SetEnableFlag(const keyboard::KeyboardEnableFlag& flag);
  void ClearEnableFlag(const keyboard::KeyboardEnableFlag& flag);

  // Returns whether |flag| has been set.
  bool IsEnableFlagSet(const keyboard::KeyboardEnableFlag& flag);

  // Calls forwarded to ash::KeyboardController.
  void ReloadKeyboardIfNeeded();
  void RebuildKeyboardIfEnabled();
  void ShowKeyboard();
  void HideKeyboard(ash::HideReason reason);
  void SetContainerType(keyboard::ContainerType container_type,
                        const gfx::Rect& target_bounds,
                        base::OnceCallback<void(bool)> callback);
  void SetKeyboardLocked(bool locked);
  void SetOccludedBounds(const std::vector<gfx::Rect>& bounds);
  void SetHitTestBounds(const std::vector<gfx::Rect>& bounds);
  bool SetAreaToRemainOnScreen(const gfx::Rect& bounds);
  void SetDraggableArea(const gfx::Rect& bounds);
  bool SetWindowBoundsInScreen(const gfx::Rect& bounds_in_screen);
  void SetKeyboardConfigFromPref(bool enabled);

  // Returns true if overscroll is enabled by the config or command line.
  bool IsKeyboardOverscrollEnabled();

  // Returns the URL to use for the virtual keyboard.
  GURL GetVirtualKeyboardUrl();

  // Returns the keyboard window, or null if the window has not been created.
  aura::Window* GetKeyboardWindow() const;

  bool is_keyboard_enabled() { return is_keyboard_enabled_; }
  bool is_keyboard_loaded() { return is_keyboard_loaded_; }
  bool is_keyboard_visible() { return is_keyboard_visible_; }

  void set_keyboard_enabled_for_test(bool enabled) {
    is_keyboard_enabled_ = enabled;
  }
  void set_keyboard_visible_for_test(bool visible) {
    is_keyboard_visible_ = visible;
  }
  void set_profile_for_test(Profile* profile) { profile_for_test_ = profile; }
  void set_virtual_keyboard_url_for_test(const GURL& url) {
    virtual_keyboard_url_for_test_ = url;
  }

 private:
  ChromeKeyboardControllerClient();

  // Called from Create() to observer session_SessionManager and initialize
  // |pref_change_registrar_| once the session starts.
  void InitializePrefObserver();

  // ash::KeyboardControllerObserver:
  void OnKeyboardEnableFlagsChanged(
      const std::set<keyboard::KeyboardEnableFlag>& flags) override;
  void OnKeyboardEnabledChanged(bool enabled) override;
  void OnKeyboardConfigChanged(const keyboard::KeyboardConfig& config) override;
  void OnKeyboardVisibilityChanged(bool visible) override;
  void OnKeyboardVisibleBoundsChanged(const gfx::Rect& screen_bounds) override;
  void OnKeyboardOccludedBoundsChanged(const gfx::Rect& screen_bounds) override;
  void OnLoadKeyboardContentsRequested() override;
  void OnKeyboardUIDestroyed() override;

  // Called when the keyboard contents have loaded. Notifies observers.
  void OnKeyboardContentsLoaded();

  // session_manager::SessionManagerObserver:
  void OnSessionStateChanged() override;

  // Sets whether the virtual keyboard is enabled from prefs.
  void SetTouchKeyboardEnabledFromPrefs();

  // Sets whether smart visibility is enabled from prefs.
  void SetSmartVisibilityFromPrefs();

  // Returns either the test profile or the active user profile.
  Profile* GetProfile();

  gfx::Rect BoundsFromScreen(const gfx::Rect& screen_bounds);

  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  raw_ptr<ash::KeyboardController> keyboard_controller_ = nullptr;

  // Set when the WS is used and OnLoadKeyboardContentsRequested is called.
  std::unique_ptr<ChromeKeyboardWebContents> keyboard_contents_;

  // Cached copy of the latest config provided by KeyboardController.
  std::optional<keyboard::KeyboardConfig> cached_keyboard_config_;

  // Cached copy of the active enabled flags provided by KeyboardController.
  std::set<keyboard::KeyboardEnableFlag> keyboard_enable_flags_;

  // Tracks the enabled state of the keyboard.
  bool is_keyboard_enabled_ = false;

  // Tracks when the keyboard content has loaded.
  bool is_keyboard_loaded_ = false;

  // Tracks the visible state of the keyboard.
  bool is_keyboard_visible_ = false;

  base::ObserverList<Observer> observers_;

  raw_ptr<Profile> profile_for_test_ = nullptr;
  GURL virtual_keyboard_url_for_test_;

  base::WeakPtrFactory<ChromeKeyboardControllerClient> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_KEYBOARD_CHROME_KEYBOARD_CONTROLLER_CLIENT_H_
