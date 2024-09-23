// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/views/devtools_event_util.h"
#include "ui/events/types/event_type.h"

namespace ui_devtools {

ui::KeyEvent ConvertToUIKeyEvent(protocol::DOM::KeyEvent* event) {
  ui::EventType event_type =
      event->getType() == protocol::DOM::KeyEvent::TypeEnum::KeyPressed
          ? ui::EventType::kKeyPressed
          : ui::EventType::kKeyReleased;
  return ui::KeyEvent(
      event_type, static_cast<ui::KeyboardCode>(event->getKeyCode()),
      static_cast<ui::DomCode>(event->getCode()), event->getFlags(),
      event->getIsChar() ? ui::DomKey::FromCharacter(event->getKey())
                         : ui::DomKey(event->getKey()),
      ui::EventTimeForNow(), event->getIsChar());
}

}  // namespace ui_devtools