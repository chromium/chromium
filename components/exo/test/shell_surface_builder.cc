// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/test/shell_surface_builder.h"

#include <tuple>

#include "ash/wm/desks/desks_util.h"
#include "ash/wm/window_positioning_utils.h"
#include "components/exo/buffer.h"
#include "components/exo/sub_surface.h"
#include "components/exo/surface.h"
#include "components/exo/test/exo_test_helper.h"
#include "components/exo/xdg_shell_surface.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "ui/aura/env.h"

#include "base/logging.h"

namespace {

// Internal structure that owns buffer, surface and subsurface instances.
// This is owned by the host window as an owned property.
struct Holder {
  exo::Surface* root_surface = nullptr;
  std::vector<std::tuple<std::unique_ptr<exo::Buffer>,
                         std::unique_ptr<exo::Surface>,
                         std::unique_ptr<exo::SubSurface>>>
      sub_surfaces;

  void AddRootSurface(const gfx::Size& size,
                      absl::optional<gfx::BufferFormat> buffer_format) {
    auto surface = std::make_unique<exo::Surface>();
    std::unique_ptr<exo::Buffer> buffer;
    if (buffer_format) {
      buffer = std::make_unique<exo::Buffer>(
          aura::Env::GetInstance()
              ->context_factory()
              ->GetGpuMemoryBufferManager()
              ->CreateGpuMemoryBuffer(size, *buffer_format,
                                      gfx::BufferUsage::GPU_READ,
                                      gpu::kNullSurfaceHandle, nullptr));
      surface->Attach(buffer.get());
    }
    root_surface = surface.get();
    sub_surfaces.push_back(
        std::make_tuple<>(std::move(buffer), std::move(surface), nullptr));
  }

  exo::Surface* AddChildSurface(exo::Surface* parent, const gfx::Rect& bounds) {
    auto buffer = std::make_unique<exo::Buffer>(
        aura::Env::GetInstance()
            ->context_factory()
            ->GetGpuMemoryBufferManager()
            ->CreateGpuMemoryBuffer(bounds.size(), gfx::BufferFormat::RGBA_8888,
                                    gfx::BufferUsage::GPU_READ,
                                    gpu::kNullSurfaceHandle, nullptr));

    auto surface = std::make_unique<exo::Surface>();
    surface->Attach(buffer.get());
    auto sub_surface = std::make_unique<exo::SubSurface>(surface.get(), parent);
    sub_surface->SetPosition(gfx::PointF(bounds.origin()));

    auto* surface_ptr = surface.get();
    sub_surfaces.push_back(std::make_tuple<>(
        std::move(buffer), std::move(surface), std::move(sub_surface)));
    return surface_ptr;
  }

  void DestroyRootSurface() {
    DCHECK(root_surface);
    for (auto& tuple : sub_surfaces) {
      if (std::get<1>(tuple).get() == root_surface) {
        std::get<0>(tuple).reset();
        std::get<1>(tuple).reset();
        std::get<2>(tuple).reset();
        break;
      }
    }
    root_surface = nullptr;
  }
};

}  // namespace

DEFINE_UI_CLASS_PROPERTY_TYPE(Holder*)

namespace exo {
namespace test {
namespace {

DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(Holder, kBuilderResourceHolderKey, nullptr)

Holder* FindHolder(Surface* surface) {
  aura::Window* window = surface->window();
  Holder* holder = window->GetProperty(kBuilderResourceHolderKey);
  while (!holder && window->parent()) {
    window = window->parent();
    holder = window->GetProperty(kBuilderResourceHolderKey);
  }
  return holder;
}

}  // namespace

ShellSurfaceBuilder::ShellSurfaceBuilder(const gfx::Size& buffer_size)
    : root_buffer_size_(buffer_size) {}

ShellSurfaceBuilder::~ShellSurfaceBuilder() = default;

ShellSurfaceBuilder& ShellSurfaceBuilder::SetNoRootBuffer() {
  DCHECK(!built_);
  root_buffer_format_.reset();
  return *this;
}

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

ShellSurfaceBuilder& ShellSurfaceBuilder::SetMaximumSize(
    const gfx::Size& size) {
  DCHECK(!built_);
  max_size_ = size;
  return *this;
}

ShellSurfaceBuilder& ShellSurfaceBuilder::SetMinimumSize(
    const gfx::Size& size) {
  DCHECK(!built_);
  min_size_ = size;
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

ShellSurfaceBuilder& ShellSurfaceBuilder::SetAsPopup() {
  DCHECK(!built_);
  popup_ = true;
  return *this;
}

// static
void ShellSurfaceBuilder::DestroyRootSurface(ShellSurfaceBase* shell_surface) {
  Holder* holder =
      shell_surface->host_window()->GetProperty(kBuilderResourceHolderKey);
  DCHECK(holder);
  holder->DestroyRootSurface();
}

// static
Surface* ShellSurfaceBuilder::AddChildSurface(Surface* parent,
                                              const gfx::Rect& bounds) {
  Holder* holder = FindHolder(parent);
  DCHECK(holder);
  return holder->AddChildSurface(parent, bounds);
}

std::unique_ptr<ShellSurface> ShellSurfaceBuilder::BuildShellSurface() {
  DCHECK(!built_);
  built_ = true;
  Holder* holder = new Holder();
  holder->AddRootSurface(root_buffer_size_, root_buffer_format_);

  int container = use_system_modal_container_
                      ? ash::kShellWindowId_SystemModalContainer
                      : ash::desks_util::GetActiveDeskContainerId();

  auto shell_surface = std::make_unique<ShellSurface>(
      holder->root_surface, origin_, can_minimize_, container);
  shell_surface->host_window()->SetProperty(kBuilderResourceHolderKey, holder);

  if (parent_shell_surface_)
    shell_surface->SetParent(parent_shell_surface_);

  if (disable_movement_)
    shell_surface->DisableMovement();

  if (!max_size_.IsEmpty())
    shell_surface->SetMaximumSize(max_size_);

  if (!min_size_.IsEmpty())
    shell_surface->SetMaximumSize(min_size_);

  if (popup_)
    shell_surface->SetPopup();

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
