// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_VALIDATOR_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_VALIDATOR_H_

#include <vector>

#include "components/web_package/signed_web_bundles/ed25519_public_key.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class GURL;

namespace web_package {
class SignedWebBundleId;
}

namespace web_app {

class IsolatedWebAppValidator {
 public:
  IsolatedWebAppValidator() = default;
  virtual ~IsolatedWebAppValidator() = default;

  // Validates that the integrity block of the Isolated Web App contains trusted
  // signatures given the `web_bundle_id`. Returns `absl::nullopt` on success,
  // or an error message if the signatures are not trusted.
  [[nodiscard]] virtual absl::optional<std::string> ValidateIntegrityBlock(
      web_package::SignedWebBundleId web_bundle_id,
      const std::vector<web_package::Ed25519PublicKey>& public_key_stack);

  // Validates that the metadata of the Isolated Web App is valid given the
  // `web_bundle_id`. Returns `absl::nullopt` on success, or an error message if
  // metadata is invalid.
  [[nodiscard]] virtual absl::optional<std::string> ValidateMetadata(
      web_package::SignedWebBundleId web_bundle_id,
      const GURL& primary_url,
      const std::vector<GURL>& entries);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_VALIDATOR_H_
