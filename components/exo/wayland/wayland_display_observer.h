// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_WAYLAND_DISPLAY_OBSERVER_H_
#define COMPONENTS_EXO_WAYLAND_WAYLAND_DISPLAY_OBSERVER_H_

#include <stdint.h>
#include <wayland-server-protocol-core.h>

#include "base/observer_list.h"
#include "ui/display/display.h"
#include "ui/display/display_observer.h"

struct wl_resource;

namespace exo {
namespace wayland {
class WaylandDisplayOutput;

// An observer that allows display information changes to be sent
// via different protocols while being synced with the wl_output's
// "done" event through WaylandDisplayHandler.
class WaylandDisplayObserver : public base::CheckedObserver {
 public:
  WaylandDisplayObserver() {}

  // Returns |true| if the observer reported any changes and needs
  // to be followed by "done" event, |false| otherwise.
  virtual bool SendDisplayMetrics(const display::Display& display,
                                  uint32_t changed_metrics) = 0;

 protected:
  ~WaylandDisplayObserver() override {}
};

class WaylandDisplayHandler : public display::DisplayObserver,
                              public WaylandDisplayObserver {
 public:
  WaylandDisplayHandler(WaylandDisplayOutput* output,
                        wl_resource* output_resource);

  WaylandDisplayHandler(const WaylandDisplayHandler&) = delete;
  WaylandDisplayHandler& operator=(const WaylandDisplayHandler&) = delete;

  ~WaylandDisplayHandler() override;
  void Initialize();
  void AddObserver(WaylandDisplayObserver* observer);
  int64_t id() const;

  // Overridden from display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  // Called when an xdg_output object is created through get_xdg_output()
  // request by the wayland client.
  void OnXdgOutputCreated(wl_resource* xdg_output_resource);
  // Unset the xdg output object.
  void UnsetXdgOutputResource();

  // TODO(b/223538419): Move this functionality into an observer
  // Called when a color_management_output object is created through
  // get_color_management_output() request by the wayland client.
  void OnGetColorManagementOutput(wl_resource* color_manager_output_resource);
  // Unset the color management object
  void UnsetColorManagementOutputResource();

 protected:
  wl_resource* output_resource() const { return output_resource_; }

  // Overridable for testing.
  virtual void XdgOutputSendLogicalPosition(const gfx::Point& position);
  virtual void XdgOutputSendLogicalSize(const gfx::Size& size);

 private:
  // Overridden from WaylandDisplayObserver:
  bool SendDisplayMetrics(const display::Display& display,
                          uint32_t changed_metrics) override;

  // Output.
  WaylandDisplayOutput* output_;

  // The output resource associated with the display.
  wl_resource* const output_resource_;

  // Resource associated with a zxdg_output_v1 object.
  wl_resource* xdg_output_resource_ = nullptr;

  // Resource associated with a zcr_color_management_output_v1 object.
  wl_resource* color_management_output_resource_ = nullptr;

  base::ObserverList<WaylandDisplayObserver> observers_;

  display::ScopedDisplayObserver display_observer_{this};
};

}  // namespace wayland
}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_WAYLAND_DISPLAY_OBSERVER_H_
