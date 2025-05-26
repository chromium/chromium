// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/events/tab_strip_event_recorder.h"

#include "base/test/bind.h"
#include "chrome/browser/ui/tabs/tab_strip_api/events/event.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_api.mojom-forward.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_api.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tabs_api::events {
namespace {

class TestableRecorder : public TabStripEventRecorder {
 public:
  void Handle(Event event) { TabStripEventRecorder::Handle(std::move(event)); }
};

TEST(TabStripServiceEventRecorderTest, Notification) {
  TestableRecorder recorder;

  bool notified = false;
  recorder.SetOnEventNotification(
      base::BindLambdaForTesting([&](Event& event) { notified = true; }));

  recorder.Handle(mojom::OnTabsCreatedEvent::New());

  ASSERT_TRUE(notified);
}

TEST(TabStripServiceEventRecorderTest, NoNotificationRegistered) {
  TestableRecorder recorder;
  recorder.Handle(mojom::OnTabsCreatedEvent::New());

  // Effectively noop, but is allowed.
}

TEST(TabStripServiceEventRecorderTest, StoppingAndStartingNotification) {
  TestableRecorder recorder;

  bool notified = false;
  recorder.SetOnEventNotification(
      base::BindLambdaForTesting([&](Event& event) { notified = true; }));

  recorder.StopNotificationAndStartRecording();

  auto event = mojom::OnTabsCreatedEvent::New();
  event->tabs.emplace_back(TabId::Type::kContent, "123");

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
