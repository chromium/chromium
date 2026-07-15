// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/waap/waap_ui_metrics_recorder.h"

#include "base/strings/strcat.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/waap/waap_ui_metrics_service.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"

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

void WaapUIMetricsRecorder::OnButtonPressedStart(const ui::Event& event) {
  if (!waap_service_ || !IsReloadButtonInputType(event.type())) {
    return;
  }

  auto input_type = ToReloadButtonInputType(event.type());

  last_input_info_.emplace(LastInputInfo{
      .time = event.time_stamp(),
      .type = input_type,
  });

  waap_service_->OnReloadButtonInput(input_type);
}

void WaapUIMetricsRecorder::DidExecuteReloadCommand(base::TimeTicks time) {
  if (!waap_service_ || !last_input_info_.has_value()) {
    return;
  }

  waap_service_->RecordReloadButtonInteractionToReload(
      last_input_info_->time, time, last_input_info_->type);
  last_input_info_.reset();
}
