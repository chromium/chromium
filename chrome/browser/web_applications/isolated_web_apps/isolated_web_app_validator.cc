// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_validator.h"

#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_trust_checker.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/common/url_constants.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_integrity_block.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace web_app {

IsolatedWebAppValidator::IsolatedWebAppValidator(
    std::unique_ptr<const IsolatedWebAppTrustChecker>
        isolated_web_app_trust_checker)
    : isolated_web_app_trust_checker_(
          std::move(isolated_web_app_trust_checker)) {}

IsolatedWebAppValidator::~IsolatedWebAppValidator() = default;

void IsolatedWebAppValidator::ValidateIntegrityBlock(
    const web_package::SignedWebBundleId& expected_web_bundle_id,
    const web_package::SignedWebBundleIntegrityBlock& integrity_block,
    base::OnceCallback<void(absl::optional<std::string>)> callback) {
  // In here, we would also validate other properties of the Integrity Block,
  // such as whether its version is supported (once we support multiple
  // Integrity Block versions).

  IsolatedWebAppTrustChecker::Result result =
      isolated_web_app_trust_checker_->IsTrusted(expected_web_bundle_id,
                                                 integrity_block);
  if (result.status != IsolatedWebAppTrustChecker::Result::Status::kTrusted) {
    std::move(callback).Run(result.message);
    return;
  }

  std::move(callback).Run(absl::nullopt);
}

base::expected<void, std::string> IsolatedWebAppValidator::ValidateMetadata(
    const web_package::SignedWebBundleId& web_bundle_id,
    const absl::optional<GURL>& primary_url,
    const std::vector<GURL>& entries) {
  // Verify that the Signed Web Bundle does not have a primary URL set. Primary
  // URLs make no sense for Isolated Web Apps - the "primary URL" should be
  // retrieved from the web app manifest's `start_url` field.
  if (primary_url.has_value()) {
    return base::unexpected("Primary URL must not be present, but was " +
                            primary_url->possibly_invalid_spec());
  }

  // Verify that the bundle only contains isolated-app:// URLs using the
  // Signed Web Bundle ID as their host.
  for (const GURL& entry : entries) {
    base::expected<IsolatedWebAppUrlInfo, std::string> url_info =
        IsolatedWebAppUrlInfo::Create(entry);
    if (!url_info.has_value()) {
      return base::unexpected("The URL of an exchange is invalid: " +
                              url_info.error());
    }

    const web_package::SignedWebBundleId& entry_web_bundle_id =
        url_info->web_bundle_id();
    if (entry_web_bundle_id != web_bundle_id) {
      return base::unexpected(
          "The URL of an exchange contains the wrong Signed Web Bundle ID: " +
          entry_web_bundle_id.id());
    }
    if (entry.has_ref()) {
      return base::unexpected(
          "The URL of an exchange is invalid: URLs must not have a fragment "
          "part.");
    }
    if (entry.has_query()) {
      return base::unexpected(
          "The URL of an exchange is invalid: URLs must not have a query "
          "part.");
    }
  }

  return base::ok();
}

}  // namespace web_app
