// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/android/add_to_homescreen_params.h"

#include "components/webapps/browser/android/shortcut_info.h"

namespace webapps {

AddToHomescreenParams::AddToHomescreenParams() = default;
AddToHomescreenParams::~AddToHomescreenParams() = default;

std::string AddToHomescreenParams::GetAppIdentifier() {
  if (app_type == AddToHomescreenParams::AppType::NATIVE) {
    return native_app_package_name;
  }
  // TODO(crbug.com/1447006): shortcut_info shouldn't be null unless
  // app_type is NATIVE.
  DUMP_WILL_BE_CHECK(shortcut_info);
  return shortcut_info ? shortcut_info->url.spec() : "";
}

bool AddToHomescreenParams::HasMaskablePrimaryIcon() {
  return app_type != AddToHomescreenParams::AppType::NATIVE &&
         shortcut_info->is_primary_icon_maskable;
}

}  // namespace webapps
