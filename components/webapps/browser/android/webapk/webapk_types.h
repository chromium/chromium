// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_ANDROID_WEBAPK_WEBAPK_TYPES_H_
#define COMPONENTS_WEBAPPS_BROWSER_ANDROID_WEBAPK_WEBAPK_TYPES_H_

namespace webapps {

// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.webapps
//
// Indicates the reason that a WebAPK update is requested.
enum class WebApkUpdateReason {
  NONE,
  OLD_SHELL_APK,
  PRIMARY_ICON_HASH_DIFFERS,
  PRIMARY_ICON_MASKABLE_DIFFERS,
  SPLASH_ICON_HASH_DIFFERS,
  SCOPE_DIFFERS,
  START_URL_DIFFERS,
  SHORT_NAME_DIFFERS,
  NAME_DIFFERS,
  BACKGROUND_COLOR_DIFFERS,
  THEME_COLOR_DIFFERS,
  ORIENTATION_DIFFERS,
  DISPLAY_MODE_DIFFERS,
  WEB_SHARE_TARGET_DIFFERS,
  MANUALLY_TRIGGERED,
  SHORTCUTS_DIFFER,
  DARK_BACKGROUND_COLOR_DIFFERS,
  DARK_THEME_COLOR_DIFFERS,
  PRIMARY_ICON_CHANGE_BELOW_THRESHOLD,
  PRIMARY_ICON_CHANGE_SHELL_UPDATE,
};

// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.webapps
//
// This enum is used to back UMA/UKM histograms, and should therefore be treated
// as append-only.
//
// Indicates the distributor or "install source" of a WebAPK.
enum class WebApkDistributor {
  BROWSER = 0,
  DEVICE_POLICY = 1,
  OTHER = 2,
};

// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.webapps
//
// Indicates the result of an WebAPK install.
//
// This enum is used to back UMA/UKM histograms, and should therefore be treated
// as append-only.
//
// LINT.IfChange(WebApkInstallResult)
enum class WebApkInstallResult {
  SUCCESS = 0,
  // Install WebAPK with the installer service (i.e. Google Play) failed.
  FAILURE = 1,
  // An install was initiated but it timed out. We did not get a response from
  // the install service so it is possible that the install will complete some
  // time in the future.
  PROBABLE_FAILURE = 2,

  // No install service to complete the install.
  NO_INSTALLER = 3,

  SERVER_URL_INVALID = 4,
  // Server returns an error or unexpected result.
  SERVER_ERROR = 5,
  // Request to server timed out.
  REQUEST_TIMEOUT = 6,
  // The request proto is invalid.
  REQUEST_INVALID = 7,

  NOT_ENOUGH_SPACE = 8,
  ICON_HASHER_ERROR = 9,
  // RESULT_MAX = 10,  // Deprecated.

  // Indicates that the WebAPK is currently already being installed and the new
  // install will be aborted. Used when the install was initiated through the
  // WebApkInstallCoordinator-service to propagate the status to the connecting
  // client.
  INSTALL_ALREADY_IN_PROGRESS = 11,
  kMaxValue = INSTALL_ALREADY_IN_PROGRESS,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/web_apk/enums.xml:WebApkInstallResult)

// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.webapps
//
// Lists the fields containing information about the app, which are shown on
// the default offline experience page.
enum class WebApkDetailsForDefaultOfflinePage {
  SHORT_NAME = 0,
  ICON,
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_ANDROID_WEBAPK_WEBAPK_TYPES_H_
