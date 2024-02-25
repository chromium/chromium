// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_ZAURA_OUTPUT_MANAGER_H_
#define COMPONENTS_EXO_WAYLAND_ZAURA_OUTPUT_MANAGER_H_

#include <aura-shell-server-protocol.h>

#include <cstdint>

#include "base/memory/raw_ptr.h"

namespace display {
class Display;
}  // namespace display

namespace exo::wayland {

inline constexpr uint32_t kZAuraOutputManagerVersion =
    ZAURA_OUTPUT_MANAGER_OVERSCAN_INSETS_SINCE_VERSION;

void bind_aura_output_manager(wl_client* client,
                              void* data,
                              uint32_t version,
                              uint32_t id);

// Responsible for relaying display/output state required by clients for all
// supported output extensions when first bound and subsequently as needed.
class AuraOutputManager {
 public:
  explicit AuraOutputManager(wl_resource* manager_resource);
  AuraOutputManager(const AuraOutputManager&) = delete;
  AuraOutputManager& operator=(const AuraOutputManager&) = delete;
  virtual ~AuraOutputManager() = default;

  // Returns nullptr if there is no metrics manager bound to `client`.
  static AuraOutputManager* Get(wl_client* client);

  // Returns the display id associated with the `output_resource`. Returns
  // display::kInvalidDisplayId if `output_resource` is nullptr.
  static int64_t GetDisplayIdForOutput(wl_resource* outout_resource);

  // Dispatches multiple events to the client for the full set of output state
  // required to correctly represent a display. Returns true if any state events
  // were sent based on the the bit-flags in `changed_metrics`.
  bool SendOutputMetrics(wl_resource* output_resource,
                         const display::Display& display,
                         uint32_t changed_metrics);

  // Dispatches the activated event for the `output_resource` to the associated
  // client.
  void SendOutputActivated(wl_resource* output_resource);

 private:
  const raw_ptr<wl_client> client_;
  const raw_ptr<wl_resource> manager_resource_;
};

}  // namespace exo::wayland

#endif  // COMPONENTS_EXO_WAYLAND_ZAURA_OUTPUT_MANAGER_H_
