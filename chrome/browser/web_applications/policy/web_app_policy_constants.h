// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_POLICY_WEB_APP_POLICY_CONSTANTS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_POLICY_WEB_APP_POLICY_CONSTANTS_H_

namespace web_app {

// Keys and values for the WebAppInstallForceList preference.
extern const char kUrlKey[];
extern const char kDefaultLaunchContainerKey[];
extern const char kDefaultLaunchContainerWindowValue[];
extern const char kDefaultLaunchContainerTabValue[];
extern const char kCreateDesktopShortcutKey[];
extern const char kFallbackAppNameKey[];
extern const char kCustomNameKey[];
extern const char kCustomIconKey[];
extern const char kCustomIconURLKey[];
extern const char kCustomIconHashKey[];
extern const char kInstallAsShortcut[];
extern const char kUninstallAndReplaceKey[];

extern const char kWildcard[];

extern const char kManifestId[];
extern const char kRunOnOsLogin[];
extern const char kAllowed[];
extern const char kBlocked[];
extern const char kRunWindowed[];
extern const char kPreventClose[];
extern const char kForceUnregisterOsIntegration[];

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_POLICY_WEB_APP_POLICY_CONSTANTS_H_
