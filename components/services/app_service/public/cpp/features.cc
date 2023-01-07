// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/features.h"

namespace apps {

BASE_FEATURE(kAppServiceLaunchWithoutMojom,
             "AppServiceLaunchWithoutMojom",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAppServiceSetPermissionWithoutMojom,
             "AppServiceSetPermissionWithoutMojom",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAppServiceUninstallWithoutMojom,
             "AppServiceUninstallWithoutMojom",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAppServiceWithoutMojom,
             "AppServiceWithoutMojom",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAppServiceGetMenuWithoutMojom,
             "AppServiceGetMenuWithoutMojom",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAppServiceCapabilityAccessWithoutMojom,
             "AppServiceCapabilityAccessWithoutMojom",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kStopMojomAppService,
             "StopMojomAppService",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace apps
