// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_DISPLAY_H_
#define COMPONENTS_EXO_DISPLAY_H_

#include <stddef.h>

#include <memory>
#include <string>

#include "base/files/scoped_file.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "components/exo/seat.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_pixmap_handle.h"

namespace gfx {
class ClientNativePixmapFactory;
}

namespace exo {
class Buffer;
class ClientControlledShellSurface;
class DataDevice;
class DataDeviceDelegate;
class DataExchangeDelegate;
class InputMethodSurfaceManager;
class InputMethodSurface;
class NotificationSurface;
class NotificationSurfaceManager;
class SharedMemory;
class ShellSurface;
class SubSurface;
class Surface;
class ToastSurface;
class ToastSurfaceManager;
class XdgShellSurface;

// The core display class. This class provides functions for creating surfaces
// and is in charge of combining the contents of multiple surfaces into one
// displayable output.
class Display {
 public:
  Display();
  explicit Display(
      std::unique_ptr<DataExchangeDelegate> data_exchange_delegate);

  Display(
      std::unique_ptr<NotificationSurfaceManager> notification_surface_manager,
      std::unique_ptr<InputMethodSurfaceManager> input_method_surface_manager,
      std::unique_ptr<ToastSurfaceManager> toast_surface_manager,
      std::unique_ptr<DataExchangeDelegate> data_exchange_delegate);

  Display(const Display&) = delete;
  Display& operator=(const Display&) = delete;

  ~Display();

  void Shutdown();

  // Creates a new surface.
  std::unique_ptr<Surface> CreateSurface();

  // Creates a shared memory segment from |shared_memory_region|. This function
  // takes ownership of the region.
  std::unique_ptr<SharedMemory> CreateSharedMemory(
      base::UnsafeSharedMemoryRegion shared_memory_region);

  // Creates a buffer for a Linux DMA-buf file descriptor.
  std::unique_ptr<Buffer> CreateLinuxDMABufBuffer(
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::NativePixmapHandle handle,
      bool y_invert);

  // Creates a shell surface for an existing surface.
  std::unique_ptr<ShellSurface> CreateShellSurface(Surface* surface);

  // Creates a xdg shell surface for an existing surface.
  std::unique_ptr<XdgShellSurface> CreateXdgShellSurface(Surface* surface);

  // Returns a remote shell surface for an existing surface using |container|.
  // If the existing surface has window session id associated, the remote shell
  // will be get from PropertyResolver. Or it will create a new remote shell.
  std::unique_ptr<ClientControlledShellSurface>
  CreateOrGetClientControlledShellSurface(Surface* surface,
                                          int container,
                                          bool default_scale_cancellation,
                                          bool supports_floated_state);

  // Creates a notification surface for a surface and notification id.
  std::unique_ptr<NotificationSurface> CreateNotificationSurface(
      Surface* surface,
      const std::string& notification_key);

  // Creates a input method surface for a surface.
  std::unique_ptr<InputMethodSurface> CreateInputMethodSurface(
      Surface* surface,
      bool default_scale_cancellation);

  // Creates a toast surface for a surface.
  std::unique_ptr<ToastSurface> CreateToastSurface(
      Surface* surface,
      bool default_scale_cancellation);

  // Creates a sub-surface for an existing surface. The sub-surface will be
  // a child of |parent|.
  std::unique_ptr<SubSurface> CreateSubSurface(Surface* surface,
                                               Surface* parent);

  // Creates a data device for a |delegate|.
  std::unique_ptr<DataDevice> CreateDataDevice(DataDeviceDelegate* delegate);

  // Obtains seat instance.
  Seat* seat() { return &seat_; }

  InputMethodSurfaceManager* input_method_surface_manager() {
    return input_method_surface_manager_.get();
  }

 private:
  std::unique_ptr<NotificationSurfaceManager> notification_surface_manager_;
  std::unique_ptr<InputMethodSurfaceManager> input_method_surface_manager_;
  std::unique_ptr<ToastSurfaceManager> toast_surface_manager_;

  Seat seat_;

  bool shutdown_ = false;

  std::unique_ptr<gfx::ClientNativePixmapFactory> client_native_pixmap_factory_;
};

}  // namespace exo

#endif  // COMPONENTS_EXO_DISPLAY_H_
