// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_TEST_SHELL_SURFACE_BUILDER_H_
#define COMPONENTS_EXO_TEST_SHELL_SURFACE_BUILDER_H_

#include <memory>

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/class_property.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace exo {
class ShellSurface;
class ShellSurfaceBase;
class Surface;

namespace test {

// A builder to create a ShellSurface for testing purpose. Its surface and
// buffer, which are typically owned by a client, are owned by the host window
// as an owned property, therefore are destroyed when the shell surface is
// destroyed.
class ShellSurfaceBuilder {
 public:
  ShellSurfaceBuilder(const gfx::Size& buffer_size);
  ShellSurfaceBuilder(ShellSurfaceBuilder& other) = delete;
  ShellSurfaceBuilder& operator=(ShellSurfaceBuilder& other) = delete;
  ~ShellSurfaceBuilder();

  // Sets parameters that are used when creating a test window.
  ShellSurfaceBuilder& SetNoRootBuffer();
  ShellSurfaceBuilder& SetRootBufferFormat(gfx::BufferFormat buffer_format);
  ShellSurfaceBuilder& SetOrigin(const gfx::Point& origin);
  ShellSurfaceBuilder& SetParent(ShellSurface* shell_surface);
  ShellSurfaceBuilder& SetUseSystemModalContainer();
  ShellSurfaceBuilder& SetNoCommit();
  ShellSurfaceBuilder& SetCanMinimize(bool can_minimize);
  ShellSurfaceBuilder& SetMaximumSize(const gfx::Size& size);
  ShellSurfaceBuilder& SetMinimumSize(const gfx::Size& size);
  ShellSurfaceBuilder& SetDisableMovement();
  ShellSurfaceBuilder& SetAsPopup();
  ShellSurfaceBuilder& SetCentered();

  // once and the object cannot be used to create multiple windows.
  [[nodiscard]] std::unique_ptr<ShellSurface> BuildShellSurface();

  // Destroy's the root surface of the given 'shell_surface'.
  static void DestroyRootSurface(ShellSurfaceBase* shell_surface);
  static Surface* AddChildSurface(Surface* parent_surface,
                                  const gfx::Rect& bounds);

 private:
  gfx::Size root_buffer_size_;
  absl::optional<gfx::BufferFormat> root_buffer_format_ =
      gfx::BufferFormat::RGBA_8888;
  gfx::Point origin_;
  gfx::Size max_size_;
  gfx::Size min_size_;
  ShellSurface* parent_shell_surface_ = nullptr;
  bool use_system_modal_container_ = false;
  bool commit_on_build_ = true;
  bool can_minimize_ = true;
  bool disable_movement_ = false;
  bool centered_ = false;
  bool popup_ = false;

  bool built_ = false;
};

}  // namespace test
}  // namespace exo

#endif  // COMPONENTS_EXO_TEST_SHELL_SURFACE_BUILDER_H_
