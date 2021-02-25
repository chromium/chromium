// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/test/shell_surface_builder.h"

#include "ash/wm/desks/desks_util.h"
#include "ash/wm/window_positioning_utils.h"
#include "components/exo/buffer.h"
#include "components/exo/surface.h"
#include "components/exo/test/exo_test_helper.h"
#include "components/exo/xdg_shell_surface.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "ui/aura/env.h"

namespace {

// Internal structure that owns buffer and surface. This is owned by
// the host window as an owned property.
struct Holder {
  std::unique_ptr<exo::Buffer> root_buffer;
  std::unique_ptr<exo::Surface> root_surface;
};

}  // namespace

DEFINE_UI_CLASS_PROPERTY_TYPE(Holder*)

namespace exo {
namespace test {
namespace {

DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(Holder, kBuilderResourceHolderKey, nullptr)

}  // namespace

ShellSurfaceBuilder::ShellSurfaceBuilder(const gfx::Size& buffer_size)
    : root_buffer_size_(buffer_size) {}

ShellSurfaceBuilder::~ShellSurfaceBuilder() = default;

ShellSurfaceBuilder& ShellSurfaceBuilder::SetRootBufferFormat(
    gfx::BufferFormat buffer_format) {
  DCHECK(!built_);
  root_buffer_format_ = buffer_format;
  return *this;
}

ShellSurfaceBuilder& ShellSurfaceBuilder::SetOrigin(const gfx::Point& origin) {
  DCHECK(!built_);
  origin_ = origin;
  return *this;
}

ShellSurfaceBuilder& ShellSurfaceBuilder::SetParent(ShellSurface* parent) {
  DCHECK(!built_);
  parent_shell_surface_ = parent;
  return *this;
}

ShellSurfaceBuilder& ShellSurfaceBuilder::SetUseSystemModalContainer() {
  DCHECK(!built_);
  use_system_modal_container_ = true;
  return *this;
}

ShellSurfaceBuilder& ShellSurfaceBuilder::SetNoCommit() {
  DCHECK(!built_);
  commit_on_build_ = false;
  return *this;
}

ShellSurfaceBuilder& ShellSurfaceBuilder::SetCanMinimize(bool can_minimize) {
  DCHECK(!built_);
  can_minimize_ = can_minimize;
  return *this;
}

ShellSurfaceBuilder& ShellSurfaceBuilder::SetDisableMovement() {
  DCHECK(!built_);
  disable_movement_ = true;
  return *this;
}

ShellSurfaceBuilder& ShellSurfaceBuilder::SetCentered() {
  DCHECK(!built_);
  centered_ = true;
  return *this;
}

// static
void ShellSurfaceBuilder::DestroyRootSurface(ShellSurfaceBase* shell_surface) {
  Holder* holder =
      shell_surface->host_window()->GetProperty(kBuilderResourceHolderKey);
  DCHECK(holder);
  holder->root_surface.reset();
}

std::unique_ptr<ShellSurface> ShellSurfaceBuilder::BuildShellSurface() {
  DCHECK(!built_);
  built_ = true;
  Holder* holder = new Holder();
  holder->root_buffer = std::make_unique<Buffer>(
      aura::Env::GetInstance()
          ->context_factory()
          ->GetGpuMemoryBufferManager()
          ->CreateGpuMemoryBuffer(root_buffer_size_, root_buffer_format_,
                                  gfx::BufferUsage::GPU_READ,
                                  gpu::kNullSurfaceHandle));

  holder->root_surface = std::make_unique<Surface>();

  int container = use_system_modal_container_
                      ? ash::kShellWindowId_SystemModalContainer
                      : ash::desks_util::GetActiveDeskContainerId();

  auto shell_surface = std::make_unique<ShellSurface>(
      holder->root_surface.get(), origin_, can_minimize_, container);
  holder->root_surface->Attach(holder->root_buffer.get());
  shell_surface->host_window()->SetProperty(kBuilderResourceHolderKey, holder);

  if (parent_shell_surface_)
    shell_surface->SetParent(parent_shell_surface_);

  if (disable_movement_)
    shell_surface->DisableMovement();

  if (commit_on_build_) {
    holder->root_surface->Commit();
    if (centered_)
      ash::CenterWindow(shell_surface->GetWidget()->GetNativeWindow());
  } else {
    // 'SetCentered' requires its shell surface to be committed when creatted.
    DCHECK(!centered_);
  }

  return shell_surface;
}

}  // namespace test
}  // namespace exo
