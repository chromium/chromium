// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_CONTROLS_PROGRESS_VIEW_H_
#define COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_CONTROLS_PROGRESS_VIEW_H_

#include "base/timer/timer.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "ui/views/view.h"

namespace views {
class ProgressBar;
class Label;
}  // namespace views

namespace media_message_center {

class COMPONENT_EXPORT(MEDIA_MESSAGE_CENTER) MediaControlsProgressView
    : public views::View {
 public:
  explicit MediaControlsProgressView(
      base::RepeatingCallback<void(double)> seek_callback);
  ~MediaControlsProgressView() override;

  void UpdateProgress(const media_session::MediaPosition& media_position);

  void SetForegroundColor(SkColor color);
  void SetBackgroundColor(SkColor color);

  // views::View:
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  const views::ProgressBar* progress_bar_for_testing() const;
  const base::string16& progress_time_for_testing() const;
  const base::string16& duration_for_testing() const;

 private:
  void SetBarProgress(double progress);
  void SetProgressTime(const base::string16& time);
  void SetDuration(const base::string16& duration);

  void HandleSeeking(const gfx::Point& location);

  views::ProgressBar* progress_bar_;
  views::Label* progress_time_;
  views::Label* duration_;

  // Timer to continually update the progress.
  base::RepeatingTimer update_progress_timer_;

  const base::RepeatingCallback<void(double)> seek_callback_;

  DISALLOW_COPY_AND_ASSIGN(MediaControlsProgressView);
};

}  // namespace media_message_center

#endif  // COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_CONTROLS_PROGRESS_VIEW_H_
