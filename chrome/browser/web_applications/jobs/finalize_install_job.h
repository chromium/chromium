// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_JOBS_FINALIZE_INSTALL_JOB_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_JOBS_FINALIZE_INSTALL_JOB_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "build/build_config.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_integrity_block_data.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/web_app_chromeos_data.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_management_type.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/common/web_app_id.h"
#include "components/webapps/isolated_web_apps/types/storage_location.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/ash/experiences/system_web_apps/types/system_web_app_data.h"
#endif

class Profile;

namespace webapps {
enum class UninstallResultCode;
enum class WebappUninstallSource;
}  // namespace webapps

namespace web_app {

class Lock;
class WithAppResources;
class IwaVersion;
class WebApp;
class WebAppProvider;
class WebAppScope;
class WebAppRegistrar;
class WebAppSyncBridge;
class WebAppInstallManager;
class WebAppIconManager;
class WebAppTranslationManager;
class OsIntegrationManager;
class WebAppOriginAssociationManager;
struct OriginAssociations;

using InstallFinalizedCallback =
    base::OnceCallback<void(const webapps::AppId& app_id,
                            webapps::InstallResultCode code)>;

struct FinalizeJobOptions {
  struct IwaOptions {
    IwaOptions(
        IsolatedWebAppStorageLocation location,
        std::optional<IsolatedWebAppIntegrityBlockData> integrity_block_data);
    ~IwaOptions();
    IwaOptions(const IwaOptions&);

    IsolatedWebAppStorageLocation location;
    std::optional<IsolatedWebAppIntegrityBlockData> integrity_block_data;
  };

  explicit FinalizeJobOptions(webapps::WebappInstallSource install_surface);
  ~FinalizeJobOptions();
  FinalizeJobOptions(const FinalizeJobOptions&);

  const WebAppManagement::Type source;
  const webapps::WebappInstallSource install_surface;
  proto::InstallState install_state =
      proto::InstallState::INSTALLED_WITH_OS_INTEGRATION;
  bool overwrite_existing_manifest_fields = true;
  bool skip_icon_writes_on_download_failure = false;

  std::optional<WebAppChromeOsData> chromeos_data;
#if BUILDFLAG(IS_CHROMEOS)
  std::optional<ash::SystemWebAppData> system_web_app_data;
#endif

  // If set, will propagate `IsolatedWebAppStorageLocation` and
  // `IntegrityBlockData` to `WebApp::isolation_data()` with the given values,
  // as well as the version from
  // `WebAppInstallInfo::isolated_web_app_version`. Will `CHECK` if
  // `web_app_info.isolated_web_app_version` is invalid.
  std::optional<IwaOptions> iwa_options;

  // These are required to be false if `install_state` is not
  // proto::INSTALLED_WITH_OS_INTEGRATION.
  bool add_to_applications_menu = true;
  bool add_to_desktop = true;
  bool add_to_quick_launch_bar = true;
  std::optional<RunOnOsLoginMode> run_on_os_login_mode;

  // Controls fetching and validation of web_app_origin_association data
  // from web origins found in manifest scope_extensions entries. If true,
  // do not validate even if scope_extensions has valid entries.
  bool skip_origin_association_validation = false;
};

// A finalizer job for the installation process, represents the last step.
// Takes WebAppInstallInfo as input, writes data to disk (e.g icons, shortcuts)
// and registers the app.
//
// This is a job based on web_app_install_finalizer.
class FinalizeInstallJob {
 public:
  FinalizeInstallJob(Profile& profile,
                     Lock* lock,
                     WithAppResources* lock_resources,
                     const WebAppInstallInfo& web_app_info,
                     const FinalizeJobOptions& options);
  ~FinalizeInstallJob();

  void Start(InstallFinalizedCallback callback);

  Profile* profile() { return &profile_.get(); }

  // Writes external config data to the web_app DB, mapped per source.
  void WriteExternalConfigMapInfo(
      WebApp& web_app,
      WebAppManagement::Type source,
      bool is_placeholder,
      GURL install_url,
      std::vector<std::string> additional_policy_ids);

  static void AdjustAppStateBeforeCommit(const WebApp* existing_app,
                                         WebApp& web_app,
                                         WebAppProvider& provider);

  static bool& DisableUserDisplayModeSyncMitigationsForTesting();

 private:
  WebAppRegistrar& registrar() const;
  WebAppSyncBridge& sync_bridge() const;
  WebAppInstallManager& install_manager() const;
  WebAppIconManager& icon_manager() const;
  WebAppTranslationManager& translation_manager() const;
  OsIntegrationManager& os_integration_manager() const;
  WebAppOriginAssociationManager& origin_association_manager() const;

  friend class WebAppInstallFinalizer;

  using CommitCallback = base::OnceCallback<void(bool success)>;

  void SetWebAppManifestFieldsAndWriteData(
      std::unique_ptr<WebApp> web_app,
      CommitCallback commit_callback,
      bool skip_icon_writes_on_download_failure);

  static void UpdateIsolationDataAndResetPendingUpdateInfo(
      WebApp* web_app,
      const IsolatedWebAppStorageLocation& location,
      const IwaVersion& version,
      const std::optional<GURL>& iwa_update_manifest_url,
      std::optional<IsolatedWebAppIntegrityBlockData> integrity_block_data);

  void CommitToSyncBridge(std::unique_ptr<WebApp> web_app,
                          CommitCallback commit_callback,
                          bool success);

  void WriteTranslations(
      const webapps::AppId& app_id,
      const base::flat_map<std::string, blink::Manifest::TranslationItem>&
          translations,
      CommitCallback commit_callback,
      bool success);

  void OnOriginAssociationValidated(
      OriginAssociations validated_origin_associations);

  void OnDatabaseCommitCompletedForInstall(InstallFinalizedCallback callback,
                                           webapps::AppId app_id,
                                           std::optional<WebAppScope> old_scope,
                                           bool success);

  void OnInstallHooksFinished(InstallFinalizedCallback callback,
                              webapps::AppId app_id);

  static void NotifyWebAppInstalledWithOsHooks(WebAppProvider* provider,
                                               webapps::AppId app_id);

  const raw_ref<Profile> profile_;
  raw_ptr<WebAppProvider> provider_ = nullptr;
  raw_ptr<base::Clock> clock_;
  // TODO(crbug.com/259703817): Transition this to a const raw_ref.
  raw_ptr<Lock> lock_ = nullptr;
  // TODO(crbug.com/259703817): Transition this to a const raw_ref.
  raw_ptr<WithAppResources> resources_lock_ = nullptr;

  WebAppInstallInfo web_app_info_;
  FinalizeJobOptions options_;
  InstallFinalizedCallback callback_;

  base::WeakPtrFactory<FinalizeInstallJob> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_JOBS_FINALIZE_INSTALL_JOB_H_
