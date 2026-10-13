// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/graphics/cast_window_manager_aura.h"

#include <memory>

#include "chromecast/graphics/cast_window_tree_host_aura.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/display/display_transform.h"
#include "ui/display/screen.h"
#include "ui/display/screen_base.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/transform.h"
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
      std::make_unique<CastWindowManagerAura>();

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

void VerifyWindowOrder(aura::Window* root_window) {
  for (size_t i = 0; i < root_window->children().size() - 1; ++i)
    EXPECT_LE(root_window->children()[i]->GetId(),
              root_window->children()[i + 1]->GetId());
}

TEST_F(CastWindowManagerAuraTest, CheckProperWindowOrdering) {
  std::unique_ptr<CastWindowManagerAura> window_manager =
      std::make_unique<CastWindowManagerAura>();

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

TEST_F(CastWindowManagerAuraTest, RotationSupportedGatesTransform) {
  std::unique_ptr<CastWindowManagerAura> window_manager =
      std::make_unique<CastWindowManagerAura>();
  aura::Window* root_window = window_manager->GetRootWindow();
  ASSERT_TRUE(root_window);
  ASSERT_TRUE(window_manager->window_tree_host());

  // Construct a 90-degree rotation transform.
  gfx::Transform rotate_90;
  rotate_90.RotateAboutZAxis(90.0);

  // By default, rotation_supported_ is false, so OnTransformChanged keeps the
  // root transform as identity.
  window_manager->window_tree_host()->OnTransformChanged(rotate_90);
  EXPECT_TRUE(root_window->transform().IsIdentity());

  // Enabling rotation_supported applies the cached transform.
  window_manager->SetRotationSupported(true);
  EXPECT_FALSE(root_window->transform().IsIdentity());

  // Disabling rotation_supported resets the root transform to identity.
  window_manager->SetRotationSupported(false);
  EXPECT_TRUE(root_window->transform().IsIdentity());
}

TEST_F(CastWindowManagerAuraTest, InitialRotationGatedOnCapability) {
  display::Screen* screen = display::Screen::Get();
  ASSERT_TRUE(screen);
  display::Display primary = screen->GetPrimaryDisplay();
  primary.set_rotation(display::Display::ROTATE_90);
  static_cast<display::ScreenBase*>(screen)->display_list().UpdateDisplay(
      primary, display::DisplayList::Type::PRIMARY);

  std::unique_ptr<CastWindowManagerAura> window_manager =
      std::make_unique<CastWindowManagerAura>();
  window_manager->SetRotationSupported(false);
  aura::Window* root_window = window_manager->GetRootWindow();
  ASSERT_TRUE(root_window);
  EXPECT_TRUE(root_window->transform().IsIdentity());

  // Enabling rotation_supported applies the initial primary display rotation
  // transform without requiring a Starboard UiEventSource event.
  window_manager->SetRotationSupported(true);
  EXPECT_FALSE(root_window->transform().IsIdentity());

  // Disabling rotation_supported resets the root transform back to identity.
  window_manager->SetRotationSupported(false);
  EXPECT_TRUE(root_window->transform().IsIdentity());
}

TEST_F(CastWindowManagerAuraTest, RotationDirectionMatchesDisplayTransform) {
  display::Screen* screen = display::Screen::Get();
  ASSERT_TRUE(screen);

  std::unique_ptr<CastWindowManagerAura> window_manager =
      std::make_unique<CastWindowManagerAura>();
  window_manager->SetRotationSupported(true);
  aura::Window* root_window = window_manager->GetRootWindow();
  ASSERT_TRUE(root_window);
  ASSERT_TRUE(window_manager->window_tree_host());

  // 90-degree clockwise transform: X' = H - Y, Y' = X (rc(0, 1) = -1,
  // rc(1, 0) = 1). Matches display::CreateRotationTransform(ROTATE_90, ...).
  gfx::Transform rotate_90 = display::CreateRotationTransform(
      display::Display::ROTATE_90, gfx::SizeF(1080.0f, 1920.0f));
  ASSERT_LT(rotate_90.rc(0, 1), 0.0f);
  ASSERT_GT(rotate_90.rc(1, 0), 0.0f);
  window_manager->window_tree_host()->OnTransformChanged(rotate_90);
  EXPECT_EQ(screen->GetPrimaryDisplay().rotation(),
            display::Display::ROTATE_90);
  EXPECT_LT(root_window->transform().rc(0, 1), 0.0f);
  EXPECT_GT(root_window->transform().rc(1, 0), 0.0f);

  // 270-degree clockwise transform: X' = Y, Y' = W - X (rc(0, 1) = 1,
  // rc(1, 0) = -1). Matches display::CreateRotationTransform(ROTATE_270, ...).
  gfx::Transform rotate_270 = display::CreateRotationTransform(
      display::Display::ROTATE_270, gfx::SizeF(1080.0f, 1920.0f));
  ASSERT_GT(rotate_270.rc(0, 1), 0.0f);
  ASSERT_LT(rotate_270.rc(1, 0), 0.0f);
  window_manager->window_tree_host()->OnTransformChanged(rotate_270);
  EXPECT_EQ(screen->GetPrimaryDisplay().rotation(),
            display::Display::ROTATE_270);
  EXPECT_GT(root_window->transform().rc(0, 1), 0.0f);
  EXPECT_LT(root_window->transform().rc(1, 0), 0.0f);

  // 180-degree transform: rc(0, 0) = -1, rc(1, 1) = -1.
  gfx::Transform rotate_180 = display::CreateRotationTransform(
      display::Display::ROTATE_180, gfx::SizeF(1920.0f, 1080.0f));
  window_manager->window_tree_host()->OnTransformChanged(rotate_180);
  EXPECT_EQ(screen->GetPrimaryDisplay().rotation(),
            display::Display::ROTATE_180);
}

TEST_F(CastWindowManagerAuraTest,
       ChildWindowResizesWithRootWindowExceptWhenHidden) {
  std::unique_ptr<CastWindowManagerAura> window_manager =
      std::make_unique<CastWindowManagerAura>();
  window_manager->SetRotationSupported(true);
  aura::Window* root_window = window_manager->GetRootWindow();
  ASSERT_TRUE(root_window);
  ASSERT_TRUE(window_manager->window_tree_host());

  window_manager->window_tree_host()->OnWindowSizeChanged(
      gfx::Size(1920, 1080));
  EXPECT_EQ(root_window->bounds().size(), gfx::Size(1920, 1080));

  CastTestWindowDelegate visible_delegate;
  aura::Window visible_window(&visible_delegate);
  visible_window.Init(ui::LAYER_NOT_DRAWN);
  window_manager->AddWindow(&visible_window);
  EXPECT_EQ(visible_window.bounds().size(), gfx::Size(1920, 1080));

  CastTestWindowDelegate hidden_delegate;
  aura::Window hidden_window(&hidden_delegate);
  hidden_window.Init(ui::LAYER_NOT_DRAWN);
  hidden_window.SetBounds(gfx::Rect(1, 1));
  window_manager->AddWindow(&hidden_window);
  EXPECT_EQ(hidden_window.bounds().size(), gfx::Size(1, 1));

  // Rotate to portrait (90 degrees): visible child resizes to 1080x1920 while
  // the 1x1 hidden window preserves its 1x1 bounds.
  gfx::Transform rotate_90 = display::CreateRotationTransform(
      display::Display::ROTATE_90, gfx::SizeF(1080.0f, 1920.0f));
  window_manager->window_tree_host()->OnTransformChanged(rotate_90);
  EXPECT_EQ(root_window->bounds().size(), gfx::Size(1080, 1920));
  EXPECT_EQ(visible_window.bounds().size(), gfx::Size(1080, 1920));
  EXPECT_EQ(hidden_window.bounds().size(), gfx::Size(1, 1));
}

}  // namespace test
}  // namespace chromecast
