// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_TYPES_UTIL_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_TYPES_UTIL_H_

// Utility functions for App Service types.

#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_types.h"

namespace apps_util {

bool IsInstalled(apps::Readiness readiness);
bool IsHumanLaunch(apps::LaunchSource launch_source);

// Checks if an app of |app_type| runs in Browser/WebContents (web apps, hosted
// apps, and packaged v1 apps).
bool AppTypeUsesWebContents(apps::AppType app_type);

}  // namespace apps_util

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_TYPES_UTIL_H_
