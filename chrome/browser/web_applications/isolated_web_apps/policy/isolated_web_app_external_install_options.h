// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_POLICY_ISOLATED_WEB_APP_EXTERNAL_INSTALL_OPTIONS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_POLICY_ISOLATED_WEB_APP_EXTERNAL_INSTALL_OPTIONS_H_

#include <optional>

#include "base/values.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/isolated_web_apps/types/iwa_version.h"
#include "components/webapps/isolated_web_apps/types/update_channel.h"
#include "url/gurl.h"

namespace web_app {

// This class contains all information to install an Isolated Web App via
// enterprise policy.
class IsolatedWebAppExternalInstallOptions final {
 public:
  // Creates an instance of the class from existing `web_bundle_id`,
  // `update_manifest_url` and optional version management settings.
  static base::expected<IsolatedWebAppExternalInstallOptions, std::string>
  Create(const web_package::SignedWebBundleId& web_bundle_id,
         const GURL& update_manifest_url,
         const UpdateChannel& update_channel = UpdateChannel::default_channel(),
         const std::optional<IwaVersion>& maybe_pinned_version = std::nullopt,
         bool allow_downgrades = false);

  // Creates an instance of the class from the enterprise policy entry.
  // The entry must contain a valid URL of the update manifest and
  // a Web Bundle ID of type Ed25519PublicKey.
  static base::expected<IsolatedWebAppExternalInstallOptions, std::string>
  FromPolicyPrefValue(const base::Value& entry);

  static base::expected<IsolatedWebAppExternalInstallOptions, std::string>
  FromPolicyPrefValue(const base::Value::Dict& entry);

  ~IsolatedWebAppExternalInstallOptions();

  IsolatedWebAppExternalInstallOptions(
      const IsolatedWebAppExternalInstallOptions& other);
  IsolatedWebAppExternalInstallOptions& operator=(
      const IsolatedWebAppExternalInstallOptions& other);

  [[nodiscard]] const GURL& update_manifest_url() const {
    return update_manifest_url_;
  }
  [[nodiscard]] const web_package::SignedWebBundleId& web_bundle_id() const {
    return web_bundle_id_;
  }
  [[nodiscard]] const UpdateChannel& update_channel() const {
    return update_channel_;
  }
  [[nodiscard]] const std::optional<IwaVersion>& pinned_version() const {
    return pinned_version_;
  }
  [[nodiscard]] bool allow_downgrades() const { return allow_downgrades_; }

 private:
  IsolatedWebAppExternalInstallOptions(
      GURL update_manifest_url,
      web_package::SignedWebBundleId web_bundle_id,
      UpdateChannel update_channel,
      bool allow_downgrades,
      std::optional<IwaVersion> pinned_version = std::nullopt);

  // Update manifest contains the info about available versions of the IWA and
  // the URLs of the corresponding Web Bundle files.
  GURL update_manifest_url_;
  // Signed Web Bundle ID identifies the app.
  web_package::SignedWebBundleId web_bundle_id_;
  // Update Channel ID to specify the desired release channel. If not specified
  // in policy, it is set to "default".
  UpdateChannel update_channel_;
  // Toggles the possibility to downgrade IWA. If this field is not specified in
  // policy, it is assumed to be false.
  bool allow_downgrades_;
  // The desired version of the IWA to pin it to.
  // If specified, the system will attempt to update the app to this version
  // and then disable all further app updates. If the chosen pinned version is
  // not available in the IWA's update manifest, the app will be pinned to its
  // currently installed version.
  std::optional<IwaVersion> pinned_version_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_POLICY_ISOLATED_WEB_APP_EXTERNAL_INSTALL_OPTIONS_H_
