// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/android/add_to_homescreen_params.h"

#include "components/webapps/browser/android/shortcut_info.h"

namespace webapps {

AddToHomescreenParams::AddToHomescreenParams(
    AppType type,
    std::unique_ptr<ShortcutInfo> info,
    const SkBitmap& primary_icon,
    const InstallableStatusCode status_code,
    const WebappInstallSource source)
    : app_type(type),
      primary_icon(std::move(primary_icon)),
      shortcut_info(std::move(info)),
      install_source(source),
      installable_status(status_code) {
  CHECK(IsWebApk() || app_type == AppType::SHORTCUT);
}

AddToHomescreenParams::AddToHomescreenParams(
    const std::string& package_name,
    const base::android::ScopedJavaGlobalRef<jobject> native_java_app_data,
    const SkBitmap& primary_icon,
    const WebappInstallSource source)
    : app_type(AppType::NATIVE),
      primary_icon(std::move(primary_icon)),
      install_source(source),
      native_app_package_name(std::move(package_name)),
      native_app_data(std::move(native_java_app_data)) {}

AddToHomescreenParams::~AddToHomescreenParams() = default;

bool AddToHomescreenParams::HasMaskablePrimaryIcon() const {
  return app_type != AppType::NATIVE && shortcut_info->is_primary_icon_maskable;
}

bool AddToHomescreenParams::IsWebApk() const {
  return IsWebApk(app_type);
}

// static
bool AddToHomescreenParams::IsWebApk(AppType type) {
  return type == AppType::WEBAPK || type == AppType::WEBAPK_DIY;
}

}  // namespace webapps
