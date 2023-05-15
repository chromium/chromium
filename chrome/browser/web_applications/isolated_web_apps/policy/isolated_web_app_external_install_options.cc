// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_external_install_options.h"

#include "base/types/expected.h"
#include "base/values.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_policy_constants.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"

namespace web_app {

IsolatedWebAppExternalInstallOptions::IsolatedWebAppExternalInstallOptions(
    const GURL& update_manifest_url,
    const web_package::SignedWebBundleId& web_bundle_id)
    : update_manifest_url_(update_manifest_url), web_bundle_id_(web_bundle_id) {
  DCHECK(update_manifest_url_.is_valid());
}

IsolatedWebAppExternalInstallOptions::IsolatedWebAppExternalInstallOptions(
    const IsolatedWebAppExternalInstallOptions& other) = default;

IsolatedWebAppExternalInstallOptions::~IsolatedWebAppExternalInstallOptions() =
    default;

// static
base::expected<IsolatedWebAppExternalInstallOptions, std::string>
IsolatedWebAppExternalInstallOptions::FromPolicyPrefValue(
    const base::Value& entry) {
  if (!entry.is_dict()) {
    return base::unexpected("Policy entry is not dictionary");
  }

  const base::Value::Dict& entry_dict = entry.GetDict();

  const std::string* const update_manifest_url_raw =
      entry_dict.FindString(kPolicyUpdateManifestUrlKey);
  if (!update_manifest_url_raw) {
    return base::unexpected(
        "Update manifest URL value is not found or has the wrong type");
  }

  const GURL update_manifest_url(*update_manifest_url_raw);
  if (!update_manifest_url.is_valid()) {
    return base::unexpected("Wrong update manifest URL format");
  }

  const std::string* const web_bundle_id_raw =
      entry_dict.FindString(kPolicyWebBundleIdKey);
  if (!web_bundle_id_raw) {
    return base::unexpected(
        "Web Bundle ID value is not found or has the wrong type");
  }

  const base::expected<web_package::SignedWebBundleId, std::string>
      web_bundle_id =
          web_package::SignedWebBundleId::Create(*web_bundle_id_raw);
  if (!web_bundle_id.has_value()) {
    return base::unexpected("Wrong Web Bundle ID value: " +
                            web_bundle_id.error());
  }

  if (web_bundle_id->type() !=
      web_package::SignedWebBundleId::Type::kEd25519PublicKey) {
    return base::unexpected("The Wed Bundle Id is not Ed25519 public key");
  }

  return IsolatedWebAppExternalInstallOptions(update_manifest_url,
                                              web_bundle_id.value());
}

}  // namespace web_app
