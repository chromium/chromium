// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_DATA_CONTROLS_PREFS_H_
#define COMPONENTS_ENTERPRISE_DATA_CONTROLS_PREFS_H_

class PrefRegistrySimple;

namespace data_controls {

// Pref that maps to the "DataControlsRules" policy.
inline constexpr char kDataControlsRulesPref[] = "data_controls.rules";

void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace data_controls

#endif  // COMPONENTS_ENTERPRISE_DATA_CONTROLS_PREFS_H_
