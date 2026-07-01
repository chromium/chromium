// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/app_id_helpers.h"

#include "components/crx_file/id_util.h"
#include "crypto/sha2.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace web_app::internal {

webapps::AppId GenerateAppId(const std::optional<std::string>& manifest_id_path,
                             const GURL& start_url) {
  if (!manifest_id_path) {
    return GenerateAppIdFromManifestId(
        GenerateManifestIdFromStartUrlOnly(start_url));
  }
  return GenerateAppIdFromManifestId(
      GenerateManifestId(manifest_id_path.value(), start_url));
}

webapps::AppId GenerateAppIdFromManifest(
    const blink::mojom::Manifest& manifest) {
  std::optional<webapps::ManifestId> manifest_id =
      webapps::ManifestId::Create(manifest.id);
  CHECK(manifest_id.has_value());
  return GenerateAppIdFromManifestId(manifest_id.value());
}

webapps::AppId GenerateAppIdFromManifestId(
    const webapps::ManifestId& manifest_id) {
  // The app ID is hashed twice: here and in GenerateId.
  // The double-hashing is for historical reasons and it needs to stay
  // this way for backwards compatibility. (Back then, a web app's input to the
  // hash needed to be formatted like an extension public key.)
  return crx_file::id_util::GenerateId(
      crypto::SHA256HashString(manifest_id.spec()));
}

webapps::ManifestId GenerateManifestIdFromStartUrlOnly(const GURL& start_url) {
  std::optional<webapps::ManifestId> manifest_id =
      webapps::ManifestId::Create(start_url);
  CHECK(manifest_id.has_value()) << start_url.spec();
  return *manifest_id;
}

webapps::ManifestId GenerateManifestId(const std::string& manifest_id_path,
                                       const GURL& start_url) {
  std::optional<webapps::ManifestId> manifest_id =
      GenerateManifestIdUnsafe(manifest_id_path, start_url);
  CHECK(manifest_id.has_value())
      << "start_url: " << start_url << ", manifest_id = " << manifest_id_path;
  return *manifest_id;
}

std::optional<webapps::ManifestId> GenerateManifestIdUnsafe(
    const std::string& manifest_id_path,
    const GURL& start_url) {
  // When manifest_id_path is specified, the manifest_id is generated from
  // <start_url_origin>/<manifest_id_path>.
  // Note: start_url.DeprecatedGetOriginAsURL().spec() returns the origin ending
  // with slash.
  const GURL manifest_id(start_url.DeprecatedGetOriginAsURL().spec() +
                         manifest_id_path);

  return webapps::ManifestId::Create(manifest_id);
}

}  // namespace web_app::internal
