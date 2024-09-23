// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_message_center/media_controls_progress_view.h"

#include "base/i18n/time_formatting.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "ui/base/metadata/metadata_impl_macros.h"
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

constexpr int kProgressBarAndTimeSpacing = 8;
constexpr int kProgressTimeFontSize = 11;
constexpr int kProgressBarHeight = 4;
constexpr int kMinClickHeight = 14;
constexpr int kMaxClickHeight = 24;
constexpr gfx::Size kTimeSpacingSize = gfx::Size(150, 10);
constexpr auto kProgressViewInsets = gfx::Insets::TLBR(15, 0, 0, 0);

constexpr int kModernProgressBarHeight = 2;
constexpr auto kModernProgressViewInsets = gfx::Insets::TLBR(8, 0, 8, 0);

}  // namespace

MediaControlsProgressView::MediaControlsProgressView(
    base::RepeatingCallback<void(double)> seek_callback,
    bool is_modern_notification)
    : is_modern_notification_(is_modern_notification),
      seek_callback_(std::move(seek_callback)) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      is_modern_notification_ ? kModernProgressViewInsets : kProgressViewInsets,
      kProgressBarAndTimeSpacing));

  progress_bar_ = AddChildView(std::make_unique<views::ProgressBar>());
  progress_bar_->SetPreferredHeight(
      is_modern_notification_ ? kModernProgressBarHeight : kProgressBarHeight);
  progress_bar_->SetPreferredCornerRadii(std::nullopt);

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
  progress_time->SetAutoColorReadabilityEnabled(false);
  progress_time_ = time_view->AddChildView(std::move(progress_time));

  auto time_spacing = std::make_unique<views::View>();
  time_spacing->SetPreferredSize(kTimeSpacingSize);
  time_spacing->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kUnbounded));
  time_view->AddChildView(std::move(time_spacing));

  auto duration = std::make_unique<views::Label>();
  duration->SetFontList(font_list);
  duration->SetAutoColorReadabilityEnabled(false);
  duration_ = time_view->AddChildView(std::move(duration));

  if (is_modern_notification_)
    time_view->SetVisible(false);
  AddChildView(std::move(time_view));
}

MediaControlsProgressView::~MediaControlsProgressView() = default;

void MediaControlsProgressView::UpdateProgress(
    const media_session::MediaPosition& media_position) {
  is_live_ = media_position.duration().is_max();
  if (is_live_ == duration_->GetVisible()) {
    duration_->SetVisible(!is_live_);
    InvalidateLayout();
  }

  // If the media is paused and |update_progress_timer_| is still running, stop
  // the timer.
  if (media_position.playback_rate() == 0 && update_progress_timer_.IsRunning())
    update_progress_timer_.Stop();

  const base::TimeDelta current_position = media_position.GetPosition();
  const base::TimeDelta duration = media_position.duration();
  // Use 1.0 for live playback, correctly, or as a fallback for those cases in
  // which the result is unfriendly.
  SetBarProgress((is_live_ || duration.is_zero() || current_position.is_inf())
                     ? 1.0
                     : current_position / duration);

  // For durations greater than 24 hours, prefer base::DURATION_WIDTH_NARROW for
  // better readability (e.g., 27h 23m 10s rather than 27:23:10).
  base::DurationFormatWidth time_format = duration >= base::Days(1)
                                              ? base::DURATION_WIDTH_NARROW
                                              : base::DURATION_WIDTH_NUMERIC;

  std::u16string elapsed_time;
  bool elapsed_time_received = base::TimeDurationFormatWithSeconds(
      current_position, time_format, &elapsed_time);

  std::u16string total_time;
  bool total_time_received =
      base::TimeDurationFormatWithSeconds(duration, time_format, &total_time);

  if (elapsed_time_received && total_time_received) {
    // If |duration| is less than an hour, we don't want to show  "0:" hours on
    // the progress times.
    if (duration < base::Hours(1)) {
      base::ReplaceFirstSubstringAfterOffset(&elapsed_time, 0, u"0:", u"");
      base::ReplaceFirstSubstringAfterOffset(&total_time, 0, u"0:", u"");
    }

    SetProgressTime(elapsed_time);
    SetDuration(is_live_ ? std::u16string() : total_time);
  }

  if (media_position.playback_rate() != 0) {
    base::TimeDelta update_frequency =
        base::Seconds(std::abs(1 / media_position.playback_rate()));
    update_progress_timer_.Start(
        FROM_HERE, update_frequency,
        base::BindRepeating(&MediaControlsProgressView::UpdateProgress,
                            base::Unretained(this), media_position));
  }
}

void MediaControlsProgressView::SetForegroundColor(SkColor color) {
  progress_bar_->SetForegroundColor(color);
}

void MediaControlsProgressView::SetForegroundColorId(ui::ColorId color_id) {
  progress_bar_->SetForegroundColorId(color_id);
}

void MediaControlsProgressView::SetBackgroundColor(SkColor color) {
  progress_bar_->SetBackgroundColor(color);
}

void MediaControlsProgressView::SetBackgroundColorId(ui::ColorId color_id) {
  progress_bar_->SetBackgroundColorId(color_id);
}

void MediaControlsProgressView::SetTextColor(SkColor color) {
  progress_time_->SetEnabledColor(color);
  duration_->SetEnabledColor(color);
}

void MediaControlsProgressView::SetTextColorId(ui::ColorId color_id) {
  progress_time_->SetEnabledColorId(color_id);
  duration_->SetEnabledColorId(color_id);
}

bool MediaControlsProgressView::OnMousePressed(const ui::MouseEvent& event) {
  if (is_live_)
    return false;

  if (!event.IsOnlyLeftMouseButton())
    return false;

  if (!is_modern_notification_ &&
      (event.y() < kMinClickHeight || event.y() > kMaxClickHeight)) {
    return false;
  }

  HandleSeeking(event.location());
  return true;
}

void MediaControlsProgressView::OnGestureEvent(ui::GestureEvent* event) {
  if (is_live_)
    return;

  if (event->type() != ui::EventType::kGestureTap) {
    return;
  }

  if (!is_modern_notification_ &&
      (event->y() < kMinClickHeight || event->y() > kMaxClickHeight)) {
    return;
  }

  HandleSeeking(event->location());
  event->SetHandled();
}

const views::ProgressBar* MediaControlsProgressView::progress_bar_for_testing()
    const {
  return progress_bar_;
}

const std::u16string& MediaControlsProgressView::progress_time_for_testing()
    const {
  return progress_time_->GetText();
}

const std::u16string& MediaControlsProgressView::duration_for_testing() const {
  return duration_->GetText();
}

bool MediaControlsProgressView::is_duration_visible_for_testing() const {
  return duration_->GetVisible();
}

void MediaControlsProgressView::SetBarProgress(double progress) {
  progress_bar_->SetValue(progress);
}

void MediaControlsProgressView::SetProgressTime(const std::u16string& time) {
  progress_time_->SetText(time);
}

void MediaControlsProgressView::SetDuration(const std::u16string& duration) {
  duration_->SetText(duration);
}

void MediaControlsProgressView::HandleSeeking(const gfx::Point& location) {
  gfx::Point location_in_bar(location);
  ConvertPointToTarget(this, progress_bar_, &location_in_bar);

  double seek_to_progress =
      static_cast<double>(location_in_bar.x()) / progress_bar_->width();
  seek_callback_.Run(seek_to_progress);
}

BEGIN_METADATA(MediaControlsProgressView)
END_METADATA

}  // namespace media_message_center
