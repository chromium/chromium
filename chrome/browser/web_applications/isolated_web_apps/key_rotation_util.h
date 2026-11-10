// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_KEY_ROTATION_UTIL_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_KEY_ROTATION_UTIL_H_

#include <optional>

#include "base/memory/raw_span.h"
#include "chrome/browser/web_applications/model/isolation_data.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"

namespace web_app {

// Provides the key rotation data associated with a particular IWA.
struct KeyRotationData {
  base::raw_span<const uint8_t> rotated_key;

  // Tells whether the current app installation contains the rotated key
  // (`iwa.isolation_data.integrity_block_data`).
  bool current_installation_has_rk;

  // Tells whether the pending update (if any) for the app contains the rotated
  // key (`iwa.isolation_data.pending_update_info.integrity_block_data`).
  bool pending_update_has_rk;
};

// Computes the key rotation data for `web_bundle_id` wrt rules above. Will
// return `std::nullopt` if there's no key rotation entry for this
// `web_bundle_id`.
std::optional<KeyRotationData> GetKeyRotationData(
    const web_package::SignedWebBundleId& web_bundle_id,
    const IsolationData& isolation_data);

// Returns true if a key rotation for `web_bundle_id` revokes the key(s) that
// signed the current installation, i.e. `isolation_data.integrity_block_data`
// contains neither the rotated key nor the previous key that is still trusted
// for existing installations. Returns false if there's no key rotation entry
// for this `web_bundle_id`. An installation without integrity block data is
// treated as revoked if a key rotation entry exists.
bool IsInstallationSigningKeyRevoked(
    const web_package::SignedWebBundleId& web_bundle_id,
    const IsolationData& isolation_data);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_KEY_ROTATION_UTIL_H_
