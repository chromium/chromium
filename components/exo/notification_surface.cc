// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/notification_surface.h"

#include "ash/public/cpp/app_types.h"
#include "components/exo/notification_surface_manager.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/surface.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/rect.h"

namespace exo {

NotificationSurface::NotificationSurface(NotificationSurfaceManager* manager,
                                         Surface* surface,
                                         const std::string& notification_key)
    : SurfaceTreeHost("ExoNotificationSurface"),
      manager_(manager),
      notification_key_(notification_key) {
  surface->AddSurfaceObserver(this);
  SetRootSurface(surface);
  host_window()->SetProperty(aura::client::kAppType,
                             static_cast<int>(ash::AppType::ARC_APP));
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

const gfx::Size& NotificationSurface::GetContentSize() const {
  return root_surface()->content_size();
}

void NotificationSurface::SetApplicationId(const char* application_id) {
  SetShellApplicationId(host_window(), base::make_optional(application_id));
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

void NotificationSurface::OnWindowAddedToRootWindow(aura::Window* window) {
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
        ->context_factory_private()
        ->GetHostFrameSinkManager()
        ->EvictSurfaces({host_window()->GetSurfaceId()});

    // Allocate a new local surface id for the next compositor frame.
    host_window()->AllocateLocalSurfaceId();

    is_embedded_ = false;

    // Set the deadline to default so Viz waits a while for the notification
    // compositor frame to arrive before showing the message center.
    host_window()->layer()->SetShowSurface(
        host_window()->GetSurfaceId(), host_window()->bounds().size(),
        SK_ColorWHITE, cc::DeadlinePolicy::UseDefaultDeadline(),
        /*stretch_content_to_fill_bounds=*/false);
  }
}

}  // namespace exo
