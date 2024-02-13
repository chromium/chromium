// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_OUTPUT_CONTROLLER_H_
#define COMPONENTS_EXO_WAYLAND_OUTPUT_CONTROLLER_H_

#include "ash/shell_observer.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/display_manager_observer.h"

struct wl_client;
struct wl_display;
struct wl_resource;

namespace exo::wayland {

class AuraOutputManagerV2;
class WaylandDisplayOutput;

// Responsible for keeping output state consistent with system display state and
// notifying connected wayland clients of any output state changes.
struct OutputController : public display::DisplayManagerObserver,
                          public ash::ShellObserver {
 public:
  using DisplayOutputMap =
      base::flat_map<int64_t, std::unique_ptr<WaylandDisplayOutput>>;
  using WaylandOutputList = std::vector<raw_ptr<const WaylandDisplayOutput>>;

  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual wl_display* GetWaylandDisplay() = 0;
    virtual void Flush() = 0;
  };

  explicit OutputController(Delegate* delegate);
  OutputController(const OutputController&) = delete;
  OutputController& operator=(const OutputController&) = delete;
  ~OutputController() override;

  // display::DisplayManagerObserver:
  void OnDidProcessDisplayChanges(
      const DisplayConfigurationChange& configuration_change) override;

  // ash::ShellObserver:
  void OnDisplayForNewWindowsChanged() override;

  // Returns the wl_resource for the wl_output bound to the `client`.
  wl_resource* GetOutputResource(wl_client* client, int64_t display_id);

 private:
  friend class OutputControllerTestApi;

  // Returns the WaylandDisplayOutput for the wl_output global associated with
  // the `display_id`.
  WaylandDisplayOutput* GetWaylandDisplayOutput(int64_t display_id);

  // Returns a list of the current active outputs tracked by the server.
  WaylandOutputList GetActiveOutputs() const;

  // Sends the necessary events for a configuration change to clients of the the
  // aura output manager. Returns true if update events were sent and a done is
  // needed.
  bool ProcessDisplayChangesForAuraOutputManager(
      const DisplayConfigurationChange& configuration_change);

  // Updates exo to align with the system's currently active window.
  void UpdateActivatedDisplayIfNecessary();

  // This tracks display-activation-change events dispatched by the controller.
  int64_t dispatched_activated_display_id_ = display::kInvalidDisplayId;

  DisplayOutputMap outputs_;
  std::unique_ptr<AuraOutputManagerV2> aura_output_manager_v2_;

  // The delegate will strictly outlive the controller.
  const raw_ptr<Delegate> delegate_;

  base::ScopedObservation<display::DisplayManager,
                          display::DisplayManagerObserver>
      display_manager_observation_{this};
};

}  // namespace exo::wayland

#endif  // COMPONENTS_EXO_WAYLAND_OUTPUT_CONTROLLER_H_
