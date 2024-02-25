// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/zwp_linux_explicit_synchronization.h"
#include "base/memory/raw_ptr.h"

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
// linux_buffer_release_v1 interface:

// Implements the buffer release interface.
class LinuxBufferRelease {
 public:
  LinuxBufferRelease(wl_resource* resource, Surface* surface)
      : resource_(resource),
        release_callback_(
            base::BindOnce(&LinuxBufferRelease::HandleExplicitRelease,
                           base::Unretained(this))) {
    surface->SetPerCommitBufferReleaseCallback(release_callback_.callback());
  }

  LinuxBufferRelease(const LinuxBufferRelease&) = delete;
  LinuxBufferRelease& operator=(const LinuxBufferRelease&) = delete;

 private:
  void HandleExplicitRelease(gfx::GpuFenceHandle release_fence) {
    if (!release_fence.is_null()) {
      // Fd will be dup'd for us.
      zwp_linux_buffer_release_v1_send_fenced_release(resource_,
                                                      release_fence.Peek());
    } else {
      zwp_linux_buffer_release_v1_send_immediate_release(resource_);
    }
    // Protocol specifies that either of these events result in the buffer
    // release object's destruction.
    wl_client_flush(wl_resource_get_client(resource_));
    wl_resource_destroy(resource_);
  }

  raw_ptr<wl_resource> resource_;
  // Use a cancelable callback in case this object is destroyed while a commit
  // is still in flight.
  base::CancelableOnceCallback<void(gfx::GpuFenceHandle)> release_callback_;
};

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

  LinuxSurfaceSynchronization(const LinuxSurfaceSynchronization&) = delete;
  LinuxSurfaceSynchronization& operator=(const LinuxSurfaceSynchronization&) =
      delete;

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
  raw_ptr<Surface> surface_;
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
  if (fence_info->status != 1) {
    // Not signalled yet.
    handle.Adopt(std::move(fence_fd));
  }
  surface->SetAcquireFence(std::make_unique<gfx::GpuFence>(std::move(handle)));
}

void linux_surface_synchronization_get_release(wl_client* client,
                                               wl_resource* resource,
                                               uint32_t id) {
  auto* surface =
      GetUserDataAs<LinuxSurfaceSynchronization>(resource)->surface();

  if (!surface) {
    wl_resource_post_error(
        resource, ZWP_LINUX_SURFACE_SYNCHRONIZATION_V1_ERROR_NO_SURFACE,
        "surface no longer exists");
    return;
  }

  if (surface->HasPendingPerCommitBufferReleaseCallback()) {
    wl_resource_post_error(
        resource, ZWP_LINUX_SURFACE_SYNCHRONIZATION_V1_ERROR_DUPLICATE_RELEASE,
        "already has a buffer release");
    return;
  }

  auto* linux_buffer_release_resource =
      wl_resource_create(client, &zwp_linux_buffer_release_v1_interface,
                         wl_resource_get_version(resource), id);

  SetImplementation(linux_buffer_release_resource, nullptr,
                    std::make_unique<LinuxBufferRelease>(
                        linux_buffer_release_resource, surface));
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
      client, &zwp_linux_surface_synchronization_v1_interface,
      wl_resource_get_version(resource), id);

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
      client, &zwp_linux_explicit_synchronization_v1_interface, version, id);

  wl_resource_set_implementation(resource,
                                 &linux_explicit_synchronization_implementation,
                                 nullptr, nullptr);
}

bool linux_surface_synchronization_validate_commit(Surface* surface) {
  if (surface->HasPendingAttachedBuffer())
    return true;

  if (surface->HasPendingAcquireFence()) {
    wl_resource* linux_surface_synchronization_resource =
        surface->GetProperty(kSurfaceSynchronizationResource);
    DCHECK(linux_surface_synchronization_resource);

    wl_resource_post_error(
        linux_surface_synchronization_resource,
        ZWP_LINUX_SURFACE_SYNCHRONIZATION_V1_ERROR_NO_BUFFER,
        "surface has acquire fence but no buffer for synchronization");

    return false;
  }

  if (surface->HasPendingPerCommitBufferReleaseCallback()) {
    wl_resource* linux_surface_synchronization_resource =
        surface->GetProperty(kSurfaceSynchronizationResource);
    DCHECK(linux_surface_synchronization_resource);

    wl_resource_post_error(
        linux_surface_synchronization_resource,
        ZWP_LINUX_SURFACE_SYNCHRONIZATION_V1_ERROR_NO_BUFFER,
        "surface has buffer_release but no buffer for synchronization");

    return false;
  }

  return true;
}

}  // namespace wayland
}  // namespace exo
