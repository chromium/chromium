// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_EVENTS_TAB_STRIP_EVENT_RECORDER_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_EVENTS_TAB_STRIP_EVENT_RECORDER_H_

#include <queue>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/tabs/tab_strip_api/adapters/tab_strip_model_adapter.h"
#include "chrome/browser/ui/tabs/tab_strip_api/events/event.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_collection_observer.h"

namespace tabs_api::events {

// This object will handle incoming external events and convert them to native
// events::Event type objects. Components in tab strip service should never
// handle external event types and should only use native |events::Event|. This
// object can also optional suppress notification and record incoming messages,
// then replay them at a specific time.
//
// The notification mechanism is a simple |RepeatingCallback|.
class TabStripEventRecorder : public TabStripModelObserver,
                              public tabs::TabCollectionObserver {
 public:
  using EventNotificationCallback =
      base::RepeatingCallback<void(const std::vector<Event>&)>;

  TabStripEventRecorder(const TabStripModelAdapter* tab_strip_model_adapter,
                        EventNotificationCallback event_notification_callback);
  TabStripEventRecorder(const TabStripEventRecorder&) = delete;
  TabStripEventRecorder& operator=(const TabStripEventRecorder&) = delete;
  ~TabStripEventRecorder() override;

  // Stops client notification and begin recording incomingevents for later
  // playback.
  void StopNotificationAndStartRecording();
  // Immediately run notification on all recorded events and stop recording.
  // Clients will be notified of future events past this call.
  void PlayRecordingsAndStartNotification();
  // Whether or not the recorder has recorded events.
  bool HasRecordedEvents() const;

  ///////////////////////////////////////////////////////////////////////////
  // Integration points with external services.

  // TabStripModelObserver overrides
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
  void TabChangedAt(content::WebContents* contents,
                    int index,
                    TabChangeType change_type) override;
  void TabBlockedStateChanged(content::WebContents* contents,
                              int index) override;
  void OnTabGroupChanged(const TabGroupChange& change) override;

  // tabs::TabCollectionObserver
  void OnChildrenAdded(const tabs::TabCollection::Position& position,
                       const tabs::TabCollectionNodes& handles,
                       bool insert_from_detached) override;

  void OnChildrenRemoved(const tabs::TabCollection::Position& position,
                         const tabs::TabCollectionNodes& handles) override;

  void OnChildMoved(const tabs::TabCollection::Position& to_position,
                    const NodeData& node_data) override;

 protected:
  // Consumes the event.
  void Handle(Event event);
  // Consumes the events.
  void Handle(std::vector<Event> event);
  void Notify(const std::vector<Event>& event);

 private:
  enum class Mode {
    kPassthrough,  // Immediately notify, do not record.
    kRecording,    // Do not notify, record incoming messages for later reply.
  };
  Mode mode_ = Mode::kPassthrough;
  // Recorded events.
  std::queue<Event> recorded_;
  raw_ptr<const TabStripModelAdapter> tab_strip_model_adapter_;
  EventNotificationCallback event_notification_callback_;
};

}  // namespace tabs_api::events

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_EVENTS_TAB_STRIP_EVENT_RECORDER_H_
