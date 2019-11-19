// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/xdg_shell_surface.h"

#include "components/exo/buffer.h"
#include "components/exo/display.h"
#include "components/exo/shell_surface.h"
#include "components/exo/surface.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/test/exo_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace exo {
namespace {

struct SurfaceTriplet {
  std::unique_ptr<Surface> surface;
  std::unique_ptr<ShellSurface> shell_surface;
  std::unique_ptr<Buffer> buffer;
};

class XdgShellSurfaceTest : public test::ExoTestBase {
 protected:
  SurfaceTriplet BuildSurface(int w, int h) {
    auto surface = std::make_unique<Surface>();
    auto shell_surface = std::make_unique<XdgShellSurface>(
        surface.get(), gfx::Point{0, 0},
        /*activatable=*/true,
        /*can_minimize=*/true, ash::desks_util::GetActiveDeskContainerId());
    auto buffer = std::make_unique<Buffer>(
        exo_test_helper()->CreateGpuMemoryBuffer({w, h}));
    surface->Attach(buffer.get());
    return {std::move(surface), std::move(shell_surface), std::move(buffer)};
  }

  // Returns the size of the surface associated with a maximized widget. If the
  // widget is |decorated| the size will be smaller due to the widget's
  // decorations.
  gfx::Size GetMaximizedSurfaceSize(bool decorated) {
    SurfaceTriplet temp = BuildSurface(1, 1);
    temp.surface->SetFrame(decorated ? SurfaceFrameType::NORMAL
                                     : SurfaceFrameType::NONE);
    temp.shell_surface->Maximize();
    temp.surface->Commit();

    EXPECT_TRUE(temp.shell_surface->GetWidget()->IsMaximized());
    return temp.shell_surface->GetWidget()->client_view()->size();
  }
};

// We don't actually care about the size of decorations. The purpose of this
// test is to ensure that enabling decorations in the way that we do actually
// causes the widget to be drawn with a (nonzero-sized) frame.
TEST_F(XdgShellSurfaceTest, DecoratedSurfaceSmallerThanUndecorated) {
  gfx::Size undecorated_size = GetMaximizedSurfaceSize(false);
  gfx::Size decorated_size = GetMaximizedSurfaceSize(true);

  // The best expectation we can have is that the window decoration must be
  // nonzero in one direction.
  int decoration_width = undecorated_size.width() - decorated_size.width();
  int decoration_height = undecorated_size.height() - decorated_size.height();
  EXPECT_GE(decoration_width, 0);
  EXPECT_GE(decoration_height, 0);
  EXPECT_GT(decoration_width + decoration_height, 0);
}

TEST_F(XdgShellSurfaceTest, UndecoratedSurfaceAutoMaximizes) {
  gfx::Size maximized_size = GetMaximizedSurfaceSize(/*decorated=*/false);

  SurfaceTriplet max_surface =
      BuildSurface(maximized_size.width(), maximized_size.height());
  max_surface.surface->Commit();
  EXPECT_TRUE(max_surface.shell_surface->GetWidget()->IsMaximized());

  SurfaceTriplet narrow_surface =
      BuildSurface(maximized_size.width() - 1, maximized_size.height());
  narrow_surface.surface->Commit();
  EXPECT_FALSE(narrow_surface.shell_surface->GetWidget()->IsMaximized());

  SurfaceTriplet short_surface =
      BuildSurface(maximized_size.width(), maximized_size.height() - 1);
  short_surface.surface->Commit();
  EXPECT_FALSE(short_surface.shell_surface->GetWidget()->IsMaximized());
}

TEST_F(XdgShellSurfaceTest, DecoratedSurfaceAutoMaximizes) {
  gfx::Size maximized_size = GetMaximizedSurfaceSize(/*decorated=*/true);

  SurfaceTriplet max_surface =
      BuildSurface(maximized_size.width(), maximized_size.height());
  max_surface.surface->SetFrame(SurfaceFrameType::NORMAL);
  max_surface.surface->Commit();
  EXPECT_TRUE(max_surface.shell_surface->GetWidget()->IsMaximized());

  SurfaceTriplet narrow_surface =
      BuildSurface(maximized_size.width() - 1, maximized_size.height());
  narrow_surface.surface->SetFrame(SurfaceFrameType::NORMAL);
  narrow_surface.surface->Commit();
  EXPECT_FALSE(narrow_surface.shell_surface->GetWidget()->IsMaximized());

  SurfaceTriplet short_surface =
      BuildSurface(maximized_size.width(), maximized_size.height() - 1);
  short_surface.surface->SetFrame(SurfaceFrameType::NORMAL);
  short_surface.surface->Commit();
  EXPECT_FALSE(short_surface.shell_surface->GetWidget()->IsMaximized());
}

TEST_F(XdgShellSurfaceTest, DontMaximizeIfStateWasModified) {
  gfx::Size maximized_size = GetMaximizedSurfaceSize(/*decorated=*/true);

  SurfaceTriplet test_surface =
      BuildSurface(maximized_size.width() + 1, maximized_size.height() + 1);
  // Explicitly restoring the window should prevent auto maximize.
  test_surface.shell_surface->Restore();
  test_surface.surface->Commit();
  EXPECT_FALSE(test_surface.shell_surface->GetWidget()->IsMaximized());
}

}  // namespace
}  // namespace exo
