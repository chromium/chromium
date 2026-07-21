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

TEST_F(SettingsWindowFinderWinTest, OnlyOneActiveInstanceAllowed) {
  TestSettingsWindowFinderWin finder1;

  finder1.Start(base::Seconds(5), base::DoNothing(), base::DoNothing());

  TestSettingsWindowFinderWin finder2;
  // Because `finder1` is active and using the global WinEvent hook instance
  // slot, `finder2` should CHECK fail if it tries to start.
  EXPECT_DEATH_IF_SUPPORTED(
      finder2.Start(base::Seconds(5), base::DoNothing(), base::DoNothing()),
      "");
}

TEST_F(SettingsWindowFinderWinTest,
       OnlyOneActiveInstanceAllowedWithLocationObserver) {
  TestSettingsWindowFinderWin finder1;

  finder1.StartObservingLocationChanges(reinterpret_cast<HWND>(0x12345),
                                        base::DoNothing());

  TestSettingsWindowFinderWin finder2;
  // Because `finder1` is active and using the global WinEvent hook instance
  // slot, `finder2` should CHECK fail if it tries to start.
  EXPECT_DEATH_IF_SUPPORTED(
      finder2.Start(base::Seconds(5), base::DoNothing(), base::DoNothing()),
      "");
}

TEST_F(SettingsWindowFinderWinTest, StopObservingReleasesGlobalInstance) {
  TestSettingsWindowFinderWin finder1;
  finder1.StartObservingLocationChanges(reinterpret_cast<HWND>(0x12345),
                                        base::DoNothing());
  finder1.StopObservingLocationChanges();

  TestSettingsWindowFinderWin finder2;
  // Since finder1 stopped observing, finder2 should be able to start without
  // crashing.
  finder2.Start(base::Seconds(5), base::DoNothing(), base::DoNothing());
}

}  // namespace
