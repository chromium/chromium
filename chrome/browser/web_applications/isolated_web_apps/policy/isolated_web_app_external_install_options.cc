// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_external_install_options.h"

#include <utility>

#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_policy_constants.h"
#include "chrome/browser/web_applications/isolated_web_apps/update_manifest/update_manifest.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"

namespace web_app {

IsolatedWebAppExternalInstallOptions::IsolatedWebAppExternalInstallOptions(
    GURL update_manifest_url,
    web_package::SignedWebBundleId web_bundle_id,
    UpdateChannel update_channel)
    : update_manifest_url_(std::move(update_manifest_url)),
      web_bundle_id_(std::move(web_bundle_id)),
      update_channel_(update_channel) {
  DCHECK(update_manifest_url_.is_valid());
}

IsolatedWebAppExternalInstallOptions::IsolatedWebAppExternalInstallOptions(
    const IsolatedWebAppExternalInstallOptions& other) = default;
IsolatedWebAppExternalInstallOptions&
IsolatedWebAppExternalInstallOptions::operator=(
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

  ASSIGN_OR_RETURN(auto web_bundle_id,
                   web_package::SignedWebBundleId::Create(*web_bundle_id_raw),
                   [](std::string error) {
                     return "Wrong Web Bundle ID value: " + std::move(error);
                   });

  if (web_bundle_id.is_for_proxy_mode()) {
    return base::unexpected(
        "This Wed Bundle Id is created for ProxyMode, so the corresponding "
        "bundle cannot be installed");
  }

  const std::string* const update_channel_raw =
      entry_dict.FindString(kPolicyUpdateChannelKey);

  if (!update_channel_raw) {
    return IsolatedWebAppExternalInstallOptions(
        std::move(update_manifest_url), std::move(web_bundle_id),
        UpdateChannel::default_channel());
  }

  auto update_channel = UpdateChannel::Create(*update_channel_raw);

  if (update_channel.has_value()) {
    return IsolatedWebAppExternalInstallOptions(
        std::move(update_manifest_url), std::move(web_bundle_id),
        std::move(update_channel.value()));
  }

  return base::unexpected("Failed to create UpdateChannel");
}
}  // namespace web_app
