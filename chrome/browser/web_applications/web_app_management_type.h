// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_MANAGEMENT_TYPE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_MANAGEMENT_TYPE_H_

#include <iosfwd>

#include "base/containers/enum_set.h"

namespace web_app {

// Installations of Web Apps have different sources of management. Apps can be
// installed by different management systems - for example an app can be both
// installed by the user and by policy. Keeping track of which installation
// managers have installed a web app allows for them to be installed by multiple
// managers at the same time, and uninstalls from one manager doesn't affect
// another - the app will stay installed as long as at least one management
// source has it installed.
//
// This enum is also used to rank installation sources, so the ordering matters.
// This enum should be zero based: values are used as index in a bitset.
// We don't use this enum values in prefs or metrics: enumerators can be
// reordered. This enum is not a strongly typed enum class: it supports implicit
// conversion to int and <> comparison operators.
namespace WebAppManagement {
enum Type {
  kMinValue = 0,
  kSystem = kMinValue,
  kIwaShimlessRma,
  // Installed by Kiosk on Chrome OS.
  kKiosk,
  kPolicy,
  kIwaPolicy,
  // Installed by APS (App Preload Service) on ChromeOS as an OEM app.
  kOem,
  kSubApp,
  kWebAppStore,
  kOneDriveIntegration,
  // User-installed web apps are managed by the sync system.or
  // user-installed apps without overlaps this is the only source that will be
  // set.
  kSync,
  kUserInstalled,
  kIwaUserInstalled,
  // Installed by APS (App Preload Service) on ChromeOS as a default app. These
  // have the same UX as kDefault apps, but are are not managed by
  // PreinstalledWebAppManager.
  kApsDefault,
  // This value is used by both the PreinstalledWebAppManager AND the
  // AndroidSmsAppSetupControllerImpl, which is a potential conflict in the
  // future.
  // TODO(dmurph): Add a new source here so that the
  // AndroidSmsAppSetupControllerImpl has its own source, and migrate those
  // installations to have the new source.
  // https://crbug.com/1314055
  kDefault,
  kMaxValue = kDefault,
};

std::ostream& operator<<(std::ostream& os, WebAppManagement::Type type);

bool IsIwaType(WebAppManagement::Type type);

}  // namespace WebAppManagement

using WebAppManagementTypes = base::EnumSet<WebAppManagement::Type,
                                            WebAppManagement::kMinValue,
                                            WebAppManagement::kMaxValue>;

// Management types that can be uninstalled by the user.
// Note: These work directly with the `webapps::IsUserUninstall` function - any
// source that returns true there can uninstall these types but not others, and
// will CHECK-fail in RemoveWebAppJob otherwise.
// All WebAppManagement::Types must be listed in either this constant or
// kNotUserUninstallableSources (located in the cc file).
inline constexpr WebAppManagementTypes kUserUninstallableSources = {
    WebAppManagement::kDefault,
    WebAppManagement::kApsDefault,
    WebAppManagement::kSync,
    WebAppManagement::kUserInstalled,
    WebAppManagement::kWebAppStore,
    WebAppManagement::kSubApp,
    WebAppManagement::kOem,
    WebAppManagement::kOneDriveIntegration,
    WebAppManagement::kIwaUserInstalled,
    WebAppManagement::kIwaShimlessRma,
};

// Management types that resulted from a user web app install.
inline constexpr WebAppManagementTypes kUserDrivenInstallSources = {
    WebAppManagement::kSync,
    WebAppManagement::kUserInstalled,
    WebAppManagement::kWebAppStore,
    WebAppManagement::kOneDriveIntegration,
    WebAppManagement::kIwaUserInstalled,
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_MANAGEMENT_TYPE_H_
