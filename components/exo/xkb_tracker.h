// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_XKB_TRACKER_H_
#define COMPONENTS_EXO_XKB_TRACKER_H_

#include <memory>
#include <string>

#include "ui/base/buildflags.h"

#if BUILDFLAG(USE_XKBCOMMON)
#include <xkbcommon/xkbcommon.h>

#include "base/memory/free_deleter.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/scoped_xkb.h"  // nogncheck
#include "ui/events/ozone/layout/xkb/xkb_modifier_converter.h"
#endif

namespace exo {

struct KeyboardModifiers;

// Tracks the state of XKB. If Chrome is configured not to link against
// libxkbcommon this class is empty.
// TODO(hidehiko): Share the state between wl_keyboard and zwp_text_input.
class XkbTracker {
 public:
  XkbTracker();
  XkbTracker(const XkbTracker&) = delete;
  XkbTracker& operator=(const XkbTracker&) = delete;
  ~XkbTracker();

#if BUILDFLAG(USE_XKBCOMMON)
  // Updates the XKB keymap based on the given keyboard layout name.
  void UpdateKeyboardLayout(const std::string& name);

  // Updates the XKB modifier state. |modifier_flags| is a bitset of
  // ui::EventFlags.
  void UpdateKeyboardModifiers(int modifier_flags);

  // Returns the keysym for the given XKB keycode, based on the current
  // keymap and its modifier state.
  uint32_t GetKeysym(uint32_t xkb_keycode) const;

  // Returns the XKB keymap data.
  std::unique_ptr<char, base::FreeDeleter> GetKeymap() const;

  // Returns the current keyboard modifiers.
  KeyboardModifiers GetModifiers() const;

 private:
  void UpdateKeyboardLayoutInternal(const xkb_rule_names* names);
  void UpdateKeyboardModifiersInternal();

  // Keeps the modifiers, so that modifiers can be recalculated
  // on keyboard layout update.
  // For CrOS we treat numlock as always on.
  int modifier_flags_ = ui::EF_NUM_LOCK_ON;

  // The XKB state used for the keyboard.
  std::unique_ptr<xkb_context, ui::XkbContextDeleter> xkb_context_{
      xkb_context_new(XKB_CONTEXT_NO_FLAGS)};
  std::unique_ptr<xkb_keymap, ui::XkbKeymapDeleter> xkb_keymap_;
  std::unique_ptr<xkb_state, ui::XkbStateDeleter> xkb_state_;
  ui::XkbModifierConverter xkb_modifier_converter_{{}};

#endif  // BUILDFLAG(USE_XKBCOMMON)
};

}  // namespace exo

#endif  // COMPONENTS_EXO_XKB_TRACKER_H_
