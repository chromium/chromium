// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_FRAME_CAPTION_BUTTONS_SNAP_CONTROLLER_H_
#define CHROMEOS_UI_FRAME_CAPTION_BUTTONS_SNAP_CONTROLLER_H_

#include "base/component_export.h"

namespace aura {
class Window;
}

namespace chromeos {

// The previewed snap state for a window, corresponding to the use of a
// PhantomWindowController.
enum class SnapDirection {
  kNone,   // No snap preview.
  kLeft,   // The phantom window controller is previewing a snap to the left.
  kRight,  // The phantom window controller is previewing a snap to the right.
};

// This interface handles snap actions to be performed on a top level window.
// The singleton that implements the interface is provided by Ash.
class COMPONENT_EXPORT(CHROMEOS_UI_FRAME) SnapController {
 public:
  virtual ~SnapController();

  static SnapController* Get();

  // Returns whether the snapping action on the size button should be enabled.
  virtual bool CanSnap(aura::Window* window) = 0;

  // Shows a preview (phantom window) for the given snap direction.
  virtual void ShowSnapPreview(aura::Window* window, SnapDirection snap) = 0;

  // Snaps the window in the given direction, if not kNone. Destroys the preview
  // window, if any.
  virtual void CommitSnap(aura::Window* window, SnapDirection snap) = 0;

 protected:
  SnapController();
};

}  // namespace chromeos

#endif  // CHROMEOS_UI_FRAME_CAPTION_BUTTONS_SNAP_CONTROLLER_H_
