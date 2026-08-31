// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/net/core/flag_descriptions.h"

#if BUILDFLAG(ENTERPRISE_PROXY)

namespace enterprise_net::flag_descriptions {

const char kEnableDynamicRouteFetchingName[] = "Enable Dynamic Route Fetching";
const char kEnableDynamicRouteFetchingDescription[] =
    "Enables fetching proxy configurations and routing rules dynamically from "
    "Provisioning Domains.";

const char kEnterpriseProxyErrorHandlingName[] =
    "Enterprise Proxy Error Handling";
const char kEnterpriseProxyErrorHandlingDescription[] =
    "Enables handling and displaying dedicated error pages for enterprise "
    "proxy authentication challenges and disguised errors.";

}  // namespace enterprise_net::flag_descriptions

#endif  // BUILDFLAG(ENTERPRISE_PROXY)
