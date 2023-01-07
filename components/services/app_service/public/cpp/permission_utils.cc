// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/permission_utils.h"

namespace apps_util {

bool IsPermissionEnabled(
    const apps::mojom::PermissionValuePtr& permission_value) {
  if (permission_value->is_tristate_value()) {
    return permission_value->get_tristate_value() ==
           apps::mojom::TriState::kAllow;
  } else if (permission_value->is_bool_value()) {
    return permission_value->get_bool_value();
  }
  return false;
}

}  // namespace apps_util
