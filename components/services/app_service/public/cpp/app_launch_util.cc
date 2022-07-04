// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/app_launch_util.h"

namespace apps {

apps::mojom::LaunchContainer ConvertLaunchContainerToMojomLaunchContainer(
    LaunchContainer launch_container) {
  switch (launch_container) {
    case LaunchContainer::kLaunchContainerWindow:
      return apps::mojom::LaunchContainer::kLaunchContainerWindow;
    case LaunchContainer::kLaunchContainerPanelDeprecated:
      return apps::mojom::LaunchContainer::kLaunchContainerPanelDeprecated;
    case LaunchContainer::kLaunchContainerTab:
      return apps::mojom::LaunchContainer::kLaunchContainerTab;
    case LaunchContainer::kLaunchContainerNone:
      return apps::mojom::LaunchContainer::kLaunchContainerNone;
  }
}

}  // namespace apps
