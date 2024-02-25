// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/notification_surface.h"
#include <cmath>

#include "components/exo/notification_surface_manager.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/surface.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace exo {

NotificationSurface::NotificationSurface(NotificationSurfaceManager* manager,
                                         Surface* surface,
                                         const std::string& notification_key)
    : SurfaceTreeHost("ExoNotificationSurface"),
      manager_(manager),
      notification_key_(notification_key) {
  surface->AddSurfaceObserver(this);
  SetRootSurface(surface);
  host_window()->Show();
  host_window()->AddObserver(this);
}

NotificationSurface::~NotificationSurface() {
  if (host_window())
    host_window()->RemoveObserver(this);
  if (added_to_manager_)
    manager_->RemoveSurface(this);
  if (root_surface())
    root_surface()->RemoveSurfaceObserver(this);
}

gfx::Size NotificationSurface::GetContentSize() const {
  float int_part;
  DCHECK(std::modf(root_surface()->content_size().width(), &int_part) == 0.0f &&
         std::modf(root_surface()->content_size().height(), &int_part) == 0.0f);
  return gfx::ToRoundedSize(root_surface()->content_size());
}

void NotificationSurface::SetApplicationId(const char* application_id) {
  SetShellApplicationId(host_window(), std::make_optional(application_id));
}

void NotificationSurface::OnSurfaceCommit() {
  SurfaceTreeHost::OnSurfaceCommit();

  // Defer AddSurface until there are contents to show.
  if (!added_to_manager_ && !host_window()->bounds().IsEmpty()) {
    added_to_manager_ = true;
    manager_->AddSurface(this);
  }

  // Only submit a compositor frame if the notification is being shown.
  // Submitting compositor frames while invisible causes Viz to hold on to
  // references to each notification buffer while it waits for an embedding (
  // or a timeout occurs). This can cause buffer starvation on the Android side,
  // leading to ANRs.
  if (is_embedded_)
    SubmitCompositorFrame();
}

void NotificationSurface::OnSurfaceDestroying(Surface* surface) {
  DCHECK_EQ(surface, root_surface());
  surface->RemoveSurfaceObserver(this);
  SetRootSurface(nullptr);
}

void NotificationSurface::OnWindowDestroying(aura::Window* window) {
  window->RemoveObserver(this);
}

void NotificationSurface::OnWindowPropertyChanged(aura::Window* window,
                                                  const void* key,
                                                  intptr_t old_value) {
  if (key == aura::client::kSkipImeProcessing) {
    SetSkipImeProcessingToDescendentSurfaces(
        window, window->GetProperty(aura::client::kSkipImeProcessing));
  }
}

void NotificationSurface::OnWindowAddedToRootWindow(aura::Window* window) {
  // Force recreating resources to submit the compositor frame w/o
  // commit request.
  root_surface()->SurfaceHierarchyResourcesLost();
  SubmitCompositorFrame();
  is_embedded_ = true;
}

void NotificationSurface::OnWindowRemovingFromRootWindow(
    aura::Window* window,
    aura::Window* new_root) {
  if (!new_root) {
    // Submit an empty compositor frame if the notification becomes invisible to
    // notify Viz that it can release its reference to the existing notification
    // buffer. We can't just evict the surface, because LayerTreeFrameSinkHolder
    // needs to hold on to resources that were used in the previous frame, if it
    // was associated with a different local surface id.
    SubmitEmptyCompositorFrame();

    // Evict the current surface. We can't only submit
    // an empty compositor frame, because then when re-opening the message
    // center, Viz may think it can use the empty compositor frame to display.
    // This will force viz to wait the given deadline for the next notification
    // compositor frame when showing the message center. This is to prevent
    // flashes when opening the message center.
    aura::Env::GetInstance()
        ->context_factory()
        ->GetHostFrameSinkManager()
        ->EvictSurfaces({GetSurfaceId()});

    // Allocate a new local surface id, with a new parent portion for the next
    // compositor frame.
    host_window()->AllocateLocalSurfaceId();
    UpdateLocalSurfaceIdFromParent(host_window()->GetLocalSurfaceId());

    is_embedded_ = false;

    // Upon closing the message center, there is layer cloning which ends up
    // setting the deadline to 0 frames. Set the deadline to default so Viz
    // waits a while for the notification compositor frame to arrive before
    // showing the message center.
    MaybeActivateSurface();
  }
}

}  // namespace exo
