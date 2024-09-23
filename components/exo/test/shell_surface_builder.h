// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_TEST_SHELL_SURFACE_BUILDER_H_
#define COMPONENTS_EXO_TEST_SHELL_SURFACE_BUILDER_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "cc/base/region.h"
#include "chromeos/ui/base/app_types.h"
#include "components/exo/client_controlled_shell_surface.h"
#include "components/exo/shell_surface.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace exo {
class ClientControlledShellSurface;
class SecurityDelegate;
class ShellSurface;
class ShellSurfaceBase;
class Surface;

namespace test {

// A builder to create a ShellSurface or ClientControlledShellSurface for
// testing purpose. Its surface and buffer, which are typically owned by a
// client, are owned by the host window as an owned property, therefore are
// destroyed when the shell surface is destroyed.
class ShellSurfaceBuilder {
 public:
  explicit ShellSurfaceBuilder(const gfx::Size& buffer_size = {0, 0});
  ShellSurfaceBuilder(ShellSurfaceBuilder& other) = delete;
  ShellSurfaceBuilder& operator=(ShellSurfaceBuilder& other) = delete;
  ~ShellSurfaceBuilder();

  // Sets parameters common for all ShellSurfaceType.
  ShellSurfaceBuilder& SetNoRootBuffer();
  ShellSurfaceBuilder& SetRootBufferFormat(gfx::BufferFormat buffer_format);
  ShellSurfaceBuilder& SetOrigin(const gfx::Point& origin);
  ShellSurfaceBuilder& SetUseSystemModalContainer();
  ShellSurfaceBuilder& EnableSystemModal();
  // When set true, some properties such as kAppType may not be set by this
  // builder as they need the widget created in the commit process.
  ShellSurfaceBuilder& SetNoCommit();
  ShellSurfaceBuilder& SetCanMinimize(bool can_minimize);
  ShellSurfaceBuilder& SetCanMaximize(bool can_maximize);
  ShellSurfaceBuilder& SetMaximumSize(const gfx::Size& size);
  ShellSurfaceBuilder& SetMinimumSize(const gfx::Size& size);
  ShellSurfaceBuilder& SetGeometry(const gfx::Rect& geometry);
  ShellSurfaceBuilder& SetInputRegion(const cc::Region& region);
  ShellSurfaceBuilder& SetFrame(SurfaceFrameType type);
  ShellSurfaceBuilder& SetFrameColors(SkColor active, SkColor inactive);
  ShellSurfaceBuilder& SetApplicationId(const std::string& application_id);
  ShellSurfaceBuilder& SetDisableMovement();
  ShellSurfaceBuilder& SetCentered();
  ShellSurfaceBuilder& SetSecurityDelegate(SecurityDelegate* security_delegate);
  ShellSurfaceBuilder& SetAppType(chromeos::AppType app_type);

  // Sets parameters defined in ShellSurface.
  ShellSurfaceBuilder& SetParent(ShellSurface* shell_surface);
  ShellSurfaceBuilder& SetAsPopup();
  ShellSurfaceBuilder& SetAsMenu();
  ShellSurfaceBuilder& SetGrab();
  ShellSurfaceBuilder& SetClientSubmitsInPixelCoordinates(bool enabled);
  ShellSurfaceBuilder& SetConfigureCallback(
      ShellSurface::ConfigureCallback callback);

  // Sets parameters defined in ClientControlledShellSurface.
  ShellSurfaceBuilder& SetWindowState(chromeos::WindowStateType window_state);
  ShellSurfaceBuilder& EnableDefaultScaleCancellation();
  ShellSurfaceBuilder& SetDelegate(
      std::unique_ptr<ClientControlledShellSurface::Delegate> delegate);
  ShellSurfaceBuilder& DisableSupportsFloatedState();
  ShellSurfaceBuilder& SetDisplayId(int64_t display_id);
  ShellSurfaceBuilder& SetBounds(const gfx::Rect& bounds);

  // Must be called only once for either of the below and the object cannot
  // be used to create multiple windows.
  [[nodiscard]] std::unique_ptr<ShellSurface> BuildShellSurface();
  [[nodiscard]] std::unique_ptr<ClientControlledShellSurface>
  BuildClientControlledShellSurface();

  // Destroy's the root surface of the given 'shell_surface'.
  static void DestroyRootSurface(ShellSurfaceBase* shell_surface);
  static Surface* AddChildSurface(Surface* parent_surface,
                                  const gfx::Rect& bounds);

 private:
  bool IsConfigurationValidForShellSurface();
  bool IsConfigurationValidForClientControlledShellSurface();
  void SetCommonPropertiesAndCommitIfNecessary(ShellSurfaceBase* shell_surface);
  int GetContainer();

  gfx::Size root_buffer_size_;
  std::optional<gfx::BufferFormat> root_buffer_format_ =
      gfx::BufferFormat::RGBA_8888;
  gfx::Point origin_;

  std::optional<gfx::Size> max_size_;
  std::optional<gfx::Size> min_size_;
  std::optional<gfx::Rect> geometry_;
  std::optional<cc::Region> input_region_;
  std::optional<SurfaceFrameType> type_;
  std::optional<SkColor> active_frame_color_;
  std::optional<SkColor> inactive_frame_color_;

  raw_ptr<SecurityDelegate> security_delegate_ = nullptr;
  chromeos::AppType app_type_ = chromeos::AppType::NON_APP;
  std::string application_id_;
  bool use_system_modal_container_ = false;
  bool system_modal_ = false;
  bool commit_on_build_ = true;
  bool can_minimize_ = true;
  bool can_maximize_ = true;
  bool disable_movement_ = false;
  bool centered_ = false;
  bool built_ = false;
  int64_t display_id_ = display::kInvalidDisplayId;
  std::optional<gfx::Rect> bounds_;

  // ShellSurface-specific parameters.
  raw_ptr<ShellSurface> parent_shell_surface_ = nullptr;
  bool popup_ = false;
  bool menu_ = false;
  bool grab_ = false;
  std::optional<bool> client_submits_surfaces_in_pixel_coordinates_;
  ShellSurface::ConfigureCallback configure_callback_;

  // ClientControlledShellSurface-specific parameters.
  std::optional<chromeos::WindowStateType> window_state_;
  bool default_scale_cancellation_ = false;
  std::unique_ptr<ClientControlledShellSurface::Delegate> delegate_;
  bool supports_floated_state_ = true;
};

}  // namespace test
}  // namespace exo

#endif  // COMPONENTS_EXO_TEST_SHELL_SURFACE_BUILDER_H_
