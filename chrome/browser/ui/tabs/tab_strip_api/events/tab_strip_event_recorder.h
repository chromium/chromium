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

namespace tabs_api::events {

// This object will handle incoming external events and convert them to native
// events::Event type objects. Components in tab strip service should never
// handle external event types and should only use native |events::Event|. This
// object can also optional suppress notification and record incoming messages,
// then replay them at a specific time.
//
// The notification mechanism is a simple |RepeatingCallback|.
class TabStripEventRecorder : public TabStripModelObserver {
 public:
  using EventNotificationCallback = base::RepeatingCallback<void(const Event&)>;

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
  void OnTabGroupChanged(const TabGroupChange& change) override;
  void TabGroupedStateChanged(TabStripModel* tab_strip_model,
                              std::optional<tab_groups::TabGroupId> old_group,
                              std::optional<tab_groups::TabGroupId> new_group,
                              tabs::TabInterface* tab,
                              int index) override;

 protected:
  void Handle(Event event);
  void Notify(Event& event);

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
