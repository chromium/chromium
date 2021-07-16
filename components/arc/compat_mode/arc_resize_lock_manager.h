// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_COMPAT_MODE_ARC_RESIZE_LOCK_MANAGER_H_
#define COMPONENTS_ARC_COMPAT_MODE_ARC_RESIZE_LOCK_MANAGER_H_

#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "components/arc/compat_mode/arc_resize_lock_pref_delegate.h"
#include "components/arc/compat_mode/resize_toggle_menu.h"
#include "components/arc/compat_mode/resize_util.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ui/aura/env.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace views {
class Widget;
}  // namespace views

namespace arc {

class ArcBridgeService;

// Manager for ARC resize lock feature.
class ArcResizeLockManager : public KeyedService,
                             public aura::EnvObserver,
                             public aura::WindowObserver {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcResizeLockManager* GetForBrowserContext(
      content::BrowserContext* context);

  ArcResizeLockManager(content::BrowserContext* browser_context,
                       ArcBridgeService* arc_bridge_service);
  ArcResizeLockManager(const ArcResizeLockManager&) = delete;
  ArcResizeLockManager& operator=(const ArcResizeLockManager&) = delete;
  ~ArcResizeLockManager() override;

  // aura::EnvObserver:
  void OnWindowInitialized(aura::Window* new_window) override;

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;
  void OnWindowDestroying(aura::Window* window) override;

  void SetPrefDelegate(ArcResizeLockPrefDelegate* delegate) {
    pref_delegate_ = delegate;
  }

  void ToggleResizeToggleMenu(views::Widget* widget);

 private:
  friend class ArcResizeLockManagerTest;

  void EnableResizeLock(aura::Window* window);
  void DisableResizeLock(aura::Window* window);

  // virtual for testing.
  virtual void UpdateCompatModeButton(aura::Window* window);

  ArcResizeLockPrefDelegate* pref_delegate_{nullptr};

  std::unique_ptr<ResizeToggleMenu> resize_toggle_menu_;

  base::flat_set<aura::Window*> resize_lock_enabled_windows_;

  base::ScopedObservation<aura::Env, aura::EnvObserver> env_observation{this};

  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      window_observations_{this};

  base::WeakPtrFactory<ArcResizeLockManager> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // COMPONENTS_ARC_COMPAT_MODE_ARC_RESIZE_LOCK_MANAGER_H_
