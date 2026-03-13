// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/events/tab_strip_event_recorder.h"

namespace tabs_api::events {

TabStripEventRecorder::TabStripEventRecorder(
    EventNotificationCallback event_notification_callback)
    : event_notification_callback_(std::move(event_notification_callback)) {}
TabStripEventRecorder::~TabStripEventRecorder() = default;

void TabStripEventRecorder::StopNotificationAndStartRecording() {
  mode_ = Mode::kRecording;
}

void TabStripEventRecorder::PlayRecordingsAndStartNotification() {
  std::vector<Event> events;
  while (HasRecordedEvents()) {
    events.push_back(std::move(recorded_.front()));
    recorded_.pop();
  }
  Notify(events);
  mode_ = Mode::kPassthrough;
}

bool TabStripEventRecorder::HasRecordedEvents() const {
  return !recorded_.empty();
}

void TabStripEventRecorder::OnEvent(Event event) {
  Handle(std::move(event));
}

void TabStripEventRecorder::OnEvents(std::vector<Event> event) {
  Handle(std::move(event));
}

void TabStripEventRecorder::Notify(const std::vector<Event>& event) {
  event_notification_callback_.Run(event);
}

void TabStripEventRecorder::Handle(Event event) {
  if (mode_ == Mode::kPassthrough) {
    std::vector<Event> bundled;
    bundled.push_back(std::move(event));
    Notify(bundled);
  } else {
    recorded_.push(std::move(event));
  }
}

void TabStripEventRecorder::Handle(std::vector<Event> events) {
  for (auto& event : events) {
    Handle(std::move(event));
  }
}

}  // namespace tabs_api::events
