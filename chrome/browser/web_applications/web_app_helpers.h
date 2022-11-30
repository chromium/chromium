// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_HELPERS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_HELPERS_H_

#include <string>

#include "chrome/browser/web_applications/web_app_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"

class GURL;
class Profile;

namespace web_app {

extern const char kCrxAppPrefix[];

// Compute a deterministic name based on the URL. We use this pseudo name
// as a key to store window location per application URLs in Browser and
// as app id for BrowserWindow, shortcut and jump list.
std::string GenerateApplicationNameFromURL(const GURL& url);

// Compute a deterministic name based on an apps's id.
std::string GenerateApplicationNameFromAppId(const AppId& app_id);

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

// Generate App id using manifest_id, if null, use start_url instead.
AppId GenerateAppId(const absl::optional<std::string>& manifest_id,
                    const GURL& start_url);
std::string GenerateAppIdUnhashed(
    const absl::optional<std::string>& manifest_id,
    const GURL& start_url);
AppId GenerateAppIdFromUnhashed(std::string unhashed_app_id);

std::string GenerateAppIdUnhashedFromManifest(
    const blink::mojom::Manifest& manifest);

AppId GenerateAppIdFromManifest(const blink::mojom::Manifest& manifest);

// Suggests recommended id to be specified to match with computed |app_id|
// generated from start_url.
std::string GenerateRecommendedId(const GURL& start_url);

// Returns whether the given |app_url| is a valid web app url.
bool IsValidWebAppUrl(const GURL& app_url);

// Searches for the first locally installed app id in the registry for which
// the |url| is in scope. If |window_only| is specified, only apps that
// open in app windows will be considered.
absl::optional<AppId> FindInstalledAppWithUrlInScope(Profile* profile,
                                                     const GURL& url,
                                                     bool window_only = false);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_HELPERS_H_
