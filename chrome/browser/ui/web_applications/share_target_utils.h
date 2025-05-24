// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_SHARE_TARGET_UTILS_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_SHARE_TARGET_UTILS_H_

#include <string>
#include <vector>

#include "chrome/browser/ui/browser_navigator_params.h"
#include "components/services/app_service/public/cpp/intent.h"

class Browser;

namespace apps {
struct ShareTarget;
}  // namespace apps

namespace web_app {

struct SharedField {
  friend bool operator==(const SharedField&, const SharedField&) = default;

  std::string name;
  std::string value;
};

std::vector<SharedField> ExtractSharedFields(
    const apps::ShareTarget& share_target,
    const apps::Intent& intent);

NavigateParams NavigateParamsForShareTarget(
    Browser* browser,
    const apps::ShareTarget& share_target,
    const apps::Intent& intent,
    const std::vector<base::FilePath>& launch_files);

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_SHARE_TARGET_UTILS_H_
