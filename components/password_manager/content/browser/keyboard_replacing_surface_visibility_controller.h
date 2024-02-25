// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_KEYBOARD_REPLACING_SURFACE_VISIBILITY_CONTROLLER_H_
#define COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_KEYBOARD_REPLACING_SURFACE_VISIBILITY_CONTROLLER_H_

#include "base/memory/weak_ptr.h"

namespace password_manager {
class ContentPasswordManagerDriver;

// This class is responsible for handling the visibility state of a keyboard
// replacing surface. A surface can be shown only once, when the state is
// `kCanBeShown`. The state can be reset, using `Reset()` to redisplay.
// The lifetime is controlled by the owner (i.e. `PasswordManagerClient`) and
// the object will be used by `TouchToFillController` and `CredManController`.
class KeyboardReplacingSurfaceVisibilityController {
 public:
  enum class State {
    kNotShownYet,
    kVisible,
    kShownBefore,
  };

  virtual ~KeyboardReplacingSurfaceVisibilityController() = default;

  // Returns `true` iff the state is not shown yet.
  // (i.e. `state` == `kNotShownYet`)
  virtual bool CanBeShown() const = 0;

  // Returns `true` iff the state is visible.
  // (i.e. `state` == `kVisible`)
  virtual bool IsVisible() const = 0;

  // Sets the state to `kVisible` if it's not visible. Adds IME suppression
  // callbacks to the passed `widget_host`.
  virtual void SetVisible(
      base::WeakPtr<password_manager::ContentPasswordManagerDriver>
          frame_driver) = 0;

  // Sets the state to `kShownBefore`.
  virtual void SetShown() = 0;

  // Resets the state to the initial (`kNotShownYet`) and removes the added IME
  // suppression if there's one.
  virtual void Reset() = 0;

  // Get a WeakPtr to the instance.
  virtual base::WeakPtr<KeyboardReplacingSurfaceVisibilityController>
  AsWeakPtr() = 0;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_KEYBOARD_REPLACING_SURFACE_VISIBILITY_CONTROLLER_H_
