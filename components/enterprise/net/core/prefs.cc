// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/net/core/prefs.h"

#include "components/prefs/pref_registry_simple.h"

#if BUILDFLAG(ENTERPRISE_PROXY)

namespace enterprise_net {

const char kProxyProvisioningDomains[] = "proxy_provisioning_domains";
const char kProvisioningDomainProxyConfigs[] =
    "provisioning_domain_proxy_configs";

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(kProxyProvisioningDomains);
  registry->RegisterDictionaryPref(kProvisioningDomainProxyConfigs);
}

}  // namespace enterprise_net

#endif  // BUILDFLAG(ENTERPRISE_PROXY)
