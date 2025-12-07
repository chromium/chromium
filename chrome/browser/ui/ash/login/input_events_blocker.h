// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LOGIN_INPUT_EVENTS_BLOCKER_H_
#define CHROME_BROWSER_UI_ASH_LOGIN_INPUT_EVENTS_BLOCKER_H_

#include "ui/events/event_handler.h"

namespace ash {

// A simple input events blocker that just makes device unresponsive.
// Should be used only for a (very) short time lock as it's confusing to the
// user.
class InputEventsBlocker : public ui::EventHandler {
 public:
  InputEventsBlocker();

  InputEventsBlocker(const InputEventsBlocker&) = delete;
  InputEventsBlocker& operator=(const InputEventsBlocker&) = delete;

  ~InputEventsBlocker() override;

  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_LOGIN_INPUT_EVENTS_BLOCKER_H_
