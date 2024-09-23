// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/reload_button.h"

#include "base/run_loop.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_test_views_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_utils.h"
#include "ui/views/test/button_test_api.h"

class ReloadButtonTest : public ChromeRenderViewHostTestHarness {
 public:
  ReloadButtonTest();

  ReloadButtonTest(const ReloadButtonTest&) = delete;
  ReloadButtonTest& operator=(const ReloadButtonTest&) = delete;

  void CheckState(bool enabled,
                  ReloadButton::Mode intended_mode,
                  ReloadButton::Mode visible_mode,
                  bool double_click_timer_running,
                  bool mode_switch_timer_running);

  // These accessors eliminate the need to declare each testcase as a friend.
  void set_mouse_hovered(bool hovered) {
    reload_.testing_mouse_hovered_ = hovered;
  }
  int reload_count() { return reload_.testing_reload_count_; }

 protected:
  ReloadButton* reload() { return &reload_; }

 private:
  ChromeTestViewsDelegate<> views_delegate_;
  ReloadButton reload_;
};

ReloadButtonTest::ReloadButtonTest() : reload_(nullptr) {
  // Set the timer delays to 0 so that timers will fire as soon as we tell the
  // message loop to run pending tasks.
  reload_.double_click_timer_delay_ = base::TimeDelta();
  reload_.mode_switch_timer_delay_ = base::TimeDelta();
}

void ReloadButtonTest::CheckState(bool enabled,
                                  ReloadButton::Mode intended_mode,
                                  ReloadButton::Mode visible_mode,
                                  bool double_click_timer_running,
                                  bool mode_switch_timer_running) {
  EXPECT_EQ(enabled, reload_.GetEnabled());
  EXPECT_EQ(intended_mode, reload_.intended_mode_);
  EXPECT_EQ(visible_mode, reload_.visible_mode_);
  EXPECT_EQ(double_click_timer_running,
            reload_.double_click_timer_.IsRunning());
  EXPECT_EQ(mode_switch_timer_running, reload_.mode_switch_timer_.IsRunning());
}

TEST_F(ReloadButtonTest, Basic) {
  // The stop/reload button starts in the "enabled reload" state with no timers
  // running.
  CheckState(true, ReloadButton::Mode::kReload, ReloadButton::Mode::kReload,
             false, false);

  // Press the button.  This should start the double-click timer.
  ui::MouseEvent e(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                   ui::EventTimeForNow(), 0, 0);
  views::test::ButtonTestApi test_api(reload());
  test_api.NotifyClick(e);
  CheckState(true, ReloadButton::Mode::kReload, ReloadButton::Mode::kReload,
             true, false);

  // Now change the mode (as if the browser had started loading the page).  This
  // should cancel the double-click timer since the button is not hovered.
  reload()->ChangeMode(ReloadButton::Mode::kStop, false);
  CheckState(true, ReloadButton::Mode::kStop, ReloadButton::Mode::kStop, false,
             false);

  // Press the button again.  This should change back to reload.
  test_api.NotifyClick(e);
  CheckState(true, ReloadButton::Mode::kReload, ReloadButton::Mode::kReload,
             false, false);
}

TEST_F(ReloadButtonTest, DoubleClickTimer) {
  // Start by pressing the button.
  ui::MouseEvent e(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                   ui::EventTimeForNow(), 0, 0);
  views::test::ButtonTestApi test_api(reload());
  test_api.NotifyClick(e);

  // Try to press the button again.  This should do nothing because the timer is
  // running.
  int original_reload_count = reload_count();
  test_api.NotifyClick(e);
  CheckState(true, ReloadButton::Mode::kReload, ReloadButton::Mode::kReload,
             true, false);
  EXPECT_EQ(original_reload_count, reload_count());

  // Hover the button, and change mode.  The visible mode should not change,
  // again because the timer is running.
  set_mouse_hovered(true);
  reload()->ChangeMode(ReloadButton::Mode::kStop, false);
  CheckState(true, ReloadButton::Mode::kStop, ReloadButton::Mode::kReload, true,
             false);

  // Now fire the timer.  This should complete the mode change.
  base::RunLoop().RunUntilIdle();
  CheckState(true, ReloadButton::Mode::kStop, ReloadButton::Mode::kStop, false,
             false);
}

TEST_F(ReloadButtonTest, DisableOnHover) {
  // Change to stop and hover.
  ui::MouseEvent e(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                   ui::EventTimeForNow(), 0, 0);
  views::test::ButtonTestApi(reload()).NotifyClick(e);
  reload()->ChangeMode(ReloadButton::Mode::kStop, false);
  set_mouse_hovered(true);

  // Now change back to reload.  This should result in a disabled stop button
  // due to the hover.
  reload()->ChangeMode(ReloadButton::Mode::kReload, false);
  CheckState(false, ReloadButton::Mode::kReload, ReloadButton::Mode::kStop,
             false, true);

  // Un-hover the button, which should allow it to reset.
  set_mouse_hovered(false);
  ui::MouseEvent e2(ui::EventType::kMouseMoved, gfx::Point(), gfx::Point(),
                    ui::EventTimeForNow(), 0, 0);
  reload()->OnMouseExited(e2);
  CheckState(true, ReloadButton::Mode::kReload, ReloadButton::Mode::kReload,
             false, false);
}

TEST_F(ReloadButtonTest, ResetOnClick) {
  // Change to stop and hover.
  ui::MouseEvent e(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                   ui::EventTimeForNow(), 0, 0);
  views::test::ButtonTestApi test_api(reload());
  test_api.NotifyClick(e);
  reload()->ChangeMode(ReloadButton::Mode::kStop, false);
  set_mouse_hovered(true);

  // Press the button.  This should change back to reload despite the hover,
  // because it's a direct user action.
  test_api.NotifyClick(e);
  CheckState(true, ReloadButton::Mode::kReload, ReloadButton::Mode::kReload,
             false, false);
}

TEST_F(ReloadButtonTest, ResetOnTimer) {
  // Change to stop, hover, and change back to reload.
  ui::MouseEvent e(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                   ui::EventTimeForNow(), 0, 0);
  views::test::ButtonTestApi(reload()).NotifyClick(e);
  reload()->ChangeMode(ReloadButton::Mode::kStop, false);
  set_mouse_hovered(true);
  reload()->ChangeMode(ReloadButton::Mode::kReload, false);

  // Now fire the stop-to-reload timer.  This should reset the button.
  base::RunLoop().RunUntilIdle();
  CheckState(true, ReloadButton::Mode::kReload, ReloadButton::Mode::kReload,
             false, false);
}
