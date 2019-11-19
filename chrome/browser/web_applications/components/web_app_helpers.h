// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_HELPERS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_HELPERS_H_

#include <string>

class GURL;

namespace web_app {

// App ID matches Extension ID.
using AppId = std::string;

// Compute a deterministic name based on the URL. We use this pseudo name
// as a key to store window location per application URLs in Browser and
// as app id for BrowserWindow, shortcut and jump list.
std::string GenerateApplicationNameFromURL(const GURL& url);

// Compute a deterministic name based on an apps's id.
std::string GenerateApplicationNameFromAppId(const AppId& app_id);

// Compute a name for Focus Mode, using counter;
// TODO(crbug.com/943194): Move this method to Focus Mode specific file.
std::string GenerateApplicationNameForFocusMode();

// Extracts the application id from the app name.
AppId GetAppIdFromApplicationName(const std::string& app_name);

// Compute the App ID (such as "fedbieoalmbobgfjapopkghdmhgncnaa") or
// App Key, from a web app's URL. Both are derived from a hash of the
// URL, but are subsequently encoded differently, for historical reasons. The
// ID is a Base-16 encoded (a=0, b=1, ..., p=15) subset of the hash, and is
// used as a directory name, sometimes on case-insensitive file systems
// (Windows). The Key is a Base-64 encoding of the hash.
//
// For PWAs (progressive web apps), the URL should be the Start URL, explicitly
// listed in the manifest.
//
// For non-PWA web apps, also known as "shortcuts", the URL is just the
// bookmark URL.
//
// App ID and App Key match Extension ID and Extension Key for migration.
AppId GenerateAppIdFromURL(const GURL& url);
std::string GenerateAppKeyFromURL(const GURL& url);

// Returns whether the given |app_url| is a valid web app url.
bool IsValidWebAppUrl(const GURL& app_url);

// Returns whether the given |app_url| is a valid extension url.
bool IsValidExtensionUrl(const GURL& app_url);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_HELPERS_H_
