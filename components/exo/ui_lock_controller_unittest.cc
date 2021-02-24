// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/ui_lock_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/wm/window_state.h"
#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ui/base/window_properties.h"
#include "components/exo/buffer.h"
#include "components/exo/display.h"
#include "components/exo/shell_surface.h"
#include "components/exo/surface.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/test/exo_test_helper.h"
#include "components/exo/ui_lock_bubble.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/wm/core/window_util.h"

namespace exo {

namespace {

struct SurfaceTriplet {
  std::unique_ptr<Surface> surface;
  std::unique_ptr<ShellSurface> shell_surface;
  std::unique_ptr<Buffer> buffer;

  aura::Window* GetAlwaysOnTopContainer() {
    aura::Window* native_window = GetTopLevelWidget()->GetNativeWindow();
    return ash::Shell::GetContainer(native_window->GetRootWindow(),
                                    ash::kShellWindowId_AlwaysOnTopContainer);
  }

  views::Widget* GetTopLevelWidget() {
    views::Widget* top_level_widget =
        views::Widget::GetTopLevelWidgetForNativeView(surface->window());
    assert(top_level_widget);
    return top_level_widget;
  }

  aura::Window* GetTopLevelWindow() {
    auto* top_level_widget = views::Widget::GetTopLevelWidgetForNativeView(
        shell_surface->host_window());
    assert(top_level_widget);
    return top_level_widget->GetNativeWindow();
  }

  ash::WindowState* GetTopLevelWindowState() {
    return ash::WindowState::Get(GetTopLevelWindow());
  }
};

class UILockControllerTest : public test::ExoTestBase {
 public:
  UILockControllerTest()
      : test::ExoTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~UILockControllerTest() override = default;

  UILockControllerTest(const UILockControllerTest&) = delete;
  UILockControllerTest& operator=(const UILockControllerTest&) = delete;

 protected:
  // test::ExoTestBase:
  void SetUp() override {
    test::ExoTestBase::SetUp();
    seat_ = std::make_unique<Seat>();
  }

  void TearDown() override {
    seat_.reset();
    test::ExoTestBase::TearDown();
  }

  SurfaceTriplet BuildSurface(int w, int h) {
    auto surface = std::make_unique<Surface>();
    auto shell_surface = std::make_unique<ShellSurface>(
        surface.get(), gfx::Point{0, 0},
        /*can_minimize=*/true, ash::desks_util::GetActiveDeskContainerId());
    auto buffer = std::make_unique<Buffer>(
        exo_test_helper()->CreateGpuMemoryBuffer({w, h}));
    surface->Attach(buffer.get());

    return {std::move(surface), std::move(shell_surface), std::move(buffer)};
  }

  std::unique_ptr<Seat> seat_;
};

class UILockControllerWithUIBubbleTest : public UILockControllerTest {
 public:
  UILockControllerWithUIBubbleTest() : UILockControllerTest() {}
  ~UILockControllerWithUIBubbleTest() override = default;

  UILockControllerWithUIBubbleTest(const UILockControllerWithUIBubbleTest&) =
      delete;
  UILockControllerWithUIBubbleTest& operator=(
      const UILockControllerWithUIBubbleTest&) = delete;

 protected:
  void SetUp() override {
    test::ExoTestBase::SetUp();
    seat_ = std::make_unique<Seat>();
    scoped_feature_list_.InitAndEnableFeature(
        chromeos::features::kExoLockNotification);
  }

  void TearDown() override {
    seat_.reset();
    test::ExoTestBase::TearDown();
  }

  bool BubbleShowingOnTop(aura::Window* always_on_top_container) {
    if (!always_on_top_container)
      return false;

    views::Widget* bubble_widget =
        seat_->GetUILockControllerForTesting()->GetBubbleForTesting();

    if (!bubble_widget)
      return false;

    for (auto* window : always_on_top_container->children()) {
      if (window == bubble_widget->GetNativeWindow() && window->IsVisible()) {
        return true;
      }
    }

    return false;
  }

  std::unique_ptr<Seat> seat_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(UILockControllerTest, HoldingEscapeExitsFullscreen) {
  SurfaceTriplet test_surface = BuildSurface(1024, 768);
  test_surface.shell_surface->SetUseImmersiveForFullscreen(false);
  test_surface.shell_surface->SetFullscreen(true);
  test_surface.surface->Commit();
  test_surface.GetTopLevelWindow()->SetProperty(
      chromeos::kEscHoldToExitFullscreen, true);
  auto* window_state = test_surface.GetTopLevelWindowState();
  EXPECT_TRUE(window_state->IsFullscreen());

  GetEventGenerator()->PressKey(ui::VKEY_ESCAPE, ui::EF_NONE);
  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(1));
  EXPECT_TRUE(window_state->IsFullscreen());  // no change yet

  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(1));
  EXPECT_FALSE(window_state->IsFullscreen());
  EXPECT_TRUE(window_state->IsNormalStateType());
}

TEST_F(UILockControllerTest, HoldingCtrlEscapeDoesNotExitFullscreen) {
  SurfaceTriplet test_surface = BuildSurface(1024, 768);
  test_surface.shell_surface->SetUseImmersiveForFullscreen(false);
  test_surface.shell_surface->SetFullscreen(true);
  test_surface.surface->Commit();
  test_surface.GetTopLevelWindow()->SetProperty(
      chromeos::kEscHoldToExitFullscreen, true);
  auto* window_state = test_surface.GetTopLevelWindowState();
  EXPECT_TRUE(window_state->IsFullscreen());

  GetEventGenerator()->PressKey(ui::VKEY_ESCAPE, ui::EF_CONTROL_DOWN);
  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(2));
  EXPECT_TRUE(window_state->IsFullscreen());
}

TEST_F(UILockControllerTest,
       HoldingEscapeOnlyExitsFullscreenIfWindowPropertySet) {
  SurfaceTriplet test_surface = BuildSurface(1024, 768);
  test_surface.shell_surface->SetUseImmersiveForFullscreen(false);
  test_surface.shell_surface->SetFullscreen(true);
  test_surface.surface->Commit();
  // Do not set chromeos::kEscHoldToExitFullscreen on TopLevelWindow.
  auto* window_state = test_surface.GetTopLevelWindowState();
  EXPECT_TRUE(window_state->IsFullscreen());

  GetEventGenerator()->PressKey(ui::VKEY_ESCAPE, ui::EF_NONE);
  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(2));
  EXPECT_TRUE(window_state->IsFullscreen());
}

TEST_F(UILockControllerTest, HoldingEscapeOnlyExitsFocusedFullscreen) {
  SurfaceTriplet test_surface1 = BuildSurface(1024, 768);
  test_surface1.shell_surface->SetUseImmersiveForFullscreen(false);
  test_surface1.shell_surface->SetFullscreen(true);
  test_surface1.surface->Commit();
  test_surface1.GetTopLevelWindow()->SetProperty(
      chromeos::kEscHoldToExitFullscreen, true);

  SurfaceTriplet test_surface2 = BuildSurface(1024, 768);
  test_surface2.shell_surface->SetUseImmersiveForFullscreen(false);
  test_surface2.shell_surface->SetFullscreen(true);
  test_surface2.surface->Commit();
  test_surface2.GetTopLevelWindow()->SetProperty(
      chromeos::kEscHoldToExitFullscreen, true);

  GetEventGenerator()->PressKey(ui::VKEY_ESCAPE, ui::EF_NONE);
  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(2));

  EXPECT_TRUE(test_surface1.GetTopLevelWindowState()->IsFullscreen());
  EXPECT_FALSE(test_surface2.GetTopLevelWindowState()->IsFullscreen());
}

TEST_F(UILockControllerTest, DestroyingWindowCancels) {
  std::unique_ptr<SurfaceTriplet> test_surface =
      std::make_unique<SurfaceTriplet>(BuildSurface(1024, 768));
  test_surface->shell_surface->SetUseImmersiveForFullscreen(false);
  test_surface->shell_surface->SetFullscreen(true);
  test_surface->surface->Commit();
  test_surface->GetTopLevelWindow()->SetProperty(
      chromeos::kEscHoldToExitFullscreen, true);
  auto* window_state = test_surface->GetTopLevelWindowState();
  EXPECT_TRUE(window_state->IsFullscreen());

  GetEventGenerator()->PressKey(ui::VKEY_ESCAPE, ui::EF_NONE);
  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(1));

  test_surface.reset();  // Destroying the Surface destroys the Window

  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(3));

  // The implicit assertion is that the code doesn't crash.
}

TEST_F(UILockControllerTest, FocusChangeCancels) {
  // Arrange: two windows, one is fullscreen and focused
  SurfaceTriplet other_surface = BuildSurface(1024, 768);
  other_surface.surface->Commit();

  SurfaceTriplet fullscreen_surface = BuildSurface(1024, 768);
  fullscreen_surface.shell_surface->SetUseImmersiveForFullscreen(false);
  fullscreen_surface.shell_surface->SetFullscreen(true);
  fullscreen_surface.surface->Commit();
  fullscreen_surface.GetTopLevelWindow()->SetProperty(
      chromeos::kEscHoldToExitFullscreen, true);

  EXPECT_EQ(fullscreen_surface.surface.get(), seat_->GetFocusedSurface());
  EXPECT_FALSE(fullscreen_surface.GetTopLevelWindowState()->IsMinimized());

  // Act: Press escape, then toggle focus back and forth
  GetEventGenerator()->PressKey(ui::VKEY_ESCAPE, ui::EF_NONE);
  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(1));

  wm::ActivateWindow(other_surface.surface->window());
  wm::ActivateWindow(fullscreen_surface.surface->window());

  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(2));

  // Assert: Fullscreen window was not minimized, despite regaining focus.
  EXPECT_FALSE(fullscreen_surface.GetTopLevelWindowState()->IsMinimized());
  EXPECT_EQ(fullscreen_surface.surface.get(), seat_->GetFocusedSurface());
}

TEST_F(UILockControllerTest, ShortHoldEscapeDoesNotExitFullscreen) {
  SurfaceTriplet test_surface = BuildSurface(1024, 768);
  test_surface.shell_surface->SetUseImmersiveForFullscreen(false);
  test_surface.shell_surface->SetFullscreen(true);
  test_surface.surface->Commit();
  test_surface.GetTopLevelWindow()->SetProperty(
      chromeos::kEscHoldToExitFullscreen, true);
  auto* window_state = test_surface.GetTopLevelWindowState();

  GetEventGenerator()->PressKey(ui::VKEY_ESCAPE, ui::EF_NONE);
  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(1));
  GetEventGenerator()->ReleaseKey(ui::VKEY_ESCAPE, ui::EF_NONE);
  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(2));

  EXPECT_TRUE(window_state->IsFullscreen());
}

TEST_F(UILockControllerTest, HoldingEscapeMinimizesIfPropertySet) {
  SurfaceTriplet test_surface = BuildSurface(1024, 768);
  test_surface.shell_surface->SetUseImmersiveForFullscreen(false);
  test_surface.shell_surface->SetFullscreen(true);
  test_surface.surface->Commit();
  test_surface.GetTopLevelWindow()->SetProperty(
      chromeos::kEscHoldToExitFullscreen, true);
  test_surface.GetTopLevelWindow()->SetProperty(
      chromeos::kEscHoldExitFullscreenToMinimized, true);
  auto* window_state = test_surface.GetTopLevelWindowState();
  EXPECT_TRUE(window_state->IsFullscreen());

  GetEventGenerator()->PressKey(ui::VKEY_ESCAPE, ui::EF_NONE);
  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(1));
  EXPECT_TRUE(window_state->IsFullscreen());  // no change yet

  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(1));
  EXPECT_FALSE(window_state->IsFullscreen());
  EXPECT_TRUE(window_state->IsMinimized());
}

TEST_F(UILockControllerTest, HoldingEscapeDoesNotMinimizeIfWindowed) {
  SurfaceTriplet test_surface = BuildSurface(1024, 768);
  test_surface.shell_surface->SetUseImmersiveForFullscreen(false);
  test_surface.surface->Commit();
  auto* window_state = test_surface.GetTopLevelWindowState();
  test_surface.GetTopLevelWindow()->SetProperty(
      chromeos::kEscHoldToExitFullscreen, true);
  test_surface.GetTopLevelWindow()->SetProperty(
      chromeos::kEscHoldExitFullscreenToMinimized, true);

  GetEventGenerator()->PressKey(ui::VKEY_ESCAPE, ui::EF_NONE);
  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(2));

  EXPECT_FALSE(window_state->IsMinimized());
}

TEST_F(UILockControllerWithUIBubbleTest, FullScreenShowsBubble) {
  SurfaceTriplet test_surface = BuildSurface(1024, 768);
  test_surface.shell_surface->SetUseImmersiveForFullscreen(false);
  test_surface.shell_surface->SetFullscreen(true);
  test_surface.surface->Commit();
  test_surface.GetTopLevelWindow()->SetProperty(
      chromeos::kEscHoldToExitFullscreen, true);

  EXPECT_TRUE(test_surface.GetTopLevelWindowState()->IsFullscreen());
  EXPECT_TRUE(BubbleShowingOnTop(test_surface.GetAlwaysOnTopContainer()));
}

TEST_F(UILockControllerWithUIBubbleTest, HoldingEscapeHidesBubble) {
  SurfaceTriplet test_surface = BuildSurface(1024, 768);
  test_surface.shell_surface->SetUseImmersiveForFullscreen(false);
  test_surface.shell_surface->SetFullscreen(true);
  test_surface.surface->Commit();
  test_surface.GetTopLevelWindow()->SetProperty(
      chromeos::kEscHoldToExitFullscreen, true);

  EXPECT_TRUE(test_surface.GetTopLevelWindowState()->IsFullscreen());
  EXPECT_TRUE(BubbleShowingOnTop(test_surface.GetAlwaysOnTopContainer()));

  GetEventGenerator()->PressKey(ui::VKEY_ESCAPE, ui::EF_NONE);
  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(3));

  EXPECT_FALSE(test_surface.GetTopLevelWindowState()->IsFullscreen());
  EXPECT_FALSE(BubbleShowingOnTop(test_surface.GetAlwaysOnTopContainer()));
}

TEST_F(UILockControllerWithUIBubbleTest, LosingFullscreenFocusHidesBubble) {
  SurfaceTriplet test_surface = BuildSurface(1024, 768);
  test_surface.shell_surface->SetUseImmersiveForFullscreen(false);
  test_surface.shell_surface->SetFullscreen(true);
  test_surface.surface->Commit();
  test_surface.GetTopLevelWindow()->SetProperty(
      chromeos::kEscHoldToExitFullscreen, true);

  EXPECT_TRUE(test_surface.GetTopLevelWindowState()->IsFullscreen());
  EXPECT_TRUE(BubbleShowingOnTop(test_surface.GetAlwaysOnTopContainer()));

  // Have surface loose fullscreen, bubble should now be hidden.
  test_surface.shell_surface->Minimize();
  test_surface.shell_surface->SetFullscreen(false);
  test_surface.surface->Commit();

  EXPECT_FALSE(test_surface.GetTopLevelWindowState()->IsFullscreen());
  EXPECT_FALSE(BubbleShowingOnTop(test_surface.GetAlwaysOnTopContainer()));
}

TEST_F(UILockControllerWithUIBubbleTest, RegainingFullscreenFocusShowsBubble) {
  SurfaceTriplet non_fullscreen_surface = BuildSurface(1024, 768);
  non_fullscreen_surface.surface->Commit();
  non_fullscreen_surface.shell_surface->Minimize();

  // non_fullscreen_surface had focus so bubble should not be showing.
  EXPECT_FALSE(non_fullscreen_surface.GetTopLevelWindowState()->IsFullscreen());
  EXPECT_FALSE(
      BubbleShowingOnTop(non_fullscreen_surface.GetAlwaysOnTopContainer()));

  // Have surface regain fullscreen, bubble should now be showing again.
  SurfaceTriplet test_surface = BuildSurface(1024, 768);
  test_surface.shell_surface->SetUseImmersiveForFullscreen(false);
  test_surface.shell_surface->SetFullscreen(true);
  test_surface.surface->Commit();
  test_surface.GetTopLevelWindow()->SetProperty(
      chromeos::kEscHoldToExitFullscreen, true);

  EXPECT_TRUE(test_surface.GetTopLevelWindowState()->IsFullscreen());
  EXPECT_TRUE(BubbleShowingOnTop(test_surface.GetAlwaysOnTopContainer()));
}

}  // namespace
}  // namespace exo
