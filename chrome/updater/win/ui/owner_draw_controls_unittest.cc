// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/ui/owner_draw_controls.h"

#include <windows.h>

#include <memory>
#include <string>
#include <vector>

#include "chrome/updater/win/ui/message_loop.h"
#include "chrome/updater/win/ui/owner_draw_controls_test_api.h"
#include "chrome/updater/win/ui/progress_wnd.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace updater::ui {

namespace {

// Thin wrappers over the test API, so the expectations below read as if they
// were calling the control directly.
bool IsMouseHovering(const CaptionButton& button) {
  return test::CaptionButtonTestApi(button).is_mouse_hovering();
}
bool IsTrackingMouseEvents(const CaptionButton& button) {
  return test::CaptionButtonTestApi(button).is_tracking_mouse_events();
}

// The caption button behavior under test never reaches the event sink, so the
// host dialog is given an inert one rather than a mock.
class NoOpProgressWndEvents : public ProgressWndEvents {
 public:
  void DoClose() override {}
  void DoExit() override {}
  bool DoLaunchBrowser(const std::string& url) override { return true; }
  bool DoRestartBrowser(bool restart_all_browsers,
                        const std::vector<GURL>& urls) override {
    return true;
  }
  bool DoReboot() override { return true; }
  void DoCancel() override {}
};

// Exercises the owner-drawn caption buttons. `ProgressWnd` is only the host:
// it is the sole production dialog that installs an `OwnerDrawTitleBar`, and
// the controls need a real parent window to be created.
class CaptionButtonTest : public ::testing::Test {
 protected:
  // Destroyed here rather than in each test body, so an early return from a
  // failed ASSERT_* cannot leak a top-level window.
  void TearDown() override {
    title_bar_.reset();
    if (progress_wnd_) {
      progress_wnd_->DestroyWindow();
      progress_wnd_.reset();
    }
  }

  void CreateTestDialog() {
    progress_wnd_ = std::make_unique<ProgressWnd>(&message_loop_, nullptr);
    progress_wnd_->SetEventSink(&events_);
    // `Initialize()` calls `Create()`, which realizes the dialog and its
    // children. `Show()` is deliberately skipped: a visible, foreground window
    // would let a physical cursor deliver mouse messages that perturb the
    // hover expectations.
    ASSERT_HRESULT_SUCCEEDED(progress_wnd_->Initialize());

    // Belt and braces, in case something makes the window visible: keep it
    // where no real cursor can be.
    ASSERT_TRUE(::SetWindowPos(progress_wnd_->hwnd(), nullptr, -10000, -10000,
                               0, 0,
                               SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE));

    title_bar_ =
        std::make_unique<test::OwnerDrawTitleBarTestApi>(*progress_wnd_);
    ASSERT_TRUE(title_bar_->title_bar_window().IsWindow());
    ASSERT_TRUE(close_button().IsWindow());
    ASSERT_TRUE(minimize_button().IsWindow());
  }

  CloseButton& close_button() { return title_bar_->close_button(); }
  MinimizeButton& minimize_button() { return title_bar_->minimize_button(); }

  MessageLoop message_loop_;
  // Declared before the dialog so that it outlives the sink pointer.
  NoOpProgressWndEvents events_;
  std::unique_ptr<ProgressWnd> progress_wnd_;
  std::unique_ptr<test::OwnerDrawTitleBarTestApi> title_bar_;
};

TEST_F(CaptionButtonTest, HoverIgnoresPointsOutsideTheClientRect) {
  ASSERT_NO_FATAL_FAILURE(CreateTestDialog());

  // Nothing pumps between the sends and the expectations, and the window is
  // parked off screen, so no WM_MOUSELEAVE can slip in.
  //
  // Both caption buttons share the state machine, so both are exercised, even
  // though only the close button is disabled in production.
  CaptionButton* const buttons[] = {&close_button(), &minimize_button()};
  for (CaptionButton* button : buttons) {
    // A move over the control highlights it and arms tracking so that the
    // highlight can be cleared again.
    ::SendMessage(button->hwnd(), WM_MOUSEMOVE, 0, 0);
    EXPECT_TRUE(IsMouseHovering(*button));
    EXPECT_TRUE(IsTrackingMouseEvents(*button));

    ::SendMessage(button->hwnd(), WM_MOUSELEAVE, 0, 0);
    EXPECT_FALSE(IsMouseHovering(*button));
    EXPECT_FALSE(IsTrackingMouseEvents(*button));

    // `BUTTON` captures the mouse while pressed, so WM_MOUSEMOVE for points
    // outside the client rect is still delivered to the control. Such moves
    // must not apply the hover highlight.
    ::SendMessage(button->hwnd(), WM_MOUSEMOVE, 0, MAKELPARAM(-1, -1));
    EXPECT_FALSE(IsMouseHovering(*button));
    EXPECT_FALSE(IsTrackingMouseEvents(*button));

    // ::PtInRect is inclusive on left/top and exclusive on right/bottom, and
    // a real drag exits across an edge, so pin both sides of that boundary.
    RECT client_rect = {};
    ASSERT_TRUE(::GetClientRect(button->hwnd(), &client_rect));
    const int width = client_rect.right - client_rect.left;
    const int height = client_rect.bottom - client_rect.top;
    ASSERT_GT(width, 0);
    ASSERT_GT(height, 0);

    ::SendMessage(button->hwnd(), WM_MOUSEMOVE, 0,
                  MAKELPARAM(width - 1, height - 1));
    EXPECT_TRUE(IsMouseHovering(*button)) << "the last inside pixel";
    ::SendMessage(button->hwnd(), WM_MOUSEMOVE, 0, MAKELPARAM(width, height));
    EXPECT_FALSE(IsMouseHovering(*button)) << "right/bottom are exclusive";

    ::SendMessage(button->hwnd(), WM_MOUSELEAVE, 0, 0);

    // Dragging back inside restores the hover highlight, and dragging back out
    // clears it again.
    ::SendMessage(button->hwnd(), WM_MOUSEMOVE, 0, 0);
    EXPECT_TRUE(IsMouseHovering(*button));
    ::SendMessage(button->hwnd(), WM_MOUSEMOVE, 0, MAKELPARAM(-1, -1));
    EXPECT_FALSE(IsMouseHovering(*button));

    ::SendMessage(button->hwnd(), WM_MOUSELEAVE, 0, 0);
    EXPECT_FALSE(IsMouseHovering(*button));
    EXPECT_FALSE(IsTrackingMouseEvents(*button));
  }
}

TEST_F(CaptionButtonTest, DisablingClearsHoverAndCancelsTracking) {
  ASSERT_NO_FATAL_FAILURE(CreateTestDialog());

  CaptionButton* const buttons[] = {&close_button(), &minimize_button()};
  for (CaptionButton* button : buttons) {
    // A disabled control does not enter the hover highlight state.
    ::EnableWindow(button->hwnd(), FALSE);
    ::SendMessage(button->hwnd(), WM_MOUSEMOVE, 0, 0);
    EXPECT_FALSE(IsMouseHovering(*button));
    EXPECT_FALSE(IsTrackingMouseEvents(*button));

    // Re-enabling allows the hover state again, and arms mouse tracking so
    // that WM_MOUSELEAVE can clear the highlight.
    ::EnableWindow(button->hwnd(), TRUE);
    ::SendMessage(button->hwnd(), WM_MOUSEMOVE, 0, 0);
    EXPECT_TRUE(IsMouseHovering(*button));
    EXPECT_TRUE(IsTrackingMouseEvents(*button));

    // Disabling while hovered clears the highlight via WM_ENABLE and cancels
    // tracking, since a disabled window never receives WM_MOUSELEAVE.
    ::EnableWindow(button->hwnd(), FALSE);
    EXPECT_FALSE(IsMouseHovering(*button));
    EXPECT_FALSE(IsTrackingMouseEvents(*button));

    // As with a stock `BUTTON`, the pointer has to move again.
    ::EnableWindow(button->hwnd(), TRUE);
    EXPECT_FALSE(IsMouseHovering(*button));
    EXPECT_FALSE(IsTrackingMouseEvents(*button));

    ::SendMessage(button->hwnd(), WM_MOUSEMOVE, 0, 0);
    EXPECT_TRUE(IsMouseHovering(*button));
    EXPECT_TRUE(IsTrackingMouseEvents(*button));

    ::SendMessage(button->hwnd(), WM_MOUSELEAVE, 0, 0);
    EXPECT_FALSE(IsMouseHovering(*button));
    EXPECT_FALSE(IsTrackingMouseEvents(*button));
  }
}

TEST_F(CaptionButtonTest, HidingClearsHoverAndCancelsTracking) {
  ASSERT_NO_FATAL_FAILURE(CreateTestDialog());

  CaptionButton* const buttons[] = {&close_button(), &minimize_button()};
  for (CaptionButton* button : buttons) {
    ::SendMessage(button->hwnd(), WM_MOUSEMOVE, 0, 0);
    EXPECT_TRUE(IsMouseHovering(*button));
    EXPECT_TRUE(IsTrackingMouseEvents(*button));

    // Hiding while hovered clears the highlight via WM_SHOWWINDOW and cancels
    // tracking, since a hidden window never receives WM_MOUSELEAVE.
    ::ShowWindow(button->hwnd(), SW_HIDE);
    EXPECT_FALSE(IsMouseHovering(*button));
    EXPECT_FALSE(IsTrackingMouseEvents(*button));

    // Re-showing does not automatically restore hover; the pointer must move
    // again.
    ::ShowWindow(button->hwnd(), SW_SHOW);
    EXPECT_FALSE(IsMouseHovering(*button));
    EXPECT_FALSE(IsTrackingMouseEvents(*button));

    ::SendMessage(button->hwnd(), WM_MOUSEMOVE, 0, 0);
    EXPECT_TRUE(IsMouseHovering(*button));
    EXPECT_TRUE(IsTrackingMouseEvents(*button));

    ::SendMessage(button->hwnd(), WM_MOUSELEAVE, 0, 0);
    EXPECT_FALSE(IsMouseHovering(*button));
    EXPECT_FALSE(IsTrackingMouseEvents(*button));
  }
}

}  // namespace
}  // namespace updater::ui
