// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_LAUNCH_UTIL_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_LAUNCH_UTIL_H_

#include "components/services/app_service/public/mojom/types.mojom.h"

namespace apps {

// Don't remove items or change the order of this enum.  It's used in
// histograms and preferences.
enum class LaunchContainer {
  kLaunchContainerWindow = 0,
  kLaunchContainerPanelDeprecated = 1,
  kLaunchContainerTab = 2,
  // For platform apps, which don't actually have a container (they just get a
  // "onLaunched" event).
  kLaunchContainerNone = 3,

  // Add any new values above this one, and update kMaxValue to the highest
  // enumerator value.
  kMaxValue = kLaunchContainerNone,
};

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_LAUNCH_UTIL_H_
