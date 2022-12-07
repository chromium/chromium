// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_FRAME_CAPTION_BUTTONS_SNAP_CONTROLLER_H_
#define CHROMEOS_UI_FRAME_CAPTION_BUTTONS_SNAP_CONTROLLER_H_

#include "base/component_export.h"

namespace aura {
class Window;
}

namespace chromeos {

// Snap ratios that correspond to the size of a window when it is snapped. A
// window with `kOneThirdSnapRatio` will snap to one third of the display,
// `kTwoThirdSnapRatio` will snap to two thirds of the display, and
// `kDefaultSnapRatio` will snap to the default half of the display.
constexpr float kOneThirdSnapRatio = 0.33f;
constexpr float kDefaultSnapRatio = 0.5f;
constexpr float kTwoThirdSnapRatio = 0.67f;

// The previewed snap state for a window, corresponding to the use of a
// PhantomWindowController.
enum class SnapDirection {
  kNone,       // No snap preview.
  kPrimary,    // The phantom window controller is previewing a snap to the
               // primary position, translated into left for landscape display
               // (or right for secondary display layout) and top (or bottom)
               // for portrait display. For more details, see
               // description for `SplitViewController::IsLayoutHorizontal()`.
  kSecondary,  // The phantom window controller is previewing a snap to the
               // secondary position, the opposite position of the primary. For
               // example, in primary portrait display, primary position is the
               // top and secondary position is the bottom.
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
  // `allow_haptic_feedback` indicates if it should send haptic feedback.
  virtual void ShowSnapPreview(aura::Window* window,
                               SnapDirection snap,
                               bool allow_haptic_feedback) = 0;

  // Snaps the window in the given direction, if not kNone. Destroys the preview
  // window, if any.
  virtual void CommitSnap(aura::Window* window,
                          SnapDirection snap,
                          float snap_ratio) = 0;

 protected:
  SnapController();
};

}  // namespace chromeos

#endif  // CHROMEOS_UI_FRAME_CAPTION_BUTTONS_SNAP_CONTROLLER_H_
