// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_FRAME_CAPTION_BUTTONS_FRAME_SIZE_BUTTON_DELEGATE_H_
#define CHROMEOS_UI_FRAME_CAPTION_BUTTONS_FRAME_SIZE_BUTTON_DELEGATE_H_

#include "base/component_export.h"
#include "ui/views/window/caption_button_types.h"

namespace gfx {
class Point;
}

namespace views {
class FrameCaptionButton;
}

namespace chromeos {

class MultitaskMenuNudgeController;
enum class SnapDirection;

// Delegate interface for FrameSizeButton.
class COMPONENT_EXPORT(CHROMEOS_UI_FRAME) FrameSizeButtonDelegate {
 public:
  enum class Animate { kYes, kNo };

  // Returns whether the minimize button is visible.
  virtual bool IsMinimizeButtonVisible() const = 0;

  // Reset the caption button views::Button::ButtonState back to normal. If
  // |animate| is Animate::kYes, the buttons will crossfade back to their
  // original icons.
  virtual void SetButtonsToNormal(Animate animate) = 0;

  // Sets the minimize and close button icons. The buttons will crossfade to
  // their new icons if |animate| is Animate::kYes.
  virtual void SetButtonIcons(views::CaptionButtonIcon minimize_button_icon,
                              views::CaptionButtonIcon close_button_icon,
                              Animate animate) = 0;

  // Returns the button closest to |position_in_screen|.
  virtual const views::FrameCaptionButton* GetButtonClosestTo(
      const gfx::Point& position_in_screen) const = 0;

  // Sets |to_hover| and |to_pressed| to STATE_HOVERED and STATE_PRESSED
  // respectively. All other buttons are to set to STATE_NORMAL.
  virtual void SetHoveredAndPressedButtons(
      const views::FrameCaptionButton* to_hover,
      const views::FrameCaptionButton* to_press) = 0;

  // Thunks to methods of the same name in FrameCaptionDelegate.
  virtual bool CanSnap() = 0;
  virtual void ShowSnapPreview(SnapDirection snap,
                               bool allow_haptic_feedback) = 0;
  virtual void CommitSnap(SnapDirection snap) = 0;

  virtual MultitaskMenuNudgeController* GetMultitaskMenuNudgeController() = 0;

 protected:
  virtual ~FrameSizeButtonDelegate() {}
};

}  // namespace chromeos

#endif  // CHROMEOS_UI_FRAME_CAPTION_BUTTONS_FRAME_SIZE_BUTTON_DELEGATE_H_
