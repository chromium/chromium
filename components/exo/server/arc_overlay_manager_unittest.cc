// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/external_arc/overlay/arc_overlay_manager.h"
#include "components/exo/buffer.h"
#include "components/exo/client_controlled_shell_surface.h"
#include "components/exo/sub_surface.h"
#include "components/exo/surface.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/test/shell_surface_builder.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/class_property.h"

namespace exo {

class ArcOverlayManagerTest : public test::ExoTestBase {
 public:
 private:
  ash::ArcOverlayManager arc_overlay_manager_;
};

TEST_F(ArcOverlayManagerTest, Basic) {
  auto shell_surface = exo::test::ShellSurfaceBuilder({256, 256})
                           .SetNoCommit()
                           .BuildClientControlledShellSurface();
  auto* surface1 = shell_surface->root_surface();

  // Create Widget
  shell_surface->SetSystemUiVisibility(false);

  auto* surface2 =
      test::ShellSurfaceBuilder::AddChildSurface(surface1, {0, 0, 128, 128});

  // Make
  surface2->Commit();
  surface1->Commit();
}

}  // namespace exo
