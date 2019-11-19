// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_TEST_EXO_TEST_HELPER_H_
#define COMPONENTS_EXO_TEST_EXO_TEST_HELPER_H_

#include <memory>

#include "ash/public/cpp/window_state_type.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"

namespace gfx {
class GpuMemoryBuffer;
}

namespace exo {
class Buffer;
class ClientControlledShellSurface;
class Surface;
class ShellSurface;

namespace test {

class ExoTestWindow {
 public:
  ExoTestWindow(std::unique_ptr<gfx::GpuMemoryBuffer> gpu_buffer,
                bool is_modal);
  ExoTestWindow(ExoTestWindow&& window);
  virtual ~ExoTestWindow();

  Surface* surface() { return surface_.get(); }
  ShellSurface* shell_surface() { return shell_surface_.get(); }
  gfx::Point origin();

 private:
  std::unique_ptr<Surface> surface_;
  std::unique_ptr<Buffer> buffer_;
  std::unique_ptr<ShellSurface> shell_surface_;
};

// A helper class that does common initialization required for Exosphere.
class ExoTestHelper {
 public:
  ExoTestHelper();
  ~ExoTestHelper();

  // Creates a GpuMemoryBuffer instance that can be used for tests.
  std::unique_ptr<gfx::GpuMemoryBuffer> CreateGpuMemoryBuffer(
      const gfx::Size& size,
      gfx::BufferFormat format = gfx::BufferFormat::RGBA_8888);

  // Creates window of size (width, height) at center of screen.
  ExoTestWindow CreateWindow(int width, int height, bool is_modal);
  std::unique_ptr<ClientControlledShellSurface>
  CreateClientControlledShellSurface(Surface* surface, bool is_modal = false);

 private:
  DISALLOW_COPY_AND_ASSIGN(ExoTestHelper);
};

}  // namespace test
}  // namespace exo

#endif  // COMPONENTS_EXO_TEST_EXO_TEST_HELPER_H_
