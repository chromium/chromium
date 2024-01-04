// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_KEYBOARD_REPLACING_SURFACE_VISIBILITY_CONTROLLER_IMPL_H_
#define COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_KEYBOARD_REPLACING_SURFACE_VISIBILITY_CONTROLLER_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "components/password_manager/content/browser/keyboard_replacing_surface_visibility_controller.h"
#include "content/public/browser/render_widget_host.h"

namespace password_manager {
class ContentPasswordManagerDriver;

// This class is responsible for handling the visibility state of a keyboard
// replacing surface. A surface can be shown only once, when the state is
// `kCanBeShown`. The state can be reset, using `Reset()` to redisplay.
// The lifetime is controlled by the owner (i.e. `PasswordManagerClient`) and
// the object will be used by `TouchToFillController` and `CredManController`.
class KeyboardReplacingSurfaceVisibilityControllerImpl final
    : public KeyboardReplacingSurfaceVisibilityController {
 public:
  enum class State {
    kNotShownYet,
    kVisible,
    kShownBefore,
  };

  KeyboardReplacingSurfaceVisibilityControllerImpl();
  KeyboardReplacingSurfaceVisibilityControllerImpl(
      const KeyboardReplacingSurfaceVisibilityControllerImpl&) = delete;
  KeyboardReplacingSurfaceVisibilityControllerImpl& operator=(
      const KeyboardReplacingSurfaceVisibilityControllerImpl&) = delete;
  ~KeyboardReplacingSurfaceVisibilityControllerImpl() override;

  // Returns `true` iff the state is not shown yet.
  // (i.e. `state` == `kNotShownYet`)
  bool CanBeShown() const override;

  // Returns `true` iff the state is visible.
  // (i.e. `state` == `kVisible`)
  bool IsVisible() const override;

  // Sets the state to `kVisible` if it's not visible. Adds IME suppression
  // callbacks to the passed `widget_host`.
  void SetVisible(base::WeakPtr<password_manager::ContentPasswordManagerDriver>
                      frame_driver) override;

  // Sets the state to `kShownBefore`.
  void SetShown() override;

  // Resets the state to the initial (`kNotShownYet`) and removes the added IME
  // suppression if there's one.
  void Reset() override;

  // Get a WeakPtr to the instance.
  base::WeakPtr<KeyboardReplacingSurfaceVisibilityController> AsWeakPtr()
      override;

 private:
  State state_ = State::kNotShownYet;
  // Password manager driver for the frame on which the Touch-To-Fill was
  // triggered.
  base::WeakPtr<password_manager::ContentPasswordManagerDriver> frame_driver_;
  content::RenderWidgetHost::SuppressShowingImeCallback suppress_callback_;
  base::WeakPtrFactory<KeyboardReplacingSurfaceVisibilityControllerImpl>
      weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_KEYBOARD_REPLACING_SURFACE_VISIBILITY_CONTROLLER_IMPL_H_
