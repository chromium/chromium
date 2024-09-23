// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_POLICY_ISOLATED_WEB_APP_POLICY_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_POLICY_ISOLATED_WEB_APP_POLICY_MANAGER_H_

#include <iosfwd>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/containers/queue.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_file.h"
#include "base/memory/weak_ptr.h"
#include "base/version.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_downloader.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_install_source.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_external_install_options.h"
#include "chrome/browser/web_applications/isolated_web_apps/update_manifest/update_manifest_fetcher.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "components/prefs/pref_change_registrar.h"
#include "net/base/backoff_entry.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/data_decoder/public/mojom/json_parser.mojom.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace web_app {

class UpdateManifest;

namespace internal {

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

  base::Value::Dict ToDebugValue() const;

  Type type() const { return type_; }

  std::string_view message() const { return message_; }

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

}  // namespace internal

// This class is responsible for installing, uninstalling, updating etc.
// of the policy installed IWAs.
class IsolatedWebAppPolicyManager {
 public:
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  explicit IsolatedWebAppPolicyManager(Profile* profile);

  IsolatedWebAppPolicyManager(const IsolatedWebAppPolicyManager&) = delete;
  IsolatedWebAppPolicyManager& operator=(const IsolatedWebAppPolicyManager&) =
      delete;
  ~IsolatedWebAppPolicyManager();

  void Start(base::OnceClosure on_started_callback);
  void SetProvider(base::PassKey<WebAppProvider>, WebAppProvider& provider);

  base::Value GetDebugValue() const;

 private:
  void CleanupAndProcessPolicyOnSessionStart();
  int GetPendingInitCount();
  void SetPendingInitCount(int pending_count);
  void ProcessPolicy(base::OnceClosure finished_closure);
  void DoProcessPolicy(AllAppsLock& lock, base::Value::Dict& debug_info);
  void OnPolicyProcessed();

  void LogAddPolicyInstallSourceResult(
      web_package::SignedWebBundleId web_bundle_id);

  void LogRemoveInstallSourceResult(
      web_package::SignedWebBundleId web_bundle_id,
      WebAppManagement::Type source,
      webapps::UninstallResultCode uninstall_code);

  void OnInstallTaskCompleted(
      web_package::SignedWebBundleId web_bundle_id,
      base::RepeatingCallback<void(internal::IwaInstaller::Result)> callback,
      internal::IwaInstaller::Result install_result);
  void OnAllInstallTasksCompleted(
      std::vector<internal::IwaInstaller::Result> install_results);

  void MaybeStartNextInstallTask();

  void CleanupOrphanedBundles(base::OnceClosure finished_closure);

  // Keeps track of the last few processing logs for debugging purposes.
  // Automatically discards older logs to keep at most `kMaxEntries`.
  class ProcessLogs {
   public:
    static constexpr size_t kMaxEntries = 10;

    ProcessLogs();
    ~ProcessLogs();

    void AppendCompletedStep(base::Value::Dict log);

    base::Value ToDebugValue() const;

   private:
    base::circular_deque<base::Value::Dict> logs_;
  };

  raw_ptr<Profile> profile_ = nullptr;
  raw_ptr<WebAppProvider> provider_ = nullptr;
  PrefChangeRegistrar pref_change_registrar_;
  ProcessLogs process_logs_;
  base::OnceClosure on_started_callback_;

  bool reprocess_policy_needed_ = false;
  bool policy_is_being_processed_ = false;
  base::Value::Dict current_process_log_;

  net::BackoffEntry install_retry_backoff_entry_;

  // We must execute install tasks in a queue, because each task uses a
  // `WebContents`, and installing an unbound number of apps in parallel would
  // use too many resources.
  base::queue<std::unique_ptr<internal::IwaInstaller>> install_tasks_;

  base::WeakPtrFactory<IsolatedWebAppPolicyManager> weak_ptr_factory_{this};
};

}  // namespace web_app
#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_POLICY_ISOLATED_WEB_APP_POLICY_MANAGER_H_
