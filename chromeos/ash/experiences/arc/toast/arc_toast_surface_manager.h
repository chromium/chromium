// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_EXPERIENCES_ARC_TOAST_ARC_TOAST_SURFACE_MANAGER_H_
#define CHROMEOS_ASH_EXPERIENCES_ARC_TOAST_ARC_TOAST_SURFACE_MANAGER_H_

#include <vector>

#include "ash/public/cpp/session/session_controller.h"
#include "ash/public/cpp/session/session_observer.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chromeos/ash/experiences/arc/arc_export.h"
#include "components/exo/toast_surface_manager.h"

namespace ash {

class ARC_EXPORT ArcToastSurfaceManager : public exo::ToastSurfaceManager,
                                          public SessionObserver {
 public:
  ArcToastSurfaceManager();
  ~ArcToastSurfaceManager() override;

  // Disallow copy and assign.
  ArcToastSurfaceManager(const ArcToastSurfaceManager&) = delete;
  ArcToastSurfaceManager& operator=(const ArcToastSurfaceManager&) = delete;

  // exo::ToastSurfaceManager:
  void AddSurface(exo::ToastSurface* surface) override;
  void RemoveSurface(exo::ToastSurface* surface) override;

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(ArcToastSurfaceManagerTest, AddRemoveSurface);

  void UpdateVisibility();

  std::vector<raw_ptr<exo::ToastSurface, VectorExperimental>> toast_surfaces_;

  bool locked_;

  base::ScopedObservation<SessionController, SessionObserver>
      scoped_observation_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_EXPERIENCES_ARC_TOAST_ARC_TOAST_SURFACE_MANAGER_H_
