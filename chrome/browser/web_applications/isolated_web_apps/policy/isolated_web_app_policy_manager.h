// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_POLICY_ISOLATED_WEB_APP_POLICY_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_POLICY_ISOLATED_WEB_APP_POLICY_MANAGER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/version.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_location.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_external_install_options.h"
#include "chrome/browser/web_applications/isolated_web_apps/update_manifest/update_manifest_fetcher.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "components/prefs/pref_change_registrar.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/data_decoder/public/mojom/json_parser.mojom.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace web_app {

class UpdateManifest;
class IsolatedWebAppDownloader;

namespace internal {

// This class installs the given collection of IWAs.
class BulkIwaInstaller {
 public:
  enum class EphemeralAppInstallResult {
    kSuccess,
    kErrorNotEphemeralSession,
    kErrorCantCreateRootDirectory,
    kErrorUpdateManifestDownloadFailed,
    kErrorUpdateManifestParsingFailed,
    kErrorWebBundleUrlCantBeDetermined,
    kErrorCantCreateIwaDirectory,
    kErrorCantDownloadWebBundle,
    kErrorCantInstallFromWebBundle,
    kUnknown,
  };

  using Result =
      std::pair<web_package::SignedWebBundleId, EphemeralAppInstallResult>;
  using ResultCallback = base::OnceCallback<void(std::vector<Result>)>;

  static constexpr char kEphemeralIwaRootDirectory[] = "EphemeralIWA";
  static constexpr char kMainSignedWebBundleFileName[] = "main.swbn";

  // This pure virtual class represents the IWA installation logic.
  // It is introduced primarily for testability reasons.
  class IwaInstallCommandWrapper {
   public:
    IwaInstallCommandWrapper() = default;
    IwaInstallCommandWrapper(const IwaInstallCommandWrapper&) = delete;
    IwaInstallCommandWrapper& operator=(const IwaInstallCommandWrapper&) =
        delete;
    virtual ~IwaInstallCommandWrapper() = default;
    virtual void Install(
        const IsolatedWebAppLocation& location,
        const IsolatedWebAppUrlInfo& url_info,
        const base::Version& expected_version,
        WebAppCommandScheduler::InstallIsolatedWebAppCallback callback) = 0;
  };

  class IwaInstallCommandWrapperImpl : public IwaInstallCommandWrapper {
   public:
    explicit IwaInstallCommandWrapperImpl(web_app::WebAppProvider* provider);
    void Install(const IsolatedWebAppLocation& location,
                 const IsolatedWebAppUrlInfo& url_info,
                 const base::Version& expected_version,
                 WebAppCommandScheduler::InstallIsolatedWebAppCallback callback)
        override;
    ~IwaInstallCommandWrapperImpl() override = default;

   private:
    const raw_ptr<web_app::WebAppProvider> provider_;
  };

  BulkIwaInstaller(
      const base::FilePath& context_dir,
      std::vector<IsolatedWebAppExternalInstallOptions>
          ephemeral_iwa_install_options,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<IwaInstallCommandWrapper> installer,
      ResultCallback ephemeral_install_cb);
  ~BulkIwaInstaller();

  // Triggers installing of the IWAs in MGS. There is no callback as so far we
  // don't care about the result of the installation: for MVP it is not critical
  // to have a complex retry mechanism for the session that would exist for just
  // several minutes.
  void InstallEphemeralApps();

  BulkIwaInstaller(const BulkIwaInstaller&) = delete;
  BulkIwaInstaller& operator=(const BulkIwaInstaller&) = delete;

 private:
  // Creating root directory where the ephemeral apps will be placed.
  void CreateIwaEphemeralRootDirectory();
  void OnIwaEphemeralRootDirectoryCreated(base::File::Error error);

  // Downloading of the update manifest of the current app.
  void DownloadUpdateManifest();

  // Callback when the update manifest has been downloaded and parsed.
  void OnUpdateManifestParsed(
      base::expected<UpdateManifest, UpdateManifestFetcher::Error>
          update_manifest);

  // Create a new directory for the exact instance of the IWA.
  void CreateIwaDirectory();
  void OnIwaDirectoryCreated(const base::FilePath& iwa_dir,
                             base::File::Error error);

  // Downloading of the Signed Web Bundle.
  void DownloadWebBundle();
  void OnWebBundleDownloaded(const base::FilePath& path, int32_t net_error);

  // Installing of the IWA using the downloaded Signed Web Bundle.
  void InstallIwa(base::FilePath path);
  void OnIwaInstalled(base::expected<InstallIsolatedWebAppCommandSuccess,
                                     InstallIsolatedWebAppCommandError> result);

  // Removes the directory where the IWA has been downloaded.
  void WipeIwaDownloadDirectory();
  void OnIwaDownloadDirectoryWiped(bool wipe_result);

  void FinishWithResult(EphemeralAppInstallResult result);
  void SetResultForAllAndFinish(EphemeralAppInstallResult result);
  void ContinueWithTheNextApp();

  data_decoder::mojom::JsonParser* GetJsonParserPtr();

  // Isolated Web Apps for installation in ephemeral managed guest session.
  std::vector<IsolatedWebAppExternalInstallOptions>
      ephemeral_iwa_install_options_;
  std::vector<IsolatedWebAppExternalInstallOptions>::iterator current_app_;
  std::unique_ptr<UpdateManifestFetcher> current_update_manifest_fetcher_;
  std::unique_ptr<IsolatedWebAppDownloader> current_bundle_downloader_;

  base::FilePath installation_dir_;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // The result vector contains the installation result for each app.
  std::vector<Result> result_vector_;
  std::unique_ptr<IwaInstallCommandWrapper> installer_;
  ResultCallback ephemeral_install_cb_;

  base::WeakPtrFactory<BulkIwaInstaller> weak_factory_{this};
};

// Uninstalls a list of IWAs based on their Web Bundle IDs.
class BulkIwaUninstaller {
 public:
  using Result =
      std::pair<web_package::SignedWebBundleId, webapps::UninstallResultCode>;
  using ResultCallback = base::OnceCallback<void(std::vector<Result>)>;

  explicit BulkIwaUninstaller(web_app::WebAppProvider& provider);
  ~BulkIwaUninstaller();

  // Uninstall the provided apps. Can be called multiple times.
  void UninstallApps(
      const std::vector<web_package::SignedWebBundleId>& web_bundle_ids,
      ResultCallback callback);

 private:
  void OnAppsUninstalled(ResultCallback callback,
                         std::vector<Result> uninstall_results);

  const raw_ref<web_app::WebAppProvider> provider_;
  base::WeakPtrFactory<BulkIwaUninstaller> weak_factory_{this};
};

}  // namespace internal

// This class is responsible for installing, uninstalling, updating etc.
// of the policy installed IWAs.
class IsolatedWebAppPolicyManager {
 public:
  explicit IsolatedWebAppPolicyManager(Profile* profile);

  IsolatedWebAppPolicyManager(const IsolatedWebAppPolicyManager&) = delete;
  IsolatedWebAppPolicyManager& operator=(const IsolatedWebAppPolicyManager&) =
      delete;
  ~IsolatedWebAppPolicyManager();

  void Start(base::OnceClosure on_started_callback);
  void SetProvider(base::PassKey<WebAppProvider>, WebAppProvider& provider);

 private:
  void ProcessPolicy();
  void DoProcessPolicy(AllAppsLock& lock, base::Value::Dict& debug_info);
  void OnPolicyProcessed();

  void Uninstall(base::OnceClosure next_step_callback);
  void OnUninstalled(base::OnceClosure next_step_callback,
                     std::vector<web_app::internal::BulkIwaUninstaller::Result>
                         uninstall_results);

  void Install(base::OnceClosure next_step_callback);
  void OnInstalled(
      base::OnceClosure next_step_callback,
      std::vector<web_app::internal::BulkIwaInstaller::Result> install_results);

  raw_ptr<Profile> profile_ = nullptr;
  raw_ptr<WebAppProvider> provider_ = nullptr;
  PrefChangeRegistrar pref_change_registrar_;
  std::unique_ptr<internal::BulkIwaInstaller> bulk_installer_;
  std::unique_ptr<internal::BulkIwaUninstaller> bulk_uninstaller_;
  base::OnceClosure on_started_callback_;
  bool reprocess_policy_needed_ = false;
  bool policy_is_being_processed_ = false;

  std::vector<IsolatedWebAppExternalInstallOptions> to_be_installed_;
  std::vector<web_package::SignedWebBundleId> to_be_removed_;

  base::WeakPtrFactory<IsolatedWebAppPolicyManager> weak_ptr_factory_{this};
};

}  // namespace web_app
#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_POLICY_ISOLATED_WEB_APP_POLICY_MANAGER_H_
