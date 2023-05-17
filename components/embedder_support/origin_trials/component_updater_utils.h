// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EMBEDDER_SUPPORT_ORIGIN_TRIALS_COMPONENT_UPDATER_UTILS_H_
#define COMPONENTS_EMBEDDER_SUPPORT_ORIGIN_TRIALS_COMPONENT_UPDATER_UTILS_H_

#include "base/values.h"

class PrefService;

namespace embedder_support {

class OriginTrialsSettingsStorage;

// Read the configuration from `manifest` and set values in `local_state`.
// If an individual configuration value is missing, reset values in
// `local_state`.
void ReadOriginTrialsConfigAndPopulateLocalState(PrefService* local_state,
                                                 base::Value::Dict manifest);

// Append the stored Origin Trial configuration overrides to the current process
// command line, if the command line does not already contain these values. This
// should be done early during browser startup.
void SetupOriginTrialsCommandLineAndSettings(
    PrefService* local_state,
    OriginTrialsSettingsStorage* settings_storage);

}  // namespace embedder_support

#endif  // COMPONENTS_EMBEDDER_SUPPORT_ORIGIN_TRIALS_COMPONENT_UPDATER_UTILS_H_
