// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_SHARE_TARGET_UTILS_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_SHARE_TARGET_UTILS_H_

#include <string>
#include <vector>

#include "chrome/browser/ui/browser_navigator_params.h"
#include "components/services/app_service/public/mojom/types.mojom.h"

class Browser;

namespace apps {
struct ShareTarget;
}  // namespace apps

namespace web_app {

struct SharedField {
  std::string name;
  std::string value;

  bool operator==(const SharedField& other) const;
  bool operator!=(const SharedField& other) const { return !(*this == other); }
};

std::vector<SharedField> ExtractSharedFields(
    const apps::ShareTarget& share_target,
    const apps::mojom::Intent& intent);

NavigateParams NavigateParamsForShareTarget(
    Browser* browser,
    const apps::ShareTarget& share_target,
    const apps::mojom::Intent& intent);

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_SHARE_TARGET_UTILS_H_
