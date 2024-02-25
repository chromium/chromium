// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_INTENT_HELPER_CUSTOM_TAB_H_
#define COMPONENTS_ARC_INTENT_HELPER_CUSTOM_TAB_H_

#include <memory>

#include "ash/components/arc/arc_export.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/controls/native/native_view_host.h"

namespace arc {

// CustomTab is responsible to embed an ARC++ custom tab.
class ARC_EXPORT CustomTab : public aura::WindowObserver {
 public:
  explicit CustomTab(aura::Window* arc_app_window);
  CustomTab(const CustomTab&) = delete;
  CustomTab& operator=(const CustomTab&) = delete;
  ~CustomTab() override;

  void Attach(gfx::NativeView view);

  // Returns the view against which a view or dialog is positioned and parented
  // in a CustomTab.
  gfx::NativeView GetHostView();

  // aura::WindowObserver:
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;
  void OnWindowStackingChanged(aura::Window* window) override;
  void OnWindowDestroying(aura::Window* window) override;

 private:
  // Updates |host_|'s bounds to deal with changes in the bounds of the
  // associated |arc_app_window|.
  void UpdateHostBounds(aura::Window* arc_app_window);

  // Ensures the window/layer orders for the NativeViewHost.
  void EnsureWindowOrders();

  // Converts the point from the given window to this view.
  void ConvertPointFromWindow(aura::Window* window, gfx::Point* point);

  std::unique_ptr<views::NativeViewHost> host_ =
      std::make_unique<views::NativeViewHost>();
  const raw_ptr<aura::Window> arc_app_window_;
  base::ScopedObservation<aura::Window, aura::WindowObserver>
      arc_app_window_observation_{this};
  base::ScopedObservation<aura::Window, aura::WindowObserver>
      other_windows_observation_{this};
  base::WeakPtrFactory<CustomTab> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // COMPONENTS_ARC_INTENT_HELPER_CUSTOM_TAB_H_
