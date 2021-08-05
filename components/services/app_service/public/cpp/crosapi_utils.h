// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_CROSAPI_UTILS_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_CROSAPI_UTILS_H_

// Utility functions for App Service crosapi usage.

#include <memory>
#include <vector>

#include "components/services/app_service/public/mojom/types.mojom.h"

namespace apps_util {

// Clone a list of apps::mojom::AppPtr.
std::vector<apps::mojom::AppPtr> CloneApps(
    const std::vector<apps::mojom::AppPtr>& clone_from);

}  // namespace apps_util

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_CROSAPI_UTILS_H_
