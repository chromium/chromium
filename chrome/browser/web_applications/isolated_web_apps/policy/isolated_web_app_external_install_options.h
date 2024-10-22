// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_POLICY_ISOLATED_WEB_APP_EXTERNAL_INSTALL_OPTIONS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_POLICY_ISOLATED_WEB_APP_EXTERNAL_INSTALL_OPTIONS_H_

#include "base/version.h"
#include "chrome/browser/web_applications/isolated_web_apps/update_manifest/update_manifest.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "url/gurl.h"

namespace base {
class Value;
}

namespace web_app {

class UpdateChannel;

// This class contains all information to install an Isolated Web App via
// enterprise policy.
class IsolatedWebAppExternalInstallOptions final {
 public:
  // Creates an instance of the class from existing `update_manifest_url` and
  // `web_bundle_id`. Uses the default update channel.
  static base::expected<IsolatedWebAppExternalInstallOptions, std::string>
  Create(const GURL& update_manifest_url,
         const web_package::SignedWebBundleId& web_bundle_id);

  // Creates an instance of the class from the enterprise policy entry.
  // The entry must contain a valid URL of the update manifest and
  // a Web Bundle ID of type Ed25519PublicKey.
  static base::expected<IsolatedWebAppExternalInstallOptions, std::string>
  FromPolicyPrefValue(const base::Value& entry);

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
  [[nodiscard]] const std::optional<base::Version>& pinned_version() const {
    return pinned_version_;
  }

 private:
  IsolatedWebAppExternalInstallOptions(
      GURL update_manifest_url,
      web_package::SignedWebBundleId web_bundle_id,
      UpdateChannel update_channel,
      std::optional<base::Version> pinned_version = std::nullopt);

  // Update manifest contains the info about available versions of the IWA and
  // the URLs of the corresponding Web Bundle files.
  GURL update_manifest_url_;
  // Signed Web Bundle ID identifies the app.
  web_package::SignedWebBundleId web_bundle_id_;
  // Update Channel ID to specify the desired release channel. If not specified
  // in policy, it is set to "default".
  UpdateChannel update_channel_;
  // The desired version of the IWA to pin it to.
  // If specified, the system will attempt to update the app to this version
  // and then disable all further app updates. If the chosen pinned version is
  // not available in the IWA's update manifest, the app will be pinned to its
  // currently installed version.
  std::optional<base::Version> pinned_version_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_POLICY_ISOLATED_WEB_APP_EXTERNAL_INSTALL_OPTIONS_H_
