// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_WAYLAND_DISPLAY_OBSERVER_H_
#define COMPONENTS_EXO_WAYLAND_WAYLAND_DISPLAY_OBSERVER_H_

#include <stdint.h>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "ui/display/display.h"

struct wl_resource;

namespace exo {
namespace wayland {
class AuraOutputManager;
class WaylandDisplayOutput;

// An observer that allows display information changes to be sent
// via different protocols while being synced with the wl_output's
// "done" event through WaylandDisplayHandler.
class WaylandDisplayObserver : public base::CheckedObserver {
 public:
  WaylandDisplayObserver();

  // Returns |true| if the observer reported any changes and needs
  // to be followed by "done" event, |false| otherwise.
  virtual bool SendDisplayMetrics(const display::Display& display,
                                  uint32_t changed_metrics) = 0;

  // Called when the server should send the active display information to the
  // client.
  virtual void SendActiveDisplay() = 0;

  // Called when wl_output is destroyed.
  virtual void OnOutputDestroyed() = 0;

 protected:
  ~WaylandDisplayObserver() override;
};

class WaylandDisplayHandler : public WaylandDisplayObserver {
 public:
  WaylandDisplayHandler(WaylandDisplayOutput* output,
                        wl_resource* output_resource);

  WaylandDisplayHandler(const WaylandDisplayHandler&) = delete;
  WaylandDisplayHandler& operator=(const WaylandDisplayHandler&) = delete;

  ~WaylandDisplayHandler() override;
  void Initialize();
  void AddObserver(WaylandDisplayObserver* observer);
  void RemoveObserver(WaylandDisplayObserver* observer);
  int64_t id() const;

  // Sends updated metrics for the wl_output and any output extensions
  // associated with this handler. Emits a final wl_output.done if any output
  // metrics events were dispatched to the client.
  void SendDisplayMetricsChanges(const display::Display& display,
                                 uint32_t changed_metrics);

  // Called when the output associated with this handler is activated and sends
  // the appropriate output events to the client.
  void SendDisplayActivated();

  // Called when an xdg_output object is created through get_xdg_output()
  // request by the wayland client.
  void OnXdgOutputCreated(wl_resource* xdg_output_resource);
  // Unset the xdg output object.
  void UnsetXdgOutputResource();

  size_t CountObserversForTesting() const;

  const wl_resource* output_resource() const { return output_resource_; }

 protected:
  // Overridable for testing.
  virtual void XdgOutputSendLogicalPosition(const gfx::Point& position);
  virtual void XdgOutputSendLogicalSize(const gfx::Size& size);
  virtual void XdgOutputSendDescription(const std::string& desc);

 private:
  // Overridden from WaylandDisplayObserver:
  bool SendDisplayMetrics(const display::Display& display,
                          uint32_t changed_metrics) override;
  void SendActiveDisplay() override;
  void OnOutputDestroyed() override;

  // Returns |true| if any metrics were send to the client and a "done" event is
  // required, |false| otherwise.
  bool SendXdgOutputMetrics(const display::Display& display,
                            uint32_t changed_metrics);

  // Gets the AuraOutputManager instance associated with this handler, may
  // return null.
  AuraOutputManager* GetAuraOutputManager();

  // Output.
  raw_ptr<WaylandDisplayOutput> output_;

  // The output resource associated with the display.
  const raw_ptr<wl_resource, DanglingUntriaged> output_resource_;

  // Resource associated with a zxdg_output_v1 object.
  raw_ptr<wl_resource, DanglingUntriaged> xdg_output_resource_ = nullptr;

  base::ObserverList<WaylandDisplayObserver> observers_;
};

}  // namespace wayland
}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_WAYLAND_DISPLAY_OBSERVER_H_
