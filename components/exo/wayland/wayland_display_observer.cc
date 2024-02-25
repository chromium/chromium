// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/wayland_display_observer.h"

#include <chrome-color-management-server-protocol.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol-core.h>
#include <xdg-output-unstable-v1-server-protocol.h>

#include "components/exo/wayland/output_metrics.h"
#include "components/exo/wayland/server_util.h"
#include "components/exo/wayland/wayland_display_output.h"
#include "components/exo/wayland/zaura_output_manager.h"
#include "components/exo/wayland/zcr_color_manager.h"
#include "ui/display/display_observer.h"
#include "ui/display/screen.h"

using DisplayMetric = display::DisplayObserver::DisplayMetric;

namespace exo {
namespace wayland {

WaylandDisplayObserver::WaylandDisplayObserver() = default;

WaylandDisplayObserver::~WaylandDisplayObserver() = default;

WaylandDisplayHandler::WaylandDisplayHandler(WaylandDisplayOutput* output,
                                             wl_resource* output_resource)
    : output_(output), output_resource_(output_resource) {}

WaylandDisplayHandler::~WaylandDisplayHandler() {
  for (auto& obs : observers_) {
    obs.OnOutputDestroyed();
  }
  if (xdg_output_resource_) {
    wl_resource_set_user_data(xdg_output_resource_, nullptr);
  }
  output_->UnregisterOutput(output_resource_);
}

void WaylandDisplayHandler::Initialize() {
  // Adding itself as an observer will send the initial display metrics.
  AddObserver(this);
  output_->RegisterOutput(output_resource_);
}

void WaylandDisplayHandler::AddObserver(WaylandDisplayObserver* observer) {
  observers_.AddObserver(observer);

  display::Display display;
  bool exists =
      display::Screen::GetScreen()->GetDisplayWithDisplayId(id(), &display);
  if (!exists) {
    // WaylandDisplayHandler is created asynchronously, and the
    // display can be deleted before created. This usually won't happen
    // in real environment, but can happen in test environment.
    return;
  }

  // Send the first round of changes to the observer.
  constexpr uint32_t kAllChanges = 0xFFFFFFFF;
  SendDisplayMetricsChanges(display, kAllChanges);
}

void WaylandDisplayHandler::RemoveObserver(WaylandDisplayObserver* observer) {
  observers_.RemoveObserver(observer);
}

int64_t WaylandDisplayHandler::id() const {
  DCHECK(output_);
  return output_->id();
}

void WaylandDisplayHandler::SendDisplayMetricsChanges(
    const display::Display& display,
    uint32_t changed_metrics) {
  CHECK(output_resource_);
  CHECK_EQ(id(), display.id());

  bool needs_done = false;

  // If supported, the aura_output_manager must have been bound by clients
  // before the wl_output associated with this WaylandDisplayHandler is bound.
  if (auto* output_manager = GetAuraOutputManager()) {
    // This sends all relevant output metrics to clients. These events are sent
    // immediately after the client binds an output and again every time display
    // metrics have changed.
    needs_done |= output_manager->SendOutputMetrics(output_resource_, display,
                                                    changed_metrics);
  }

  for (auto& observer : observers_) {
    needs_done |= observer.SendDisplayMetrics(display, changed_metrics);
  }

  if (needs_done) {
    if (wl_resource_get_version(output_resource_) >=
        WL_OUTPUT_DONE_SINCE_VERSION) {
      wl_output_send_done(output_resource_);
    }
    wl_client_flush(wl_resource_get_client(output_resource_));
  }
}

void WaylandDisplayHandler::SendDisplayActivated() {
  for (auto& observer : observers_) {
    observer.SendActiveDisplay();
  }
}

void WaylandDisplayHandler::OnXdgOutputCreated(
    wl_resource* xdg_output_resource) {
  DCHECK(!xdg_output_resource_);
  xdg_output_resource_ = xdg_output_resource;

  display::Display display;
  if (!display::Screen::GetScreen()->GetDisplayWithDisplayId(id(), &display)) {
    return;
  }

  if (SendXdgOutputMetrics(display, 0xFFFFFFFF)) {
    if (wl_resource_get_version(output_resource_) >=
        WL_OUTPUT_DONE_SINCE_VERSION) {
      wl_output_send_done(output_resource_);
    }
    wl_client_flush(wl_resource_get_client(output_resource_));
  }
}

void WaylandDisplayHandler::UnsetXdgOutputResource() {
  DCHECK(xdg_output_resource_);
  xdg_output_resource_ = nullptr;
}

void WaylandDisplayHandler::XdgOutputSendLogicalPosition(
    const gfx::Point& position) {
  zxdg_output_v1_send_logical_position(xdg_output_resource_, position.x(),
                                       position.y());
}

void WaylandDisplayHandler::XdgOutputSendLogicalSize(const gfx::Size& size) {
  zxdg_output_v1_send_logical_size(xdg_output_resource_, size.width(),
                                   size.height());
}

void WaylandDisplayHandler::XdgOutputSendDescription(const std::string& desc) {
  if (wl_resource_get_version(xdg_output_resource_) <
      ZXDG_OUTPUT_V1_DESCRIPTION_SINCE_VERSION) {
    return;
  }
  zxdg_output_v1_send_description(xdg_output_resource_, desc.c_str());
}

bool WaylandDisplayHandler::SendDisplayMetrics(const display::Display& display,
                                               uint32_t changed_metrics) {
  if (!output_resource_) {
    return false;
  }

  // There is no need to check DISPLAY_METRIC_PRIMARY because when primary
  // changes, bounds always changes. (new primary should have had non
  // 0,0 origin).
  // Only exception is when switching to newly connected primary with
  // the same bounds. This happens when you're in docked mode, suspend,
  // unplug the display, then resume to the internal display which has
  // the same resolution. Since metrics does not change, there is no need
  // to notify clients.

  const OutputMetrics output_metrics(display);
  bool result = false;

  if (changed_metrics & (DisplayMetric::DISPLAY_METRIC_BOUNDS |
                         DisplayMetric::DISPLAY_METRIC_ROTATION |
                         DisplayMetric::DISPLAY_METRIC_REFRESH_RATE)) {
    wl_output_send_geometry(
        output_resource_, output_metrics.origin.x(), output_metrics.origin.y(),
        output_metrics.physical_size_mm.width(),
        output_metrics.physical_size_mm.height(), output_metrics.subpixel,
        output_metrics.make.c_str(), output_metrics.model.c_str(),
        output_metrics.panel_transform);
    wl_output_send_mode(output_resource_, output_metrics.mode_flags,
                        output_metrics.physical_size_px.width(),
                        output_metrics.physical_size_px.height(),
                        output_metrics.refresh_mhz);
    result = true;
  }

  if (changed_metrics & DisplayMetric::DISPLAY_METRIC_DEVICE_SCALE_FACTOR) {
    if (wl_resource_get_version(output_resource_) >=
        WL_OUTPUT_SCALE_SINCE_VERSION) {
      wl_output_send_scale(output_resource_, output_metrics.scale);
      result = true;
    }
  }

  if (SendXdgOutputMetrics(display, changed_metrics)) {
    result = true;
  }

  return result;
}

bool WaylandDisplayHandler::SendXdgOutputMetrics(
    const display::Display& display,
    uint32_t changed_metrics) {
  if (!xdg_output_resource_) {
    return false;
  }

  const OutputMetrics output_metrics(display);
  bool result = false;

  if (changed_metrics & (DisplayMetric::DISPLAY_METRIC_BOUNDS |
                         DisplayMetric::DISPLAY_METRIC_ROTATION |
                         DisplayMetric::DISPLAY_METRIC_DEVICE_SCALE_FACTOR)) {
    XdgOutputSendLogicalPosition(output_metrics.logical_origin);
    XdgOutputSendLogicalSize(output_metrics.logical_size);
    XdgOutputSendDescription(output_metrics.description);
    if (wl_resource_get_version(xdg_output_resource_) < 3) {
      zxdg_output_v1_send_done(xdg_output_resource_);
    }
    result = true;
  }

  return result;
}

void WaylandDisplayHandler::SendActiveDisplay() {
  if (auto* output_manager = GetAuraOutputManager()) {
    output_manager->SendOutputActivated(output_resource_);
  }
}

void WaylandDisplayHandler::OnOutputDestroyed() {
  // destroying itself.
  RemoveObserver(this);
}

AuraOutputManager* WaylandDisplayHandler::GetAuraOutputManager() {
  wl_client* client = wl_resource_get_client(output_resource_);
  CHECK(client);
  return AuraOutputManager::Get(client);
}

size_t WaylandDisplayHandler::CountObserversForTesting() const {
  size_t count = 0;
  for (auto& obs : observers_) {
    if (&obs != this) {
      count++;
    }
  }
  return count;
}

}  // namespace wayland
}  // namespace exo
