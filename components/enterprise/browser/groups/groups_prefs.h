// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_GROUPS_GROUPS_PREFS_H_
#define COMPONENTS_ENTERPRISE_BROWSER_GROUPS_GROUPS_PREFS_H_

class PrefRegistrySimple;

namespace enterprise_groups {

// The name of the preference that stores the enterprise group IDs from CBCM
// policy data.
inline constexpr char kEnterpriseGroupsBrowserPref[] =
    "enterprise_groups.browser";

void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

}  // namespace enterprise_groups

#endif  // COMPONENTS_ENTERPRISE_BROWSER_GROUPS_GROUPS_PREFS_H_
