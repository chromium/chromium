// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/events/tab_strip_event_recorder.h"

#include "base/notimplemented.h"
#include "chrome/browser/ui/tabs/tab_strip_api/events/event_transformation.h"

namespace tabs_api::events {

TabStripEventRecorder::TabStripEventRecorder(
    const TabStripModelAdapter* tab_strip_model_adapter,
    EventNotificationCallback event_notification_callback)
    : tab_strip_model_adapter_(tab_strip_model_adapter),
      event_notification_callback_(std::move(event_notification_callback)) {}
TabStripEventRecorder::~TabStripEventRecorder() = default;

void TabStripEventRecorder::StopNotificationAndStartRecording() {
  mode_ = Mode::kRecording;
}

void TabStripEventRecorder::PlayRecordingsAndStartNotification() {
  while (HasRecordedEvents()) {
    auto event = std::move(recorded_.front());
    recorded_.pop();
    Notify(event);
  }
  mode_ = Mode::kPassthrough;
}

bool TabStripEventRecorder::HasRecordedEvents() const {
  return !recorded_.empty();
}

void TabStripEventRecorder::Notify(Event& event) {
  event_notification_callback_.Run(event);
}

void TabStripEventRecorder::Handle(Event event) {
  if (mode_ == Mode::kPassthrough) {
    Notify(event);
  } else {
    recorded_.push(std::move(event));
  }
}

void TabStripEventRecorder::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  switch (change.type()) {
    case TabStripModelChange::Type::kSelectionOnly:
      NOTIMPLEMENTED();
      break;
    case TabStripModelChange::Type::kInserted:
      Handle(ToEvent(*change.GetInsert(), tab_strip_model));
      break;
    case TabStripModelChange::Type::kRemoved:
      Handle(ToEvent(*change.GetRemove()));
      break;
    case TabStripModelChange::Type::kMoved:
      Handle(ToEvent(*change.GetMove()));
      break;
    case TabStripModelChange::Type::kReplaced:
      NOTIMPLEMENTED();
      break;
  }
}

void TabStripEventRecorder::TabChangedAt(content::WebContents* contents,
                                         int index,
                                         TabChangeType change_type) {
  Handle(ToEvent(tab_strip_model_adapter_, index, change_type));
}

void TabStripEventRecorder::OnTabGroupChanged(const TabGroupChange& change) {
  switch (change.type) {
    case TabGroupChange::Type::kCreated:
      Handle(ToTabGroupCreatedEvent(change));
      break;
    case TabGroupChange::Type::kEditorOpened:
      NOTIMPLEMENTED();
      break;
    case TabGroupChange::Type::kVisualsChanged:
      Handle(ToTabGroupVisualsChangedEvent(change));
      break;
    case TabGroupChange::Type::kMoved:
      NOTIMPLEMENTED();
      break;
    case TabGroupChange::Type::kClosed:
      NOTIMPLEMENTED();
      break;
  }
  // When opening a saved tab group from the bookmark, OnTabGroupAdded() won't
  // be called. However, OnTabGroupChanged() is called with the added group.
}

void TabStripEventRecorder::TabGroupedStateChanged(
    TabStripModel* tab_strip_model,
    std::optional<tab_groups::TabGroupId> old_group,
    std::optional<tab_groups::TabGroupId> new_group,
    tabs::TabInterface* tab,
    int index) {
  Handle(FromTabGroupedStateChangedToTabMovedEvent(tab_strip_model, old_group,
                                                   new_group, tab, index));
}

}  // namespace tabs_api::events
