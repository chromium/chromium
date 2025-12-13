// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/waap/waap_ui_metrics_recorder.h"

#include "base/strings/strcat.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/waap/waap_ui_metrics_service.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"
#include "ui/views/controls/button/button.h"  // For Button::STATE_*

namespace {

// Converts a ui::EventType to a ReloadButtonInputType.
// If the event type is not relevant to the ReloadButton, this will fail
// immediately.
WaapUIMetricsRecorder::ReloadButtonInputType ToReloadButtonInputType(
    const ui::EventType& type) {
  switch (type) {
    case ui::EventType::kMouseReleased:
      return WaapUIMetricsRecorder::ReloadButtonInputType::kMouseRelease;
    case ui::EventType::kKeyPressed:
      return WaapUIMetricsRecorder::ReloadButtonInputType::kKeyPress;
    default:
      NOTREACHED();
  }
}

// Returns true if the event type is relevant to the ReloadButton.
bool IsReloadButtonInputType(const ui::EventType& type) {
  switch (type) {
    case ui::EventType::kMouseReleased:
    case ui::EventType::kKeyPressed:
      return true;
    default:
      return false;
  }
}

}  // namespace

WaapUIMetricsRecorder::WaapUIMetricsRecorder(Profile* profile)
    : waap_service_(WaapUIMetricsService::Get(profile)) {}

WaapUIMetricsRecorder::~WaapUIMetricsRecorder() = default;

void WaapUIMetricsRecorder::OnMouseEntered(base::TimeTicks time) {
  if (mouse_entered_time_.is_null()) {
    mouse_entered_time_ = time;
  }
}

void WaapUIMetricsRecorder::OnMouseExited(base::TimeTicks time) {
  // The next mouseenter will only happen after this mouseexit.
  mouse_entered_time_ = base::TimeTicks();
}

void WaapUIMetricsRecorder::OnMousePressed(base::TimeTicks time) {
  // There can be multiple mousepress events before the mouserelease.
  // Only the timestamp of the last mousepress is remembered.
  mouse_pressed_time_ = time;
}

void WaapUIMetricsRecorder::OnMouseReleased(base::TimeTicks time) {
  mouse_pressed_time_ = base::TimeTicks();
}

void WaapUIMetricsRecorder::OnButtonPressedStart(
    const ui::Event& event,
    ReloadButtonMode current_mode) {
  if (!waap_service_ || !IsReloadButtonInputType(event.type())) {
    return;
  }

  auto input_type = ToReloadButtonInputType(event.type());

  last_input_info_.emplace(LastInputInfo{
      .time = event.time_stamp(),
      .type = input_type,
      .mode_at_input = current_mode,
  });

  waap_service_->OnReloadButtonInput(input_type);
}

void WaapUIMetricsRecorder::DidExecuteStopCommand(base::TimeTicks time) {
  if (!waap_service_ || !last_input_info_.has_value()) {
    return;
  }

  waap_service_->OnReloadButtonInputToStop(last_input_info_->time, time,
                                           last_input_info_->type);
}

// Called after the Reload command has been executed.
void WaapUIMetricsRecorder::DidExecuteReloadCommand(base::TimeTicks time) {
  if (!waap_service_ || !last_input_info_.has_value()) {
    return;
  }

  waap_service_->OnReloadButtonInputToReload(last_input_info_->time, time,
                                             last_input_info_->type);
}

void WaapUIMetricsRecorder::OnChangeVisibleMode(ReloadButtonMode current_mode,
                                                ReloadButtonMode intended_mode,
                                                base::TimeTicks time) {
  if (!waap_service_ || current_mode == intended_mode) {
    return;
  }

  pending_mode_change_.emplace(PendingModeChange{
      .start_time = time,
      .target_mode = intended_mode,
  });
}

void WaapUIMetricsRecorder::OnPaintFramePresented(ReloadButtonMode visible_mode,
                                                  int button_state,
                                                  base::TimeTicks now) {
  static bool is_first_paint_recorded = false;
  if (!waap_service_) {
    return;
  }

  // Log first paint metrics.
  if (!is_first_paint_recorded) {
    is_first_paint_recorded = true;
    waap_service_->OnFirstPaint(now);
    waap_service_->OnFirstContentfulPaint(now);
  }

  // Log MousePressToNextPaint.
  if (button_state == views::Button::STATE_PRESSED &&
      !mouse_pressed_time_.is_null()) {
    waap_service_->OnReloadButtonMousePressToNextPaint(mouse_pressed_time_,
                                                       now);
    mouse_pressed_time_ = base::TimeTicks();  // Reset
  }

  // Log MouseHoverToNextPaint.
  if (button_state == views::Button::STATE_HOVERED &&
      !mouse_entered_time_.is_null()) {
    waap_service_->OnReloadButtonMouseHoverToNextPaint(mouse_entered_time_,
                                                       now);
    mouse_entered_time_ = base::TimeTicks();  // Reset
  }

  // Log InputToNextPaint.
  if (last_input_info_.has_value()) {
    waap_service_->OnReloadButtonInputToNextPaint(last_input_info_->time, now,
                                                  last_input_info_->type);
    last_input_info_.reset();
  }

  // Log ChangeVisibleModeToNextPaint.
  if (pending_mode_change_.has_value() &&
      visible_mode == pending_mode_change_->target_mode) {
    waap_service_->OnReloadButtonChangeVisibleModeToNextPaint(
        pending_mode_change_->start_time, now, visible_mode);
    pending_mode_change_.reset();
  }
}
