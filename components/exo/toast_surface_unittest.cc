// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/toast_surface.h"

#include "components/exo/test/exo_test_base.h"
#include "components/exo/test/exo_test_helper.h"
#include "components/exo/toast_surface_manager.h"

namespace exo {

class ToastSurfaceTest : public test::ExoTestBase, public ToastSurfaceManager {
 public:
  ToastSurfaceTest() = default;
  ToastSurfaceTest(const ToastSurfaceTest&) = delete;
  ToastSurfaceTest& operator=(const ToastSurfaceTest&) = delete;

  // Overridden from ToastSurfaceManager:
  void AddSurface(ToastSurface* surface) override {}
  void RemoveSurface(ToastSurface* surface) override {}
};

TEST_F(ToastSurfaceTest, ToastSurfaceShouldNotBeActivatable) {
  std::unique_ptr<Surface> surface(new Surface);
  auto shell_surface =
      exo_test_helper()->CreateToastSurface(surface.get(), this);
  surface->Commit();

  views::Widget* widget = shell_surface->GetWidget();
  EXPECT_FALSE(widget->CanActivate());
}

}  // namespace exo
