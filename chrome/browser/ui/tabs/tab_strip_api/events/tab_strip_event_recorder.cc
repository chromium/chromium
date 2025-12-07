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

void TabStripEventRecorder::OnChildrenAdded(
    const tabs::TabCollection::Position& position,
    const tabs::TabCollectionNodes& handles,
    bool insert_from_detached) {
  for (const auto& handle : handles) {
    if (const auto* tab_handle_ptr = std::get_if<tabs::TabHandle>(&handle)) {
      Handle(ToEvent(*tab_handle_ptr, position, tab_strip_model_adapter_));
    } else if (const auto* collection_handle_ptr =
                   std::get_if<tabs::TabCollectionHandle>(&handle)) {
      Handle(ToEvent(*collection_handle_ptr, position, tab_strip_model_adapter_,
                     insert_from_detached));
    }
  }
}

void TabStripEventRecorder::OnChildrenRemoved(
    const tabs::TabCollection::Position& position,
    const tabs::TabCollectionNodes& handles) {
  Handle(ToEvent(handles));
}

void TabStripEventRecorder::OnChildMoved(
    const tabs::TabCollection::Position& to_position,
    const NodeData& node_data) {
  const tabs::TabCollection::Position& from_position = node_data.position;
  const tabs::TabCollection::NodeHandle node_handle = node_data.handle;

  Handle(ToEvent(to_position, from_position, node_handle));
}

void TabStripEventRecorder::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  // Avoid listening to add, remove and move changes as this is handled by the
  // TabCollection observation methods.
  if (change.type() == TabStripModelChange::Type::kReplaced) {
    NOTIMPLEMENTED();
  }

  if (selection.active_tab_changed() || selection.selection_changed()) {
    Handle(ToEvent(selection, tab_strip_model_adapter_));
  }
}

void TabStripEventRecorder::TabChangedAt(content::WebContents* contents,
                                         int index,
                                         TabChangeType change_type) {
  Handle(ToEvent(tab_strip_model_adapter_, index, change_type));
}

void TabStripEventRecorder::TabBlockedStateChanged(
    content::WebContents* contents,
    int index) {
  TabChangedAt(contents, index, TabChangeType::kAll);
}

void TabStripEventRecorder::OnTabGroupChanged(const TabGroupChange& change) {
  if (change.type == TabGroupChange::Type::kEditorOpened) {
    NOTIMPLEMENTED();
    return;
  }

  if (change.type == TabGroupChange::Type::kVisualsChanged) {
    Handle(ToEvent(change));
    return;
  }
}

}  // namespace tabs_api::events
