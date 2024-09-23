// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_DATA_CONTROLS_CORE_BROWSER_TEST_UTILS_H_
#define COMPONENTS_ENTERPRISE_DATA_CONTROLS_CORE_BROWSER_TEST_UTILS_H_

#include <string>
#include <vector>

#include "components/prefs/pref_service.h"

namespace data_controls {

// Sets the Data Controls policy for testing.
void SetDataControls(PrefService* prefs,
                     std::vector<std::string> rules,
                     bool machine_scope = true);

}  // namespace data_controls

#endif  // COMPONENTS_ENTERPRISE_DATA_CONTROLS_CORE_BROWSER_TEST_UTILS_H_
