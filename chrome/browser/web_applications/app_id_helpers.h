// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_APP_ID_HELPERS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_APP_ID_HELPERS_H_

#include <optional>
#include <string>

#include "components/webapps/common/web_app_id.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"

class GURL;

namespace web_app::internal {

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
                             const GURL& start_url);

// Generates the chrome-specific `webapps::AppId` from the spec-defined
// manifest. See the `webapps::AppId` type for more information. This will
// CHECK-fail if the `id` field is not present on the manifest.
webapps::AppId GenerateAppIdFromManifest(
    const blink::mojom::Manifest& manifest);

// Generates the chrome-specific `webapps::AppId` from the spec-defined manifest
// id. See the `webapps::AppId` type for more information.
webapps::AppId GenerateAppIdFromManifestId(
    const webapps::ManifestId& manifest_id);

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
std::optional<webapps::ManifestId> GenerateManifestIdUnsafe(
    const std::string& manifest_id_path,
    const GURL& start_url);

}  // namespace web_app::internal

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_APP_ID_HELPERS_H_
