// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_WAYLAND_KEYBOARD_DELEGATE_H_
#define COMPONENTS_EXO_WAYLAND_WAYLAND_KEYBOARD_DELEGATE_H_

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "build/buildflag.h"
#include "components/exo/keyboard_delegate.h"
#include "components/exo/keyboard_observer.h"
#include "components/exo/wayland/server_util.h"
#include "components/exo/wayland/wayland_input_delegate.h"
#include "ui/base/buildflags.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

#if defined(OS_CHROMEOS)
#include "ash/ime/ime_controller.h"
#include "ash/shell.h"
#include "ui/events/ozone/layout/xkb/xkb_keyboard_layout_engine.h"
#endif

#if BUILDFLAG(USE_XKBCOMMON)
#include <xkbcommon/xkbcommon.h>
#include "ui/events/keycodes/scoped_xkb.h"  // nogncheck
#endif

struct wl_client;
struct wl_resource;

namespace exo {
namespace wayland {
class SerialTracker;

// Keyboard delegate class that accepts events for surfaces owned by the same
// client as a keyboard resource.
class WaylandKeyboardDelegate : public WaylandInputDelegate,
                                public KeyboardDelegate,
                                public KeyboardObserver
#if defined(OS_CHROMEOS)
    ,
                                public ash::ImeController::Observer
#endif
{
#if BUILDFLAG(USE_XKBCOMMON)
 public:
  explicit WaylandKeyboardDelegate(wl_resource* keyboard_resource,
                                   SerialTracker* serial_tracker);

#if defined(OS_CHROMEOS)
  ~WaylandKeyboardDelegate() override;
#endif

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

#if defined(OS_CHROMEOS)
  // Overridden from ImeController::Observer:
  void OnCapsLockChanged(bool enabled) override;
  void OnKeyboardLayoutNameChanged(const std::string& layout_name) override;
#endif

 private:
  // Returns the corresponding key given a dom code.
  uint32_t DomCodeToKey(ui::DomCode code) const;

  // Returns a set of Xkb modififers given the current |modifier_flags_|.
  uint32_t ModifierFlagsToXkbModifiers();

  // Sends the current |modifier_flags_| to the client.
  void SendKeyboardModifiers();

#if defined(OS_CHROMEOS)
  // Send the named keyboard layout to the client.
  void SendNamedLayout(const std::string& layout_name);
#endif

  // Send the keyboard layout named by XKB rules to the client.
  void SendLayout(const xkb_rule_names* names);

  // The client who own this keyboard instance.
  wl_client* client() const;

  // The keyboard resource associated with the keyboard.
  wl_resource* const keyboard_resource_;

  // The Xkb state used for the keyboard.
  std::unique_ptr<xkb_context, ui::XkbContextDeleter> xkb_context_;
  std::unique_ptr<xkb_keymap, ui::XkbKeymapDeleter> xkb_keymap_;
  std::unique_ptr<xkb_state, ui::XkbStateDeleter> xkb_state_;

  // The delegate will keep its clients updated with these modifiers. For CrOS
  // we treat numlock as always on.
  int modifier_flags_ = ui::EF_NUM_LOCK_ON;

  // Owned by Server, which always outlives this delegate.
  SerialTracker* const serial_tracker_;

  DISALLOW_COPY_AND_ASSIGN(WaylandKeyboardDelegate);
#endif
};

}  // namespace wayland
}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_WAYLAND_KEYBOARD_DELEGATE_H_
