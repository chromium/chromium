// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/default_browser/settings_window_finder_win.h"

#include <windows.h>

#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Subclass to mock out the real OS interaction.
class TestSettingsWindowFinderWin : public SettingsWindowFinderWin {
 protected:
  HWND FindSettingsTopLevelWindow() const override {
    // Always simulate that the window is not open initially.
    return nullptr;
  }
};

// Subclass that additionally fakes the window-matching heuristics so
// HandleWinEvent() can be driven directly with sentinel HWNDs.
class EventTestSettingsWindowFinderWin : public TestSettingsWindowFinderWin {
 public:
  explicit EventTestSettingsWindowFinderWin(HWND settings_hwnd)
      : settings_hwnd_(settings_hwnd) {}
  using SettingsWindowFinderWin::HandleWinEvent;

 protected:
  bool IsLikelySettingsWindow(HWND hwnd) const override {
    return hwnd == settings_hwnd_;
  }
  HWND GetRootWindow(HWND hwnd) const override { return hwnd; }

 private:
  const HWND settings_hwnd_;
};

class SettingsWindowFinderWinTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(SettingsWindowFinderWinTest, TimeoutTriggersCallback) {
  TestSettingsWindowFinderWin finder;
  bool timeout_called = false;
  bool found_called = false;

  finder.Start(base::Seconds(5),
               base::BindLambdaForTesting([&](HWND) { found_called = true; }),
               base::BindLambdaForTesting([&]() { timeout_called = true; }));

  // Fast-forward just before the timeout.
  task_environment_.FastForwardBy(base::Seconds(4));
  EXPECT_FALSE(timeout_called);
  EXPECT_FALSE(found_called);

  // Fast-forward past the timeout.
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(timeout_called);
  EXPECT_FALSE(found_called);
}

TEST_F(SettingsWindowFinderWinTest, StopCancelsTimeout) {
  TestSettingsWindowFinderWin finder;
  bool timeout_called = false;

  finder.Start(base::Seconds(5), base::DoNothing(),
               base::BindLambdaForTesting([&]() { timeout_called = true; }));

  // Explicitly stop the finder before the timeout triggers.
  finder.Stop();
  task_environment_.FastForwardBy(base::Seconds(6));

  EXPECT_FALSE(timeout_called);
}

TEST_F(SettingsWindowFinderWinTest, DestructorCancelsTimeout) {
  bool timeout_called = false;

  {
    TestSettingsWindowFinderWin finder;
    finder.Start(base::Seconds(5), base::DoNothing(),
                 base::BindLambdaForTesting([&]() { timeout_called = true; }));

    // `finder` goes out of scope and is destroyed here.
  }

  // Fast-forward past the expected timeout duration.
  task_environment_.FastForwardBy(base::Seconds(6));

  // The timer should have been destroyed, so the callback is never run.
  EXPECT_FALSE(timeout_called);
}

TEST_F(SettingsWindowFinderWinTest,
       NewInstanceDeactivatesPreviousActiveInstance) {
  TestSettingsWindowFinderWin finder1;
  bool finder1_timeout_called = false;
  finder1.Start(
      base::Seconds(5), base::DoNothing(),
      base::BindLambdaForTesting([&]() { finder1_timeout_called = true; }));

  TestSettingsWindowFinderWin finder2;
  bool finder2_timeout_called = false;
  // Starting finder2 should succeed and deactivate finder1.
  finder2.Start(
      base::Seconds(5), base::DoNothing(),
      base::BindLambdaForTesting([&]() { finder2_timeout_called = true; }));

  // Fast-forward past the timeout.
  task_environment_.FastForwardBy(base::Seconds(6));

  // finder1 was deactivated, so its timeout callback should NOT run.
  EXPECT_FALSE(finder1_timeout_called);
  // finder2 is active, so its timeout callback should run.
  EXPECT_TRUE(finder2_timeout_called);
}

TEST_F(SettingsWindowFinderWinTest,
       NewInstanceDeactivatesPreviousLocationObserver) {
  TestSettingsWindowFinderWin finder1;
  bool finder1_timeout_called = false;
  finder1.Start(
      base::Seconds(5), base::DoNothing(),
      base::BindLambdaForTesting([&]() { finder1_timeout_called = true; }));

  TestSettingsWindowFinderWin finder2;
  // Starting finder2 with location changes should succeed and deactivate
  // finder1.
  finder2.StartObservingLocationChanges(reinterpret_cast<HWND>(0x12345),
                                        base::DoNothing());

  // Fast-forward past the timeout.
  task_environment_.FastForwardBy(base::Seconds(6));

  // finder1 was deactivated by finder2's location observer startup.
  EXPECT_FALSE(finder1_timeout_called);
}

TEST_F(SettingsWindowFinderWinTest, FoundPathIgnoresDestroyEvents) {
  HWND sentinel = reinterpret_cast<HWND>(0x5E771);
  EventTestSettingsWindowFinderWin finder(sentinel);
  bool found_called = false;
  bool timeout_called = false;
  finder.Start(base::Seconds(5),
               base::BindLambdaForTesting([&](HWND) { found_called = true; }),
               base::BindLambdaForTesting([&]() { timeout_called = true; }));

  finder.HandleWinEvent(EVENT_OBJECT_DESTROY, sentinel, OBJID_WINDOW);
  EXPECT_FALSE(found_called);

  // Events for non-window objects are ignored too.
  finder.HandleWinEvent(EVENT_OBJECT_SHOW, sentinel, OBJID_CLIENT);
  EXPECT_FALSE(found_called);

  // With no acceptable event delivered, the finder times out normally.
  task_environment_.FastForwardBy(base::Seconds(6));
  EXPECT_TRUE(timeout_called);
  EXPECT_FALSE(found_called);
}

TEST_F(SettingsWindowFinderWinTest, FoundPathAcceptsCreateShowAndUncloak) {
  for (DWORD event : {static_cast<DWORD>(EVENT_OBJECT_CREATE),
                      static_cast<DWORD>(EVENT_OBJECT_SHOW),
                      static_cast<DWORD>(EVENT_OBJECT_UNCLOAKED)}) {
    HWND sentinel = reinterpret_cast<HWND>(0x5E771);
    EventTestSettingsWindowFinderWin finder(sentinel);
    HWND found_hwnd = nullptr;
    bool timeout_called = false;
    finder.Start(base::Seconds(5), base::BindLambdaForTesting([&](HWND hwnd) {
                   found_hwnd = hwnd;
                 }),
                 base::BindLambdaForTesting([&]() { timeout_called = true; }));

    finder.HandleWinEvent(event, sentinel, OBJID_WINDOW);
    EXPECT_EQ(found_hwnd, sentinel) << "event=0x" << std::hex << event;

    // Finding the window cancels the timeout.
    task_environment_.FastForwardBy(base::Seconds(6));
    EXPECT_FALSE(timeout_called) << "event=0x" << std::hex << event;
  }
}

// Destroy/hide/cloak transitions of the observed window must notify the
// owner (closing a UWP window often cloaks or hides its frame rather than
// destroying it, and emits no location-change event), while the same events
// for unrelated windows must not.
TEST_F(SettingsWindowFinderWinTest, ObservedWindowStateChangesNotifyOwner) {
  HWND sentinel = reinterpret_cast<HWND>(0x5E771);
  for (DWORD event : {static_cast<DWORD>(EVENT_OBJECT_LOCATIONCHANGE),
                      static_cast<DWORD>(EVENT_OBJECT_DESTROY),
                      static_cast<DWORD>(EVENT_OBJECT_HIDE),
                      static_cast<DWORD>(EVENT_OBJECT_CLOAKED)}) {
    EventTestSettingsWindowFinderWin finder(sentinel);
    int notified = 0;
    finder.StartObservingLocationChanges(
        sentinel, base::BindLambdaForTesting([&]() { ++notified; }));
    finder.HandleWinEvent(event, sentinel, OBJID_WINDOW);
    EXPECT_EQ(notified, 1) << "event=0x" << std::hex << event;
    // The same event for an unrelated window is ignored.
    finder.HandleWinEvent(event, reinterpret_cast<HWND>(0xBAD), OBJID_WINDOW);
    EXPECT_EQ(notified, 1) << "event=0x" << std::hex << event;
    finder.StopObservingLocationChanges();
  }
}

TEST_F(SettingsWindowFinderWinTest, FoundReleasesGlobalInstance) {
  HWND sentinel = reinterpret_cast<HWND>(0x5E771);
  EventTestSettingsWindowFinderWin finder1(sentinel);
  finder1.Start(base::Seconds(5), base::DoNothing(), base::DoNothing());
  finder1.HandleWinEvent(EVENT_OBJECT_SHOW, sentinel, OBJID_WINDOW);

  TestSettingsWindowFinderWin finder2;
  // Since finder1 delivered its window and stopped, finder2 should be able to
  // start without crashing.
  finder2.Start(base::Seconds(5), base::DoNothing(), base::DoNothing());
}

}  // namespace
