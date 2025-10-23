// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/events/tab_strip_event_recorder.h"

#include "base/test/bind.h"
#include "chrome/browser/ui/tabs/tab_strip_api/events/event.h"
#include "components/browser_apis/tab_strip/tab_strip_api.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tabs_api::events {
namespace {

class TestableRecorder : public TabStripEventRecorder {
 public:
  // init'ing with nullptr is not correct, but we need to redesign the event
  // transformation code to make this more easy to test.
  explicit TestableRecorder(EventNotificationCallback notification)
      : TabStripEventRecorder(nullptr, std::move(notification)) {}
  void Handle(Event event) { TabStripEventRecorder::Handle(std::move(event)); }
};

TEST(TabStripServiceEventRecorderTest, Notification) {
  bool notified = false;
  TestableRecorder recorder(base::BindLambdaForTesting(
      [&](const std::vector<Event>& event) { notified = true; }));

  recorder.Handle(mojom::OnTabsCreatedEvent::New());

  ASSERT_TRUE(notified);
}

TEST(TabStripServiceEventRecorderTest, StoppingAndStartingNotification) {
  bool notified = false;
  TestableRecorder recorder(base::BindLambdaForTesting(
      [&](const std::vector<Event>& event) { notified = true; }));

  recorder.StopNotificationAndStartRecording();

  auto event = mojom::OnTabsCreatedEvent::New();
  auto container = tabs_api::mojom::TabCreatedContainer::New();
  event->tabs.emplace_back(std::move(container));

  recorder.Handle(std::move(event));

  // notification should have been suppressed.
  ASSERT_FALSE(notified);

  // re-enabling it should cause immediate playback.
  recorder.PlayRecordingsAndStartNotification();

  ASSERT_TRUE(notified);

  // handling another notification should cause another notify.
  notified = false;
  recorder.Handle(mojom::OnTabsCreatedEvent::New());
  ASSERT_TRUE(notified);
}

}  // namespace
}  // namespace tabs_api::events
