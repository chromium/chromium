// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/zcr_vsync_feedback.h"

#include <vsync-feedback-unstable-v1-server-protocol.h>

#include "base/memory/raw_ptr.h"
#include "components/exo/vsync_timing_manager.h"
#include "components/exo/wayland/server_util.h"

namespace exo {
namespace wayland {
namespace {

////////////////////////////////////////////////////////////////////////////////
// vsync_timing_interface:

// Implements VSync timing interface by monitoring updates to VSync parameters.
class VSyncTiming final : public VSyncTimingManager::Observer {
 public:
  explicit VSyncTiming(wl_resource* timing_resource)
      : timing_resource_(timing_resource) {
    WMHelper::GetInstance()->GetVSyncTimingManager().AddObserver(this);
  }

  VSyncTiming(const VSyncTiming&) = delete;
  VSyncTiming& operator=(const VSyncTiming&) = delete;

  ~VSyncTiming() override {
    WMHelper::GetInstance()->GetVSyncTimingManager().RemoveObserver(this);
  }

  // Overridden from VSyncTimingManager::Observer:
  void OnUpdateVSyncParameters(base::TimeTicks timebase,
                               base::TimeDelta interval) override {
    uint64_t timebase_us = timebase.since_origin().InMicroseconds();
    uint64_t interval_us = interval.InMicroseconds();

    zcr_vsync_timing_v1_send_update(timing_resource_, timebase_us & 0xffffffff,
                                    timebase_us >> 32, interval_us & 0xffffffff,
                                    interval_us >> 32);
    wl_client_flush(wl_resource_get_client(timing_resource_));
  }

 private:
  // The VSync timing resource.
  const raw_ptr<wl_resource> timing_resource_;
};

void vsync_timing_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct zcr_vsync_timing_v1_interface vsync_timing_implementation = {
    vsync_timing_destroy};

////////////////////////////////////////////////////////////////////////////////
// vsync_feedback_interface:

void vsync_feedback_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void vsync_feedback_get_vsync_timing(wl_client* client,
                                     wl_resource* resource,
                                     uint32_t id,
                                     wl_resource* output) {
  wl_resource* timing_resource =
      wl_resource_create(client, &zcr_vsync_timing_v1_interface, 1, id);
  SetImplementation(timing_resource, &vsync_timing_implementation,
                    std::make_unique<VSyncTiming>(timing_resource));
}

const struct zcr_vsync_feedback_v1_interface vsync_feedback_implementation = {
    vsync_feedback_destroy, vsync_feedback_get_vsync_timing};

}  // namespace

void bind_vsync_feedback(wl_client* client,
                         void* data,
                         uint32_t version,
                         uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &zcr_vsync_feedback_v1_interface, 1, id);

  wl_resource_set_implementation(resource, &vsync_feedback_implementation,
                                 nullptr, nullptr);
}

}  // namespace wayland
}  // namespace exo
