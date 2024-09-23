// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_FINALIZER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_FINALIZER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_integrity_block_data.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/scope_extension_info.h"
#include "chrome/browser/web_applications/web_app_chromeos_data.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/common/web_app_id.h"
#include "third_party/skia/include/core/SkBitmap.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/system_web_apps/types/system_web_app_data.h"
#endif

class Profile;

namespace webapps {
enum class UninstallResultCode;
enum class WebappUninstallSource;
}  // namespace webapps

namespace web_app {

class IsolatedWebAppStorageLocation;
class WebApp;
class WebAppProvider;

// An finalizer for the installation process, represents the last step.
// Takes WebAppInstallInfo as input, writes data to disk (e.g icons, shortcuts)
// and registers an app.
class WebAppInstallFinalizer {
 public:
  using InstallFinalizedCallback =
      base::OnceCallback<void(const webapps::AppId& app_id,
                              webapps::InstallResultCode code)>;
  using UninstallWebAppCallback =
      base::OnceCallback<void(webapps::UninstallResultCode code)>;
  using RepeatingUninstallCallback =
      base::RepeatingCallback<void(const webapps::AppId& app_id,
                                   webapps::UninstallResultCode code)>;

  struct FinalizeOptions {
    struct IwaOptions {
      IwaOptions(
          IsolatedWebAppStorageLocation location,
          std::optional<IsolatedWebAppIntegrityBlockData> integrity_block_data);
      ~IwaOptions();
      IwaOptions(const IwaOptions&);

      IsolatedWebAppStorageLocation location;
      std::optional<IsolatedWebAppIntegrityBlockData> integrity_block_data;
    };

    explicit FinalizeOptions(webapps::WebappInstallSource install_surface);
    ~FinalizeOptions();
    FinalizeOptions(const FinalizeOptions&);

    const WebAppManagement::Type source;
    const webapps::WebappInstallSource install_surface;
    proto::InstallState install_state =
        proto::InstallState::INSTALLED_WITH_OS_INTEGRATION;
    bool overwrite_existing_manifest_fields = true;
    bool skip_icon_writes_on_download_failure = false;

    std::optional<WebAppChromeOsData> chromeos_data;
#if BUILDFLAG(IS_CHROMEOS_ASH)
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

    // Controls fetching and validation of web_app_origin_association data
    // from web origins found in manifest scope_extensions entries. If true,
    // do not validate even if scope_extensions has valid entries.
    bool skip_origin_association_validation = false;
  };

  explicit WebAppInstallFinalizer(Profile* profile);
  WebAppInstallFinalizer(const WebAppInstallFinalizer&) = delete;
  WebAppInstallFinalizer& operator=(const WebAppInstallFinalizer&) = delete;
  virtual ~WebAppInstallFinalizer();

  // Write the WebApp data to disk and register the app.
  void FinalizeInstall(const WebAppInstallInfo& web_app_info,
                       const FinalizeOptions& options,
                       InstallFinalizedCallback callback);

  // Write the new WebApp data to disk and update the app.
  // TODO(crbug.com/40759394): Chrome fails to update the manifest
  // if the app window needing update closes at the same time as Chrome.
  // Therefore, the manifest may not always update as expected.
  // Virtual for testing.
  virtual void FinalizeUpdate(const WebAppInstallInfo& web_app_info,
                              InstallFinalizedCallback callback);

  bool CanReparentTab(const webapps::AppId& app_id,
                      bool shortcut_created) const;
  void ReparentTab(const webapps::AppId& app_id,
                   bool shortcut_created,
                   content::WebContents* web_contents);

  void SetProvider(base::PassKey<WebAppProvider>, WebAppProvider& provider);
  void Start();
  void Shutdown();

  Profile* profile() { return profile_; }

  // Writes external config data to the web_app DB, mapped per source.
  void WriteExternalConfigMapInfo(
      WebApp& web_app,
      WebAppManagement::Type source,
      bool is_placeholder,
      GURL install_url,
      std::vector<std::string> additional_policy_ids);

 private:
  using CommitCallback = base::OnceCallback<void(bool success)>;

  void UpdateIsolationDataAndResetPendingUpdateInfo(
      WebApp* web_app,
      const IsolatedWebAppStorageLocation& location,
      const base::Version& version,
      std::optional<IsolatedWebAppIntegrityBlockData> integrity_block_data);

  void SetWebAppManifestFieldsAndWriteData(
      const WebAppInstallInfo& web_app_info,
      std::unique_ptr<WebApp> web_app,
      CommitCallback commit_callback,
      bool skip_icon_writes_on_download_failure);

  void WriteTranslations(
      const webapps::AppId& app_id,
      const base::flat_map<std::string, blink::Manifest::TranslationItem>&
          translations,
      CommitCallback commit_callback,
      bool success);

  void CommitToSyncBridge(std::unique_ptr<WebApp> web_app,
                          CommitCallback commit_callback,
                          bool success);

  void OnOriginAssociationValidated(WebAppInstallInfo web_app_info,
                                    FinalizeOptions options,
                                    InstallFinalizedCallback callback,
                                    webapps::AppId app_id,
                                    ScopeExtensions validated_scope_extensions);

  void OnDatabaseCommitCompletedForInstall(InstallFinalizedCallback callback,
                                           webapps::AppId app_id,
                                           FinalizeOptions finalize_options,
                                           bool success);

  void OnInstallHooksFinished(InstallFinalizedCallback callback,
                              webapps::AppId app_id);
  void NotifyWebAppInstalledWithOsHooks(webapps::AppId app_id);

  bool ShouldUpdateOsHooks(const webapps::AppId& app_id);

  void OnDatabaseCommitCompletedForUpdate(
      InstallFinalizedCallback callback,
      webapps::AppId app_id,
      std::string old_name,
      FileHandlerUpdateAction file_handlers_need_os_update,
      const WebAppInstallInfo& web_app_info,
      bool success);

  void OnUpdateHooksFinished(InstallFinalizedCallback callback,
                             webapps::AppId app_id);

  // Returns a value indicating whether the file handlers registered with the OS
  // should be updated. Used to avoid unnecessary updates. TODO(estade): why
  // does this optimization exist when other OS hooks don't have similar
  // optimizations?
  FileHandlerUpdateAction GetFileHandlerUpdateAction(
      const webapps::AppId& app_id,
      const WebAppInstallInfo& new_web_app_info);

  const raw_ptr<Profile> profile_;
  raw_ptr<WebAppProvider> provider_ = nullptr;

  bool started_ = false;

  base::WeakPtrFactory<WebAppInstallFinalizer> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_FINALIZER_H_
