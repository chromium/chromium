// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_POLICY_ISOLATED_WEB_APP_EXTERNAL_INSTALL_OPTIONS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_POLICY_ISOLATED_WEB_APP_EXTERNAL_INSTALL_OPTIONS_H_

#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "url/gurl.h"

namespace base {
class Value;
}

namespace web_app {

// This class contains all information to install an Isolated Web App via
// enterprise policy.
class IsolatedWebAppExternalInstallOptions final {
 public:
  // Created the instance of the class from the enterprise policy entry.
  // The entry must contain a valid URL of the update manifest and
  // a Web Bundle ID of type Ed25519PublicKey.
  static base::expected<IsolatedWebAppExternalInstallOptions, std::string>
  FromPolicyPrefValue(const base::Value& entry);
  ~IsolatedWebAppExternalInstallOptions();

  IsolatedWebAppExternalInstallOptions(
      const IsolatedWebAppExternalInstallOptions& other);
  IsolatedWebAppExternalInstallOptions& operator=(
      const IsolatedWebAppExternalInstallOptions& other);

  const GURL& update_manifest_url() const { return update_manifest_url_; }
  const web_package::SignedWebBundleId& web_bundle_id() const {
    return web_bundle_id_;
  }

 private:
  IsolatedWebAppExternalInstallOptions(
      GURL update_manifest_url,
      web_package::SignedWebBundleId web_bundle_id);

  // Update manifest contains the info about available versions of the IWA and
  // the URLs of the corresponding Web Bundle files.
  GURL update_manifest_url_;
  // Signed Web Bundle ID identifies the app.
  web_package::SignedWebBundleId web_bundle_id_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_POLICY_ISOLATED_WEB_APP_EXTERNAL_INSTALL_OPTIONS_H_
