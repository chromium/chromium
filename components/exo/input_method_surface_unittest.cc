// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/input_method_surface.h"

#include "ash/shell.h"
#include "components/exo/buffer.h"
#include "components/exo/input_method_surface_manager.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/test/exo_test_helper.h"
#include "ui/display/display.h"

namespace exo {

class InputMethodSurfaceTest : public test::ExoTestBase,
                               public InputMethodSurfaceManager {
 public:
  InputMethodSurfaceTest() = default;
  InputMethodSurfaceTest(const InputMethodSurfaceTest&) = delete;
  InputMethodSurfaceTest& operator=(const InputMethodSurfaceTest&) = delete;

  // Overridden from InputMethodSurfaceTest:
  InputMethodSurface* GetSurface() const override { return nullptr; }
  void AddSurface(InputMethodSurface* surface) override {}
  void RemoveSurface(InputMethodSurface* surface) override {}
  void OnTouchableBoundsChanged(InputMethodSurface* surface) override {}
};

TEST_F(InputMethodSurfaceTest, SetGeometryShouldIgnoreWorkArea) {
  UpdateDisplay("800x600");

  int64_t display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::DisplayManager* display_manager =
      ash::Shell::Get()->display_manager();

  // With work area top insets.
  display_manager->UpdateWorkAreaOfDisplay(display_id,
                                           gfx::Insets(200, 0, 0, 0));

  gfx::Size buffer_size(800, 600);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  auto shell_surface =
      exo_test_helper()->CreateInputMethodSurface(surface.get(), this);
  surface->Attach(buffer.get());
  shell_surface->SetGeometry(gfx::Rect(buffer_size));
  surface->Commit();

  views::Widget* widget = shell_surface->GetWidget();
  EXPECT_EQ(gfx::Rect(buffer_size), widget->GetWindowBoundsInScreen());
}

}  // namespace exo
