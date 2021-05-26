// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_LOCAL_SEARCH_SERVICE_SERVICE_SANDBOX_TYPE_H_
#define CHROMEOS_COMPONENTS_LOCAL_SEARCH_SERVICE_SERVICE_SANDBOX_TYPE_H_

#include "content/public/browser/service_process_host.h"
#include "sandbox/policy/sandbox_type.h"

// This file maps service classes to sandbox types. See
// ServiceProcessHost::Launch() for how these templates are consumed.

// chromeos::local_search_service::mojom::LocalSearchService
namespace chromeos {
namespace local_search_service {
namespace mojom {
class LocalSearchService;
}
}  // namespace local_search_service
}  // namespace chromeos
template <>
inline sandbox::policy::SandboxType content::GetServiceSandboxType<
    chromeos::local_search_service::mojom::LocalSearchService>() {
  return sandbox::policy::SandboxType::kUtility;
}

#endif  // CHROMEOS_COMPONENTS_LOCAL_SEARCH_SERVICE_SERVICE_SANDBOX_TYPE_H_
