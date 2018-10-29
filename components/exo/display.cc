// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/display.h"

#include <iterator>
#include <utility>

#include "ash/public/cpp/shell_window_ids.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "components/exo/client_controlled_shell_surface.h"
#include "components/exo/data_device.h"
#include "components/exo/file_helper.h"
#include "components/exo/input_method_surface.h"
#include "components/exo/input_method_surface_manager.h"
#include "components/exo/notification_surface.h"
#include "components/exo/notification_surface_manager.h"
#include "components/exo/shared_memory.h"
#include "components/exo/shell_surface.h"
#include "components/exo/sub_surface.h"
#include "components/exo/surface.h"
#include "components/exo/xdg_shell_surface.h"
#include "ui/gfx/linux/client_native_pixmap_factory_dmabuf.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

#if defined(USE_OZONE)
#include <GLES2/gl2extchromium.h>
#include "components/exo/buffer.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl_native_pixmap.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "ui/ozone/public/ozone_switches.h"
#endif

namespace exo {

////////////////////////////////////////////////////////////////////////////////
// Display, public:

Display::Display() : Display(nullptr, nullptr, std::unique_ptr<FileHelper>()) {}

Display::Display(NotificationSurfaceManager* notification_surface_manager,
                 InputMethodSurfaceManager* input_method_surface_manager,
                 std::unique_ptr<FileHelper> file_helper)
    : notification_surface_manager_(notification_surface_manager),
      input_method_surface_manager_(input_method_surface_manager),
      file_helper_(std::move(file_helper))
#if defined(USE_OZONE)
      ,
      client_native_pixmap_factory_(
          gfx::CreateClientNativePixmapFactoryDmabuf())
#endif
{
}

Display::~Display() {}

std::unique_ptr<Surface> Display::CreateSurface() {
  TRACE_EVENT0("exo", "Display::CreateSurface");

  return base::WrapUnique(new Surface);
}

std::unique_ptr<SharedMemory> Display::CreateSharedMemory(
    base::UnsafeSharedMemoryRegion shared_memory_region) {
  TRACE_EVENT1("exo", "Display::CreateSharedMemory", "size",
               shared_memory_region.GetSize());

  if (!shared_memory_region.IsValid())
    return nullptr;

  return std::make_unique<SharedMemory>(std::move(shared_memory_region));
}

#if defined(USE_OZONE)
std::unique_ptr<Buffer> Display::CreateLinuxDMABufBuffer(
    const gfx::Size& size,
    gfx::BufferFormat format,
    const std::vector<gfx::NativePixmapPlane>& planes,
    std::vector<base::ScopedFD>&& fds) {
  TRACE_EVENT1("exo", "Display::CreateLinuxDMABufBuffer", "size",
               size.ToString());

  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::NATIVE_PIXMAP;
  for (auto& fd : fds)
    handle.native_pixmap_handle.fds.emplace_back(std::move(fd));

  for (auto& plane : planes)
    handle.native_pixmap_handle.planes.push_back(plane);

  std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer =
      gpu::GpuMemoryBufferImplNativePixmap::CreateFromHandle(
          client_native_pixmap_factory_.get(), handle, size, format,
          gfx::BufferUsage::GPU_READ,
          gpu::GpuMemoryBufferImpl::DestructionCallback());
  if (!gpu_memory_buffer) {
    LOG(ERROR) << "Failed to create GpuMemoryBuffer from handle";
    return nullptr;
  }

  // Using zero-copy for optimal performance.
  bool use_zero_copy = true;

  // TODO(dcastagna): Re-enable NV12 format as HW overlay once b/113362843
  // is addressed.
  bool is_overlay_candidate = format != gfx::BufferFormat::YUV_420_BIPLANAR;

  return std::make_unique<Buffer>(
      std::move(gpu_memory_buffer), GL_TEXTURE_EXTERNAL_OES,
      // COMMANDS_COMPLETED queries are required by native pixmaps.
      GL_COMMANDS_COMPLETED_CHROMIUM, use_zero_copy, is_overlay_candidate);
}
#endif

std::unique_ptr<ShellSurface> Display::CreateShellSurface(Surface* surface) {
  TRACE_EVENT1("exo", "Display::CreateShellSurface", "surface",
               surface->AsTracedValue());
  if (surface->HasSurfaceDelegate()) {
    DLOG(ERROR) << "Surface has already been assigned a role";
    return nullptr;
  }

  return std::make_unique<ShellSurface>(
      surface, gfx::Point(), true /* activatable */, false /* can_minimize */,
      ash::kShellWindowId_DefaultContainer);
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
      surface, gfx::Point(), true /* activatable */, false /* can_minimize */,
      ash::kShellWindowId_DefaultContainer);
}

std::unique_ptr<ClientControlledShellSurface>
Display::CreateClientControlledShellSurface(
    Surface* surface,
    int container,
    double default_device_scale_factor) {
  TRACE_EVENT2("exo", "Display::CreateRemoteShellSurface", "surface",
               surface->AsTracedValue(), "container", container);

  if (surface->HasSurfaceDelegate()) {
    DLOG(ERROR) << "Surface has already been assigned a role";
    return nullptr;
  }

  // Remote shell surfaces in system modal container cannot be minimized.
  bool can_minimize = container != ash::kShellWindowId_SystemModalContainer;

  std::unique_ptr<ClientControlledShellSurface> shell_surface(
      std::make_unique<ClientControlledShellSurface>(surface, can_minimize,
                                                     container));
  DCHECK_GE(default_device_scale_factor, 1.0);
  shell_surface->SetScale(default_device_scale_factor);
  return shell_surface;
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

  return std::make_unique<NotificationSurface>(notification_surface_manager_,
                                               surface, notification_key);
}

std::unique_ptr<DataDevice> Display::CreateDataDevice(
    DataDeviceDelegate* delegate) {
  return std::make_unique<DataDevice>(delegate, &seat_, file_helper_.get());
}

std::unique_ptr<InputMethodSurface> Display::CreateInputMethodSurface(
    Surface* surface,
    double default_device_scale_factor) {
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

  return std::make_unique<InputMethodSurface>(
      input_method_surface_manager_, surface, default_device_scale_factor);
}

}  // namespace exo
