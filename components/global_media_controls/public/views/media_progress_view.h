// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_VIEWS_MEDIA_PROGRESS_VIEW_H_
#define COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_VIEWS_MEDIA_PROGRESS_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/color/color_id.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/views/layout/box_layout_view.h"

namespace media_session {
struct MediaPosition;
}  // namespace media_session

namespace global_media_controls {

enum class DragState {
  kDragStarted = 0,
  kDragEnded = 1,
};

enum class PlaybackStateChangeForDragging {
  kPauseForDraggingStarted = 0,
  kResumeForDraggingEnded = 1,
};

class COMPONENT_EXPORT(GLOBAL_MEDIA_CONTROLS) MediaProgressView
    : public views::BoxLayoutView,
      public gfx::AnimationDelegate {
  METADATA_HEADER(MediaProgressView, views::BoxLayoutView)

 public:
  // |MediaProgressView| draws a straight or squiggly progress line with the
  // given color IDs and runs the given callbacks when certain conditions are
  // met.
  //
  // |drag_state_change_callback|: Runs when the user starts or ends dragging
  // the progress view.
  //
  // |playback_state_change_for_dragging_callback|: Runs when the user starts or
  // ends dragging the progress view, and the media is playing before dragging
  // starts so its playback state needs to change.
  //
  // |seek_callback|: Runs when the progress view wants the media to seek to a
  // new position since the user interacts with the progress view.
  //
  // |on_update_progress_callback|: Runs when the progress view wants to inform
  // the current progress position since it can change after time.
  explicit MediaProgressView(
      bool use_squiggly_line,
      ui::ColorId playing_foreground_color_id,
      ui::ColorId playing_background_color_id,
      ui::ColorId paused_foreground_color_id,
      ui::ColorId paused_background_color_id,
      ui::ColorId focus_ring_color_id,
      base::RepeatingCallback<void(DragState)> drag_state_change_callback,
      base::RepeatingCallback<void(PlaybackStateChangeForDragging)>
          playback_state_change_for_dragging_callback,
      base::RepeatingCallback<void(double)> seek_callback,
      base::RepeatingCallback<void(base::TimeDelta)>
          on_update_progress_callback);
  MediaProgressView(const MediaProgressView&) = delete;
  MediaProgressView& operator=(const MediaProgressView&) = delete;
  ~MediaProgressView() override;

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
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
  bool use_paused_colors_for_testing() const;
  void set_update_progress_timer_for_testing(
      std::unique_ptr<base::OneShotTimer> test_timer);
  void set_switch_progress_colors_delay_timer_for_testing(
      std::unique_ptr<base::OneShotTimer> test_timer);
  void set_progress_drag_started_delay_timer_for_testing(
      std::unique_ptr<base::OneShotTimer> test_timer);

 private:
  // Fires an accessibility event if the progress has changed.
  void MaybeNotifyAccessibilityValueChanged();

  // Handles the event when user drags the progress line using a mouse or
  // gesture on a tablet. If the user only intends to click, these functions are
  // still called but the work will be skipped.
  void OnProgressDragStarted(double location);
  void DelayedProgressDragStarted(double location);
  void OnProgressDragEnded();

  // Updates the colors of the progress view based on whether the media is
  // paused.
  void UpdateProgressColors(bool is_paused);

  // Handles the event when user seeks to a new location on the progress view.
  void HandleSeeking(double location);

  // Returns the new current progress value given the new media position.
  double CalculateNewValue(base::TimeDelta new_position);

  // Returns whether the given seek position is valid to be handled.
  bool IsValidSeekPosition(int x, int y);

  // Init parameters.
  bool use_squiggly_line_;
  ui::ColorId playing_foreground_color_id_;
  ui::ColorId playing_background_color_id_;
  ui::ColorId paused_foreground_color_id_;
  ui::ColorId paused_background_color_id_;
  ui::ColorId focus_ring_color_id_;
  const base::RepeatingCallback<void(DragState)> drag_state_change_callback_;
  const base::RepeatingCallback<void(PlaybackStateChangeForDragging)>
      playback_state_change_for_dragging_callback_;
  const base::RepeatingCallback<void(double)> seek_callback_;
  const base::RepeatingCallback<void(base::TimeDelta)>
      on_update_progress_callback_;

  // Current progress value in the range from 0.0 to 1.0.
  double current_value_ = 0.0;

  // Current media position and media duration.
  base::TimeDelta current_position_;
  base::TimeDelta media_duration_ = base::TimeDelta::Max();

  // Fraction of the progress amplitude used for progress path to transition
  // between squiggly and straight lines, in the range from 0.0 to 1.0.
  double progress_amp_fraction_ = 0;

  // The media position last announced for accessibility.
  base::TimeDelta last_announced_position_;

  // The progress phase offset changing as time passes for the progress wave to
  // move.
  int phase_offset_ = 0;

  // Animation for progress path to transition between squiggly and straight
  // lines.
  gfx::SlideAnimation slide_animation_;

  // Timer to continuously update the progress value if the media is playing.
  std::unique_ptr<base::OneShotTimer> update_progress_timer_ =
      std::make_unique<base::OneShotTimer>();

  // Timer to delay switching the colors for the progress view.
  std::unique_ptr<base::OneShotTimer> switch_progress_colors_delay_timer_ =
      std::make_unique<base::OneShotTimer>();

  // Timer to delay considering the user is dragging the progress view rather
  // than clicking.
  std::unique_ptr<base::OneShotTimer> progress_drag_started_delay_timer_ =
      std::make_unique<base::OneShotTimer>();

  // True if the media is paused.
  bool is_paused_ = true;

  // True if the media is a live stream.
  bool is_live_ = false;

  // Whether the media is currently paused due to the user dragging the progress
  // line.
  bool paused_for_dragging_ = false;

  // Whether we should use the paused colors for the progress view.
  bool use_paused_colors_ = true;

  // Stroke width for the straight progress line that changes depending on
  // whether the user is dragging the progress line.
  int straight_progress_stroke_width_ = 0;
};

}  // namespace global_media_controls

#endif  // COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_VIEWS_MEDIA_PROGRESS_VIEW_H_
