// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_NET_CORE_PREFS_H_
#define COMPONENTS_ENTERPRISE_NET_CORE_PREFS_H_

#include "components/enterprise/buildflags/buildflags.h"

#if BUILDFLAG(ENTERPRISE_PROXY)

class PrefRegistrySimple;

namespace enterprise_net {

// Preference to store the value of the "ProxyProvisioningDomains" policy.
extern const char kProxyProvisioningDomains[];

// Preference to store cached Provisioning Domain configurations.
extern const char kProvisioningDomainProxyConfigs[];

// Registers profile preferences in the registry.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace enterprise_net

#endif  // BUILDFLAG(ENTERPRISE_PROXY)

#endif  // COMPONENTS_ENTERPRISE_NET_CORE_PREFS_H_
