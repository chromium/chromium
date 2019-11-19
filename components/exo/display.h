// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_DISPLAY_H_
#define COMPONENTS_EXO_DISPLAY_H_

#include <stddef.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "components/exo/seat.h"

#if defined(USE_OZONE)
#include "base/files/scoped_file.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_pixmap_handle.h"
#endif

namespace gfx {
class ClientNativePixmapFactory;
}

namespace exo {
class ClientControlledShellSurface;
class DataDevice;
class DataDeviceDelegate;
class FileHelper;
class InputMethodSurfaceManager;
class NotificationSurface;
class NotificationSurfaceManager;
class SharedMemory;
class SubSurface;
class Surface;

#if defined(OS_CHROMEOS)
class InputMethodSurface;
class ShellSurface;
class XdgShellSurface;
#endif

#if defined(USE_OZONE)
class Buffer;
#endif

// The core display class. This class provides functions for creating surfaces
// and is in charge of combining the contents of multiple surfaces into one
// displayable output.
class Display {
 public:
  Display();

#if defined(OS_CHROMEOS)
  Display(NotificationSurfaceManager* notification_surface_manager,
          InputMethodSurfaceManager* input_method_surface_manager,
          std::unique_ptr<FileHelper> file_helper);
#endif  // defined(OS_CHROMEOS)

  ~Display();

  // Creates a new surface.
  std::unique_ptr<Surface> CreateSurface();

  // Creates a shared memory segment from |shared_memory_region|. This function
  // takes ownership of the region.
  std::unique_ptr<SharedMemory> CreateSharedMemory(
      base::UnsafeSharedMemoryRegion shared_memory_region);

#if defined(USE_OZONE)
  // Creates a buffer for a Linux DMA-buf file descriptor.
  std::unique_ptr<Buffer> CreateLinuxDMABufBuffer(
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::NativePixmapHandle handle,
      bool y_invert);
#endif  // defined(USE_OZONE)

#if defined(OS_CHROMEOS)
  // Creates a shell surface for an existing surface.
  std::unique_ptr<ShellSurface> CreateShellSurface(Surface* surface);

  // Creates a xdg shell surface for an existing surface.
  std::unique_ptr<XdgShellSurface> CreateXdgShellSurface(Surface* surface);

  // Creates a remote shell surface for an existing surface using |container|.
  // The surface is scaled by 1 / |default_device_scale_factor|.
  std::unique_ptr<ClientControlledShellSurface>
  CreateClientControlledShellSurface(Surface* surface,
                                     int container,
                                     double default_device_scale_factor);

  // Creates a notification surface for a surface and notification id.
  std::unique_ptr<NotificationSurface> CreateNotificationSurface(
      Surface* surface,
      const std::string& notification_key);

  // Creates a input method surface for a surface.
  std::unique_ptr<InputMethodSurface> CreateInputMethodSurface(
      Surface* surface,
      double default_device_scale_factor);
#endif  // defined(OS_CHROMEOS)

  // Creates a sub-surface for an existing surface. The sub-surface will be
  // a child of |parent|.
  std::unique_ptr<SubSurface> CreateSubSurface(Surface* surface,
                                               Surface* parent);

  // Creates a data device for a |delegate|.
  std::unique_ptr<DataDevice> CreateDataDevice(DataDeviceDelegate* delegate);

  // Obtains seat instance.
  Seat* seat() { return &seat_; }

 private:
#if defined(OS_CHROMEOS)
  NotificationSurfaceManager* notification_surface_manager_ = nullptr;
  InputMethodSurfaceManager* input_method_surface_manager_ = nullptr;
#endif  // defined(OS_CHROMEOS)

  std::unique_ptr<FileHelper> file_helper_;
  Seat seat_;

#if defined(USE_OZONE)
  std::unique_ptr<gfx::ClientNativePixmapFactory> client_native_pixmap_factory_;
#endif  // defined(USE_OZONE)

  DISALLOW_COPY_AND_ASSIGN(Display);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_DISPLAY_H_
