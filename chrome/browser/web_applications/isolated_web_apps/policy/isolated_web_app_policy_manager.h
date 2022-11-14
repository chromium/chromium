// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_POLICY_ISOLATED_WEB_APP_POLICY_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_POLICY_ISOLATED_WEB_APP_POLICY_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_external_install_options.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/data_decoder/public/mojom/json_parser.mojom.h"

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace web_app {

// This component is responsible for installing, uninstalling, updating etc.
// of the policy installed IWAs.
class IsolatedWebAppPolicyManager {
 public:
  enum class EphemeralAppInstallResult {
    kSuccess,
    kErrorNotEphemeralSession,
    kErrorCantCreateRootDirectory,
    kErrorUpdateManifestDownloadFailed,
    kErrorUpdateManifestParsingFailed,
    kUnknown,
  };
  static constexpr char kEphemeralIwaRootDirectory[] = "EphemeralIWA";

  IsolatedWebAppPolicyManager(
      const base::FilePath& context_dir,
      std::vector<IsolatedWebAppExternalInstallOptions>
          ephemeral_iwa_install_options,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      base::OnceCallback<void(std::vector<EphemeralAppInstallResult>)>
          ephemeral_install_cb);
  ~IsolatedWebAppPolicyManager();

  // Triggers installing of the IWAs in MGS. There is no callback as so far we
  // don't care about the result of the installation: for MVP it is not critical
  // to have a complex retry mechanism for the session that would exist for just
  // several minutes.
  void InstallEphemeralApps();

  IsolatedWebAppPolicyManager(const IsolatedWebAppPolicyManager&) = delete;
  IsolatedWebAppPolicyManager& operator=(const IsolatedWebAppPolicyManager&) =
      delete;

 private:
  // Creating root directory where the ephemeral apps will be placed.
  void CreateIwaEphemeralRootDirectory();
  void OnIwaEphemeralRootDirectoryCreated(base::File::Error error);

  // Downloading of the update manifest of the current app.
  void DownloadUpdateManifest();
  void OnUpdateManifestDownloaded(
      std::unique_ptr<network::SimpleURLLoader> simple_loader,
      std::unique_ptr<std::string>);

  // Parsing of the update manifest from JSON string to Value tree.
  void ParseUpdateManifest(const std::string& manifest_content);
  void OnUpdateManifestParsed(absl::optional<base::Value> result,
                              const absl::optional<std::string>& error);

  void SetResultForCurrentEphemeralApp(EphemeralAppInstallResult result);
  void SetResultForAllEphemeralApps(EphemeralAppInstallResult result);
  void ContinueWithTheNextApp();
  data_decoder::mojom::JsonParser* GetJsonParserPtr();

  // Isolated Web Apps for installation in ephemeral managed guest session.
  std::vector<IsolatedWebAppExternalInstallOptions>
      ephemeral_iwa_install_options_;
  std::vector<IsolatedWebAppExternalInstallOptions>::iterator current_app_;
  const base::FilePath installation_dir_;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // The result vector contains the installation result for each app.
  std::vector<EphemeralAppInstallResult> result_vector_;
  base::OnceCallback<void(std::vector<EphemeralAppInstallResult>)>
      ephemeral_install_cb_;

  data_decoder::DataDecoder data_decoder_;
  // Dont use this variable directly. Use GetJsonParserPtr() instead.
  mojo::Remote<data_decoder::mojom::JsonParser> json_parser_;

  base::WeakPtrFactory<IsolatedWebAppPolicyManager> weak_factory_{this};
};

}  // namespace web_app
#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_POLICY_ISOLATED_WEB_APP_POLICY_MANAGER_H_
