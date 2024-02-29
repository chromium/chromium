// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_VALIDATOR_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_VALIDATOR_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/types/expected.h"
#include "chrome/browser/web_applications/isolated_web_apps/error/unusable_swbn_file_error.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_signature_stack.h"

class GURL;

namespace web_package {
class SignedWebBundleId;
}

namespace web_app {

class IsolatedWebAppTrustChecker;

class IsolatedWebAppValidator {
 public:
  explicit IsolatedWebAppValidator(
      std::unique_ptr<const IsolatedWebAppTrustChecker>
          isolated_web_app_trust_checker);
  virtual ~IsolatedWebAppValidator();

  // Validates that the integrity block of the Isolated Web App contains trusted
  // public keys given the `web_bundle_id`. Returns `std::nullopt` on success,
  // or an error message if the public keys are not trusted.
  virtual void ValidateIntegrityBlock(
      const web_package::SignedWebBundleId& expected_web_bundle_id,
      const web_package::SignedWebBundleIntegrityBlock& integrity_block,
      bool dev_mode,
      base::OnceCallback<void(std::optional<std::string>)> callback);

  // Validates that the metadata of the Isolated Web App is valid given the
  // `web_bundle_id`.
  [[nodiscard]] virtual base::expected<void, UnusableSwbnFileError>
  ValidateMetadata(const web_package::SignedWebBundleId& web_bundle_id,
                   const std::optional<GURL>& primary_url,
                   const std::vector<GURL>& entries);

 private:
  std::unique_ptr<const IsolatedWebAppTrustChecker>
      isolated_web_app_trust_checker_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_VALIDATOR_H_
