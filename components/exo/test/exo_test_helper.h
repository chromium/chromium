// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_TEST_EXO_TEST_HELPER_H_
#define COMPONENTS_EXO_TEST_EXO_TEST_HELPER_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ui/base/window_state_type.h"
#include "components/exo/client_controlled_shell_surface.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"

namespace gfx {
class GpuMemoryBuffer;
}

namespace exo {
class ClientControlledShellSurface;
class InputMethodSurface;
class InputMethodSurfaceManager;
class Surface;
class ToastSurface;
class ToastSurfaceManager;

namespace test {

class ClientControlledShellSurfaceDelegate
    : public ClientControlledShellSurface::Delegate {
 public:
  explicit ClientControlledShellSurfaceDelegate(
      ClientControlledShellSurface* shell_surface,
      bool delay_commit = false);
  ~ClientControlledShellSurfaceDelegate() override;
  ClientControlledShellSurfaceDelegate(
      const ClientControlledShellSurfaceDelegate&) = delete;
  ClientControlledShellSurfaceDelegate& operator=(
      const ClientControlledShellSurfaceDelegate&) = delete;

 protected:
  // ClientControlledShellSurface::Delegate:
  void OnGeometryChanged(const gfx::Rect& geometry) override;
  void OnStateChanged(chromeos::WindowStateType old_state_type,
                      chromeos::WindowStateType new_state_type) override;
  void OnBoundsChanged(chromeos::WindowStateType current_state,
                       chromeos::WindowStateType requested_state,
                       int64_t display_id,
                       const gfx::Rect& bounds_in_display,
                       bool is_resize,
                       int bounds_change) override;
  void OnDragStarted(int component) override;
  void OnDragFinished(int x, int y, bool canceled) override;
  void OnZoomLevelChanged(ZoomChange zoom_change) override;
  void Commit();

  raw_ptr<ClientControlledShellSurface, ExperimentalAsh> shell_surface_;
  bool delay_commit_;
};

// A helper class that does common initialization required for Exosphere.
class ExoTestHelper {
 public:
  ExoTestHelper();

  ExoTestHelper(const ExoTestHelper&) = delete;
  ExoTestHelper& operator=(const ExoTestHelper&) = delete;

  ~ExoTestHelper();

  // Creates a GpuMemoryBuffer instance that can be used for tests.
  std::unique_ptr<gfx::GpuMemoryBuffer> CreateGpuMemoryBuffer(
      const gfx::Size& size,
      gfx::BufferFormat format = gfx::BufferFormat::RGBA_8888);

  std::unique_ptr<ClientControlledShellSurface>
  CreateClientControlledShellSurface(Surface* surface,
                                     bool is_modal = false,
                                     bool default_scale_cancellation = false);
  std::unique_ptr<InputMethodSurface> CreateInputMethodSurface(
      Surface* surface,
      InputMethodSurfaceManager* surface_manager,
      bool default_scale_cancellation = true);
  std::unique_ptr<ToastSurface> CreateToastSurface(
      Surface* surface,
      ToastSurfaceManager* surface_manager,
      bool default_scale_cancellation = true);
};

}  // namespace test
}  // namespace exo

#endif  // COMPONENTS_EXO_TEST_EXO_TEST_HELPER_H_
