// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/graphics/cast_window_manager_aura.h"

#include <memory>

#include "ui/aura/client/focus_client.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/test/views_test_base.h"

namespace chromecast {
namespace test {
namespace {

class CastTestWindowDelegate : public aura::test::TestWindowDelegate {
 public:
  CastTestWindowDelegate() : key_code_(ui::VKEY_UNKNOWN) {}

  CastTestWindowDelegate(const CastTestWindowDelegate&) = delete;
  CastTestWindowDelegate& operator=(const CastTestWindowDelegate&) = delete;

  ~CastTestWindowDelegate() override {}

  // Overridden from TestWindowDelegate:
  void OnKeyEvent(ui::KeyEvent* event) override {
    key_code_ = event->key_code();
  }

  ui::KeyboardCode key_code() { return key_code_; }

 private:
  ui::KeyboardCode key_code_;
};

class TestWindow {
 public:
  explicit TestWindow(int id) : window_(&delegate_) {
    window_.Init(ui::LAYER_NOT_DRAWN);
    window_.SetId(id);
    window_.SetBounds(gfx::Rect(0, 0, 1280, 720));
  }

  TestWindow(const TestWindow&) = delete;
  TestWindow& operator=(const TestWindow&) = delete;

  aura::Window* window() { return &window_; }

 private:
  CastTestWindowDelegate delegate_;
  aura::Window window_;
};

}  // namespace

// ViewsTestBase needed so that views/widget initialization is setup correctly
// for test runs.
using CastWindowManagerAuraTest = views::ViewsTestBase;

TEST_F(CastWindowManagerAuraTest, InitialWindowId) {
  CastTestWindowDelegate window_delegate;
  aura::Window window(&window_delegate);
  window.Init(ui::LAYER_NOT_DRAWN);

  // We have chosen WindowId::BOTTOM to match the initial window ID of an Aura
  // window so that z-ordering works correctly.
  EXPECT_EQ(window.GetId(), CastWindowManager::WindowId::BOTTOM);
}

TEST_F(CastWindowManagerAuraTest, WindowInput) {
  std::unique_ptr<CastWindowManagerAura> window_manager =
      std::make_unique<CastWindowManagerAura>(true /* enable input */);

  CastTestWindowDelegate window_delegate;
  aura::Window window(&window_delegate);
  window.Init(ui::LAYER_NOT_DRAWN);
  window.SetName("event window");
  window_manager->AddWindow(&window);
  window.SetBounds(gfx::Rect(0, 0, 1280, 720));
  window.Show();
  EXPECT_FALSE(window.IsRootWindow());
  EXPECT_TRUE(window.GetHost());

  // Confirm that the Aura focus client tracks window focus correctly.
  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(&window);
  EXPECT_TRUE(focus_client);
  EXPECT_FALSE(focus_client->GetFocusedWindow());
  window.Focus();
  EXPECT_EQ(&window, focus_client->GetFocusedWindow());

  // Confirm that a keyboard event is delivered to the window.
  ui::test::EventGenerator event_generator(window.GetRootWindow());
  event_generator.PressKey(ui::VKEY_0, ui::EF_NONE);
  EXPECT_EQ(ui::VKEY_0, window_delegate.key_code());
}

TEST_F(CastWindowManagerAuraTest, WindowInputDisabled) {
  std::unique_ptr<CastWindowManagerAura> window_manager =
      std::make_unique<CastWindowManagerAura>(false /* enable input */);

  CastTestWindowDelegate window_delegate;
  aura::Window window(&window_delegate);
  window.Init(ui::LAYER_NOT_DRAWN);
  window.SetName("event window");
  window_manager->AddWindow(&window);
  window.SetBounds(gfx::Rect(0, 0, 1280, 720));
  window.Show();
  EXPECT_FALSE(window.IsRootWindow());
  EXPECT_TRUE(window.GetHost());

  // Confirm that the Aura focus client tracks window focus correctly.
  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(&window);
  EXPECT_TRUE(focus_client);
  EXPECT_FALSE(focus_client->GetFocusedWindow());
  window.Focus();
  EXPECT_EQ(&window, focus_client->GetFocusedWindow());

  // Confirm that a key event is *not* delivered to the window when input is
  // disabled.
  ui::test::EventGenerator event_generator(window.GetRootWindow());
  event_generator.PressKey(ui::VKEY_0, ui::EF_NONE);
  EXPECT_EQ(ui::VKEY_UNKNOWN, window_delegate.key_code());
}

void VerifyWindowOrder(aura::Window* root_window) {
  for (size_t i = 0; i < root_window->children().size() - 1; ++i)
    EXPECT_LE(root_window->children()[i]->GetId(),
              root_window->children()[i + 1]->GetId());
}

TEST_F(CastWindowManagerAuraTest, CheckProperWindowOrdering) {
  std::unique_ptr<CastWindowManagerAura> window_manager =
      std::make_unique<CastWindowManagerAura>(false /* enable input */);

  TestWindow window1(1);
  TestWindow window3(3);
  window_manager->AddWindow(window1.window());
  window_manager->AddWindow(window3.window());
  window1.window()->Show();
  window3.window()->Show();
  // Verify update for top window.
  VerifyWindowOrder(window_manager->GetRootWindow());

  TestWindow window0(0);
  window_manager->AddWindow(window0.window());
  window0.window()->Show();
  // Verify update for bottom window.
  VerifyWindowOrder(window_manager->GetRootWindow());

  TestWindow window2(2);
  window_manager->AddWindow(window2.window());
  window2.window()->Show();
  // Verify update for middle window.
  VerifyWindowOrder(window_manager->GetRootWindow());

  TestWindow window4(4);
  TestWindow window5(5);
  TestWindow window6(6);
  window_manager->AddWindow(window6.window());
  window_manager->AddWindow(window4.window());
  window_manager->AddWindow(window5.window());
  window5.window()->Show();
  // Verify update with hidden windows.
  VerifyWindowOrder(window_manager->GetRootWindow());

  TestWindow window7(2);
  window_manager->AddWindow(window7.window());
  window7.window()->Show();
  // Verify update with same window ID.
  VerifyWindowOrder(window_manager->GetRootWindow());
  EXPECT_EQ(window7.window(), window_manager->GetRootWindow()->children()[3]);

  window2.window()->Hide();
  window2.window()->Show();
  // Verify update ordering with same window ID.
  EXPECT_EQ(window2.window(), window_manager->GetRootWindow()->children()[3]);
}

}  // namespace test
}  // namespace chromecast
