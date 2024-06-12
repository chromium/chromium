// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_HELPERS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_HELPERS_H_

#include <optional>
#include <string>

#include "base/functional/callback_helpers.h"
#include "components/webapps/common/web_app_id.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"
#include "url/gurl.h"

class GURL;
class Profile;

namespace web_app {

class WebApp;

extern const char kCrxAppPrefix[];

// Compute a deterministic name based on the URL. We use this pseudo name
// as a key to store window location per application URLs in Browser and
// as app id for BrowserWindow, shortcut and jump list.
std::string GenerateApplicationNameFromURL(const GURL& url);

// Compute a deterministic name based on an apps's id.
std::string GenerateApplicationNameFromAppId(const webapps::AppId& app_id);

// Extracts the application id from the app name.
webapps::AppId GetAppIdFromApplicationName(const std::string& app_name);

// Compute the webapps::AppId using the given start_url and optional manifest
// id path, which is the path component of the manifest id defined by the spec.
// This mimics what is given to the spec algorithm as the json manifest_id in
// https://www.w3.org/TR/appmanifest/#id-member. The `manifest_id_path` can
// include query arguments and/or fragments, although the fragment will be
// removed. See the `webapps::AppId` type for more information.
//
// This should only be used if a `Manifest` object is not available.
//
// TODO(b/281881755): Change the optional parameter to required, and refactor
// calls with std::nullopt to `GenerateManifestIdFromStartUrlOnly`.
webapps::AppId GenerateAppId(const std::optional<std::string>& manifest_id_path,
                             const GURL& start_url,
                             const std::optional<webapps::ManifestId>&
                                 parent_manifest_id = std::nullopt);

// Generates the chrome-specific `webapps::AppId` from the spec-defined
// manifest. See the `webapps::AppId` type for more information. This will
// CHECK-fail if the `id` field is not present on the manifest.
webapps::AppId GenerateAppIdFromManifest(
    const blink::mojom::Manifest& manifest,
    const std::optional<webapps::ManifestId>& parent_manifest_id =
        std::nullopt);

// Generates the chrome-specific `webapps::AppId` from the spec-defined manifest
// id. See the `webapps::AppId` type for more information.
webapps::AppId GenerateAppIdFromManifestId(

    const webapps::ManifestId& manifest_id,
    const std::optional<webapps::ManifestId>& parent_manifest_id =
        std::nullopt);

// Generates a manifest id by only the start_url, which matches the spec
// algorithm in https://www.w3.org/TR/appmanifest/#id-member where the `id` json
// member is not present or an empty string. To include an identifier path,
// please use `GenerateManifestId`.
//
// This should only be used if a `Manifest` object is not available.
webapps::ManifestId GenerateManifestIdFromStartUrlOnly(const GURL& start_url);

// Returns a resolved manifest id given the relative `manifest_id_path`,
// as per the spec algorithm at https://www.w3.org/TR/appmanifest/#id-member.
// The `manifest_id_path` can include query arguments and/or fragments, although
// the fragment will be removed. If there is no `manifest_id_path`, then
// GenerateManifestIdFromStartUrlOnly can be used.
//
// This should only be used if a `Manifest` object is not available.
webapps::ManifestId GenerateManifestId(const std::string& manifest_id_path,
                                       const GURL& start_url);

// Same as above but does not CHECK that the resulting id is valid. Only used
// for sync parsing to avoid crashes, and ignore bad sync data.
webapps::ManifestId GenerateManifestIdUnsafe(
    const std::string& manifest_id_path,
    const GURL& start_url);

// Returns whether the given |app_url| is a valid web app url.
bool IsValidWebAppUrl(const GURL& app_url);

// Adds chrome://`host` as an origin that IsValidWebAppUrl will consider valid.
// The returned ScopedClosureRunner undoes this registration.
base::ScopedClosureRunner AddValidWebAppChromeUrlHostForTesting(
    const std::string& host);

// Searches for the first locally installed app id in the registry for which
// the |url| is in scope. If |window_only| is specified, only apps that
// open in app windows will be considered.
std::optional<webapps::AppId> FindInstalledAppWithUrlInScope(
    Profile* profile,
    const GURL& url,
    bool window_only = false);

// Searches for the first app id in the registry which is not locally installed
// and for which the |url| is in scope.
bool IsNonLocallyInstalledAppWithUrlInScope(Profile* profile, const GURL& url);

// Tests if `app` is marked as a placeholder app or appears to be one despite
// not being marked due to corruption, see: https://crbug.com/1427340
bool LooksLikePlaceholder(const WebApp& app);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_HELPERS_H_
