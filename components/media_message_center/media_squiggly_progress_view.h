// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_SQUIGGLY_PROGRESS_VIEW_H_
#define COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_SQUIGGLY_PROGRESS_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/color/color_id.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/views/layout/box_layout_view.h"

namespace media_session {
struct MediaPosition;
}  // namespace media_session

namespace media_message_center {

class COMPONENT_EXPORT(MEDIA_MESSAGE_CENTER) MediaSquigglyProgressView
    : public views::BoxLayoutView,
      public gfx::AnimationDelegate {
 public:
  METADATA_HEADER(MediaSquigglyProgressView);
  explicit MediaSquigglyProgressView(
      ui::ColorId playing_foreground_color_id,
      ui::ColorId playing_background_color_id,
      ui::ColorId paused_foreground_color_id,
      ui::ColorId paused_background_color_id,
      ui::ColorId focus_ring_color_id,
      base::RepeatingCallback<void(bool)> dragging_callback,
      base::RepeatingCallback<void(double)> seek_callback);
  MediaSquigglyProgressView(const MediaSquigglyProgressView&) = delete;
  MediaSquigglyProgressView& operator=(const MediaSquigglyProgressView&) =
      delete;
  ~MediaSquigglyProgressView() override;

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  bool HandleAccessibleAction(const ui::AXActionData& action_data) override;
  void VisibilityChanged(View* starting_from, bool is_visible) override;
  void AddedToWidget() override;
  void OnPaint(gfx::Canvas* canvas) override;
  void OnFocus() override;
  void OnBlur() override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // Updates the progress in UI given the new media position.
  void UpdateProgress(const media_session::MediaPosition& media_position);

  // Helper functions for testing:
  double current_value_for_testing() const;
  bool is_paused_for_testing() const;
  bool is_live_for_testing() const;

 private:
  // Fires an accessibility event if the progress has changed.
  void MaybeNotifyAccessibilityValueChanged();

  // Handles the event when user seeks to a new location on the progress view.
  void HandleSeeking(double location);

  // Returns the new current progress value given the new media position.
  double CalculateNewValue(base::TimeDelta new_position);

  // Returns whether the given seek position is valid to be handled.
  bool IsValidSeekPosition(int x, int y);

  // Init parameters.
  ui::ColorId playing_foreground_color_id_;
  ui::ColorId playing_background_color_id_;
  ui::ColorId paused_foreground_color_id_;
  ui::ColorId paused_background_color_id_;
  ui::ColorId focus_ring_color_id_;
  const base::RepeatingCallback<void(bool)> dragging_callback_;
  const base::RepeatingCallback<void(double)> seek_callback_;

  // Current progress value in the range from 0.0 to 1.0.
  double current_value_ = 0.0;

  // Current media position and media duration.
  base::TimeDelta current_position_;
  base::TimeDelta media_duration_ = base::TimeDelta::Max();

  // Fraction of the progress amplitude used for progress path to transition
  // between squiggly and straight lines, in the range from 0.0 to 1.0.
  double progress_amp_fraction_ = 0;

  // The percentage progress value last announced for accessibility.
  int last_announced_percentage_ = -1;

  // The progress phase offset changing as time passes for the progress wave to
  // move.
  int phase_offset_ = 0;

  // Animation for progress path to transition between squiggly and straight
  // lines.
  gfx::SlideAnimation slide_animation_;

  // Timer to continuously update the progress value if the media is playing.
  base::OneShotTimer update_progress_timer_;

  // True if the media is paused.
  bool is_paused_ = true;

  // True if the media is a live stream.
  bool is_live_ = false;

  // Whether the media is currently paused due to the user dragging the progress
  // line.
  bool paused_for_dragging_ = false;
};

}  // namespace media_message_center

#endif  // COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_SQUIGGLY_PROGRESS_VIEW_H_
