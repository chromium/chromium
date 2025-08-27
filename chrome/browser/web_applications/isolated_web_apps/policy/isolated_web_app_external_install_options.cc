// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_external_install_options.h"

#include <optional>
#include <utility>

#include "base/strings/stringprintf.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_policy_constants.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/isolated_web_apps/types/iwa_version.h"
#include "components/webapps/isolated_web_apps/types/update_channel.h"

namespace web_app {

IsolatedWebAppExternalInstallOptions::IsolatedWebAppExternalInstallOptions(
    GURL update_manifest_url,
    web_package::SignedWebBundleId web_bundle_id,
    UpdateChannel update_channel,
    bool allow_downgrades,
    std::optional<IwaVersion> pinned_version)
    : update_manifest_url_(std::move(update_manifest_url)),
      web_bundle_id_(std::move(web_bundle_id)),
      update_channel_(std::move(update_channel)),
      allow_downgrades_(allow_downgrades),
      pinned_version_(std::move(pinned_version)) {
  DCHECK(update_manifest_url_.is_valid());
  // allow_downgrades cannot be toggled on without specifying pinned_version
  // field.
  CHECK(!allow_downgrades_ || pinned_version_);
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
IsolatedWebAppExternalInstallOptions::Create(
    const web_package::SignedWebBundleId& web_bundle_id,
    const GURL& update_manifest_url,
    const UpdateChannel& update_channel,
    const std::optional<IwaVersion>& maybe_pinned_version,
    bool allow_downgrades) {
  if (web_bundle_id.is_for_proxy_mode()) {
    return base::unexpected("Bundle created for proxy mode");
  }

  if (!update_manifest_url.is_valid()) {
    return base::unexpected("Bad update manifest URL");
  }

  if (allow_downgrades && !maybe_pinned_version) {
    return base::unexpected("Pinned version required to allow downgrades");
  }

  return IsolatedWebAppExternalInstallOptions(
      update_manifest_url, web_bundle_id, update_channel, allow_downgrades,
      maybe_pinned_version);
}

// static
base::expected<IsolatedWebAppExternalInstallOptions, std::string>
IsolatedWebAppExternalInstallOptions::FromPolicyPrefValue(
    const base::Value& entry) {
  if (!entry.is_dict()) {
    return base::unexpected("Policy entry is not dictionary");
  }
  return FromPolicyPrefValue(entry.GetDict());
}

// static
base::expected<IsolatedWebAppExternalInstallOptions, std::string>
IsolatedWebAppExternalInstallOptions::FromPolicyPrefValue(
    const base::Value::Dict& entry) {
  const std::string* const update_manifest_url_raw =
      entry.FindString(kPolicyUpdateManifestUrlKey);
  if (!update_manifest_url_raw) {
    return base::unexpected(
        "Update manifest URL value is not found or has the wrong type");
  }

  const GURL update_manifest_url(*update_manifest_url_raw);
  if (!update_manifest_url.is_valid()) {
    return base::unexpected("Wrong update manifest URL format");
  }

  const std::string* const web_bundle_id_raw =
      entry.FindString(kPolicyWebBundleIdKey);
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

  std::optional<IwaVersion> maybe_pinned_version;

  const auto* pinned_version_raw = entry.FindString(kPolicyPinnedVersionKey);
  if (pinned_version_raw) {
    ASSIGN_OR_RETURN(IwaVersion pinned_version,
                     IwaVersion::Create(*pinned_version_raw),
                     [&](IwaVersion::IwaVersionParseError error) {
                       return base::StringPrintf(
                           "Pinned version has invalid format: %s",
                           web_app::IwaVersion::GetErrorString(error).c_str());
                     });
    maybe_pinned_version = std::move(pinned_version);
  }

  const bool allow_downgrades =
      entry.FindBool(kPolicyAllowDowngradesKey).value_or(false);

  if (allow_downgrades && !maybe_pinned_version) {
    return base::unexpected(
        "Downgrades cannot be toggled on by allow_downgrades without "
        "specifying pinned_version.");
  }

  const std::string* const update_channel_raw =
      entry.FindString(kPolicyUpdateChannelKey);

  if (!update_channel_raw) {
    return IsolatedWebAppExternalInstallOptions(
        std::move(update_manifest_url), std::move(web_bundle_id),
        UpdateChannel::default_channel(), allow_downgrades,
        std::move(maybe_pinned_version));
  }

  auto update_channel = UpdateChannel::Create(*update_channel_raw);

  if (update_channel.has_value()) {
    return IsolatedWebAppExternalInstallOptions(
        std::move(update_manifest_url), std::move(web_bundle_id),
        std::move(update_channel.value()), allow_downgrades,
        std::move(maybe_pinned_version));
  }

  return base::unexpected("Failed to create UpdateChannel from policy value");
}
}  // namespace web_app
