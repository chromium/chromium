// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_OUTPUT_CONFIGURATION_CHANGE_H_
#define COMPONENTS_EXO_WAYLAND_OUTPUT_CONFIGURATION_CHANGE_H_

#include <vector>

#include "base/memory/raw_ptr.h"

namespace exo::wayland {

class WaylandDisplayOutput;

using WaylandOutputList = std::vector<raw_ptr<const WaylandDisplayOutput>>;

// Pairs the changed output with a bitvector of DisplayMetric changes.
using WaylandOutputChangedList =
    std::vector<std::pair<raw_ptr<const WaylandDisplayOutput>, uint32_t>>;

// Encapsulates an atomic change to the output configuration tracked by Exo.
struct OutputConfigurationChange {
  OutputConfigurationChange();
  OutputConfigurationChange(OutputConfigurationChange&& other);
  OutputConfigurationChange& operator=(OutputConfigurationChange&& other);
  OutputConfigurationChange(const OutputConfigurationChange&) = delete;
  OutputConfigurationChange& operator=(const OutputConfigurationChange&) =
      delete;
  ~OutputConfigurationChange();

  // New outputs for user-visible displays added to the system.
  WaylandOutputList added_outputs;

  // Outputs for displays removed from the system's configuration.
  WaylandOutputList removed_outputs;

  // Existing outputs updated to reflect updated display metrics.
  WaylandOutputChangedList changed_outputs;
};

}  // namespace exo::wayland

#endif  // COMPONENTS_EXO_WAYLAND_OUTPUT_CONFIGURATION_CHANGE_H_
