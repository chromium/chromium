// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_GRAPHICS_CAST_FOCUS_CLIENT_AURA_H_
#define CHROMECAST_GRAPHICS_CAST_FOCUS_CLIENT_AURA_H_

#include <vector>

#include "base/observer_list.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/window_observer.h"
#include "ui/wm/public/activation_client.h"

namespace aura {
class Window;
}

namespace chromecast {

class CastFocusClientAura : public aura::WindowObserver,
                            public aura::client::FocusClient,
                            public wm::ActivationClient {
 public:
  CastFocusClientAura();

  CastFocusClientAura(const CastFocusClientAura&) = delete;
  CastFocusClientAura& operator=(const CastFocusClientAura&) = delete;

  ~CastFocusClientAura() override;

  // aura::client::FocusClient implementation:
  void AddObserver(aura::client::FocusChangeObserver* observer) override;
  void RemoveObserver(aura::client::FocusChangeObserver* observer) override;
  void FocusWindow(aura::Window* window) override;
  void ResetFocusWithinActiveWindow(aura::Window* window) override;
  aura::Window* GetFocusedWindow() override;

  // Overridden from wm::ActivationClient:
  void AddObserver(wm::ActivationChangeObserver* observer) override;
  void RemoveObserver(wm::ActivationChangeObserver* observer) override;
  void ActivateWindow(aura::Window* window) override;
  void DeactivateWindow(aura::Window* window) override;
  const aura::Window* GetActiveWindow() const override;
  aura::Window* GetActivatableWindow(aura::Window* window) const override;
  const aura::Window* GetToplevelWindow(
      const aura::Window* window) const override;
  bool CanActivateWindow(const aura::Window* window) const override;

 private:
  // aura::WindowObserver implementation:
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override;
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowHierarchyChanging(const HierarchyChangeParams& params) override;

  // Change the focused window and notify focus observers.
  void UpdateWindowFocus();
  // Get the window that should be focused.
  aura::Window* GetWindowToFocus();
  // Get the top-most window in a window's hierarchy to determine z order.
  // This is the window directly under the root window (a child window of the
  // root window).
  aura::Window* GetZOrderWindow(aura::Window* window) const {
    return const_cast<aura::Window*>(
        GetZOrderWindow(const_cast<const aura::Window*>(window)));
  }
  const aura::Window* GetZOrderWindow(const aura::Window* window) const;

  base::ObserverList<aura::client::FocusChangeObserver> focus_observers_;

  // Track the currently focused window, which isn't necessarily a top-level
  // window.
  aura::Window* focused_window_;
  // Track the windows that we've focused in the past, so that we can restore
  // focus to them.  We assume that this is a small list so that we can perform
  // linear ops on it.
  std::vector<aura::Window*> focusable_windows_;
};

}  // namespace chromecast

#endif  // CHROMECAST_GRAPHICS_CAST_FOCUS_CLIENT_AURA_H_
