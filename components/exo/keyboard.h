// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_KEYBOARD_H_
#define COMPONENTS_EXO_KEYBOARD_H_

#include <memory>

#include "ash/ime/ime_controller_impl.h"
#include "ash/public/cpp/keyboard/keyboard_controller_observer.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "components/exo/key_state.h"
#include "components/exo/keyboard_observer.h"
#include "components/exo/seat_observer.h"
#include "components/exo/surface_observer.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"

namespace ui {
enum class DomCode : uint32_t;
class KeyEvent;
}

namespace exo {
class KeyboardDelegate;
class KeyboardDeviceConfigurationDelegate;
class Seat;
class Surface;

// This class implements a client keyboard that represents one or more keyboard
// devices.
class Keyboard : public ui::EventHandler,
                 public SurfaceObserver,
                 public SeatObserver,
                 public ash::KeyboardControllerObserver,
                 public ash::ImeController::Observer {
 public:
  Keyboard(std::unique_ptr<KeyboardDelegate> delegate, Seat* seat);
  Keyboard(const Keyboard&) = delete;
  Keyboard& operator=(const Keyboard&) = delete;
  ~Keyboard() override;

  KeyboardDelegate* delegate() const { return delegate_.get(); }

  bool HasDeviceConfigurationDelegate() const;
  void SetDeviceConfigurationDelegate(
      KeyboardDeviceConfigurationDelegate* delegate);

  // Management of the observer list.
  void AddObserver(KeyboardObserver* observer);
  bool HasObserver(KeyboardObserver* observer) const;
  void RemoveObserver(KeyboardObserver* observer);

  void SetNeedKeyboardKeyAcks(bool need_acks);
  bool AreKeyboardKeyAcksNeeded() const;

  void AckKeyboardKey(uint32_t serial, bool handled);

  // Overridden from ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;

  // Overridden from SurfaceObserver:
  void OnSurfaceDestroying(Surface* surface) override;

  // Overridden from SeatObserver:
  void OnSurfaceFocused(Surface* gained_focus,
                        Surface* lost_focus,
                        bool has_focused_surface) override;
  void OnKeyboardModifierUpdated() override;

  // Overridden from ash::KeyboardControllerObserver:
  void OnKeyboardEnableFlagsChanged(
      const std::set<keyboard::KeyboardEnableFlag>& flags) override;
  void OnKeyRepeatSettingsChanged(
      const ash::KeyRepeatSettings& settings) override;

  // Overridden from ash::ImeController::Observer:
  void OnCapsLockChanged(bool enabled) override;
  void OnKeyboardLayoutNameChanged(const std::string& layout_name) override;

  Surface* focused_surface_for_testing() const { return focus_; }

 private:
  // Returns a set of keys with keys that should not be handled by the surface
  // filtered out from pressed_keys_.
  base::flat_map<PhysicalCode, base::flat_set<KeyState>>
  GetPressedKeysForSurface(Surface* surface);

  // Change keyboard focus to |surface|.
  void SetFocus(Surface* surface);

  // Processes expired key state changes in |pending_key_acks_| as they have not
  // been acknowledged.
  void ProcessExpiredPendingKeyAcks();

  // Schedule next call of ProcessExpiredPendingKeyAcks after |delay|
  void ScheduleProcessExpiredPendingKeyAcks(base::TimeDelta delay);

  // Adds/Removes pre or post event handler depending on if key acks are needed.
  // If key acks are needed, pre target handler will be added because this class
  // wants to dispatch keys before they are consumed by Chrome. Otherwise, post
  // target handler will be added because all accelerators should be handled by
  // Chrome before they are dispatched by this class.
  void AddEventHandler();
  void RemoveEventHandler();

  // Notify the current keyboard type.
  void UpdateKeyboardType();

  // The delegate instance that all events except for events about device
  // configuration are dispatched to.
  std::unique_ptr<KeyboardDelegate> delegate_;

  // Seat that the Keyboard recieves focus events from.
  const raw_ptr<Seat> seat_;

  // The delegate instance that events about device configuration are dispatched
  // to.
  raw_ptr<KeyboardDeviceConfigurationDelegate> device_configuration_delegate_ =
      nullptr;

  // Indicates that each key event is expected to be acknowledged.
  bool are_keyboard_key_acks_needed_ = false;

  // The current focus surface for the keyboard.
  raw_ptr<Surface> focus_ = nullptr;

  // Set of currently pressed keys. First value is a platform code and second
  // value is the code that was delivered to client. See Seat.h for more
  // details.
  base::flat_map<PhysicalCode, base::flat_set<KeyState>> pressed_keys_;

  // Key state changes that are expected to be acknowledged.
  using KeyStateChange = std::pair<ui::KeyEvent, base::TimeTicks>;
  base::flat_map<uint32_t, KeyStateChange> pending_key_acks_;

  // Indicates that a ProcessExpiredPendingKeyAcks call is pending.
  bool process_expired_pending_key_acks_pending_ = false;

  // Delay until a key state change expected to be acknowledged is expired.
  const base::TimeDelta expiration_delay_for_pending_key_acks_;

  // Tracks whether the last key event is the target of autorepeat.
  bool auto_repeat_enabled_ = true;

  // True when the ARC app window is focused.
  // TODO(yhanada, https://crbug.com/847500): Remove this when we find a way to
  // fix https://crbug.com/847500 without breaking ARC++ apps.
  bool focused_on_ime_supported_surface_ = false;

  base::ObserverList<KeyboardObserver>::Unchecked observer_list_;

  base::WeakPtrFactory<Keyboard> weak_ptr_factory_{this};
};

}  // namespace exo

#endif  // COMPONENTS_EXO_KEYBOARD_H_
