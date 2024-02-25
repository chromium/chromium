// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_NOTIFICATION_SURFACE_H_
#define COMPONENTS_EXO_NOTIFICATION_SURFACE_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "components/exo/surface_observer.h"
#include "components/exo/surface_tree_host.h"
#include "ui/gfx/geometry/size.h"

namespace exo {
class NotificationSurfaceManager;
class Surface;

// Handles notification surface role of a given surface.
class NotificationSurface : public SurfaceTreeHost,
                            public SurfaceObserver,
                            public aura::WindowObserver {
 public:
  NotificationSurface(NotificationSurfaceManager* manager,
                      Surface* surface,
                      const std::string& notification_key);

  NotificationSurface(const NotificationSurface&) = delete;
  NotificationSurface& operator=(const NotificationSurface&) = delete;

  ~NotificationSurface() override;

  // Get the content size of the |root_surface()|.
  gfx::Size GetContentSize() const;

  void SetApplicationId(const char* application_id);

  const std::string& notification_key() const { return notification_key_; }

  // Overridden from SurfaceDelegate:
  void OnSurfaceCommit() override;

  // Overridden from SurfaceObserver:
  void OnSurfaceDestroying(Surface* surface) override;

  // Overridden from WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old_value) override;
  void OnWindowAddedToRootWindow(aura::Window* window) override;
  void OnWindowRemovingFromRootWindow(aura::Window* window,
                                      aura::Window* new_root) override;

 private:
  const raw_ptr<NotificationSurfaceManager> manager_;  // Not owned.
  const std::string notification_key_;

  bool added_to_manager_ = false;

  // True if the notification is visible by e.g. being embedded in the message
  // center.
  bool is_embedded_ = false;
};

}  // namespace exo

#endif  // COMPONENTS_EXO_NOTIFICATION_SURFACE_H_
