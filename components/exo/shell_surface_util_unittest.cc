// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/shell_surface_util.h"

#include "components/exo/buffer.h"
#include "components/exo/display.h"
#include "components/exo/shell_surface.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/test/shell_surface_builder.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"

namespace exo {
namespace {

using ShellSurfaceUtilTest = test::ExoTestBase;

void SetPositionAtOrigin(ui::LocatedEvent* event, aura::Window* target) {
  ui::Event::DispatcherApi test_api(event);
  test_api.set_target(target);
  gfx::Point point;
  aura::Window::ConvertPointToTarget(target, target->GetRootWindow(), &point);
  event->set_location(point);
}

TEST_F(ShellSurfaceUtilTest, TargetForLocatedEvent) {
  auto shell_surface = test::ShellSurfaceBuilder({20, 20})
                           .SetOrigin({10, 10})
                           .BuildShellSurface();
  auto* root_surface = shell_surface->root_surface();
  auto* child_surface = test::ShellSurfaceBuilder::AddChildSurface(
      root_surface, {10, 10, 10, 10});
  child_surface->Commit();
  root_surface->Commit();

  ui::MouseEvent mouse_event(ui::EventType::kMousePressed, gfx::Point(0, 0),
                             gfx::Point(0, 0), ui::EventTimeForNow(), 0, 0);
  aura::Window* root_window = root_surface->window()->GetRootWindow();
  ui::Event::DispatcherApi(&mouse_event).set_target(root_window);
  EXPECT_EQ(nullptr, GetTargetSurfaceForLocatedEvent(&mouse_event));

  SetPositionAtOrigin(&mouse_event, root_surface->window());
  EXPECT_EQ(root_surface, GetTargetSurfaceForLocatedEvent(&mouse_event));
  SetPositionAtOrigin(&mouse_event, child_surface->window());
  EXPECT_EQ(child_surface, GetTargetSurfaceForLocatedEvent(&mouse_event));

  // Capture
  auto* shell_surface_window = shell_surface->GetWidget()->GetNativeWindow();
  shell_surface_window->SetCapture();
  ui::Event::DispatcherApi(&mouse_event).set_target(shell_surface_window);
  mouse_event.set_location({-1, -1});
  EXPECT_EQ(root_surface, GetTargetSurfaceForLocatedEvent(&mouse_event));
  mouse_event.set_location({1, 1});
  EXPECT_EQ(root_surface, GetTargetSurfaceForLocatedEvent(&mouse_event));
  mouse_event.set_location({11, 11});
  EXPECT_EQ(child_surface, GetTargetSurfaceForLocatedEvent(&mouse_event));
  shell_surface.reset();
}

TEST_F(ShellSurfaceUtilTest, TargetForKeyboardFocus) {
  auto shell_surface = test::ShellSurfaceBuilder({20, 20})
                           .SetOrigin({10, 10})
                           .BuildShellSurface();
  auto* root_surface = shell_surface->root_surface();
  auto* child_surface = test::ShellSurfaceBuilder::AddChildSurface(
      root_surface, {10, 10, 10, 10});

  EXPECT_EQ(root_surface,
            GetTargetSurfaceForKeyboardFocus(child_surface->window()));
  EXPECT_EQ(root_surface,
            GetTargetSurfaceForKeyboardFocus(root_surface->window()));
  EXPECT_EQ(root_surface,
            GetTargetSurfaceForKeyboardFocus(shell_surface->host_window()));
  EXPECT_EQ(root_surface, GetTargetSurfaceForKeyboardFocus(
                              shell_surface->GetWidget()->GetNativeWindow()));
}

// No explicit verifications are needed for this test as this test just tries to
// catch potential crashes.
TEST_F(ShellSurfaceUtilTest, ClientControlledTargetForKeyboardFocus) {
  Display display;
  auto shell_surface = exo::test::ShellSurfaceBuilder({256, 256})
                           .BuildClientControlledShellSurface();

  shell_surface->set_delegate(
      std::make_unique<test::ClientControlledShellSurfaceDelegate>(
          shell_surface.get(), true));
  shell_surface->SetMinimized();
  auto* surface = shell_surface->root_surface();
  surface->Commit();

  shell_surface->GetWidget()->Hide();
  shell_surface->OnSurfaceCommit();

  shell_surface->GetWidget()->GetNativeWindow()->Focus();
}

}  // namespace
}  // namespace exo
