// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_ZAURA_OUTPUT_MANAGER_V2_H_
#define COMPONENTS_EXO_WAYLAND_ZAURA_OUTPUT_MANAGER_V2_H_

#include <unordered_set>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/exo/wayland/output_configuration_change.h"

struct wl_resource;
struct wl_client;

namespace exo::wayland {

class WaylandDisplayOutput;

inline constexpr uint32_t kZAuraOutputManagerV2Version = 1;

void bind_aura_output_manager_v2(wl_client* client,
                                 void* data,
                                 uint32_t version,
                                 uint32_t id);

// This class is responsible for propagating information about display
// configuration state changes atomically to participating clients.
class AuraOutputManagerV2 {
 public:
  using ActiveOutputGetter = base::RepeatingCallback<WaylandOutputList()>;

  // Used to manage the lifetime of client-bound handles to the aura output
  // manager.
  class UserData {
   public:
    UserData(AuraOutputManagerV2* output_manager,
             wl_resource* outout_manager_resouce);
    ~UserData();

   private:
    const base::WeakPtr<AuraOutputManagerV2> output_manager_;
    const raw_ptr<wl_resource> outout_manager_resouce_;
  };

  explicit AuraOutputManagerV2(ActiveOutputGetter active_output_getter);
  AuraOutputManagerV2(const AuraOutputManagerV2&) = delete;
  AuraOutputManagerV2& operator=(const AuraOutputManagerV2&) = delete;
  virtual ~AuraOutputManagerV2();

  // Called when the system's display configuration has changed. Returns true if
  // changes were propagated to clients and a done event is necessary to
  // complete the transaction.
  bool OnDidProcessDisplayChanges(
      const OutputConfigurationChange& configuration_change);

  // Dispatches the activated event to all bound clients for the global
  // `output`.
  void SendOutputActivated(const WaylandDisplayOutput& output);

  // Notifies clients of the end of the display configuration change
  // transaction.
  void SendDone();

  // Called by UserData when the wrapped resource is created and destroyed
  // respectively.
  void Register(wl_resource* manager_resource);
  void Unregister(wl_resource* manager_resource);

  base::WeakPtr<AuraOutputManagerV2> GetWeakPtr();

 private:
  // Dispatches output metrics conditional on `changed_metrics` to all bound
  // clients for the global `output`. Returns true if updates were dispatched.
  bool SendOutputMetrics(const WaylandDisplayOutput& output,
                         uint32_t changed_metrics);

  // Dispatches output metrics to a specific client bound to the output manager.
  void SendOutputMetricsForClient(const WaylandDisplayOutput& output,
                                  wl_resource* manager_resource);

  // A set of resources for clients bound to the aura output manager global.
  std::unordered_set<raw_ptr<wl_resource, CtnExperimental>> manager_resources_;

  // Gets the currently active outputs tracked by the server.
  const ActiveOutputGetter active_output_getter_;

  base::WeakPtrFactory<AuraOutputManagerV2> weak_factory_{this};
};

}  // namespace exo::wayland

#endif  // COMPONENTS_EXO_WAYLAND_ZAURA_OUTPUT_MANAGER_V2_H_
