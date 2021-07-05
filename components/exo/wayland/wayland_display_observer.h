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
  ~WaylandDisplayHandler() override;
  void AddObserver(WaylandDisplayObserver* observer);
  int64_t id() const;

  // Overridden from display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

 private:
  // Overridden from WaylandDisplayObserver:
  bool SendDisplayMetrics(const display::Display& display,
                          uint32_t changed_metrics) override;

  // Returns the transform that a compositor will apply to a surface to
  // compensate for the rotation of an output device.
  wl_output_transform OutputTransform(display::Display::Rotation rotation);

  // Output.
  WaylandDisplayOutput* output_;

  // The output resource associated with the display.
  wl_resource* const output_resource_;

  base::ObserverList<WaylandDisplayObserver> observers_;

  display::ScopedDisplayObserver display_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(WaylandDisplayHandler);
};

}  // namespace wayland
}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_WAYLAND_DISPLAY_OBSERVER_H_
