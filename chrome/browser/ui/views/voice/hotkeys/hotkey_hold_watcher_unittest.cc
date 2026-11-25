// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/voice/hotkeys/hotkey_hold_watcher.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/types/event_type.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace voice {
namespace {

using ::testing::_;
using Result = HotkeyHoldWatcher::Result;

constexpr base::TimeDelta kHoldThreshold = base::Milliseconds(250);

class HotkeyHoldWatcherTest : public views::ViewsTestBase {
 public:
  HotkeyHoldWatcherTest()
      : views::ViewsTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    views::ViewsTestBase::SetUp();
    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    ON_CALL(browser_, RegisterDidBecomeInactive(_))
        .WillByDefault(
            [this](BrowserWindowInterface::DidBecomeInactiveCallback callback) {
              return browser_inactive_callbacks_.Add(std::move(callback));
            });
  }

  void TearDown() override {
    watcher_.reset();
    widget_.reset();
    views::ViewsTestBase::TearDown();
  }

 protected:
  void StartWatching() {
    // Unretained is safe: `this` owns `watcher_`.
    watcher_ = std::make_unique<HotkeyHoldWatcher>(
        browser_, *widget_, hotkey_, kHoldThreshold,
        base::BindOnce(&HotkeyHoldWatcherTest::OnHoldThresholdReached,
                       base::Unretained(this)),
        base::BindOnce(&HotkeyHoldWatcherTest::OnFinished,
                       base::Unretained(this)));
  }

  void ReachHoldThreshold() {
    task_environment()->FastForwardBy(kHoldThreshold);
  }

  void SendKeyEvent(ui::EventType type, ui::KeyboardCode key_code) {
    ASSERT_TRUE(watcher_);
    const ui::KeyEvent event(type, key_code, hotkey_.modifiers());
    watcher_->OnEvent(event);
  }

  void OnHoldThresholdReached() {
    ++threshold_reached_count_;
    if (destroy_on_threshold_reached_) {
      watcher_.reset();
    }
  }

  void OnFinished(Result result) {
    result_ = result;
    // Like the watcher's owners, destroy it from its completion callback.
    watcher_.reset();
  }

  const ui::Accelerator hotkey_{ui::VKEY_D,
                                ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN};
  testing::NiceMock<MockBrowserWindowInterface> browser_;
  base::RepeatingCallbackList<void(BrowserWindowInterface*)>
      browser_inactive_callbacks_;
  std::unique_ptr<views::Widget> widget_;
  // Declared after the callback lists, which its subscriptions refer to.
  std::unique_ptr<HotkeyHoldWatcher> watcher_;
  int threshold_reached_count_ = 0;
  bool destroy_on_threshold_reached_ = false;
  std::optional<Result> result_;
};

TEST_F(HotkeyHoldWatcherTest, UsesHoldThreshold) {
  StartWatching();
  EXPECT_EQ(watcher_->hold_threshold_for_testing(), kHoldThreshold);
  EXPECT_FALSE(watcher_->is_press_and_hold_mode());
}

TEST_F(HotkeyHoldWatcherTest, HoldPastThresholdEntersPressAndHoldMode) {
  StartWatching();
  ReachHoldThreshold();
  EXPECT_TRUE(watcher_->is_press_and_hold_mode());
}

TEST_F(HotkeyHoldWatcherTest, ThresholdCallbackRunsOnceAtThreshold) {
  StartWatching();
  task_environment()->FastForwardBy(kHoldThreshold - base::Milliseconds(1));
  EXPECT_EQ(threshold_reached_count_, 0);
  task_environment()->FastForwardBy(base::Milliseconds(1));
  EXPECT_EQ(threshold_reached_count_, 1);
  task_environment()->FastForwardBy(kHoldThreshold);
  EXPECT_EQ(threshold_reached_count_, 1);
}

TEST_F(HotkeyHoldWatcherTest, TapDoesNotRunThresholdCallback) {
  StartWatching();
  SendKeyEvent(ui::EventType::kKeyReleased, ui::VKEY_D);
  task_environment()->FastForwardBy(kHoldThreshold);
  EXPECT_EQ(threshold_reached_count_, 0);
}

TEST_F(HotkeyHoldWatcherTest, ThresholdCallbackMayDestroyWatcher) {
  destroy_on_threshold_reached_ = true;
  StartWatching();
  ReachHoldThreshold();
  EXPECT_FALSE(watcher_);
  EXPECT_FALSE(result_);
}

TEST_F(HotkeyHoldWatcherTest, ReleaseBeforeThresholdIsTapped) {
  StartWatching();
  SendKeyEvent(ui::EventType::kKeyReleased, ui::VKEY_D);
  EXPECT_EQ(result_, Result::kTapped);
  EXPECT_FALSE(watcher_);
}

TEST_F(HotkeyHoldWatcherTest, ReleaseAfterThresholdIsReleased) {
  StartWatching();
  ReachHoldThreshold();
  SendKeyEvent(ui::EventType::kKeyReleased, ui::VKEY_D);
  EXPECT_EQ(result_, Result::kReleased);
  EXPECT_FALSE(watcher_);
}

TEST_F(HotkeyHoldWatcherTest, ModifierReleaseAfterThresholdIsReleased) {
  StartWatching();
  ReachHoldThreshold();
  SendKeyEvent(ui::EventType::kKeyReleased, ui::VKEY_SHIFT);
  EXPECT_EQ(result_, Result::kReleased);
}

TEST_F(HotkeyHoldWatcherTest, NonChordKeyReleaseIsIgnored) {
  StartWatching();
  ReachHoldThreshold();
  SendKeyEvent(ui::EventType::kKeyReleased, ui::VKEY_A);
  EXPECT_FALSE(result_);
  EXPECT_TRUE(watcher_);
}

TEST_F(HotkeyHoldWatcherTest, ChordKeyRepeatsAreIgnored) {
  StartWatching();
  ReachHoldThreshold();
  SendKeyEvent(ui::EventType::kKeyPressed, ui::VKEY_D);
  SendKeyEvent(ui::EventType::kKeyPressed, ui::VKEY_MENU);
  EXPECT_FALSE(result_);
  EXPECT_TRUE(watcher_);
}

TEST_F(HotkeyHoldWatcherTest, ProcessKeyIsIgnored) {
  StartWatching();
  ReachHoldThreshold();
  SendKeyEvent(ui::EventType::kKeyPressed, ui::VKEY_PROCESSKEY);
  EXPECT_FALSE(result_);
  EXPECT_TRUE(watcher_);
}

TEST_F(HotkeyHoldWatcherTest, SecondaryKeyBeforeThresholdIsTapped) {
  StartWatching();
  SendKeyEvent(ui::EventType::kKeyPressed, ui::VKEY_A);
  EXPECT_EQ(result_, Result::kTapped);
}

TEST_F(HotkeyHoldWatcherTest, SecondaryKeyAfterThresholdIsAborted) {
  StartWatching();
  ReachHoldThreshold();
  SendKeyEvent(ui::EventType::kKeyPressed, ui::VKEY_A);
  EXPECT_EQ(result_, Result::kAborted);
}

TEST_F(HotkeyHoldWatcherTest, BrowserInactiveAfterThresholdIsAborted) {
  StartWatching();
  ReachHoldThreshold();
  browser_inactive_callbacks_.Notify(&browser_);
  EXPECT_EQ(result_, Result::kAborted);
}

TEST_F(HotkeyHoldWatcherTest, WidgetDestructionAfterThresholdIsAborted) {
  StartWatching();
  ReachHoldThreshold();
  widget_.reset();
  EXPECT_EQ(result_, Result::kAborted);
}

TEST_F(HotkeyHoldWatcherTest, DestructionDoesNotRunCallback) {
  StartWatching();
  ReachHoldThreshold();
  watcher_.reset();
  EXPECT_FALSE(result_);
}

TEST(HotkeyHoldWatcherIsChordKeyTest, MatchesMainKeyAndModifiers) {
  const ui::Accelerator hold_accelerator(ui::VKEY_SPACE,
                                         ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN);
  EXPECT_TRUE(HotkeyHoldWatcher::IsChordKey(hold_accelerator, ui::VKEY_SPACE));
  EXPECT_TRUE(HotkeyHoldWatcher::IsChordKey(hold_accelerator, ui::VKEY_MENU));
  EXPECT_TRUE(HotkeyHoldWatcher::IsChordKey(hold_accelerator, ui::VKEY_LMENU));
  EXPECT_TRUE(HotkeyHoldWatcher::IsChordKey(hold_accelerator, ui::VKEY_RMENU));
  EXPECT_TRUE(HotkeyHoldWatcher::IsChordKey(hold_accelerator, ui::VKEY_SHIFT));
  EXPECT_TRUE(HotkeyHoldWatcher::IsChordKey(hold_accelerator, ui::VKEY_LSHIFT));
  EXPECT_TRUE(HotkeyHoldWatcher::IsChordKey(hold_accelerator, ui::VKEY_RSHIFT));

  EXPECT_FALSE(
      HotkeyHoldWatcher::IsChordKey(hold_accelerator, ui::VKEY_CONTROL));
  EXPECT_FALSE(HotkeyHoldWatcher::IsChordKey(hold_accelerator, ui::VKEY_A));
}

}  // namespace
}  // namespace voice
