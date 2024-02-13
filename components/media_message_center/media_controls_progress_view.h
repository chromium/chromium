// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_CONTROLS_PROGRESS_VIEW_H_
#define COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_CONTROLS_PROGRESS_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/color/color_id.h"
#include "ui/views/view.h"

namespace media_session {
struct MediaPosition;
}  // namespace media_session

namespace views {
class ProgressBar;
class Label;
}  // namespace views

namespace media_message_center {

class COMPONENT_EXPORT(MEDIA_MESSAGE_CENTER) MediaControlsProgressView
    : public views::View {
  METADATA_HEADER(MediaControlsProgressView, views::View)

 public:
  explicit MediaControlsProgressView(
      base::RepeatingCallback<void(double)> seek_callback,
      bool is_modern_notification = false);
  MediaControlsProgressView(const MediaControlsProgressView&) = delete;
  MediaControlsProgressView& operator=(const MediaControlsProgressView&) =
      delete;
  ~MediaControlsProgressView() override;

  void UpdateProgress(const media_session::MediaPosition& media_position);

  void SetForegroundColor(SkColor color);
  void SetForegroundColorId(ui::ColorId color_id);
  void SetBackgroundColor(SkColor color);
  void SetBackgroundColorId(ui::ColorId color_id);
  void SetTextColor(SkColor color);
  void SetTextColorId(ui::ColorId color_id);

  // views::View:
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  const views::ProgressBar* progress_bar_for_testing() const;
  const std::u16string& progress_time_for_testing() const;
  const std::u16string& duration_for_testing() const;
  bool is_duration_visible_for_testing() const;

 private:
  void SetBarProgress(double progress);
  void SetProgressTime(const std::u16string& time);
  void SetDuration(const std::u16string& duration);

  void HandleSeeking(const gfx::Point& location);

  raw_ptr<views::ProgressBar> progress_bar_;
  raw_ptr<views::Label> progress_time_;
  raw_ptr<views::Label> duration_;

  const bool is_modern_notification_;

  // Timer to continually update the progress.
  base::RepeatingTimer update_progress_timer_;

  const base::RepeatingCallback<void(double)> seek_callback_;

  // Used to track if the media is a live stream. i.e. Has an infinite duration.
  bool is_live_ = false;
};

}  // namespace media_message_center

#endif  // COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_CONTROLS_PROGRESS_VIEW_H_
