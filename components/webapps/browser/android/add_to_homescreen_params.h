// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_ANDROID_ADD_TO_HOMESCREEN_PARAMS_H_
#define COMPONENTS_WEBAPPS_BROWSER_ANDROID_ADD_TO_HOMESCREEN_PARAMS_H_

#include <memory>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace webapps {

struct ShortcutInfo;

struct AddToHomescreenParams {
  // This enum backs a UMA histogram, so it should be treated as append-only.
  // A Java counterpart will be generated for this enum.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.webapps
  enum class AppType {
    // Native Android app.
    NATIVE,
    // WebAPK installed from websites with PWA manifest.
    WEBAPK,
    // Shortcut, which opens in a new tab in Chrome browser app.
    SHORTCUT,
    // WebAPK installed from websites without PWA manifest.
    WEBAPK_DIY,
    // Auto-minted TWA installed from websites with PWA manifest.
    TWA,
    kMaxValue = TWA,
  };

  AddToHomescreenParams() = delete;
  AddToHomescreenParams(AppType type,
                        std::unique_ptr<ShortcutInfo> info,
                        const SkBitmap& primary_icon,
                        const InstallableStatusCode status_code,
                        const WebappInstallSource source);
  AddToHomescreenParams(
      const std::string& package_name,
      const base::android::ScopedJavaGlobalRef<jobject> native_java_app_data,
      const SkBitmap& primary_icon,
      const WebappInstallSource source);
  ~AddToHomescreenParams();

  bool HasMaskablePrimaryIcon() const;
  bool IsWebApk() const;
  static bool IsWebApk(AppType type);

  // Returns the AppType that should be used to install a web app that was
  // determined to be installable. The argument `has_manifest` specifies whether
  // the web app has PWA manifest.
  static AppType GetWebAppInstallType(bool has_manifest);

  AppType app_type;
  SkBitmap primary_icon;
  std::unique_ptr<ShortcutInfo> shortcut_info;
  WebappInstallSource install_source;
  InstallableStatusCode installable_status;
  std::string native_app_package_name;
  base::android::ScopedJavaGlobalRef<jobject> native_app_data;
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_ANDROID_ADD_TO_HOMESCREEN_PARAMS_H_
