// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/ui_lock_controller.h"

#include "ash/public/cpp/app_types.h"
#include "ash/shell.h"
#include "ash/wm/window_state.h"
#include "components/exo/buffer.h"
#include "components/exo/display.h"
#include "components/exo/shell_surface.h"
#include "components/exo/surface.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/test/exo_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/wm/core/window_util.h"

namespace exo {
namespace {

struct SurfaceTriplet {
  std::unique_ptr<Surface> surface;
  std::unique_ptr<ShellSurface> shell_surface;
  std::unique_ptr<Buffer> buffer;

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
        /*activatable=*/true,
        /*can_minimize=*/true, ash::desks_util::GetActiveDeskContainerId());
    auto buffer = std::make_unique<Buffer>(
        exo_test_helper()->CreateGpuMemoryBuffer({w, h}));
    surface->Attach(buffer.get());

    return {std::move(surface), std::move(shell_surface), std::move(buffer)};
  }

  std::unique_ptr<Seat> seat_;
};

void SetAppType(SurfaceTriplet& surface, ash::AppType appType) {
  surface.GetTopLevelWindow()->SetProperty(aura::client::kAppType,
                                           static_cast<int>(appType));
}

TEST_F(UILockControllerTest, HoldingEscapeExitsFullscreen) {
  SurfaceTriplet test_surface = BuildSurface(1024, 768);
  test_surface.shell_surface->SetUseImmersiveForFullscreen(false);
  test_surface.shell_surface->SetFullscreen(true);
  test_surface.surface->Commit();
  SetAppType(test_surface, ash::AppType::CROSTINI_APP);
  auto* window_state = test_surface.GetTopLevelWindowState();
  EXPECT_TRUE(window_state->IsFullscreen());

  GetEventGenerator()->PressKey(ui::VKEY_ESCAPE, ui::EF_NONE);
  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(1));
  EXPECT_TRUE(window_state->IsFullscreen());  // no change yet

  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(1));
  EXPECT_FALSE(window_state->IsFullscreen());
}

TEST_F(UILockControllerTest, HoldingCtrlEscapeDoesNotExitFullscreen) {
  SurfaceTriplet test_surface = BuildSurface(1024, 768);
  test_surface.shell_surface->SetUseImmersiveForFullscreen(false);
  test_surface.shell_surface->SetFullscreen(true);
  test_surface.surface->Commit();
  SetAppType(test_surface, ash::AppType::CROSTINI_APP);
  auto* window_state = test_surface.GetTopLevelWindowState();
  EXPECT_TRUE(window_state->IsFullscreen());

  GetEventGenerator()->PressKey(ui::VKEY_ESCAPE, ui::EF_CONTROL_DOWN);
  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(2));
  EXPECT_TRUE(window_state->IsFullscreen());
}

TEST_F(UILockControllerTest, HoldingEscapeOnlyAffectsCrostiniApps) {
  SurfaceTriplet test_surface = BuildSurface(1024, 768);
  test_surface.shell_surface->SetUseImmersiveForFullscreen(false);
  test_surface.shell_surface->SetFullscreen(true);
  test_surface.surface->Commit();
  SetAppType(test_surface, ash::AppType::ARC_APP);
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
  SetAppType(test_surface1, ash::AppType::CROSTINI_APP);

  SurfaceTriplet test_surface2 = BuildSurface(1024, 768);
  test_surface2.shell_surface->SetUseImmersiveForFullscreen(false);
  test_surface2.shell_surface->SetFullscreen(true);
  test_surface2.surface->Commit();
  SetAppType(test_surface2, ash::AppType::CROSTINI_APP);

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
  SetAppType(*test_surface, ash::AppType::CROSTINI_APP);
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
  SetAppType(other_surface, ash::AppType::CROSTINI_APP);

  SurfaceTriplet fullscreen_surface = BuildSurface(1024, 768);
  fullscreen_surface.shell_surface->SetUseImmersiveForFullscreen(false);
  fullscreen_surface.shell_surface->SetFullscreen(true);
  fullscreen_surface.surface->Commit();
  SetAppType(fullscreen_surface, ash::AppType::CROSTINI_APP);

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

TEST_F(UILockControllerTest, EscapeDoesNotExitImmersiveFullscreen) {
  SurfaceTriplet test_surface = BuildSurface(1024, 768);
  test_surface.shell_surface->SetFullscreen(true);
  test_surface.surface->Commit();
  SetAppType(test_surface, ash::AppType::CROSTINI_APP);
  auto* window_state = test_surface.GetTopLevelWindowState();

  GetEventGenerator()->PressKey(ui::VKEY_ESCAPE, ui::EF_NONE);
  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(2));

  EXPECT_TRUE(window_state->IsFullscreen());
}

TEST_F(UILockControllerTest, ShortHoldEscapeDoesNotExitFullscreen) {
  SurfaceTriplet test_surface = BuildSurface(1024, 768);
  test_surface.shell_surface->SetUseImmersiveForFullscreen(false);
  test_surface.shell_surface->SetFullscreen(true);
  test_surface.surface->Commit();
  SetAppType(test_surface, ash::AppType::CROSTINI_APP);
  auto* window_state = test_surface.GetTopLevelWindowState();

  GetEventGenerator()->PressKey(ui::VKEY_ESCAPE, ui::EF_NONE);
  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(1));
  GetEventGenerator()->ReleaseKey(ui::VKEY_ESCAPE, ui::EF_NONE);
  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(2));

  EXPECT_TRUE(window_state->IsFullscreen());
}

TEST_F(UILockControllerTest, HoldingEscapeDoesNotMinimizeIfWindowed) {
  SurfaceTriplet test_surface = BuildSurface(1024, 768);
  test_surface.shell_surface->SetUseImmersiveForFullscreen(false);
  test_surface.surface->Commit();
  SetAppType(test_surface, ash::AppType::CROSTINI_APP);
  auto* window_state = test_surface.GetTopLevelWindowState();

  GetEventGenerator()->PressKey(ui::VKEY_ESCAPE, ui::EF_NONE);
  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(2));

  EXPECT_FALSE(window_state->IsMinimized());
}

}  // namespace
}  // namespace exo
