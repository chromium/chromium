// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_KEYBOARD_OBSERVER_H_
#define COMPONENTS_EXO_KEYBOARD_OBSERVER_H_

namespace exo {
class Keyboard;

// Observers to the Keyboard are notified when the Keyboard destructs.
class KeyboardObserver {
 public:
  virtual ~KeyboardObserver() = default;

  // Called at the top of the keyboard's destructor, to give observers a change
  // to remove themselves.
  virtual void OnKeyboardDestroying(Keyboard* keyboard) {}

  // Called just before KeyboardDelegate::OnKeyboardKey().
  // KeyboardDelegate::OnKeyboardKey() may not be called, specifically if IME
  // consumed the key event, but this is always.
  virtual void OnKeyboardKey(base::TimeTicks time_stamp,
                             ui::DomCode code,
                             bool pressed) {}
};

}  // namespace exo

#endif  // COMPONENTS_EXO_KEYBOARD_OBSERVER_H_
