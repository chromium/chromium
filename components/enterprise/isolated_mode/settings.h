// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_ISOLATED_MODE_SETTINGS_H_
#define COMPONENTS_ENTERPRISE_ISOLATED_MODE_SETTINGS_H_

#include "components/version_info/channel.h"

class PrefService;

namespace enterprise_isolated_mode {

// Returns true if Isolated Mode should replace Incognito mode.
// First checks the command line switch (if not in Beta/Stable).
// Otherwise, requires both the kEnableEnterpriseIsolatedMode Feature and the
// IsolatedModeSettings policy.
bool IsolatedModeReplacesIncognito(const PrefService& pref_service,
                                   version_info::Channel channel);

namespace switches {
inline constexpr char kForceEnterpriseIsolatedModeReplacesIncognito[] =
    "force-enterprise-isolated-mode-replaces-incognito";
}  // namespace switches

}  // namespace enterprise_isolated_mode

#endif  // COMPONENTS_ENTERPRISE_ISOLATED_MODE_SETTINGS_H_
