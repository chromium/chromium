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
#include "components/exo/wm_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/class_property.h"
#include "ui/wm/core/window_util.h"

namespace exo {

namespace {

constexpr char kNoEscHoldAppId[] = "no-esc-hold";

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
  class TestPropertyResolver : public exo::WMHelper::AppPropertyResolver {
   public:
    TestPropertyResolver() = default;
    ~TestPropertyResolver() override = default;
    void PopulateProperties(
        const Params& params,
        ui::PropertyHandler& out_properties_container) override {
      out_properties_container.SetProperty(chromeos::kEscHoldToExitFullscreen,
                                           params.app_id != kNoEscHoldAppId);
    }
  };

  // test::ExoTestBase:
  void SetUp() override {
    test::ExoTestBase::SetUp();
    seat_ = std::make_unique<Seat>();
    scoped_feature_list_.InitAndEnableFeature(
        chromeos::features::kExoLockNotification);
    WMHelper::GetInstance()->RegisterAppPropertyResolver(
        std::make_unique<TestPropertyResolver>());
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
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(UILockControllerTest, HoldingEscapeExitsFullscreen) {
  SurfaceTriplet test_surface = BuildSurface(1024, 768);
  test_surface.shell_surface->SetUseImmersiveForFullscreen(false);
  test_surface.shell_surface->SetFullscreen(true);
  test_surface.surface->Commit();
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
  auto* window_state = test_surface.GetTopLevelWindowState();
  EXPECT_TRUE(window_state->IsFullscreen());

  GetEventGenerator()->PressKey(ui::VKEY_ESCAPE, ui::EF_CONTROL_DOWN);
  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(2));
  EXPECT_TRUE(window_state->IsFullscreen());
}

TEST_F(UILockControllerTest,
       HoldingEscapeOnlyExitsFullscreenIfWindowPropertySet) {
  // Do not set chromeos::kEscHoldToExitFullscreen on TopLevelWindow.
  SurfaceTriplet test_surface = BuildSurface(1024, 768);
  test_surface.shell_surface->SetApplicationId(kNoEscHoldAppId);
  test_surface.shell_surface->SetUseImmersiveForFullscreen(false);
  test_surface.shell_surface->SetFullscreen(true);
  test_surface.surface->Commit();
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

  SurfaceTriplet test_surface2 = BuildSurface(1024, 768);
  test_surface2.shell_surface->SetUseImmersiveForFullscreen(false);
  test_surface2.shell_surface->SetFullscreen(true);
  test_surface2.surface->Commit();

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
      chromeos::kEscHoldExitFullscreenToMinimized, true);

  GetEventGenerator()->PressKey(ui::VKEY_ESCAPE, ui::EF_NONE);
  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(2));

  EXPECT_FALSE(window_state->IsMinimized());
}

TEST_F(UILockControllerTest, FullScreenShowsBubble) {
  SurfaceTriplet test_surface = BuildSurface(1024, 768);
  test_surface.shell_surface->SetUseImmersiveForFullscreen(false);
  test_surface.shell_surface->SetFullscreen(true);
  test_surface.surface->Commit();

  EXPECT_TRUE(test_surface.GetTopLevelWindowState()->IsFullscreen());
  EXPECT_TRUE(seat_->GetUILockControllerForTesting()->IsBubbleVisibleForTesting(
      test_surface.GetTopLevelWindow()));
}

TEST_F(UILockControllerTest, BubbleClosesAfterDuration) {
  SurfaceTriplet test_surface = BuildSurface(1024, 768);
  test_surface.shell_surface->SetUseImmersiveForFullscreen(false);
  test_surface.shell_surface->SetFullscreen(true);
  test_surface.surface->Commit();

  EXPECT_TRUE(seat_->GetUILockControllerForTesting()->IsBubbleVisibleForTesting(
      test_surface.GetTopLevelWindow()));
  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(5));
  EXPECT_FALSE(
      seat_->GetUILockControllerForTesting()->IsBubbleVisibleForTesting(
          test_surface.GetTopLevelWindow()));
}

TEST_F(UILockControllerTest, HoldingEscapeHidesBubble) {
  SurfaceTriplet test_surface = BuildSurface(1024, 768);
  test_surface.shell_surface->SetUseImmersiveForFullscreen(false);
  test_surface.shell_surface->SetFullscreen(true);
  test_surface.surface->Commit();

  EXPECT_TRUE(test_surface.GetTopLevelWindowState()->IsFullscreen());
  EXPECT_TRUE(seat_->GetUILockControllerForTesting()->IsBubbleVisibleForTesting(
      test_surface.GetTopLevelWindow()));

  GetEventGenerator()->PressKey(ui::VKEY_ESCAPE, ui::EF_NONE);
  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(3));

  EXPECT_FALSE(test_surface.GetTopLevelWindowState()->IsFullscreen());
  EXPECT_FALSE(
      seat_->GetUILockControllerForTesting()->IsBubbleVisibleForTesting(
          test_surface.GetTopLevelWindow()));
}

TEST_F(UILockControllerTest, LosingFullscreenHidesBubble) {
  SurfaceTriplet test_surface = BuildSurface(1024, 768);
  test_surface.shell_surface->SetUseImmersiveForFullscreen(false);
  test_surface.shell_surface->SetFullscreen(true);
  test_surface.surface->Commit();

  EXPECT_TRUE(test_surface.GetTopLevelWindowState()->IsFullscreen());
  EXPECT_TRUE(seat_->GetUILockControllerForTesting()->IsBubbleVisibleForTesting(
      test_surface.GetTopLevelWindow()));

  // Have surface loose fullscreen, bubble should now be hidden.
  test_surface.shell_surface->Minimize();
  test_surface.shell_surface->SetFullscreen(false);
  test_surface.surface->Commit();

  EXPECT_FALSE(test_surface.GetTopLevelWindowState()->IsFullscreen());
  EXPECT_FALSE(
      seat_->GetUILockControllerForTesting()->IsBubbleVisibleForTesting(
          test_surface.GetTopLevelWindow()));
}

TEST_F(UILockControllerTest, BubbleIsReshown) {
  SurfaceTriplet test_surface = BuildSurface(1024, 768);
  test_surface.shell_surface->SetUseImmersiveForFullscreen(false);
  test_surface.shell_surface->SetFullscreen(true);
  test_surface.surface->Commit();

  EXPECT_TRUE(seat_->GetUILockControllerForTesting()->IsBubbleVisibleForTesting(
      test_surface.GetTopLevelWindow()));

  // Stop fullscreen.
  test_surface.shell_surface->SetFullscreen(false);
  EXPECT_FALSE(
      seat_->GetUILockControllerForTesting()->IsBubbleVisibleForTesting(
          test_surface.GetTopLevelWindow()));

  // Fullscreen should show bubble since it did not stay visible for duration.
  test_surface.shell_surface->SetFullscreen(true);
  EXPECT_TRUE(seat_->GetUILockControllerForTesting()->IsBubbleVisibleForTesting(
      test_surface.GetTopLevelWindow()));

  // After duration, bubble should be removed.
  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(5));
  EXPECT_FALSE(
      seat_->GetUILockControllerForTesting()->IsBubbleVisibleForTesting(
          test_surface.GetTopLevelWindow()));

  // Bubble is not shown after fullscreen toggle.
  test_surface.shell_surface->SetFullscreen(false);
  test_surface.shell_surface->SetFullscreen(true);
  EXPECT_FALSE(
      seat_->GetUILockControllerForTesting()->IsBubbleVisibleForTesting(
          test_surface.GetTopLevelWindow()));
}

}  // namespace
}  // namespace exo
