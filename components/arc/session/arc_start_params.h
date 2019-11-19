// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_SESSION_ARC_START_PARAMS_H_
#define COMPONENTS_ARC_SESSION_ARC_START_PARAMS_H_

#include <stdint.h>

#include "base/macros.h"

namespace arc {

// Parameters to start request.
struct StartParams {
  enum class PlayStoreAutoUpdate {
    // Play Store auto-update is left unchanged.
    AUTO_UPDATE_DEFAULT = 0,
    // Play Store auto-update is forced to on.
    AUTO_UPDATE_ON,
    // Play Store auto-update is forced to off.
    AUTO_UPDATE_OFF,
  };

  StartParams();
  ~StartParams();
  StartParams(StartParams&& other);
  StartParams& operator=(StartParams&& other);

  bool native_bridge_experiment = false;
  int lcd_density = -1;

  // Experiment flag for go/arc-file-picker.
  bool arc_file_picker_experiment = false;

  // Optional mode for play store auto-update.
  PlayStoreAutoUpdate play_store_auto_update =
      PlayStoreAutoUpdate::AUTO_UPDATE_DEFAULT;

  // Experiment flag for ARC Custom Tabs.
  bool arc_custom_tabs_experiment = false;

  // Experiment flag for ARC Print Spooler.
  bool arc_print_spooler_experiment = false;

  // The number of logical CPU cores that are currently disabled on the host.
  // This parameter is used only for starting ARCVM.
  uint32_t num_cores_disabled = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(StartParams);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_SESSION_ARC_START_PARAMS_H_
