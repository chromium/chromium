// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_CHROME_KEYBOARD_CONTROLLER_CLIENT_H_
#define CHROME_BROWSER_UI_ASH_CHROME_KEYBOARD_CONTROLLER_CLIENT_H_

#include "ash/public/interfaces/keyboard_controller.mojom.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "url/gurl.h"

class Profile;

namespace service_manager {
class Connector;
}

// This class implements mojom::KeyboardControllerObserver and makes calls
// into the mojom::KeyboardController service.
class ChromeKeyboardControllerClient
    : public ash::mojom::KeyboardControllerObserver {
 public:
  // Convenience observer allowing UI classes to observe the global instance of
  // this class instead of setting up mojo bindings.
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Forwards the 'OnKeyboardVisibilityChanged' mojo observer method.
    // This is used by oobe and login to adjust the UI.
    virtual void OnKeyboardVisibilityChanged(bool visible) = 0;
  };

  // This class uses a static getter and only supports a single instance.
  explicit ChromeKeyboardControllerClient(
      service_manager::Connector* connector);
  ~ChromeKeyboardControllerClient() override;

  // Static getter. The single instance must be instantiated first.
  static ChromeKeyboardControllerClient* Get();

  // Used in tests to determine whether this has been instantiated.
  static bool HasInstance();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns the cached KeyboardConfig value.
  keyboard::mojom::KeyboardConfig GetKeyboardConfig();

  // Sets the new keyboard configuration and updates the cached config.
  void SetKeyboardConfig(const keyboard::mojom::KeyboardConfig& config);

  // Sets/clears the privided keyboard enable state.
  void SetEnableFlag(const keyboard::mojom::KeyboardEnableFlag& state);
  void ClearEnableFlag(const keyboard::mojom::KeyboardEnableFlag& state);

  // Reloads the virtual keyboard if enabled.
  void ReloadKeyboard();

  // Returns true if overscroll is enabled by the config or command line.
  bool IsKeyboardOverscrollEnabled();

  // Returns the URL to use for the virtual keyboard.
  GURL GetVirtualKeyboardUrl();

  bool is_keyboard_enabled() { return is_keyboard_enabled_; }

  void FlushForTesting();

  void set_profile_for_test(Profile* profile) { profile_for_test_ = profile; }
  void set_virtual_keyboard_url_for_test(const GURL& url) {
    virtual_keyboard_url_for_test_ = url;
  }

 private:
  void OnGetInitialKeyboardConfig(keyboard::mojom::KeyboardConfigPtr config);

  // keyboard::mojom::KeyboardControllerObserver:
  void OnKeyboardEnabledChanged(bool enabled) override;
  void OnKeyboardConfigChanged(
      keyboard::mojom::KeyboardConfigPtr config) override;
  void OnKeyboardVisibilityChanged(bool visible) override;
  void OnKeyboardVisibleBoundsChanged(const gfx::Rect& bounds) override;

  // Returns either the test profile or the active user profile.
  Profile* GetProfile();

  ash::mojom::KeyboardControllerPtr keyboard_controller_ptr_;
  mojo::AssociatedBinding<ash::mojom::KeyboardControllerObserver>
      keyboard_controller_observer_binding_{this};

  // Cached copy of the latest config provided by mojom::KeyboardController.
  keyboard::mojom::KeyboardConfigPtr cached_keyboard_config_;

  // Tracks the enabled state of the keyboard.
  bool is_keyboard_enabled_ = false;

  base::ObserverList<Observer> observers_;

  Profile* profile_for_test_ = nullptr;
  GURL virtual_keyboard_url_for_test_;

  base::WeakPtrFactory<ChromeKeyboardControllerClient> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ChromeKeyboardControllerClient);
};

#endif  // CHROME_BROWSER_UI_ASH_CHROME_KEYBOARD_CONTROLLER_CLIENT_H_
