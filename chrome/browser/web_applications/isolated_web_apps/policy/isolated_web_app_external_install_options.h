// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_POLICY_ISOLATED_WEB_APP_EXTERNAL_INSTALL_OPTIONS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_POLICY_ISOLATED_WEB_APP_EXTERNAL_INSTALL_OPTIONS_H_

#include "base/files/file_path.h"
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
  // Delete for now as SignedWebBundleId has no copy assignment operator.
  IsolatedWebAppExternalInstallOptions& operator=(
      const IsolatedWebAppExternalInstallOptions& other) = delete;

  const GURL& update_manifest_url() const { return update_manifest_url_; }
  const web_package::SignedWebBundleId& web_bundle_id() const {
    return web_bundle_id_;
  }

  void set_web_bundle_url(const GURL& url) { web_bundle_url_ = url; }

  const GURL& web_bundle_url() const { return web_bundle_url_; }

  void set_app_directory(const base::FilePath& app_directory) {
    app_directory_ = app_directory;
  }

  void reset_app_directory() { app_directory_.clear(); }

  const base::FilePath& app_directory() const { return app_directory_; }

 private:
  IsolatedWebAppExternalInstallOptions(
      const GURL& update_manifest_url,
      const web_package::SignedWebBundleId& web_bundle_id);
  // Update manifest contains the info about available versions of the IWA and
  // the URLs of the corresponding Web Bundle files.
  GURL update_manifest_url_;
  // Signed Web Bundle ID identifies the app.
  web_package::SignedWebBundleId web_bundle_id_;

  // The URL to be used to download Web Bundle.
  GURL web_bundle_url_;
  // The directory where the Signed Web Bundle was or will be downloaded to.
  base::FilePath app_directory_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_POLICY_ISOLATED_WEB_APP_EXTERNAL_INSTALL_OPTIONS_H_
