// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/event_utils.h"

#include "ui/events/event.h"

namespace event_utils {

bool IsPossibleDispositionEvent(const ui::Event& event) {
  return event.IsMouseEvent() && (event.flags() &
             (ui::EF_LEFT_MOUSE_BUTTON | ui::EF_MIDDLE_MOUSE_BUTTON));
}

}  // namespace event_utils
