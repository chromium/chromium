// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/display.h"

#include <GLES2/gl2extchromium.h>
#include <iterator>
#include <memory>
#include <utility>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/wm/desks/desks_util.h"
#include "base/command_line.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "build/build_config.h"
#include "components/exo/buffer.h"
#include "components/exo/client_controlled_shell_surface.h"
#include "components/exo/data_device.h"
#include "components/exo/data_exchange_delegate.h"
#include "components/exo/input_method_surface.h"
#include "components/exo/input_method_surface_manager.h"
#include "components/exo/notification_surface.h"
#include "components/exo/notification_surface_manager.h"
#include "components/exo/shared_memory.h"
#include "components/exo/shell_surface.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/sub_surface.h"
#include "components/exo/surface.h"
#include "components/exo/toast_surface.h"
#include "components/exo/toast_surface_manager.h"
#include "components/exo/xdg_shell_surface.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl_native_pixmap.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "ui/gfx/linux/client_native_pixmap_factory_dmabuf.h"
#include "ui/ozone/public/ozone_switches.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace exo {

////////////////////////////////////////////////////////////////////////////////
// Display, public:

Display::Display(std::unique_ptr<DataExchangeDelegate> data_exchange_delegate)
    : seat_(std::move(data_exchange_delegate)),
      client_native_pixmap_factory_(
          gfx::CreateClientNativePixmapFactoryDmabuf()) {}

Display::Display() : Display(std::unique_ptr<DataExchangeDelegate>(nullptr)) {}

Display::Display(
    std::unique_ptr<NotificationSurfaceManager> notification_surface_manager,
    std::unique_ptr<InputMethodSurfaceManager> input_method_surface_manager,
    std::unique_ptr<ToastSurfaceManager> toast_surface_manager,
    std::unique_ptr<DataExchangeDelegate> data_exchange_delegate)
    : notification_surface_manager_(std::move(notification_surface_manager)),
      input_method_surface_manager_(std::move(input_method_surface_manager)),
      toast_surface_manager_(std::move(toast_surface_manager)),
      seat_(std::move(data_exchange_delegate)),
      client_native_pixmap_factory_(
          gfx::CreateClientNativePixmapFactoryDmabuf()) {}

Display::~Display() {
  Shutdown();
}

void Display::Shutdown() {
  if (shutdown_)
    return;
  shutdown_ = true;
  seat_.Shutdown();
}

std::unique_ptr<Surface> Display::CreateSurface() {
  TRACE_EVENT0("exo", "Display::CreateSurface");
  return std::make_unique<Surface>();
}

std::unique_ptr<SharedMemory> Display::CreateSharedMemory(
    base::UnsafeSharedMemoryRegion shared_memory_region) {
  TRACE_EVENT1("exo", "Display::CreateSharedMemory", "size",
               shared_memory_region.GetSize());

  if (!shared_memory_region.IsValid())
    return nullptr;

  return std::make_unique<SharedMemory>(std::move(shared_memory_region));
}

std::unique_ptr<Buffer> Display::CreateLinuxDMABufBuffer(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::NativePixmapHandle handle,
    bool y_invert) {
  TRACE_EVENT1("exo", "Display::CreateLinuxDMABufBuffer", "size",
               size.ToString());

  gfx::GpuMemoryBufferHandle gmb_handle;
  gmb_handle.type = gfx::NATIVE_PIXMAP;
  gmb_handle.native_pixmap_handle = std::move(handle);

  const gfx::BufferUsage buffer_usage = gfx::BufferUsage::GPU_READ;

  // COMMANDS_COMPLETED queries are required by native pixmaps.
  const unsigned query_type = GL_COMMANDS_COMPLETED_CHROMIUM;

  // Using zero-copy for optimal performance.
  const bool use_zero_copy = true;
  const bool is_overlay_candidate = true;

  return Buffer::CreateBufferFromGMBHandle(
      std::move(gmb_handle), size, format, buffer_usage, query_type,
      use_zero_copy, is_overlay_candidate, y_invert);
}

std::unique_ptr<ShellSurface> Display::CreateShellSurface(Surface* surface) {
  TRACE_EVENT1("exo", "Display::CreateShellSurface", "surface",
               surface->AsTracedValue());
  if (surface->HasSurfaceDelegate()) {
    DLOG(ERROR) << "Surface has already been assigned a role";
    return nullptr;
  }

  return std::make_unique<ShellSurface>(
      surface, gfx::Point(), /*can_minimize=*/false,
      ash::desks_util::GetActiveDeskContainerId());
}

std::unique_ptr<XdgShellSurface> Display::CreateXdgShellSurface(
    Surface* surface) {
  TRACE_EVENT1("exo", "Display::CreateXdgShellSurface", "surface",
               surface->AsTracedValue());
  if (surface->HasSurfaceDelegate()) {
    DLOG(ERROR) << "Surface has already been assigned a role";
    return nullptr;
  }

  return std::make_unique<XdgShellSurface>(
      surface, gfx::Point(), /*can_minimize=*/false,
      ash::desks_util::GetActiveDeskContainerId());
}

std::unique_ptr<ClientControlledShellSurface>
Display::CreateOrGetClientControlledShellSurface(
    Surface* surface,
    int container,
    bool default_scale_cancellation,
    bool supports_floated_state) {
  TRACE_EVENT2("exo", "Display::CreateRemoteShellSurface", "surface",
               surface->AsTracedValue(), "container", container);

  if (surface->HasSurfaceDelegate()) {
    DLOG(ERROR) << "Surface has already been assigned a role";
    return nullptr;
  }

  // Remote shell surfaces in system modal container cannot be minimized.
  bool can_minimize = container != ash::kShellWindowId_SystemModalContainer;

  std::unique_ptr<ClientControlledShellSurface> shell_surface;

  int window_session_id = surface->GetWindowSessionId();
  if (window_session_id > 0) {
    // Root surface has window session id, try get shell surface from external
    // source first.
    ui::PropertyHandler handler;
    WMHelper::AppPropertyResolver::Params params;
    params.window_session_id = window_session_id;
    WMHelper::GetInstance()->PopulateAppProperties(params, handler);
    shell_surface =
        base::WrapUnique(GetShellClientControlledShellSurface(&handler));
  }

  if (shell_surface) {
    shell_surface->RebindRootSurface(surface, can_minimize, container,
                                     default_scale_cancellation,
                                     supports_floated_state);
  } else {
    shell_surface = std::make_unique<ClientControlledShellSurface>(
        surface, can_minimize, container, default_scale_cancellation,
        supports_floated_state);
  }
  return shell_surface;
}

std::unique_ptr<NotificationSurface> Display::CreateNotificationSurface(
    Surface* surface,
    const std::string& notification_key) {
  TRACE_EVENT2("exo", "Display::CreateNotificationSurface", "surface",
               surface->AsTracedValue(), "notification_key", notification_key);

  if (!notification_surface_manager_ ||
      notification_surface_manager_->GetSurface(notification_key)) {
    DLOG(ERROR) << "Invalid notification key, key=" << notification_key;
    return nullptr;
  }

  return std::make_unique<NotificationSurface>(
      notification_surface_manager_.get(), surface, notification_key);
}

std::unique_ptr<InputMethodSurface> Display::CreateInputMethodSurface(
    Surface* surface,
    bool default_scale_cancellation) {
  TRACE_EVENT1("exo", "Display::CreateInputMethodSurface", "surface",
               surface->AsTracedValue());

  if (!input_method_surface_manager_) {
    DLOG(ERROR) << "Input method surface cannot be registered";
    return nullptr;
  }

  if (surface->HasSurfaceDelegate()) {
    DLOG(ERROR) << "Surface has already been assigned a role";
    return nullptr;
  }

  std::unique_ptr<InputMethodSurface> input_method_surface(
      std::make_unique<InputMethodSurface>(input_method_surface_manager_.get(),
                                           surface,
                                           default_scale_cancellation));
  return input_method_surface;
}

std::unique_ptr<ToastSurface> Display::CreateToastSurface(
    Surface* surface,
    bool default_scale_cancellation) {
  TRACE_EVENT1("exo", "Display::CreateToastSurface", "surface",
               surface->AsTracedValue());

  if (!toast_surface_manager_) {
    DLOG(ERROR) << "Toast surface cannot be registered";
    return nullptr;
  }

  if (surface->HasSurfaceDelegate()) {
    DLOG(ERROR) << "Surface has already been assigned a role";
    return nullptr;
  }

  std::unique_ptr<ToastSurface> toast_surface(std::make_unique<ToastSurface>(
      toast_surface_manager_.get(), surface, default_scale_cancellation));
  return toast_surface;
}

std::unique_ptr<SubSurface> Display::CreateSubSurface(Surface* surface,
                                                      Surface* parent) {
  TRACE_EVENT2("exo", "Display::CreateSubSurface", "surface",
               surface->AsTracedValue(), "parent", parent->AsTracedValue());

  if (surface->window()->Contains(parent->window())) {
    DLOG(ERROR) << "Parent is contained within surface's hierarchy";
    return nullptr;
  }

  if (surface->HasSurfaceDelegate()) {
    DLOG(ERROR) << "Surface has already been assigned a role";
    return nullptr;
  }

  return std::make_unique<SubSurface>(surface, parent);
}

std::unique_ptr<DataDevice> Display::CreateDataDevice(
    DataDeviceDelegate* delegate) {
  return std::make_unique<DataDevice>(delegate, seat());
}

}  // namespace exo
