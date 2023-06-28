// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/wayland_display_observer.h"

#include <wayland-server-core.h>
#include <xdg-output-unstable-v1-server-protocol.h>

#include <string>

#include "ash/shell.h"
#include "chrome-color-management-server-protocol.h"
#include "components/exo/wayland/output_metrics.h"
#include "components/exo/wayland/server_util.h"
#include "components/exo/wayland/wayland_display_output.h"
#include "components/exo/wayland/zaura_output_manager.h"
#include "components/exo/wayland/zcr_color_manager.h"
#include "ui/display/display_observer.h"
#include "ui/display/screen.h"
#include "wayland-server-protocol-core.h"

namespace exo {
namespace wayland {

WaylandDisplayObserver::WaylandDisplayObserver() = default;

WaylandDisplayObserver::~WaylandDisplayObserver() = default;

WaylandDisplayHandler::WaylandDisplayHandler(WaylandDisplayOutput* output,
                                             wl_resource* output_resource)
    : output_(output), output_resource_(output_resource) {
  // At construction time the client object is guaranteed to exist.
  wl_client* client = wl_resource_get_client(output_resource_);
  CHECK(client);
  client_destroy_listener_.listener.notify =
      &WaylandDisplayHandler::OnClientDestroyed;
  wl_client_add_destroy_listener(client, &client_destroy_listener_.listener);
}

WaylandDisplayHandler::~WaylandDisplayHandler() {
  // Remove the listener to cover the case where the client outlives the
  // handler.
  if (!client_destroy_listener_.notified) {
    wl_list_remove(&client_destroy_listener_.listener.link);
  }

  ash::Shell::Get()->RemoveShellObserver(this);
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
  ash::Shell::Get()->AddShellObserver(this);
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
  constexpr uint32_t all_changes = 0xFFFFFFFF;
  OnDisplayMetricsChanged(display, all_changes);
}

void WaylandDisplayHandler::RemoveObserver(WaylandDisplayObserver* observer) {
  observers_.RemoveObserver(observer);
}

int64_t WaylandDisplayHandler::id() const {
  DCHECK(output_);
  return output_->id();
}

void WaylandDisplayHandler::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  DCHECK(output_resource_);

  if (id() != display.id()) {
    return;
  }

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

void WaylandDisplayHandler::OnDisplayForNewWindowsChanged() {
  DCHECK(output_resource_);
  if (id() != display::Screen::GetScreen()->GetDisplayForNewWindows().id()) {
    return;
  }

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
  OnDisplayMetricsChanged(display, 0xFFFFFFFF);
}

void WaylandDisplayHandler::UnsetXdgOutputResource() {
  DCHECK(xdg_output_resource_);
  xdg_output_resource_ = nullptr;
}

bool WaylandDisplayHandler::IsClientDestroyedForTesting() const {
  return client_destroy_listener_.notified;
}

AuraOutputManager* WaylandDisplayHandler::GetAuraOutputManagerForTesting() {
  return GetAuraOutputManager();
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

// static.
void WaylandDisplayHandler::OnClientDestroyed(struct wl_listener* listener,
                                              void* data) {
  ClientDestroyListener* client_destroy_listener = wl_container_of(
      listener, /*sample=*/client_destroy_listener, /*member=*/listener);
  client_destroy_listener->notified = true;
  wl_list_remove(&client_destroy_listener->listener.link);
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
  // the same bounds. This happens whenyou're in docked mode, suspend,
  // unplug the display, then resume to the internal display which has
  // the same resolution. Since metrics does not change, there is no need
  // to notify clients.
  if (!(changed_metrics &
        (DISPLAY_METRIC_BOUNDS | DISPLAY_METRIC_DEVICE_SCALE_FACTOR |
         DISPLAY_METRIC_ROTATION))) {
    return false;
  }

  const OutputMetrics output_metrics(display);

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

  if (xdg_output_resource_) {
    XdgOutputSendLogicalPosition(output_metrics.logical_origin);
    XdgOutputSendLogicalSize(output_metrics.logical_size);
    XdgOutputSendDescription(output_metrics.description);
    if (wl_resource_get_version(xdg_output_resource_) < 3) {
      zxdg_output_v1_send_done(xdg_output_resource_);
    }
  } else {
    if (wl_resource_get_version(output_resource_) >=
        WL_OUTPUT_SCALE_SINCE_VERSION) {
      wl_output_send_scale(output_resource_, output_metrics.scale);
    }
  }

  return true;
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
  // If the client has begun destruction avoid attempting to access the client's
  // AuraOutputManager instance as libwayland may have freed the object's memory
  // but not yet updated the data structures used to find the object (see
  // crbug.com/1433187).
  if (client_destroy_listener_.notified) {
    return nullptr;
  }

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
