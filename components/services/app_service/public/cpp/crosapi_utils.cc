// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/crosapi_utils.h"

namespace apps_util {

std::vector<apps::mojom::AppPtr> CloneApps(
    const std::vector<apps::mojom::AppPtr>& clone_from) {
  std::vector<apps::mojom::AppPtr> clone_to;
  for (const auto& app : clone_from) {
    clone_to.push_back(app->Clone());
  }
  return clone_to;
}

}  // namespace apps_util
