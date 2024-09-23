// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/test/shell_surface_builder.h"

#include <tuple>

#include "ash/wm/desks/desks_util.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "components/exo/buffer.h"
#include "components/exo/display.h"
#include "components/exo/security_delegate.h"
#include "components/exo/sub_surface.h"
#include "components/exo/surface.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/test/test_security_delegate.h"
#include "components/exo/xdg_shell_surface.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "ui/aura/env.h"
#include "ui/display/types/display_constants.h"

namespace {

// Internal structure that owns buffer, surface and subsurface instances.
// This is owned by the host window as an owned property.
struct Holder {
  raw_ptr<exo::Surface, DanglingUntriaged> root_surface = nullptr;
  std::vector<std::tuple<std::unique_ptr<exo::Buffer>,
                         std::unique_ptr<exo::Surface>,
                         std::unique_ptr<exo::SubSurface>>>
      sub_surfaces;
  std::unique_ptr<exo::SecurityDelegate> security_delegate_;

  void AddRootSurface(const gfx::Size& size,
                      std::optional<gfx::BufferFormat> buffer_format) {
    auto surface = std::make_unique<exo::Surface>();
    std::unique_ptr<exo::Buffer> buffer;
    if (!size.IsEmpty() && buffer_format) {
      buffer = exo::test::ExoTestHelper::CreateBuffer(size, *buffer_format);
      surface->Attach(buffer.get());
    }
    root_surface = surface.get();
    sub_surfaces.push_back(
        std::make_tuple<>(std::move(buffer), std::move(surface), nullptr));
  }

  exo::Surface* AddChildSurface(exo::Surface* parent, const gfx::Rect& bounds) {
    auto buffer = exo::test::ExoTestHelper::CreateBuffer(bounds.size());
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

  exo::SecurityDelegate* CreateTestSecurityDelegate() {
    security_delegate_ = std::make_unique<exo::test::TestSecurityDelegate>();
    return security_delegate_.get();
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

ShellSurfaceBuilder& ShellSurfaceBuilder::SetUseSystemModalContainer() {
  DCHECK(!built_);
  use_system_modal_container_ = true;
  return *this;
}

ShellSurfaceBuilder& ShellSurfaceBuilder::EnableSystemModal() {
  DCHECK(!built_);
  system_modal_ = true;
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

ShellSurfaceBuilder& ShellSurfaceBuilder::SetCanMaximize(bool can_maximize) {
  DCHECK(!built_);
  can_maximize_ = can_maximize;
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

ShellSurfaceBuilder& ShellSurfaceBuilder::SetGeometry(
    const gfx::Rect& geometry) {
  DCHECK(!built_);
  geometry_ = geometry;
  return *this;
}

ShellSurfaceBuilder& ShellSurfaceBuilder::SetInputRegion(
    const cc::Region& region) {
  DCHECK(!built_);
  input_region_ = region;
  return *this;
}

ShellSurfaceBuilder& ShellSurfaceBuilder::SetFrame(SurfaceFrameType type) {
  DCHECK(!built_);
  type_ = type;
  return *this;
}

ShellSurfaceBuilder& ShellSurfaceBuilder::SetFrameColors(SkColor active,
                                                         SkColor inactive) {
  DCHECK(!built_);
  active_frame_color_ = active;
  inactive_frame_color_ = inactive;
  return *this;
}

ShellSurfaceBuilder& ShellSurfaceBuilder::SetApplicationId(
    const std::string& application_id) {
  DCHECK(!built_);
  application_id_ = application_id;
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

ShellSurfaceBuilder& ShellSurfaceBuilder::SetSecurityDelegate(
    SecurityDelegate* security_delegate) {
  DCHECK(!built_);
  security_delegate_ = security_delegate;
  return *this;
}

ShellSurfaceBuilder& ShellSurfaceBuilder::SetAppType(
    chromeos::AppType app_type) {
  DCHECK(!built_);
  app_type_ = app_type;
  return *this;
}

ShellSurfaceBuilder& ShellSurfaceBuilder::SetParent(ShellSurface* parent) {
  DCHECK(!built_);
  parent_shell_surface_ = parent;
  return *this;
}

ShellSurfaceBuilder& ShellSurfaceBuilder::SetAsPopup() {
  DCHECK(!built_);
  popup_ = true;
  return *this;
}

ShellSurfaceBuilder& ShellSurfaceBuilder::SetAsMenu() {
  DCHECK(!built_);
  menu_ = true;
  return SetAsPopup();
}

ShellSurfaceBuilder& ShellSurfaceBuilder::SetGrab() {
  DCHECK(!built_);
  grab_ = true;
  return *this;
}

ShellSurfaceBuilder& ShellSurfaceBuilder::SetClientSubmitsInPixelCoordinates(
    bool enabled) {
  DCHECK(!built_);
  client_submits_surfaces_in_pixel_coordinates_ = enabled;
  return *this;
}

ShellSurfaceBuilder& ShellSurfaceBuilder::SetWindowState(
    chromeos::WindowStateType window_state) {
  DCHECK(!built_);
  window_state_ = window_state;
  return *this;
}

ShellSurfaceBuilder& ShellSurfaceBuilder::EnableDefaultScaleCancellation() {
  DCHECK(!built_);
  default_scale_cancellation_ = true;
  return *this;
}

ShellSurfaceBuilder& ShellSurfaceBuilder::SetDelegate(
    std::unique_ptr<ClientControlledShellSurface::Delegate> delegate) {
  DCHECK(!built_);
  delegate_ = std::move(delegate);
  return *this;
}

ShellSurfaceBuilder& ShellSurfaceBuilder::DisableSupportsFloatedState() {
  DCHECK(!built_);
  supports_floated_state_ = false;
  return *this;
}

ShellSurfaceBuilder& ShellSurfaceBuilder::SetDisplayId(int64_t display_id) {
  DCHECK(!built_);
  DCHECK_NE(display_id, display::kInvalidDisplayId);
  display_id_ = display_id;
  return *this;
}

ShellSurfaceBuilder& ShellSurfaceBuilder::SetBounds(const gfx::Rect& bounds) {
  DCHECK(!built_);
  bounds_.emplace(bounds);
  return *this;
}

ShellSurfaceBuilder& ShellSurfaceBuilder::SetConfigureCallback(
    ShellSurface::ConfigureCallback configure_callback) {
  configure_callback_ = configure_callback;
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
  // Create a ShellSurface instance.
  DCHECK(!built_);
  DCHECK(IsConfigurationValidForShellSurface());
  built_ = true;

  auto holder = std::make_unique<Holder>();
  holder->AddRootSurface(root_buffer_size_, root_buffer_format_);
  auto shell_surface = std::make_unique<XdgShellSurface>(
      holder->root_surface, origin_, can_minimize_, GetContainer());

  if (!configure_callback_.is_null()) {
    shell_surface->set_configure_callback(configure_callback_);
  }

  shell_surface->host_window()->SetProperty(kBuilderResourceHolderKey,
                                            std::move(holder));

  // Set the properties specific to ShellSurface.
  if (parent_shell_surface_)
    shell_surface->SetParent(parent_shell_surface_);
  if (popup_)
    shell_surface->SetPopup();
  if (menu_)
    shell_surface->SetMenu();
  if (grab_) {
    shell_surface->Grab();
  }
  if (client_submits_surfaces_in_pixel_coordinates_.has_value()) {
    shell_surface->set_client_submits_surfaces_in_pixel_coordinates(
        client_submits_surfaces_in_pixel_coordinates_.value());
  }

  if (window_state_.has_value()) {
    switch (window_state_.value()) {
      case chromeos::WindowStateType::kDefault:
      case chromeos::WindowStateType::kNormal:
        shell_surface->Restore();
        break;
      case chromeos::WindowStateType::kMaximized:
        shell_surface->Maximize();
        break;
      case chromeos::WindowStateType::kMinimized:
        shell_surface->Minimize();
        break;
      case chromeos::WindowStateType::kFullscreen:
        shell_surface->SetFullscreen(/*fullscreen=*/true,
                                     /*display_id=*/display::kInvalidDisplayId);
        break;
      default:
        // Other states are not supported as initial state in ShellSurface.
        NOTREACHED_IN_MIGRATION();
    }
  }

  SetCommonPropertiesAndCommitIfNecessary(shell_surface.get());

  // The widget becomes available after the first commit.
  if (shell_surface->GetWidget() && app_type_ != chromeos::AppType::NON_APP) {
    shell_surface->GetWidget()->GetNativeWindow()->SetProperty(
        chromeos::kAppTypeKey, app_type_);
  }
  return shell_surface;
}

std::unique_ptr<ClientControlledShellSurface>
ShellSurfaceBuilder::BuildClientControlledShellSurface() {
  // Create a ClientControlledShellSurface instance.
  DCHECK(!built_);
  DCHECK(IsConfigurationValidForClientControlledShellSurface());
  built_ = true;
  auto holder = std::make_unique<Holder>();
  holder->AddRootSurface(root_buffer_size_, root_buffer_format_);
  auto shell_surface = Display().CreateOrGetClientControlledShellSurface(
      holder->root_surface, GetContainer(), default_scale_cancellation_,
      supports_floated_state_);
  shell_surface->host_window()->SetProperty(kBuilderResourceHolderKey,
                                            std::move(holder));

  // Set the properties specific to ClientControlledShellSurface.
  shell_surface->SetApplicationId(!application_id_.empty()
                                      ? application_id_.c_str()
                                      : "org.chromium.arc.1");
  // ARC's default min size is non-empty.
  if (!min_size_.has_value())
    shell_surface->SetMinimumSize(gfx::Size(1, 1));
  if (delegate_) {
    shell_surface->set_delegate(std::move(delegate_));
  } else {
    shell_surface->set_delegate(
        std::make_unique<ClientControlledShellSurfaceDelegate>(
            shell_surface.get()));
  }

  if (window_state_.has_value()) {
    switch (window_state_.value()) {
      case chromeos::WindowStateType::kDefault:
      case chromeos::WindowStateType::kNormal:
        shell_surface->SetRestored();
        break;
      case chromeos::WindowStateType::kMaximized:
        shell_surface->SetMaximized();
        break;
      case chromeos::WindowStateType::kMinimized:
        shell_surface->SetMinimized();
        break;
      case chromeos::WindowStateType::kFullscreen:
        shell_surface->SetFullscreen(/*fullscreen=*/true,
                                     /*display_id=*/display::kInvalidDisplayId);
        break;
      case chromeos::WindowStateType::kPrimarySnapped:
        shell_surface->SetSnapPrimary(chromeos::kDefaultSnapRatio);
        break;
      case chromeos::WindowStateType::kSecondarySnapped:
        shell_surface->SetSnapSecondary(chromeos::kDefaultSnapRatio);
        break;
      case chromeos::WindowStateType::kPip:
        shell_surface->SetPip();
        break;
      default:
        NOTREACHED_IN_MIGRATION();
    }
  }

  SetCommonPropertiesAndCommitIfNecessary(shell_surface.get());

  // The widget becomes available after the first commit.
  if (shell_surface->GetWidget()) {
    CHECK(app_type_ == chromeos::AppType::NON_APP ||
          app_type_ == chromeos::AppType::ARC_APP)
        << "Incompatible app type is set for ClientControlledShellSurface.";
    shell_surface->GetWidget()->GetNativeWindow()->SetProperty(
        chromeos::kAppTypeKey, chromeos::AppType::ARC_APP);
  }

  shell_surface->SetCanMaximize(can_maximize_);

  return shell_surface;
}

bool ShellSurfaceBuilder::IsConfigurationValidForShellSurface() {
  return !default_scale_cancellation_ && !delegate_;
}

bool ShellSurfaceBuilder::
    IsConfigurationValidForClientControlledShellSurface() {
  return !parent_shell_surface_ && !popup_;
}

void ShellSurfaceBuilder::SetCommonPropertiesAndCommitIfNecessary(
    ShellSurfaceBase* shell_surface) {
  if (display_id_ != display::kInvalidDisplayId) {
    shell_surface->SetDisplay(display_id_);
  }

  if (bounds_) {
    shell_surface->SetWindowBounds(*bounds_);
  }

  if (disable_movement_)
    shell_surface->DisableMovement();

  if (max_size_.has_value())
    shell_surface->SetMaximumSize(max_size_.value());

  if (min_size_.has_value())
    shell_surface->SetMinimumSize(min_size_.value());

  if (geometry_.has_value())
    shell_surface->SetGeometry(geometry_.value());

  if (input_region_.has_value()) {
    shell_surface->root_surface()->SetInputRegion(input_region_.value());
  }

  if (type_.has_value()) {
    shell_surface->root_surface()->SetFrame(type_.value());
  }

  if (active_frame_color_.has_value()) {
    shell_surface->root_surface()->SetFrameColors(
        active_frame_color_.value(), inactive_frame_color_.value());
  }

  if (system_modal_) {
    shell_surface->SetSystemModal(true);
  }

  if (security_delegate_) {
    shell_surface->SetSecurityDelegate(security_delegate_);
  } else {
    auto* holder =
        shell_surface->host_window()->GetProperty(kBuilderResourceHolderKey);
    shell_surface->SetSecurityDelegate(holder->CreateTestSecurityDelegate());
  }

  if (commit_on_build_) {
    shell_surface->root_surface()->Commit();
    if (centered_) {
      auto* window = shell_surface->GetWidget()->GetNativeWindow();
      const display::Display display =
          display::Screen::GetScreen()->GetDisplayNearestWindow(window);
      gfx::Rect center_bounds = display.work_area();
      center_bounds.ClampToCenteredSize(window->bounds().size());
      window->SetBoundsInScreen(center_bounds, display);
    }
  } else {
    // 'SetCentered' requires its shell surface to be committed when creatted.
    DCHECK(!centered_);
  }
}

int ShellSurfaceBuilder::GetContainer() {
  return use_system_modal_container_
             ? ash::kShellWindowId_SystemModalContainer
             : ash::desks_util::GetActiveDeskContainerId();
}

}  // namespace test
}  // namespace exo
