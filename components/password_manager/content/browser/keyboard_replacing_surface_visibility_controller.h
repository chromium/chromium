// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_KEYBOARD_REPLACING_SURFACE_VISIBILITY_CONTROLLER_H_
#define COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_KEYBOARD_REPLACING_SURFACE_VISIBILITY_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "content/public/browser/render_widget_host.h"

namespace password_manager {

// This class is responsible for handling the visibility state of a keyboard
// replacing surface. A surface can be shown only once, when the state is
// `kCanBeShown`. The state can be reset, using `Reset()` to redisplay.
// The lifetime is controlled by the owner (i.e. `PasswordManagerClient`) and
// the object will be used by `TouchToFillController` and `CredManController`.
class KeyboardReplacingSurfaceVisibilityController
    : public base::SupportsWeakPtr<
          KeyboardReplacingSurfaceVisibilityController> {
 public:
  enum class State {
    kNotShownYet,
    kVisible,
    kShownBefore,
  };

  KeyboardReplacingSurfaceVisibilityController();
  KeyboardReplacingSurfaceVisibilityController(
      const KeyboardReplacingSurfaceVisibilityController&) = delete;
  KeyboardReplacingSurfaceVisibilityController& operator=(
      const KeyboardReplacingSurfaceVisibilityController&) = delete;
  ~KeyboardReplacingSurfaceVisibilityController();

  // Returns `true` iff the state is not shown yet.
  // (i.e. `state` == `kNotShownYet`)
  bool CanBeShown() const;

  // Returns `true` iff the state is visible.
  // (i.e. `state` == `kVisible`)
  bool IsVisible() const;

  // Sets the state to `kVisible` if it's not visible. Adds IME suppression
  // callbacks to the passed `widget_host`.
  void SetVisible(raw_ptr<content::RenderWidgetHost> widget_host);

  // Sets the state to `kShownBefore`.
  void SetShown();

  // Resets the state to the initial (`kNotShownYet`) and removes the added IME
  // suppression if there's one.
  void Reset();

 private:
  State state_ = State::kNotShownYet;
  raw_ptr<content::RenderWidgetHost> widget_host_;
  content::RenderWidgetHost::SuppressShowingImeCallback suppress_callback_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_KEYBOARD_REPLACING_SURFACE_VISIBILITY_CONTROLLER_H_
