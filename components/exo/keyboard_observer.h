// Copyright 2017 The Chromium Authors. All rights reserved.
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
  virtual void OnKeyboardDestroying(Keyboard* keyboard) = 0;
};

}  // namespace exo

#endif  // COMPONENTS_EXO_KEYBOARD_OBSERVER_H_
