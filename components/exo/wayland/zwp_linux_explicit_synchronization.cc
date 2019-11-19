// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/zwp_linux_explicit_synchronization.h"

#include <linux-explicit-synchronization-unstable-v1-server-protocol.h>
#include <sync/sync.h>

#include "components/exo/surface.h"
#include "components/exo/surface_observer.h"
#include "components/exo/wayland/server_util.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/gpu_fence_handle.h"

namespace exo {
namespace wayland {

namespace {

// A property key containing a pointer to the surface_synchronization resource
// associated with the surface object.
DEFINE_UI_CLASS_PROPERTY_KEY(wl_resource*,
                             kSurfaceSynchronizationResource,
                             nullptr)

////////////////////////////////////////////////////////////////////////////////
// linux_surface_synchronization_v1 interface:

// Implements the surface synchronization interface, providing explicit
// synchronization for surface buffers using dma-fences.
class LinuxSurfaceSynchronization : public SurfaceObserver {
 public:
  explicit LinuxSurfaceSynchronization(wl_resource* resource, Surface* surface)
      : surface_(surface) {
    surface_->AddSurfaceObserver(this);
    surface_->SetProperty(kSurfaceSynchronizationResource, resource);
  }
  ~LinuxSurfaceSynchronization() override {
    if (surface_) {
      surface_->RemoveSurfaceObserver(this);
      surface_->SetAcquireFence(nullptr);
      surface_->SetProperty(kSurfaceSynchronizationResource,
                            static_cast<wl_resource*>(nullptr));
    }
  }

  Surface* surface() { return surface_; }

  // Overridden from SurfaceObserver:
  void OnSurfaceDestroying(Surface* surface) override {
    surface->RemoveSurfaceObserver(this);
    surface_ = nullptr;
  }

 private:
  Surface* surface_;

  DISALLOW_COPY_AND_ASSIGN(LinuxSurfaceSynchronization);
};

void linux_surface_synchronization_destroy(struct wl_client* client,
                                           struct wl_resource* resource) {
  wl_resource_destroy(resource);
}

void linux_surface_synchronization_set_acquire_fence(wl_client* client,
                                                     wl_resource* resource,
                                                     int32_t fd) {
  auto fence_fd = base::ScopedFD(fd);
  auto* surface =
      GetUserDataAs<LinuxSurfaceSynchronization>(resource)->surface();

  if (!surface) {
    wl_resource_post_error(
        resource, ZWP_LINUX_SURFACE_SYNCHRONIZATION_V1_ERROR_NO_SURFACE,
        "Associated surface has been destroyed");
    return;
  }

  if (surface->HasPendingAcquireFence()) {
    wl_resource_post_error(
        resource, ZWP_LINUX_SURFACE_SYNCHRONIZATION_V1_ERROR_DUPLICATE_FENCE,
        "surface already has an acquire fence");
    return;
  }

  auto fence_info =
      std::unique_ptr<sync_fence_info_data, void (*)(sync_fence_info_data*)>{
          sync_fence_info(fence_fd.get()), sync_fence_info_free};
  if (!fence_info) {
    wl_resource_post_error(
        resource, ZWP_LINUX_SURFACE_SYNCHRONIZATION_V1_ERROR_INVALID_FENCE,
        "the provided acquire fence is invalid");
    return;
  }

  gfx::GpuFenceHandle handle;
  handle.type = gfx::GpuFenceHandleType::kAndroidNativeFenceSync;
  handle.native_fd = base::FileDescriptor(std::move(fence_fd));

  surface->SetAcquireFence(std::make_unique<gfx::GpuFence>(handle));
}

void linux_surface_synchronization_get_release(wl_client* client,
                                               wl_resource* resource,
                                               uint32_t id) {
  NOTIMPLEMENTED_LOG_ONCE();
}

const struct zwp_linux_surface_synchronization_v1_interface
    linux_surface_synchronization_implementation = {
        linux_surface_synchronization_destroy,
        linux_surface_synchronization_set_acquire_fence,
        linux_surface_synchronization_get_release,
};

////////////////////////////////////////////////////////////////////////////////
// linux_explicit_synchronization_v1 interface:

void linux_explicit_synchronization_destroy(struct wl_client* client,
                                            struct wl_resource* resource) {
  wl_resource_destroy(resource);
}

void linux_explicit_synchronization_get_synchronization(
    wl_client* client,
    wl_resource* resource,
    uint32_t id,
    wl_resource* surface_resource) {
  Surface* surface = GetUserDataAs<Surface>(surface_resource);
  if (surface->GetProperty(kSurfaceSynchronizationResource) != nullptr) {
    wl_resource_post_error(
        resource,
        ZWP_LINUX_EXPLICIT_SYNCHRONIZATION_V1_ERROR_SYNCHRONIZATION_EXISTS,
        "a synchronization object for the surface already exists");
    return;
  }

  wl_resource* linux_surface_synchronization_resource = wl_resource_create(
      client, &zwp_linux_surface_synchronization_v1_interface, 1, id);

  SetImplementation(linux_surface_synchronization_resource,
                    &linux_surface_synchronization_implementation,
                    std::make_unique<LinuxSurfaceSynchronization>(
                        linux_surface_synchronization_resource, surface));
}

const struct zwp_linux_explicit_synchronization_v1_interface
    linux_explicit_synchronization_implementation = {
        linux_explicit_synchronization_destroy,
        linux_explicit_synchronization_get_synchronization};

}  // namespace

void bind_linux_explicit_synchronization(wl_client* client,
                                         void* data,
                                         uint32_t version,
                                         uint32_t id) {
  wl_resource* resource = wl_resource_create(
      client, &zwp_linux_explicit_synchronization_v1_interface, 1, id);

  wl_resource_set_implementation(resource,
                                 &linux_explicit_synchronization_implementation,
                                 nullptr, nullptr);
}

bool linux_surface_synchronization_validate_commit(Surface* surface) {
  if (surface->HasPendingAcquireFence() &&
      !surface->HasPendingAttachedBuffer()) {
    wl_resource* linux_surface_synchronization_resource =
        surface->GetProperty(kSurfaceSynchronizationResource);
    DCHECK(linux_surface_synchronization_resource);

    wl_resource_post_error(
        linux_surface_synchronization_resource,
        ZWP_LINUX_SURFACE_SYNCHRONIZATION_V1_ERROR_NO_BUFFER,
        "surface has acquire fence but no buffer for synchronization");

    return false;
  }

  return true;
}

}  // namespace wayland
}  // namespace exo
