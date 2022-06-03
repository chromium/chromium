// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/external_arc/overlay/arc_overlay_manager.h"
#include "components/exo/buffer.h"
#include "components/exo/client_controlled_shell_surface.h"
#include "components/exo/sub_surface.h"
#include "components/exo/surface.h"
#include "components/exo/test/exo_test_base.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/class_property.h"

namespace exo {

class ArcOverlayManagerTest : public test::ExoTestBase {
 public:
 private:
  ash::ArcOverlayManager arc_overlay_manager_;
};

TEST_F(ArcOverlayManagerTest, Basic) {
  gfx::Size buffer_size(256, 256);

  std::unique_ptr<Surface> surface1(new Surface);
  auto shell_surface(
      exo_test_helper()->CreateClientControlledShellSurface(surface1.get()));

  // Create Widget
  shell_surface->SetSystemUiVisibility(false);

  std::unique_ptr<Surface> surface2(new Surface);
  auto sub_surface =
      std::make_unique<SubSurface>(surface2.get(), surface1.get());

  std::unique_ptr<Buffer> buffer2(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));

  surface2->Attach(buffer2.get());

  // Make
  surface2->Commit();
  surface1->Commit();
}

}  // namespace exo
