// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_message_center/media_controls_progress_view.h"

#include "base/i18n/time_formatting.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font_list.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/view_class_properties.h"

namespace media_message_center {

namespace {

constexpr SkColor kTimeColor = gfx::kGoogleGrey200;
constexpr int kProgressBarAndTimeSpacing = 8;
constexpr int kProgressTimeFontSize = 11;
constexpr int kProgressBarHeight = 4;
constexpr int kMinClickHeight = 14;
constexpr int kMaxClickHeight = 24;
constexpr gfx::Size kTimeSpacingSize = gfx::Size(150, 10);
constexpr gfx::Insets kProgressViewInsets = gfx::Insets(15, 0, 0, 0);

}  // namespace

MediaControlsProgressView::MediaControlsProgressView(
    base::RepeatingCallback<void(double)> seek_callback)
    : seek_callback_(std::move(seek_callback)) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, kProgressViewInsets,
      kProgressBarAndTimeSpacing));

  progress_bar_ = AddChildView(
      std::make_unique<views::ProgressBar>(kProgressBarHeight, false));

  // Font list for text views.
  gfx::Font default_font;
  int font_size_delta = kProgressTimeFontSize - default_font.GetFontSize();
  gfx::Font font = default_font.Derive(font_size_delta, gfx::Font::NORMAL,
                                       gfx::Font::Weight::NORMAL);
  gfx::FontList font_list(font);

  auto time_view = std::make_unique<views::View>();
  auto* time_view_layout =
      time_view->SetLayoutManager(std::make_unique<views::FlexLayout>());
  time_view_layout->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCollapseMargins(true);

  auto progress_time = std::make_unique<views::Label>();
  progress_time->SetFontList(font_list);
  progress_time->SetEnabledColor(kTimeColor);
  progress_time->SetAutoColorReadabilityEnabled(false);
  progress_time_ = time_view->AddChildView(std::move(progress_time));

  auto time_spacing = std::make_unique<views::View>();
  time_spacing->SetPreferredSize(kTimeSpacingSize);
  time_spacing->SetProperty(views::kFlexBehaviorKey,
                            views::FlexSpecification::ForSizeRule(
                                views::MinimumFlexSizeRule::kPreferred,
                                views::MaximumFlexSizeRule::kUnbounded));
  time_view->AddChildView(std::move(time_spacing));

  auto duration = std::make_unique<views::Label>();
  duration->SetFontList(font_list);
  duration->SetEnabledColor(SK_ColorWHITE);
  duration->SetAutoColorReadabilityEnabled(false);
  duration_ = time_view->AddChildView(std::move(duration));

  AddChildView(std::move(time_view));
}

MediaControlsProgressView::~MediaControlsProgressView() = default;

void MediaControlsProgressView::UpdateProgress(
    const media_session::MediaPosition& media_position) {
  // If the media is paused and |update_progress_timer_| is still running, stop
  // the timer.
  if (media_position.playback_rate() == 0 && update_progress_timer_.IsRunning())
    update_progress_timer_.Stop();

  base::TimeDelta current_position = media_position.GetPosition();
  base::TimeDelta duration = media_position.duration();

  double progress = current_position.InSecondsF() / duration.InSecondsF();
  SetBarProgress(progress);

  // Time formatting can't yet represent durations greater than 24 hours in
  // base::DURATION_WIDTH_NUMERIC format.
  base::DurationFormatWidth time_format =
      duration >= base::TimeDelta::FromDays(1) ? base::DURATION_WIDTH_NARROW
                                               : base::DURATION_WIDTH_NUMERIC;

  base::string16 elapsed_time;
  bool elapsed_time_received = base::TimeDurationFormatWithSeconds(
      current_position, time_format, &elapsed_time);

  base::string16 total_time;
  bool total_time_received =
      base::TimeDurationFormatWithSeconds(duration, time_format, &total_time);

  if (elapsed_time_received && total_time_received) {
    // If |duration| is less than an hour, we don't want to show  "0:" hours on
    // the progress times.
    if (duration < base::TimeDelta::FromHours(1)) {
      base::ReplaceFirstSubstringAfterOffset(
          &elapsed_time, 0, base::ASCIIToUTF16("0:"), base::ASCIIToUTF16(""));
      base::ReplaceFirstSubstringAfterOffset(
          &total_time, 0, base::ASCIIToUTF16("0:"), base::ASCIIToUTF16(""));
    }

    SetProgressTime(elapsed_time);
    SetDuration(total_time);
  }

  if (media_position.playback_rate() != 0) {
    base::TimeDelta update_frequency = base::TimeDelta::FromSecondsD(
        std::abs(1 / media_position.playback_rate()));
    update_progress_timer_.Start(
        FROM_HERE, update_frequency,
        base::Bind(&MediaControlsProgressView::UpdateProgress,
                   base::Unretained(this), media_position));
  }
}

void MediaControlsProgressView::SetForegroundColor(SkColor color) {
  progress_bar_->SetForegroundColor(color);
}

void MediaControlsProgressView::SetBackgroundColor(SkColor color) {
  progress_bar_->SetBackgroundColor(color);
}

bool MediaControlsProgressView::OnMousePressed(const ui::MouseEvent& event) {
  if (!event.IsOnlyLeftMouseButton() || event.y() < kMinClickHeight ||
      event.y() > kMaxClickHeight) {
    return false;
  }

  HandleSeeking(event.location());
  return true;
}

void MediaControlsProgressView::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() != ui::ET_GESTURE_TAP || event->y() < kMinClickHeight ||
      event->y() > kMaxClickHeight) {
    return;
  }

  HandleSeeking(event->location());
  event->SetHandled();
}

const views::ProgressBar* MediaControlsProgressView::progress_bar_for_testing()
    const {
  return progress_bar_;
}

const base::string16& MediaControlsProgressView::progress_time_for_testing()
    const {
  return progress_time_->GetText();
}

const base::string16& MediaControlsProgressView::duration_for_testing() const {
  return duration_->GetText();
}

void MediaControlsProgressView::SetBarProgress(double progress) {
  progress_bar_->SetValue(progress);
}

void MediaControlsProgressView::SetProgressTime(const base::string16& time) {
  progress_time_->SetText(time);
}

void MediaControlsProgressView::SetDuration(const base::string16& duration) {
  duration_->SetText(duration);
}

void MediaControlsProgressView::HandleSeeking(const gfx::Point& location) {
  gfx::Point location_in_bar(location);
  ConvertPointToTarget(this, progress_bar_, &location_in_bar);

  double seek_to_progress =
      static_cast<double>(location_in_bar.x()) / progress_bar_->width();
  seek_callback_.Run(seek_to_progress);
}

}  // namespace media_message_center
