// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_WAYLAND_KEYBOARD_DELEGATE_H_
#define COMPONENTS_EXO_WAYLAND_WAYLAND_KEYBOARD_DELEGATE_H_

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "build/buildflag.h"
#include "components/exo/keyboard_delegate.h"
#include "components/exo/keyboard_observer.h"
#include "components/exo/wayland/server_util.h"
#include "components/exo/wayland/wayland_input_delegate.h"
#include "ui/base/buildflags.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

#if defined(OS_CHROMEOS)
#include "ash/ime/ime_controller_impl.h"
#endif

#if BUILDFLAG(USE_XKBCOMMON)
#include <xkbcommon/xkbcommon.h>
#include "ui/events/keycodes/scoped_xkb.h"  // nogncheck
#endif

struct wl_client;
struct wl_resource;

namespace exo {

class XkbTracker;

namespace wayland {
class SerialTracker;

// Keyboard delegate class that accepts events for surfaces owned by the same
// client as a keyboard resource.
class WaylandKeyboardDelegate : public WaylandInputDelegate,
                                public KeyboardDelegate,
                                public KeyboardObserver
#if defined(OS_CHROMEOS)
    ,
                                public ash::ImeControllerImpl::Observer
#endif
{
#if BUILDFLAG(USE_XKBCOMMON)
 public:
  WaylandKeyboardDelegate(wl_resource* keyboard_resource,
                          SerialTracker* serial_tracker);
  ~WaylandKeyboardDelegate() override;

  // Overridden from KeyboardDelegate:
  void OnKeyboardDestroying(Keyboard* keyboard) override;
  bool CanAcceptKeyboardEventsForSurface(Surface* surface) const override;
  void OnKeyboardEnter(
      Surface* surface,
      const base::flat_map<ui::DomCode, ui::DomCode>& pressed_keys) override;
  void OnKeyboardLeave(Surface* surface) override;
  uint32_t OnKeyboardKey(base::TimeTicks time_stamp,
                         ui::DomCode key,
                         bool pressed) override;
  void OnKeyboardModifiers(int modifier_flags) override;
  void OnKeyRepeatSettingsChanged(bool enabled,
                                  base::TimeDelta delay,
                                  base::TimeDelta interval) override;

#if defined(OS_CHROMEOS)
  // Overridden from ImeControllerImpl::Observer:
  void OnCapsLockChanged(bool enabled) override;
  void OnKeyboardLayoutNameChanged(const std::string& layout_name) override;
#endif

 private:
  // Returns the corresponding key given a dom code.
  uint32_t DomCodeToKey(ui::DomCode code) const;

  // Sends the current modifiers to the client.
  void SendKeyboardModifiers();

  // Send the current keyboard layout to the client.
  void SendLayout();

  // The client who own this keyboard instance.
  wl_client* client() const;

  // The keyboard resource associated with the keyboard.
  wl_resource* const keyboard_resource_;

  // Owned by Server, which always outlives this delegate.
  SerialTracker* const serial_tracker_;

  // TODO(hidehiko): Move this to the server in order to share it with
  // zwp_text_input.
  std::unique_ptr<XkbTracker> xkb_tracker_;

  DISALLOW_COPY_AND_ASSIGN(WaylandKeyboardDelegate);
#endif
};

// Exposed for testing.
int32_t GetWaylandRepeatRateForTesting(bool enabled, base::TimeDelta interval);

}  // namespace wayland
}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_WAYLAND_KEYBOARD_DELEGATE_H_
