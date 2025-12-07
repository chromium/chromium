// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/event_utils.h"

#include "base/i18n/rtl.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/types/event_type.h"

namespace event_utils {

bool IsPossibleDispositionEvent(const ui::Event& event) {
  return event.IsMouseEvent() && (event.flags() & (ui::EF_LEFT_MOUSE_BUTTON |
                                                   ui::EF_MIDDLE_MOUSE_BUTTON));
}

std::optional<ReorderDirection> GetReorderCommandForKeyboardEvent(
    const ui::KeyEvent& event) {
  constexpr int kModifierFlag =
#if BUILDFLAG(IS_MAC)
      ui::EF_COMMAND_DOWN;
#else
      ui::EF_CONTROL_DOWN;
#endif
  if (event.type() != ui::EventType::kKeyPressed ||
      (event.flags() & kModifierFlag) == 0) {
    return std::nullopt;
  }

  const bool is_right = event.key_code() == ui::VKEY_RIGHT;
  const bool is_left = event.key_code() == ui::VKEY_LEFT;
  if (!is_left && !is_right) {
    return std::nullopt;
  }

  const bool is_rtl = base::i18n::IsRTL();
  const bool is_next = (is_right && !is_rtl) || (is_left && is_rtl);
  return is_next ? ReorderDirection::kNext : ReorderDirection::kPrevious;
}

}  // namespace event_utils
