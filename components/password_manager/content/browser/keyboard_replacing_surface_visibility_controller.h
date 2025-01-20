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
  virtual ~KeyboardReplacingSurfaceVisibilityController() = default;

  // Returns `true` iff the surface can be shown.
  virtual bool CanBeShown() const = 0;

  // Returns `true` iff the surface is visible.
  virtual bool IsVisible() const = 0;

  // Sets the surface to visible if it's not visible. Adds IME suppression
  // callbacks to the passed `frame_driver`.
  virtual void SetVisible(
      base::WeakPtr<password_manager::ContentPasswordManagerDriver>
          frame_driver) = 0;

  // Sets the surface to shown.
  virtual void SetShown() = 0;

  // Sets the surface to can be shown.
  virtual void SetCanBeShown() = 0;

  // Resets the surface to the initial state and removes the added IME
  // suppression if there's one.
  virtual void Reset() = 0;

  // Get a WeakPtr to the instance.
  virtual base::WeakPtr<KeyboardReplacingSurfaceVisibilityController>
  AsWeakPtr() = 0;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_KEYBOARD_REPLACING_SURFACE_VISIBILITY_CONTROLLER_H_
