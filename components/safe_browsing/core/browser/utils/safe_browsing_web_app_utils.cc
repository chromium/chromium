// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/utils/safe_browsing_web_app_utils.h"

#include <string_view>

#include "base/strings/strcat.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace safe_browsing {

std::optional<SafeBrowsingWebAppKey> GetSafeBrowsingWebAppKey(
    const GURL& start_url,
    const GURL& manifest_id) {
  url::Origin app_origin = url::Origin::Create(start_url);
  if (app_origin.opaque()) {
    return std::nullopt;
  }
  if (manifest_id.is_valid() && !app_origin.IsSameOriginWith(manifest_id)) {
    return std::nullopt;
  }

  const GURL& url_for_id = manifest_id.is_valid() ? manifest_id : start_url;
  std::string_view id_path_piece = url_for_id.PathForRequestPiece();
  // This is possible in some pathological cases, but should typically not
  // happen for a well-formed https URL.
  if (id_path_piece.empty()) {
    return std::nullopt;
  }
  // Remove the leading slash character.
  id_path_piece.remove_prefix(1);

  SafeBrowsingWebAppKey web_app_key;
  web_app_key.set_id_or_start_path(std::string(id_path_piece));

  if (app_origin.port() !=
      url::DefaultPortForScheme(start_url.scheme_piece())) {
    web_app_key.set_start_url_origin(
        base::StrCat({start_url.host_piece(), ":", start_url.port_piece()}));
  } else {
    web_app_key.set_start_url_origin(start_url.host());
  }

  return web_app_key;
}

}  // namespace safe_browsing
