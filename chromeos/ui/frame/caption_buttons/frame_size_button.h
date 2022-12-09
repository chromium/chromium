// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_FRAME_CAPTION_BUTTONS_FRAME_SIZE_BUTTON_H_
#define CHROMEOS_UI_FRAME_CAPTION_BUTTONS_FRAME_SIZE_BUTTON_H_

#include <memory>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "chromeos/ui/frame/caption_buttons/frame_size_button_delegate.h"
#include "chromeos/ui/frame/multitask_menu/multitask_menu_metrics.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/display/display_observer.h"
#include "ui/views/window/frame_caption_button.h"

namespace chromeos {

// The maximize/restore button.
// When the mouse is pressed over the size button or the size button is touched:
// - The minimize and close buttons are set to snap left and snap right
//   respectively.
// - The size button stays pressed while the mouse is over the buttons to snap
//   left and to snap right. The button underneath the mouse is hovered.
// When the drag terminates, the action for the button underneath the mouse
// is executed. For the sake of simplicity, the size button is the event
// handler for a click starting on the size button and the entire drag.
// When the mouse is long pressed or long hovered over the size button, the
// multitask menu bubble shows up.
class COMPONENT_EXPORT(CHROMEOS_UI_FRAME) FrameSizeButton
    : public views::FrameCaptionButton,
      public display::DisplayObserver {
 public:
  METADATA_HEADER(FrameSizeButton);

  FrameSizeButton(PressedCallback callback, FrameSizeButtonDelegate* delegate);

  FrameSizeButton(const FrameSizeButton&) = delete;
  FrameSizeButton& operator=(const FrameSizeButton&) = delete;

  ~FrameSizeButton() override;

  void ShowMultitaskMenu(MultitaskMenuEntryType entry_type);

  // views::Button:
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseCaptureLost() override;
  void OnMouseMoved(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void StateChanged(views::Button::ButtonState old_state) override;
  void PaintButtonContents(gfx::Canvas* canvas) override;

  // display::DisplayObserver:
  void OnDisplayTabletStateChanged(display::TabletState state) override;

  // Cancel the snap operation if we're currently in snap mode. The snap
  // preview will be deleted and the button will be set back to its normal mode.
  void CancelSnap();

  void set_delay_to_set_buttons_to_snap_mode(int delay_ms) {
    set_buttons_to_snap_mode_delay_ms_ = delay_ms;
  }
  bool in_snap_mode_for_testing() { return in_snap_mode_; }

 private:
  class PieAnimation;
  class SnappingWindowObserver;

  // Starts |set_buttons_to_snap_mode_timer_|.
  void StartSetButtonsToSnapModeTimer(const ui::LocatedEvent& event);

  // Starts the pie animation, which gives a visual inidicator of when the
  // multitask menu will show up on long press or long touch, where `entry_type`
  // indicates the method the user started this animation (but hasn't shown the
  // menu yet).
  void StartPieAnimation(base::TimeDelta duration,
                         MultitaskMenuEntryType entry_type);

  // Animates the buttons adjacent to the size button to snap left and right.
  void AnimateButtonsToSnapMode();

  // Sets the buttons adjacent to the size button to snap left and right.
  // Passing in ANIMATE_NO progresses the animation (if any) to the end.
  void SetButtonsToSnapMode(FrameSizeButtonDelegate::Animate animate);

  // Asks the delegate to update the appearance of adjacent buttons and show a
  // phantom window.
  void UpdateSnapPreview(const ui::LocatedEvent& event);

  // Returns the button which should be hovered (if any) while in "snap mode"
  // for |event|.
  const views::FrameCaptionButton* GetButtonToHover(
      const ui::LocatedEvent& event) const;

  // Snaps the window based on |event|. Returns true if a snap occurred, false
  // if no snap (i.e. a preview was cancelled).
  bool CommitSnap(const ui::LocatedEvent& event);

  // Sets the buttons adjacent to the size button to minimize and close again.
  // Clears any state set while snapping was enabled. |animate| indicates
  // whether the buttons should animate back to their original icons.
  void SetButtonsToNormalMode(FrameSizeButtonDelegate::Animate animate);

  // Show Multitask Menu when pie animation is completed, where `entry_type`
  // indicates the method the user started and completed this animation and show
  // the menu.
  void OnPieAnimationCompleted(MultitaskMenuEntryType entry_type);
  void DestroyPieAnimation();

  // Not owned.
  raw_ptr<FrameSizeButtonDelegate> delegate_;

  // The window observer to observe the to-be-snapped window.
  std::unique_ptr<SnappingWindowObserver> snapping_window_observer_;

  // Location of the event which started |set_buttons_to_snap_mode_timer_| in
  // view coordinates.
  gfx::Point set_buttons_to_snap_mode_timer_event_location_;

  // The delay between the user pressing the size button and the buttons
  // adjacent to the size button morphing into buttons for snapping left and
  // right.
  int set_buttons_to_snap_mode_delay_ms_;

  base::OneShotTimer set_buttons_to_snap_mode_timer_;

  // Creates an animation to add indication to when long hover and long press to
  // show multitask menu and snap buttons will trigger.
  std::unique_ptr<PieAnimation> pie_animation_;

  // Whether the buttons adjacent to the size button snap the window left and
  // right.
  bool in_snap_mode_ = false;

  absl::optional<display::ScopedDisplayObserver> display_observer_;
};

}  // namespace chromeos

#endif  // CHROMEOS_UI_FRAME_CAPTION_BUTTONS_FRAME_SIZE_BUTTON_H_
