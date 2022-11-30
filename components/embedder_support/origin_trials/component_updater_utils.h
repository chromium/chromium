// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EMBEDDER_SUPPORT_ORIGIN_TRIALS_COMPONENT_UPDATER_UTILS_H_
#define COMPONENTS_EMBEDDER_SUPPORT_ORIGIN_TRIALS_COMPONENT_UPDATER_UTILS_H_

class PrefService;

namespace base {
class Value;
}  // namespace base

namespace embedder_support {

// Read the configuration from |manifest| and set values in |local_state|.
// If an individual configuration value is missing, reset values in
// local_state|.
void ReadOriginTrialsConfigAndPopulateLocalState(PrefService* local_state,
                                                 base::Value manifest);

}  // namespace embedder_support

#endif  // COMPONENTS_EMBEDDER_SUPPORT_ORIGIN_TRIALS_COMPONENT_UPDATER_UTILS_H_
