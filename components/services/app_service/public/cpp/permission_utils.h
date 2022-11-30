// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_PERMISSION_UTILS_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_PERMISSION_UTILS_H_

#include "components/services/app_service/public/mojom/types.mojom.h"

namespace apps_util {

// Check whether the |permission_value| equavalent to permission enabled.
// The permission value could be a TriState or a bool. If it is TriState,
// only Allow represent permission enabled.
bool IsPermissionEnabled(
    const apps::mojom::PermissionValuePtr& permission_value);

}  // namespace apps_util

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_PERMISSION_UTILS_H_
