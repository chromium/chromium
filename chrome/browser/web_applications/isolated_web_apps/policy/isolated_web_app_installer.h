// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_POLICY_ISOLATED_WEB_APP_INSTALLER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_POLICY_ISOLATED_WEB_APP_INSTALLER_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_downloader.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_install_source.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_external_install_options.h"
#include "chrome/browser/web_applications/isolated_web_apps/update_manifest/update_manifest_fetcher.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace web_app {

class UpdateManifest;
class WebAppProvider;

enum class IwaInstallerResultType {
  kSuccess,
  kErrorCantCreateTempFile,
  kErrorUpdateManifestDownloadFailed,
  kErrorUpdateManifestParsingFailed,
  kErrorWebBundleUrlCantBeDetermined,
  kErrorCantDownloadWebBundle,
  kErrorCantInstallFromWebBundle,
  kErrorManagedGuestSessionInstallDisabled,
};

class IwaInstallerResult {
 public:
  using Type = IwaInstallerResultType;

  explicit IwaInstallerResult(Type type, std::string message = "");

  [[nodiscard]] base::Value::Dict ToDebugValue() const;

  [[nodiscard]] Type type() const { return type_; }

  [[nodiscard]] std::string_view message() const { return message_; }

 private:
  Type type_;
  std::string message_;
};

// This class installs an IWA based on a policy configuration.
class IwaInstaller {
 public:
  using Result = IwaInstallerResult;
  using ResultCallback = base::OnceCallback<void(Result)>;

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
        const IsolatedWebAppInstallSource& install_source,
        const IsolatedWebAppUrlInfo& url_info,
        const base::Version& expected_version,
        WebAppCommandScheduler::InstallIsolatedWebAppCallback callback) = 0;
  };

  class IwaInstallCommandWrapperImpl : public IwaInstallCommandWrapper {
   public:
    explicit IwaInstallCommandWrapperImpl(web_app::WebAppProvider* provider);
    IwaInstallCommandWrapperImpl(const IwaInstallCommandWrapperImpl&) = delete;
    IwaInstallCommandWrapperImpl& operator=(
        const IwaInstallCommandWrapperImpl&) = delete;
    void Install(const IsolatedWebAppInstallSource& install_source,
                 const IsolatedWebAppUrlInfo& url_info,
                 const base::Version& expected_version,
                 WebAppCommandScheduler::InstallIsolatedWebAppCallback callback)
        override;
    ~IwaInstallCommandWrapperImpl() override = default;

   private:
    const raw_ptr<web_app::WebAppProvider> provider_;
  };

  IwaInstaller(
      IsolatedWebAppExternalInstallOptions install_options,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<IwaInstallCommandWrapper> install_command_wrapper,
      base::Value::List& log,
      ResultCallback callback);
  ~IwaInstaller();

  // Starts installing the IWA in session (user or MGS).
  void Start();

  IwaInstaller(const IwaInstaller&) = delete;
  IwaInstaller& operator=(const IwaInstaller&) = delete;

 private:
  void CreateTempFile(base::OnceClosure next_step_callback);
  void OnTempFileCreated(base::OnceClosure next_step_callback,
                         ScopedTempWebBundleFile bundle);

  // Downloading of the update manifest of the current app.
  void DownloadUpdateManifest(
      base::OnceCallback<void(GURL, base::Version)> next_step_callback);

  // Callback when the update manifest has been downloaded and parsed.
  void OnUpdateManifestParsed(
      base::OnceCallback<void(GURL, base::Version)> next_step_callback,
      base::expected<UpdateManifest, UpdateManifestFetcher::Error>
          fetch_result);

  // Downloading of the Signed Web Bundle.
  void DownloadWebBundle(
      base::OnceCallback<void(base::Version)> next_step_callback,
      GURL web_bundle_url,
      base::Version expected_version);
  void OnWebBundleDownloaded(base::OnceClosure next_step_callback,
                             int32_t net_error);

  // Installing of the IWA using the downloaded Signed Web Bundle.
  void RunInstallCommand(base::Version expected_version);
  void OnIwaInstalled(base::expected<InstallIsolatedWebAppCommandSuccess,
                                     InstallIsolatedWebAppCommandError> result);

  void Finish(Result result);

  IsolatedWebAppExternalInstallOptions install_options_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<IwaInstallCommandWrapper> install_command_wrapper_;
  raw_ref<base::Value::List> log_;
  ResultCallback callback_;

  ScopedTempWebBundleFile bundle_;

  std::unique_ptr<UpdateManifestFetcher> update_manifest_fetcher_;
  std::unique_ptr<IsolatedWebAppDownloader> bundle_downloader_;

  base::WeakPtrFactory<IwaInstaller> weak_factory_{this};
};

class IwaInstallerFactory {
 public:
  using IwaInstallerFactoryCallback =
      base::RepeatingCallback<std::unique_ptr<IwaInstaller>(
          IsolatedWebAppExternalInstallOptions,
          scoped_refptr<network::SharedURLLoaderFactory>,
          base::Value::List&,
          WebAppProvider*,
          IwaInstaller::ResultCallback)>;

  static std::unique_ptr<IwaInstaller> Create(
      IsolatedWebAppExternalInstallOptions install_options,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      base::Value::List& log,
      WebAppProvider* provider,
      IwaInstaller::ResultCallback callback);

  static IwaInstallerFactoryCallback& GetIwaInstallerFactory();
};

std::ostream& operator<<(std::ostream& os,
                         IwaInstallerResultType install_result_type);

}  // namespace web_app
#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_POLICY_ISOLATED_WEB_APP_INSTALLER_H_
