// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WINDOW_OCCLUSION_MANAGER_H_
#define COMPONENTS_EXO_WINDOW_OCCLUSION_MANAGER_H_

#include <vector>

#include "ash/public/cpp/session/session_observer.h"
#include "base/scoped_multi_source_observation.h"
#include "components/exo/wm_helper.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_occlusion_tracker.h"

namespace aura {
class Window;
}  // namespace aura

namespace exo {

// This class keeps the occlusion state locked while the screen is locked. This
// is to prevent exo clients who keeps track of occlusion state, to change its
// state when screen is locked.  For example, a video player app may switch to
// picture in picture mode when it is occluded by another window, but locking
// screen should not trigger this transition.
// Note that this will not lock the occlusion state for a window newly created
// while a screen is locked.
class WindowOcclusionManager : public ash::SessionObserver,
                               public aura::WindowObserver,
                               public WMHelper::ExoWindowObserver {
 public:
  using StateLockers = std::vector<
      std::unique_ptr<aura::WindowOcclusionTracker::ScopedLockState>>;

  WindowOcclusionManager();
  WindowOcclusionManager(const WindowOcclusionManager&) = delete;
  WindowOcclusionManager operator=(const WindowOcclusionManager&) = delete;
  ~WindowOcclusionManager() override;

  // ash::SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  // WMHelper::ExoWindowObserver:
  void OnExoWindowCreated(aura::Window* window) override;

 private:
  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      tracked_window_observations_{this};
  StateLockers state_lockers_;
};

}  // namespace exo

#endif  // COMPONENTS_EXO_WINDOW_OCCLUSION_MANAGER_H_
